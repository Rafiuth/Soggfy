//@ts-check
//TODO: typescript port?

(function Soggfy() {
    //@ts-ignore
    const Platform = document.querySelector("#main")?._reactRootContainer?._internalRoot?.current?.child?.child?.stateNode?.props?.children?.props?.children?.props?.children?.props?.platform;
    const Player = Platform?.getPlayerAPI();
    const CosmosAsync = Player?._cosmos;
    const WebAPI = Platform?.getAdManagers()?.hpto?.hptoApi?.webApi; //TODO: this api reports telemetry, is it a good idea to use it?

    if (!Platform) {
        setTimeout(Soggfy, 500);
        return;
    }
    //these will be changed using string replace by JsInjector, don't change.
    const IS_INJECTED = false;
    const IS_DEBUG = false || !IS_INJECTED;

    const Log = function (...args) {
        if (IS_DEBUG) {
            console.log(...args);
        }
    }

    let g_config = {
        enabled: true,
        downloadLyrics: true,
        embedCoverArt: true,
        outputFormat: {
            args: "-c copy",
            ext: ""
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
    let g_playerTracker;
    let g_playlistHook;

    function isEnabled()
    {
        return g_config.enabled && g_conn.isConnected;
    }
    class PlayerStateTracker
    {
        constructor()
        {
            this._playbacks = new Map();

            Utils.createHook(Player._events._emitter.__proto__, "createEvent", (stage, args, ret) => {
                let eventType = args[0];
                let data = args[1];
                if (stage === "pre" && eventType === "update" && data?.playbackId) {
                    if (!this._playbacks.has(data.playbackId)) {
                        this._playbacks.set(data.playbackId, data);
                    }
                }
            });
        }

        /** Returns limited track metadata for a given playbackId, or null. */
        getPlaybackTrack(playbackId)
        {
            return this._playbacks.get(playbackId)?.item;
        }

        async getPlaybackMetadata(playbackId, remove = true)
        {
            let playback = this._playbacks.get(playbackId);
            if (remove) {
                this._playbacks.delete(playbackId);
            }
            let track = playback.item;
            let type = this._getTrackType(track.uri);
            let meta = type === "track"
                ? await this._getTrackMetaProps(track)
                : await this._getPodcastMetaProps(track);
            
            let data = {
                type: type,
                playbackId: playback.playbackId,
                trackUri: track.uri,
                metadata: meta,
                pathVars: this._getPathVariables(meta),
                lyrics: "",
                lyricsExt: "",
                coverArtId: track.metadata.image_xlarge_url.replaceAll(":", "_")
            };
            let coverData = await Resources.getImageData(track.metadata.image_xlarge_url);

            let lyrics = g_config.downloadLyrics ? await this._getLyrics(track) : null;
            if (lyrics) {
                data.lyrics = this._convertLyricsToLRC(lyrics);
                data.lyricsExt = lyrics.isSynced ? "lrc" : "txt";
                data.metadata.lyrics = lyrics.lines.map(v => v.text).join('\n');
            }
            return { info: data, coverData: coverData };
        }
        async _getTrackMetaProps(track)
        {
            let meta = track.metadata;
            let extraMeta = await Resources.getTrackMetadataWG(track.uri);

            //https://community.mp3tag.de/t/a-few-questions-about-the-disc-number-disc-total-columns/18698/8
            //https://help.mp3tag.de/main_tags.html
            //TODO: mp3 uses "track: x/n", totaltracks and totaldiscs is vorbis only
            let { year, month, day } = extraMeta.album.date;
            
            return {
                title:          meta.title,
                album_artist:   meta.artist_name,
                album:          meta.album_title,
                artist:         extraMeta.artist.map(v => v.name).join("/"),
                track:          meta.album_track_number,
                totaltracks:    meta.album_track_count,
                disc:           meta.album_disc_number,
                totaldiscs:     meta.album_disc_count,
                date:           [year, month, day].map(Utils.padInt2).join('-'), //YYYY-MM-DD
                publisher:      extraMeta.album.label,
                language:       extraMeta.language_of_performance[0],
                isrc:           extraMeta.external_id.find(v => v.type === "isrc")?.id,
                comment:        Resources.getOpenTrackURL(track.uri),
                ITUNESADVISORY: meta.is_explicit ? "1" : undefined
            };
        }
        async _getPodcastMetaProps(track)
        {
            let meta = await Resources.getEpisodeMetadata(track.uri);

            return {
                title:          meta.name,
                album:          meta.show.name,
                album_artist:   meta.show.publisher,
                description:    meta.description,
                podcastdesc:    meta.show.description,
                podcasturl:     meta.external_urls.spotify,
                publisher:      meta.show.publisher,
                date:           meta.release_date,
                language:       meta.language,
                comment:        Resources.getOpenTrackURL(track.uri),
                podcast:        "1",
                ITUNESADVISORY: meta.explicit ? "1" : undefined
            };
        }
        _getPathVariables(meta)
        {
            return {
                track_name:     meta.title,
                artist_name:    meta.album_artist,
                album_name:     meta.album,
                track_num:      meta.track,
                release_year:   meta.date.split('-')[0],
                multi_disc_path: meta.totaldiscs > 1 ? `/CD ${meta.disc}` : "",
                multi_disc_paren: meta.totaldiscs > 1 ? ` (CD ${meta.disc})` : ""
            };
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
        async _getLyrics(track)
        {
            if (track.metadata.has_lyrics !== "true") {
                return null;
            }
            let resp = await Resources.getColorAndLyricsWG(track.uri, track.metadata.image_url);
            let lyrics = resp.lyrics;

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
    }

    //Message flow:
    //client = js, server = cpp
    //SYNC_CONFIG: {...}
    //  Always sent by the server after connection is established;
    //  Client only sends toplevel fields changes.
    //TRACK_META:
    //  Sent by server when a track is done playing and ready to be saved, 
    //  the client responds with all metadata required to save the track.
    //  Note that the client will remove the playbackId from the tracking map.
    //DOWNLOAD_STATUS:
    //  Sent by client to populate playlist download status indicator
    //  Server responds with a list of { tracks: [path|null+reason]}.
    //  Server also sends it once a download is aborted or completed, with the respective playbackId.
    //OPEN_FOLDER:
    //  Sent by client to open a file explorer window with the specified file selected.
    class Connection
    {
        static SYNC_CONFIG      = 1;  //C <> S
        static TRACK_META       = 2;  //C <> S
        static DOWNLOAD_STATUS  = 3;  //S <> S
        static OPEN_FOLDER      = 4;  //C -> S
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
        /** 
         * Gets or sets a nested field specified by the path array
         * Ex: accessObjectPath(obj, ["track","metadata","name"]) -> obj.track.metadata.name
         * @param {any} obj
         * @param {string[]} path
         * @param {any} newValue Value to set the final field
         */
        static accessObjectPath(obj, path, newValue = undefined)
        {
            let lastField = path.at(-1);
            for (let i = 0; i < path.length - 1; i++) {
                obj = obj[path[i]];
            }
            if (newValue !== undefined) {
                obj[lastField] = newValue;
            }
            return obj[lastField];
        }
    }
    class Resources
    {
        static async getTrackMetadataWG(uri)
        {
            let trackId = this.idToHex(this.getUriId(uri, "track"));
            let res = await WebAPI.spotifyTransport.build()
                .withHost("https://spclient.wg.spotify.com/metadata/4")
                .withPath(`/track/${trackId}`)
                .withEndpointIdentifier("/track/{trackId}")
                .send();
            return res.body;
        }
        static async getColorAndLyricsWG(trackUri, coverUri)
        {
            let res = await WebAPI.spotifyTransport.build()
                .withHost("https://spclient.wg.spotify.com/color-lyrics/v2")
                .withPath(`/track/${this.getUriId(trackUri)}/image/${encodeURIComponent(coverUri)}`)
                .withQueryParameters({
                    format: "json",
                    vocalRemoval: false
                }).withEndpointIdentifier("/track/{trackId}")
                .send();
            return res.body;
        }
        static async getEpisodeMetadata(uri)
        {
            return await WebAPI.getEpisode(this.getUriId(uri, "episode"));
        }
        static async getAlbumTracks(uri, offset, limit)
        {
            let id = this.getUriId(uri, "album");
            let params = {
                offset: offset,
                limit: limit
            };
            let ep = WebAPI.endpoints.Album;
            let resp = await ep.getAlbumTracks(WebAPI.spotifyTransport, id, params);
            return resp.body;
        }

        static async getImageData(uri)
        {
            //Spotify actually does some blackmagic and sets this id directly to <img> src
            //There's no way to get the original data out of it without reencoding.
            //this EP is public so it's probably ok to do another request
            let url = "https://i.scdn.co/image/" + this.getUriId(uri, "image");
            let resp = await fetch(url);
            return await resp.arrayBuffer();
        }

        static getOpenTrackURL(uri)
        {
            let parts = uri.split(':');
            return `https://open.spotify.com/${parts[1]}/${parts[2]}`;
        }
        static getUriId(uri, expType = null)
        {
            let parts = uri.split(':');
            if (expType && parts[1] !== expType) {
                throw Error(`Expected URI of type '${expType}', got '${uri}'`);
            }
            return parts[2];
        }

        /** Converts a hex string into a base62 spotify id */
        static hexToId(str)
        {
            const alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

            let val = BigInt("0x" + str);
            let digits = [];
            while (digits.length < 22) {
                let digit = Number(val % 62n);
                val /= 62n;
                digits.push(alphabet.charAt(digit));
            }
            return digits.reverse().join('');
        }
        /** Converts a base62 spotify id to a hex string */
        static idToHex(str)
        {
            const alphabet = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";

            let val = 0n;
            for (let i = 0; i < str.length; i++) {
                let digit = alphabet.indexOf(str.charAt(i));
                val = (val * 62n) + BigInt(digit);
            }
            return val.toString(16).padStart(32, '0');
        }
    }
    class PlaylistHook
    {
        constructor()
        {
            this._obs = new MutationObserver((mutations) => {
                let dirtyRows = [];
                for (let mut of mutations) {
                    for (let node of mut.addedNodes) {
                        if (node.parentElement?.matches('div[role="presentation"]:not([class])')) {
                            dirtyRows.push(node.firstChild);
                        }
                    }
                }
                if (dirtyRows.length > 0) {
                    this._sendUpdateRequest(dirtyRows);
                }
            });
            let container = document.querySelector(".main-view-container__scroll-node-child");
            this._obs.observe(container, {
                subtree: true,
                childList: true
            });
        }

        updateRows(tracks)
        {
            let listSection = document.querySelector('section[data-testid="playlist-page"],[data-testid="album-page"]');
            let listDiv = listSection.querySelector('div[tabindex="0"]');
            let listDivRows = listDiv.querySelector('div[role="presentation"]:not([class])');

            for (let rowWrapper of listDivRows.children) {
                let row = rowWrapper.firstElementChild;
                let info = this._getRowTrackInfo(row, listSection);
                let status = tracks[info.trackUri];
                if (!status) continue;
                let ok = !!status.path;

                if (!row["__sgf_state_elem"]) {
                    let node = document.createElement("div");
                    node.className = "sgf-status-indicator";
                    node.innerHTML = `
<div class="sgf-status-indicator-card">
    ${ok
        ? `
<div class="sgf-status-browse-button">
    <svg width="24" height="24" viewBox="0 0 24 24" fill="#ddd">
        <path xmlns="http://www.w3.org/2000/svg" d="M20 6h-8l-2-2H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm0 12H4V8h16v10z"></path>
    </svg>
    <span style="padding-left: 4px; font-size: 16px; color: #ddd;">Open Folder</span>
</div>`
        : `
    <span style="font-size: 16px; color: #ddd;">${status.errorMessage}</span>
`
    }
</div>
<svg role="img" height="16" width="16" viewBox="0 0 16 16" fill="${ok ? "#33ff33" : "#ff2424"}" style="margin-top: 2px;">
    ${ok 
        ? `<path d="M13.985 2.383L5.127 12.754 1.388 8.375l-.658.77 4.397 5.149 9.618-11.262z"></path>`
        : `<path d="M14.354 2.353l-.708-.707L8 7.293 2.353 1.646l-.707.707L7.293 8l-5.647 5.646.707.708L8 8.707l5.646 5.647.708-.708L8.707 8z"></path>`
    }
</svg>`;
                    let infoColDiv = row.lastElementChild;
                    infoColDiv.prepend(node);

                    /** @type HTMLDivElement */
                    let browseBtn = node.querySelector(".sgf-status-browse-button");
                    if (browseBtn) {
                        browseBtn.onclick = () => {
                            g_conn.sendMessage(Connection.OPEN_FOLDER, { path: status.path });
                        };
                    }
                    row["__sgf_state_elem"] = node;
                }
            }
        }
        _sendUpdateRequest(dirtyRows)
        {
            let tracks = [];
            let listSection = document.querySelector('section[data-testid="playlist-page"],[data-testid="album-page"]');
            
            for (let row of dirtyRows) {
                let info = this._getRowTrackInfo(row, listSection);

                tracks.push({
                    uri: info.trackUri,
                    vars: {
                        track_name: info.trackName,
                        artist_name: info.artistName,
                        album_name: info.albumName,
                    },
                    unkVars: {
                        track_num: `\d+`,
                        release_year: `\d+`,
                        multi_disc_path: `(\/CD \d+)?`,
                        multi_disc_paren: `( \(CD \d+\))?`,
                        _ext: `\.(mp3|m4a|mp4|ogg|opus)$`
                    }
                });
            }
            g_conn.sendMessage(Connection.DOWNLOAD_STATUS, {
                pathTemplate: g_config.savePaths.track.audio.replace(/\.\w+$/, "{_ext}"),
                tracks: tracks
            });
        }
        _getRowTrackInfo(row, listSection)
        {
            let albumName =
                row.querySelector('a[href^="/album"]')?.innerText ??
                listSection.querySelector('section[data-testid="album-page"] span h1').innerText;
            
            let menuBtn = row.querySelector(UI.cssSelectors.rowMoreButton);
            let extraProps = UI.getReactProps(row, menuBtn).menu.props;

            return {
                trackUri: extraProps.uri,
                trackName: row.querySelector(UI.cssSelectors.rowTitle).innerText,
                artistName: row.querySelector(UI.cssSelectors.rowSubTitle).firstChild.innerText,
                albumName: albumName
            };
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
    width: 64px;
    margin-right: 4px;
    text-align: right;
    font-size: 11px;
    background: #333;
    border: none;
}
.sgf-slider-label:hover, .sgf-slider-label:focus {
    border: solid;
    border-width: 1px;
    border-color: #444;
}
`;
        static _trackStatusStyle = `
.sgf-status-indicator {
    background: transparent;
    border: 0;
}
.sgf-status-indicator-card {
    display: flex;
    position: absolute;
    background: #222;
    border-radius: 4px;
    top: -18px;
    padding: 4px;
    transform: translateX(-50%);
    box-shadow: 2px 2px 6px 4px rgb(0 0 0 / 25%);
    opacity: 0;
    transition: opacity 0.1s ease-out 0.5s;
}
.sgf-status-indicator:hover .sgf-status-indicator-card {
    opacity: 1;
    height: auto;
}
.sgf-status-browse-button {
    background: transparent;
    border: 0;
    height: 24px;
    display: flex;
    cursor: pointer;
}`;
        
        static _styleElement;
        static _settingsButton;
        static cssSelectors;

        static async install()
        {
            this.cssSelectors = await this.extractCssMappings(
                "rowTitle", "rowSubTitle", "rowSectionEnd", "trackListRow", "rowMoreButton"
            );
            this._styleElement = document.createElement("style");
            this._styleElement.innerHTML = [this._settingsStyle, this._trackStatusStyle].join('\n');
            document.head.append(this._styleElement);

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

        /** 
         * Extracts the specified CSS class mappings from xpui.js.
         * Note: This function is expansive and results should be cached.
         */
        static async extractCssMappings(...names)
        {
            let req = await fetch("/xpui.js");
            let js = await req.text();

            let pattern = `(${names.join('|')}):\\s*"(.+?)"`;
            let regex = new RegExp(pattern, "g");
            let results = {};
            
            let match;
            while (match = regex.exec(js)) {
                let key = match[1];
                let val = match[2];
                results[key] = "." + val;
            }
            return results;
        }

        /**
         * Returns the properties from a react element.
         * @param {Element} rootElem A react element that is the parent of the target.
         * @param {Element} targetElem The child element to get properties for.
         */
        static getReactProps(rootElem, targetElem)
        {
            const keyof_ReactEventHandlers =
                Object.keys(rootElem)
                      .find(k => k.startsWith("__reactEventHandlers$"));
            
            //find the path from elem to target
            let path = [];
            let node = targetElem;
            while (node !== rootElem) {
                let parent = node.parentElement;
                let index = 0;
                for (let child of parent.children) {
                    if (child[keyof_ReactEventHandlers]) index++;
                    if (child === node) break;
                }
                path.push({ next: node, index: index });
                node = parent;
            }
            //now find the react state
            let state = node[keyof_ReactEventHandlers];
            for (let i = path.length - 1; i >= 0; i--) {
                let loc = path[i];
                
                //find the react state children, ignoring "non element" children
                let childStateIndex = 0;
                let childElemIndex = 0;
                while (childStateIndex < state.children.length) {
                    let isElem = state.children[childStateIndex] instanceof Object;
                    if (isElem && ++childElemIndex === loc.index) break;
                    childStateIndex++;
                }
                state = state.children[childStateIndex].props;
                node = loc.next;
            }
            return state;
        }
        
        static createSettingsDialog()
        {
            let onChange = (key, newValue) => {
                let finalValue = Utils.accessObjectPath(g_config, key.split('.'), newValue);

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
                "MP3 256K":         { ext: "mp3", args: "-c:a libmp3lame -b:a 256k -id3v2_version 3 -c:v copy" },
                "MP3 192K":         { ext: "mp3", args: "-c:a libmp3lame -b:a 192k -id3v2_version 3 -c:v copy" },
                "M4A 256K (AAC)":   { ext: "m4a", args: "-c:a aac -b:a 256k -disposition:v attached_pic -c:v copy" }, //TODO: aac quality disclaimer / libfdk
                "M4A 192K (AAC)":   { ext: "m4a", args: "-c:a aac -b:a 192k -disposition:v attached_pic -c:v copy" },
                "Opus 160K":        { ext: "opus",args: "-c:a libopus -b:a 160k" },
                "Custom":           { ext: "mp3", args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy" },
            };
            let customFormatSection = this.createSubSection(
                this.createRowTable("FFmpeg arguments", this.createTextInput("outputFormat.args", onChange)),
                //TODO: allow this to be editable
                this.createRow("Extension",             this.createSelect("outputFormat.ext", ["mp3","m4a","mp4","ogg","opus"], onChange))
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
            //@ts-ignore
            node.querySelector(".sgf-settings-closeBtn").onclick = () => node.remove();
            //@ts-ignore
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
<input class="sgf-slider-label"></input>
<input class="sgf-slider" type="range" min="${min}" max="${max}" step="${step}" value="${initialValue}">
`;
            /** @type {HTMLInputElement} */
            let slider = node.querySelector(".sgf-slider");
            /** @type {HTMLInputElement} */
            let label = node.querySelector(".sgf-slider-label");
            label.value = formatter(initialValue);
            
            let updateLabel = () => label.value = formatter(parseFloat(slider.value));
            slider.oninput = updateLabel;
            
            //remove custom format when editing via the textbox
            label.oninput = () => slider.value = label.value;
            label.onfocus = () => label.value = slider.value;
            label.onblur = updateLabel;
            
            if (callback) {
                let fireCallback = () => callback(key, parseFloat(slider.value));
                label.onchange = fireCallback;
                slider.onchange = fireCallback;
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

    g_playerTracker = new PlayerStateTracker();
    UI.install();
    g_playlistHook = new PlaylistHook();

    g_conn = new Connection((type, content, binaryContent) => {
        switch (type) {
            case Connection.READY: {
                UI.setClickable(true);
                break;
            }
            case Connection.CLOSED: {
                if (!IS_DEBUG) {
                    UI.setClickable(false);
                }
                break;
            }
            case Connection.TRACK_META: {
                g_playerTracker.getPlaybackMetadata(content.playbackId).then(data => {
                    g_conn.sendMessage(Connection.TRACK_META, data.info, data.coverData);
                });
                break;
            }
            case Connection.SYNC_CONFIG: {
                for (let key in content) {
                    g_config[key] = content[key];
                }
                break;
            }
            case Connection.DOWNLOAD_STATUS: {
                if (content.playbackId) {
                    let info = g_playerTracker.getPlaybackTrack(content.playbackId);
                    if (info) {
                        g_playlistHook.updateRows({
                            [info.uri]: {
                                errorMessage: content.message
                            }
                        });
                    }
                } else {
                    g_playlistHook.updateRows(content.tracks);
                }
                break;
            }
        }
    });
})();