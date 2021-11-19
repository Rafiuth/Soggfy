//@ts-check

(function Soggfy() {
    //@ts-ignore
    const Platform = document.querySelector("#main")?._reactRootContainer?._internalRoot?.current?.child?.child?.stateNode?.props?.children?.props?.children?.props?.children?.props?.platform;
    const Player = Platform?.getPlayerAPI();
    const CosmosAsync = Player?._cosmos;

    if (!Platform) {
        setTimeout(Soggfy, 500);
        return;
    }
    const Log = function(msg) {
        console.log(msg);
    }

    let g_config = {
        enabled: true,
        downloadLyrics: true,
        embedCoverArt: true,
        outputFormat: {
            args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy",
            ext: "mp3"
        },
        playbackSpeed: 1.0,
        savePaths: {
            track: {
                audio: "{user_home}/Music/Soggfy/{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.ogg",
                cover: "{user_home}/Music/Soggfy/{artist_name}/{album_name}{multi_disc_path}/cover.jpg"
            },
            podcast: {
                audio: "{user_home}/Music/SoggfyPodcasts/{artist_name}/{album_name}/{track_name}.ogg",
                cover: ""
            }
        }
    };
    let g_conn = null;

    function isEnabled()
    {
        return g_config.enabled && g_conn.isConnected;
    }
    class PlayerStateTracker
    {
        constructor()
        {
            this._currState = {
                playback: null,
                playedFromStart: false,
                extraMetadata: null
            };
            Utils.createHook(Player._events._emitter.__proto__, "createEvent", (stage, args, ret) => {
                if (stage !== "pre" || !args[1]) return;

                let eventType = args[0];
                let data = args[1];
                
                switch (eventType) {
                    case "update": {
                        if (data.playbackId) {
                            this._handlePlayerState(data);
                        }
                        break;
                    }
                    case "transport_before_return_response": {
                        if (this._isMetadataReqForCurrTrack(data.response.url, data.response.body)) {
                            this._currState.extraMetadata = data.response.body;
                        }
                        break;
                    }
                }
            });
        }
        _isMetadataReqForCurrTrack(url, body)
        {
            //https://spclient.wg.spotify.com/metadata/4/track/8f738932d94a42b186b7567172012b04?market=from_token
            let match = /metadata\/\d\/track\/([0-9a-f]+)/.exec(url);
            if (match) {
                let hexId = match[1];
                let id = "spotify:track:" + this._hexToId(hexId);
                return id == this._currState.playback?.item?.uri;
            }
            return false;
        }

        //called when the player state changes
        _handlePlayerState(state)
        {
            //detect if track was changed
            if (this._currState.playback?.playbackId !== state.playbackId) {
                if (this._currState.playback?.item) {
                    this._onTrackDone(this._currState);
                }
                Log(`Change track: ${this._currState.playback?.playbackId} '${this._currState.playback?.item?.metadata?.title}' -> ${state.playbackId} '${state.item.metadata.title}' pos=${state.positionAsOfTimestamp}`);
                this._currState = {
                    playback: state,
                    playedFromStart: state.positionAsOfTimestamp === 0,
                    extraMetadata: null
                };
            }
            this._currState.playback = state;
        }

        async _onTrackDone(data)
        {
            let track = data.playback.item;
            let coverData = null;
            let msg = {
                type: this._getTrackType(track.uri),
                playbackId: data.playback.playbackId,
                trackUri: track.uri,
                metadata: {},
                pathVars: {},
                lyrics: "",
                coverArtId: track.metadata.image_xlarge_url.replaceAll(":", "_"),
                save: data.playedFromStart,
            };
            if (msg.save) {
                msg.metadata = msg.type === "track"
                    ? await this._getTrackMetaProps(track, data.extraMetadata)
                    : await this._getPodcastMetaProps(track);
                msg.pathVars = this._getPathVariables(msg.metadata);
                coverData = await this._getImageData(track.metadata.image_xlarge_url);

                let lyrics = g_config.downloadLyrics ? await this._getLyrics(track) : null;
                if (lyrics) {
                    msg.lyrics = this._convertLyricsToLRC(lyrics);
                    msg.lyricsExt = lyrics.isSynced ? "lrc" : "txt";
                    msg.metadata.lyrics = lyrics.lines.map(v => v.text).join('\n');
                }
            }
            Log(`Track done: ${msg.metadata.title} save=${msg.save}`);
            g_conn.sendMessage(Connection.TRACK_DONE, msg, coverData);
        }
        async _getTrackMetaProps(track, extraMeta)
        {
            let meta = track.metadata;
            //https://community.mp3tag.de/t/a-few-questions-about-the-disc-number-disc-total-columns/18698/8
            //https://help.mp3tag.de/main_tags.html
            //TODO: mp3 uses "track: x/n", totaltracks and totaldiscs is vorbis only
            let { year, month, day } = extraMeta.album.date;
            
            return {
                title:          meta.title,
                album_artist:   meta.album_artist_name,
                album:          meta.album_title,
                artist:         this._getMetadataList(meta, "artist_name").join("/"),
                track:          meta.album_track_number,
                totaltracks:    meta.album_track_count,
                disc:           meta.album_disc_number,
                totaldiscs:     meta.album_disc_count,
                date:           [year, month, day].map(Utils.padInt2).join('-'), //YYYY-MM-DD
                publisher:      extraMeta.album.label,
                language:       extraMeta.language_of_performance[0],
                isrc:           extraMeta.external_id.find(v => v.type === "isrc")?.id,
                comment:        this._getTrackUrl(track.uri),
                ITUNESADVISORY: meta.is_explicit ? "1" : undefined
            };
        }
        async _getPodcastMetaProps(track)
        {
            //TODO: get metadata from fetch hook
            let id = track.uri.substring("spotify:episode:".length);
            let meta = await CosmosAsync.get(`https://api.spotify.com/v1/episodes/${id}`);

            return {
                title:      meta.name,
                album:      meta.show.name,
                description: meta.description,
                podcastdesc: meta.show.description,
                podcasturl: meta.external_urls.spotify,
                publisher:  meta.show.publisher,
                date:       meta.release_date,
                language:   meta.language,
                comment:    this._getTrackUrl(track.uri),
                podcast:    "1",
                ITUNESADVISORY: meta.explicit ? "1" : undefined
            };
        }
        _getPathVariables(meta)
        {
            return {
                track_name: meta.title,
                artist_name: meta.album_artist,
                album_name: meta.album,
                track_num: meta.track,
                release_year: meta.date.split('-')[0],
                multi_disc_path: meta.totaldiscs > 1 ? `/CD ${meta.disc}` : "",
                multi_disc_paren: meta.totaldiscs > 1 ? ` CD ${meta.disc}` : ""
            };
        }
        _getMetadataList(meta, key)
        {
            let result = [meta[key]];
            for (let i = 1, val; val = meta[key + ":" + i]; i++) {
                result.push(val);
            }
            return result;
        }
        _getTrackType(uri)
        {
            if (uri.startsWith("spotify:track:")) {
                return "track";
            }
            if (uri.startsWith("spotify:episode:")) {
                return "podcast";
            }
            return "unknown";
        }
        async _getImageData(id)
        {
            //Spotify actually does some blackmagic and sets this id directly to <img> src
            //There's no way to get the original data out of it without reencoding.
            //this EP is public so it's probably ok to do another request
            if (id?.startsWith("spotify:image:")) {
                let url = "https://i.scdn.co/image/" + id.substring("spotify:image:".length);

                let resp = await fetch(url);
                let data = await resp.arrayBuffer();
                return data;
            }
            return null;
        }
        async _getLyrics(track)
        {
            if (track.metadata.has_lyrics !== "true") {
                return null;
            }
            //xpui.js  withPath(`/track/${encodeURIComponent(n)}/image/${encodeURIComponent(t)}`)
            let trackId = track.uri.substring("spotify:track:".length);
            let coverUrl = encodeURIComponent(track.metadata.image_url);
            let url = `https://spclient.wg.spotify.com/color-lyrics/v2/track/${trackId}/image/${coverUrl}?format=json&market=from_token`;
            let data = await CosmosAsync.get(url);
            let lyrics = data.lyrics;

            return {
                lang: lyrics.language,
                isSynced: ["LINE_SYNCED", "SYLLABLE_SYNCED"].includes(lyrics.syncType),
                lines: lyrics.lines.map(ln => ({
                    time: parseInt(ln.startTimeMs),
                    text: ln.words
                }))
            };
        }
        _convertLyricsToLRC(data)
        {
            //https://en.wikipedia.org/wiki/LRC_(file_format)#Simple_format
            //https://github.com/FFmpeg/FFmpeg/blob/master/libavformat/lrcdec.c#L88
            //(number of digits doesn't seem to matter)
            let text = "";
            for (let line of data.lines) {
                if (data.isSynced) {
                    let time = line.time;
                    let mm = Utils.padInt2(time / 1000 / 60);
                    let ss = Utils.padInt2(time / 1000 % 60);
                    let cs = Utils.padInt2(time % 1000 / 10);
                    text += `[${mm}:${ss}.${cs}]`;
                }
                text += line.text + '\n';
            }
            return text;
        }
        _getTrackUrl(id)
        {
            let parts = id.split(':');
            return `https://open.spotify.com/${parts[1]}/${parts[2]}`;
        }

        /**
         * Converts a hex string into a base62 spotify id
         * @param {string} str
         */
        _hexToId(str)
        {
            const alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

            let val = BigInt("0x" + str);
            let digits = [];
            while (val != 0n) {
                let digit = Number(val % 62n);
                val /= 62n;
                digits.push(alphabet.charAt(digit));
            }
            return digits.reverse().join('');
        }
    }
    class Connection
    {
        static TRACK_DONE       = 1;  //C -> S
        static SYNC_CONFIG      = 2;  //C <> S
        static READY            = -1; //Internal
        static CLOSED           = -2; //Internal
        
        constructor(msgHandler)
        {
            this._msgHandler = msgHandler;
            this._reconnect();
        }
        _reconnect()
        {
            this._ws = new WebSocket("ws://127.0.0.1:28653/sgf_ctrl");
            this._ws.binaryType = "arraybuffer";
            this._ws.onmessage = this._onMessage.bind(this);
            this._ws.onopen = () => {
                this._msgHandler(Connection.READY, {});
            };
            this._ws.onclose = (ev) => {
                if (ev.code != 1006) {
                    Log(`Control server connection closed unexpectedly. code=${ev.code} reason='${ev.reason}'`);
                }
                this._msgHandler(Connection.CLOSED, {});
                setTimeout(this._reconnect.bind(this), 15000);
            };
        }

        get isConnected()
        {
            return this._ws.readyState === WebSocket.OPEN;
        }

        /**
         * @param {number} type
         * @param {any} content
         * @param {ArrayBuffer} binaryContent
         */
        sendMessage(type, content, binaryContent = null)
        {
            let contentStr = JSON.stringify(content);
            let contentData = new TextEncoder().encode(contentStr);
            let data = new Uint8Array(5 + contentData.length + (binaryContent?.byteLength ?? 0));
            let view = new DataView(data.buffer);
            view.setUint8(0, type);
            view.setInt32(1, contentData.length, true);
            data.set(contentData, 5);
            if (binaryContent) {
                data.set(new Uint8Array(binaryContent), 5 + contentData.length);
            }
            this._ws.send(data);
        }

        _onMessage(ev)
        {
            let view = new DataView(ev.data);
            let type = view.getUint8(0);
            let contentLen = view.getInt32(1, true);
            let contentData = new Uint8Array(ev.data, 5, contentLen);
            let binaryContent = new Uint8Array(ev.data, 5 + contentLen);

            let contentStr = new TextDecoder().decode(contentData);
            let content = JSON.parse(contentStr);

            this._msgHandler(type, content, binaryContent);
        }
    }
    class Utils
    {
        static createHook(obj, funcName, detour)
        {
            //https://stackoverflow.com/a/62333813
            let originalFunc = obj[funcName];
            obj[funcName] = function (...args) {
                detour("pre", args);

                let ret = originalFunc.apply(this, args);

                if (ret instanceof Promise) {
                    return ret.then(val => {
                        detour("post", args, val);
                        return val;
                    });
                } else {
                    detour("post", args, ret);
                    return ret;
                }
            }
        }
        static padInt2(x, digits = 2)
        {
            return Math.floor(x).toString().padStart(digits, '0');
        }

        /**
         * Searches for a path given a root object
         * @param {Set<string>} props A set of properties to look for
         * @param {string[]} path The current path, should be empty when calling this
         * @param {Set<any>} visitedObjs 
         * @returns 
         */
        static findPath(obj, props, path, visitedObjs)
        {
            visitedObjs.add(obj);

            for (let key in obj) {
                if (props.has(key)) {
                    return true;
                }
                let child = obj[key];
                if (!child || child instanceof String || child instanceof Number || visitedObjs.has(child)) continue;

                path.push(key);
                if (Utils.findPath(child, props, path, visitedObjs)) {
                    return path;
                }
                path.pop();
            }
        }
        
        static objectPathIndexer(obj, key, newValue = undefined)
        {
            let path = key.split('.');
            let lastField = path.pop();
            for (let field of path) {
                obj = obj[field];
            }
            if (newValue !== undefined) {
                obj[lastField] = newValue;
            }
            return obj[lastField];
        }
    }
    class UI
    {
        static _settingsStyle = `
.sgf-settings-overlay {
    display: flex;
    align-items: center;
    justify-content: center;

    background-color: rgba(0, 0, 0, 0.7);
    position: absolute;
    width: 100%;
    height: 100%;
    top: 0;
    left: 0;
    right: 0;
    bottom: 0;
    overflow: hidden;
    z-index: 999;

    user-select: none;
}
.sgf-settings-modal {
    width: 40rem;
    height: 90%;
    display: block;
}
.sgf-settings-container {
    background-color: #333;
    border-radius: 10px;
    box-shadow: 0px 0px 8px 4px rgb(0, 0, 0, 0.15);
    height: inherit;
    display: flex;
    flex-direction: column;
}
.sgf-settings-header {
    display: flex;
    align-items: baseline;
    border-bottom: 1px solid rgba(255,255,255,.1);
    justify-content: space-between;
    padding: 32px 32px 12px;
}
.sgf-header-title { /* main-type-alto */
    font-size: 32px;
    font-weight: 700;
    letter-spacing: -.04em;
    line-height: 36px;
    text-transform: none;
}
.sgf-settings-closeBtn {
    background-color: transparent;
    border: 0;
    padding: 8px;
}
.sgf-settings-closeBtn:hover {
    transform: scale(1.1);
}
.sgf-settings-closeBtn:active {
    transform: scale(0.9);
}

.sgf-settings-elements {
    overflow: auto;
    padding: 16px 32px 0;
}

.sgf-setting-row {
    display: flex;
    align-items: center;
    flex-direction: row;
    margin: 4px 0px;
    min-height: 37px;
}
.sgf-setting-row .col.description {
    float: left;
    padding-right: 15px;
    cursor: default;
    flex: 1;
}
.sgf-setting-row .col.action {
    float: right;
    text-align: right;
}

.sgf-setting-section {

}

.sgf-toggle-switch {
    appearance: none;
    display: inline-block;
    position: relative;
    width: 42px;
    height: 24px;
    border-radius: 24px;
    background-color: #535353;
    transition: all 0.1s ease;
}
.sgf-toggle-switch:after {
    content: "";
    position: absolute;
    top: 2px;
    left: 2px;
    width: 20px;
    height: 20px;
    border-radius: 50%;
    background: #fff;
    box-shadow: 0 0 4px rgb(0 0 0 / 20%);
    transition: all 0.1s ease;
}
.sgf-toggle-switch:checked {
    background-color: #1db954;
}
.sgf-toggle-switch:checked:after {
    transform: translatex(18px)
}

.sgf-select {
    width: 100%;
    border-radius: 4px;
    border: 0;
    background-color: #555;
    color: #ddd;
    font-family: arial;
    font-size: 14px;
    font-weight: 400;
    height: 32px;
    line-height: 20px;
    padding: 0 32px 0 12px;
}

.sgf-text-input {
    width: 100%;
    height: 32px;
    padding: 0 5px;
    border-radius: 3px;
    border: 0;
    background-color: #555;
    color: #ddd;
    font-family: arial;
    font-size: 14px;
    font-weight: 400;
}

.sgf-slider-wrapper {
    display: flex;
    flex-direction: row;
    align-items: center;
}
.sgf-slider {
    appearance: none;
    position: relative;
    width: 100%;
    height: 8px;
    border-radius: 4px;
    background: #535353;
    outline: none;
}
.sgf-slider::-webkit-slider-thumb {
    appearance: none;
    width: 16px;
    height: 16px;
    border-radius: 50%;
    background: #1db954;
}
.sgf-slider-label {
    margin-right: 4px;
    text-align: right;
    font-size: 11px;
}
`;
        
        static _settingsButton;
        
        static install()
        {
            this._settingsButton = this.addTopbarButton(
                "Soggfy",
                //TODO: design a icon for this
                //https://fonts.google.com/icons?selected=Material+Icons:settings&icon.query=down
                `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M18,15v3H6v-3H4v3c0,1.1,0.9,2,2,2h12c1.1,0,2-0.9,2-2v-3H18z M17,11l-1.41-1.41L13,12.17V4h-2v8.17L8.41,9.59L7,11l5,5 L17,11z"></path></svg>`,
                () => document.body.append(this.createSettingsDialog())
            );
        }
        static setClickable(val)
        {
            this._settingsButton.disabled = !val;
        }
        
        static createSettingsDialog()
        {
            let onChange = (key, newValue) => {
                let finalValue = Utils.objectPathIndexer(g_config, key, newValue);

                if (g_conn.isConnected && newValue !== undefined) {
                    let delta = {};
                    let field = key.split('.')[0]; //sync only supports topmost field
                    delta[field] = g_config[field];
                    g_conn.sendMessage(Connection.SYNC_CONFIG, delta);
                }
                return finalValue;
            };
            
            let defaultFormats = {
                "Original OGG":     { ext: "",    args: "-c copy" },
                "MP3 320K":         { ext: "mp3", args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy" },
                "MP3 192K":         { ext: "mp3", args: "-c:a libmp3lame -b:a 192k -id3v2_version 3 -c:v copy" },
                "M4A 256K (AAC)":   { ext: "m4a", args: "-c:a aac -b:a 256k -disposition:v attached_pic -c:v copy" },
                "M4A 160K (AAC)":   { ext: "m4a", args: "-c:a aac -b:a 160k -disposition:v attached_pic -c:v copy" },
                "Opus 160K":        { ext: "opus",args: "-c:a libopus -b:a 160k" },
                "Custom":           { ext: "mp3", args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy" },
            };
            let customFormatSection = this.createSubSection(
                this.createRowTable("FFmpeg arguments", this.createTextInput("outputFormat.args", onChange)),
                //TODO: allow this to be editable
                this.createRow("Extension",             this.createSelect("outputFormat.ext", ["mp3","m4a","mp4","mkv","mka","ogg","opus"], onChange))
            );
            customFormatSection.style.display = "none";
            
            let onFormatChange = (key, name) => {
                if (name === undefined) {
                    let currFormat = g_config.outputFormat;
                    let preset =
                        Object.entries(defaultFormats)
                              .find(v => v[1].args === currFormat.args && v[1].ext === currFormat.ext);
                    name = preset?.[0] ?? "Custom";
                } else {
                    onChange("outputFormat", defaultFormats[name]);
                }
                customFormatSection.style.display = name === "Custom" ? "block" : "none";
                return name;
            };
            let updateCoverPath = (key, save) => {
                let newPath = undefined;
                //`save` may also be `undefined`
                if (save === true) {
                    //save cover art in the first directory containing the album_name variable
                    let path = onChange(key.replace(".cover", ".audio"));
                    let parts = path.split(/[\/\\\\]/);
                    let albumDirIdx = parts.findIndex(d => d.includes("{album_name}"));
                    let hasAlbumDir = albumDirIdx >= 0 && albumDirIdx + 1 < parts.length;
                    newPath = hasAlbumDir ? parts.slice(0, albumDirIdx + 1).join('/') + "/cover.jpg" : "";
                } else if (save === false) {
                    newPath = "";
                }
                return onChange(key, newPath) !== "";
            };
            
            return this.createSettingOverlay(
                this.createSection("General",
                    this.createRow("Output format",     this.createSelect("outputFormat", Object.getOwnPropertyNames(defaultFormats), onFormatChange)),
                    customFormatSection,
                    this.createRow("Embed cover art",   this.createToggle("embedCoverArt", onChange)),
                    this.createRow("Download lyrics",   this.createToggle("downloadLyrics", onChange)),
                    this.createRow("Playback speed",    this.createSlider("playbackSpeed", 1, 30, 1, val => val + "x", onChange))
                ),
                this.createSection("Download Paths",
                    this.createRowTable("Songs",        this.createTextInput("savePaths.track.audio", onChange)),
                    this.createRowTable("Podcasts",     this.createTextInput("savePaths.podcast.audio", onChange)),
                    this.createRow("Save cover art separately", this.createToggle("saveCoverArt", (key, newValue) => {
                        return updateCoverPath("savePaths.track.cover", newValue) || 
                               updateCoverPath("savePaths.podcast.cover", newValue);
                    })),
                )
            );
        }
        static createSettingOverlay(...elements)
        {
            let node = document.createElement("div");
            node.style.display = "block";
            node.innerHTML = `
<style>${this._settingsStyle}</style>
<div class="sgf-settings-overlay">
    <div class="sgf-settings-modal" tabindex="1" role="dialog">
        <div class="sgf-settings-container">
            <div class="sgf-settings-header">
                <h1 class="sgf-header-title" as="h1">Soggfy settings</h1>

                <button aria-label="Close" class="sgf-settings-closeBtn">
                    <svg width="18" height="18" viewBox="0 0 32 32" xmlns="http://www.w3.org/2000/svg">
                        <title>Close</title>
                        <path d="M31.098 29.794L16.955 15.65 31.097 1.51 29.683.093 15.54 14.237 1.4.094-.016 1.508 14.126 15.65-.016 29.795l1.414 1.414L15.54 17.065l14.144 14.143" fill="currentColor" fill-rule="evenodd"></path>
                    </svg>
                </button>
            </div>
            <div class="sgf-settings-elements">
            </div>
        </div>
    </div>
</div>`;
            node.querySelector(".sgf-settings-elements").append(...elements);
            node.querySelector(".sgf-settings-closeBtn").onclick = () => node.remove();
            node.querySelector(".sgf-settings-overlay").onclick = (ev) => {
                if (!node.querySelector(".sgf-settings-container").contains(ev.target)) {
                    node.remove();
                }
            };
            return node;
        }
        
        static addTopbarButton(title, icon, callback)
        {
            let backButton = document.querySelector(".Root__top-bar").querySelector("button");
            let topbarContainer = backButton.parentElement;

            let button = document.createElement("button");
            button.className = backButton.classList[0];
            button.innerHTML = icon;
            button.onclick = callback;
            topbarContainer.append(button);

            return button;
        }

        static createToggle(key, callback = null)
        {
            let node = document.createElement("input");
            node.className = "sgf-toggle-switch";
            node.type = "checkbox";
            node.checked = callback(key);

            if (callback) {
                node.onchange = () => callback(key, node.checked);
            }
            return node;
        }
        static createSelect(key, options, callback = null)
        {
            let node = document.createElement("select");
            node.className = "sgf-select";
            
            for (let i = 0; i < options.length; i++) {
                let opt = document.createElement("option");
                opt.setAttribute("value", i.toString());
                opt.innerText = options[i];

                node.appendChild(opt);
            }
            node.value = options.indexOf(callback(key)).toString();
            
            if (callback) {
                node.onchange = () => callback(key, options[parseInt(node.value)]);
            }
            return node;
        }
        static createTextInput(key, callback = null)
        {
            let node = document.createElement("input");
            node.className = "sgf-text-input";
            node.value = callback(key);
            
            if (callback) {
                node.onchange = () => callback(key, node.value);
            }
            return node;
        }
        static createSlider(key, min, max, step = 1, formatter = null, callback = null)
        {
            formatter ??= x => x.toString();
            let initialValue = callback(key);
            
            let node = document.createElement("div");
            node.className = "sgf-slider-wrapper";
            node.innerHTML = `
<span class="sgf-slider-label"></span>
<input class="sgf-slider" type="range" min="${min}" max="${max}" step="${step}" value="${initialValue}">
`;
            let input = node.querySelector("input");
            let label = node.querySelector("span");
            label.innerText = formatter(initialValue);

            input.oninput = () => {
                label.innerText = formatter(parseFloat(input.value));
            };
            if (callback) {
                input.onchange = () => callback(key, parseFloat(input.value));
            }
            return node;
        }

        static createRow(desc, action)
        {
            let node = document.createElement("div");
            node.className = "sgf-setting-row";
            node.innerHTML = `
<label class="col description">${desc}</label>
<div class="col action"></div>`;
            node.querySelector(".action").appendChild(action);
            return node;
        }
        static createRowTable(desc, ...elements)
        {
            let node = document.createElement("div");
            node.className = "sgf-setting-row-table";
            node.innerHTML = `
<label class="col description">${desc}</label>`;
            node.append(...elements);
            return node;
        }
        static createSection(desc, ...elements)
        {
            let node = document.createElement("div");
            node.className = "sgf-setting-section";
            node.innerHTML = `<h2>${desc}</h2>`;
            node.append(...elements);
            return node;
        }
        static createSubSection(...elements)
        {
            let node = document.createElement("div");
            node.className = "sgf-setting-section";
            node.style.marginLeft = "20px";
            node.append(...elements);
            return node;
        }
    }

    g_conn = new Connection((type, content, binaryContent) => {
        switch (type) {
            case Connection.READY: {
                UI.setClickable(true);
                break;
            }
            case Connection.CLOSED: {
                UI.setClickable(false);
                break;
            }
            case Connection.SYNC_CONFIG: {
                for (let key in content) {
                    g_config[key] = content[key];
                }
                break;
            }
        }
    });
    
    UI.install();
    new PlayerStateTracker();
})();