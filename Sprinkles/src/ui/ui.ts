import config, { isTrackIgnored } from "../config";
import { Connection, MessageType } from "../connection";
import Utils from "../utils";
import UIC from "./components";
import { Icons, MergedStyles } from "./ui-assets";
import { Platform, Player, SpotifyUtils } from "../spotify-apis";
import Resources from "../resources";

import { PathTemplate, TemplatedSearchTree } from "../path-template";
import { StatusIndicator } from "./status-indicator";

export default class UI {
    public readonly statusIndicator: StatusIndicator;
    private configWatchers = new Array<{ key: string, cb: (newValue: any) => void }>();

    constructor(
        private conn: Connection
    ) {
        this.statusIndicator = new StatusIndicator(conn);

        let style = document.createElement("style");
        style.innerHTML = MergedStyles;
        document.head.appendChild(style);

        let topbarDiv = this.addTopbarButtons();

        let bodyObs = new MutationObserver(() => {
            let menuList = document.querySelector("#context-menu ul");
            if (menuList !== null && !menuList["_sgf_handled"]) {
                this.onContextMenuOpened(menuList);
            }
            // Lyrics page will delete topbar header 
            if (!topbarDiv.isConnected) {
                this.addTopbarButtons(topbarDiv);
            }
        });
        //playlist context menus are delayed, so we need to observe subtrees too
        bodyObs.observe(document.body, { childList: true, subtree: true });
    }

    private addTopbarButtons(existingDiv = undefined) {
        let fwdButton = document.querySelector("[data-testid='top-bar-forward-button'], .main-topBar-forward, .main-topBar-responsiveForward");
        let topbarContainer = fwdButton.parentElement;
        let buttonClass = fwdButton.classList[0];

        if (existingDiv) {
            topbarContainer.append(existingDiv);
            return;
        }

        let div = document.createElement("div");
        div.className = "sgf-topbar-retractor";
        div.innerHTML = `
<button class=${buttonClass}>${Icons.FileDownload}</button>
<button class=${buttonClass}>${Icons.Sliders}</button>`;
        
        //@ts-ignore
        div.children[0].onclick = () => {
            this.updateConfig("downloaderEnabled", !config.downloaderEnabled);
            //reset track when enabling, or if it's speeded when disabling
            if (config.downloaderEnabled || config.playbackSpeed !== 1.0) {
                SpotifyUtils.resetCurrentTrack(!config.downloaderEnabled);
            }
        };
        //@ts-ignore
        div.children[1].onclick = () => {
            document.body.append(this.createSettingsDialog());
        };
        topbarContainer.append(div);

        this.watchConfig("downloaderEnabled", v => div.children[0].innerHTML = v ? Icons.FileDownload : Icons.FileDownloadOff);
        return div;
    }

    private async createM3U(uri: string, trackUris?: string[]) {
        let info = await Resources.getTracks(uri);
        let tracks = info.tracks;
        
        let saveResult = await this.conn.request(MessageType.OPEN_FILE_PICKER, {
            type: 2 /* SAVE_FILE */,
            initialPath: config.savePaths.basePath + `/${PathTemplate.escapePath(info.name)}.m3u8`,
            fileTypes: ["M3U Playlist|*.m3u8"]
        });
        if (!saveResult.success) return;

        let tree = new TemplatedSearchTree(config.savePaths.track);
        for (let track of tracks) {
            if (trackUris?.length >= 2 && !trackUris.includes(track.uri)) continue;
            tree.add(track.uri, track.vars);
        }
        let statusResp = await this.conn.request(MessageType.DOWNLOAD_STATUS, {
            searchTree: tree.root,
            basePath: config.savePaths.basePath,
            relativeTo: PathTemplate.getParentPath(saveResult.path)
        });
        let data = `#EXTM3U\n#PLAYLIST:${info.name}\n\n`;
        let numExported = 0;

        for (let track of tracks) {
            let loc = statusResp.results[track.uri];
            if (!loc) continue;
            
            data += `#EXTINF:${(track.durationMs / 1000).toFixed(0)},${track.vars.artist_name} - ${track.vars.track_name}\n`;
            data += `${loc.path.replaceAll('\\', '/')}\n\n`;
            numExported++;
        }
        this.conn.send(MessageType.WRITE_FILE, { path: saveResult.path, mode: "replace", text: data });
        this.showNotification(Icons.DoneBig, `Exported ${numExported} tracks`);
    }

    private createSettingsDialog() {
        //TODO: refactor (+ react port?)
        let onChange = this.updateConfig.bind(this);

        let defaultFormats = {
            "Original OGG":         { ext: "",    args: "-c copy" },
            "MP3 320K":             { ext: "mp3", args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy" },
            "MP3 256K":             { ext: "mp3", args: "-c:a libmp3lame -b:a 256k -id3v2_version 3 -c:v copy" },
            "MP3 192K":             { ext: "mp3", args: "-c:a libmp3lame -b:a 192k -id3v2_version 3 -c:v copy" },
            // https://trac.ffmpeg.org/wiki/Encode/AAC
            "M4A 256K (FDK AAC)":   { ext: "m4a", args: "-c:a libfdk_aac -b:a 256k -cutoff 20k -disposition:v attached_pic -c:v copy" },
            "M4A 224K VBR (FDK AAC)":{ext: "m4a", args: "-c:a libfdk_aac -vbr 5 -disposition:v attached_pic -c:v copy" },
            "M4A 160K (FDK AAC)":   { ext: "m4a", args: "-c:a libfdk_aac -b:a 160k -cutoff 18k -disposition:v attached_pic -c:v copy" },
            "Opus 160K":            { ext: "opus",args: "-c:a libopus -b:a 160k" },
            "Custom":               { ext: "mp3", args: "-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy" },
        };
        let extensions = {
            "MP3": "mp3", "M4A": "m4a", "MP4": "mp4",
            "OGG": "ogg", "Opus": "opus"
        };
        let customFormatSection = UIC.subSection(
            UIC.rows("FFmpeg arguments", UIC.textInput("outputFormat.args", onChange)),
            //TODO: allow this to be editable
            UIC.row("Extension", UIC.select("outputFormat.ext", extensions, onChange))
        );
        customFormatSection.style.display = "none";
        
        let onFormatChange = (key: string, name?: string) => {
            if (name === undefined) {
                let currFormat = config.outputFormat;
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

        let basePathTextInput = UIC.textInput("savePaths.basePath", onChange);
        let browseBasePath = async () => {
            let pickResult = await this.conn.request(MessageType.OPEN_FILE_PICKER, {
                type: 3 /* SELECT_FOLDER */,
                initialPath: config.savePaths.basePath
            });
            basePathTextInput.value = pickResult.path;
            onChange("savePaths.basePath", pickResult.path);
        };

        let pathVarTags = [];
        
        for (let pv of PathTemplate.Vars) {
            let name = `{${pv.name}}`;
            let tag = UIC.tagButton(name, () => {
                Platform.getClipboardAPI().copy(name);
                UIC.notification("Copied", tag, "up", true, 1);
            });
            tag.title = pv.desc;
            pathVarTags.push(tag);
        }

        let canvasPathTxt = UIC.rows("Canvas template", UIC.textInput("savePaths.canvas", onChange));

        let invalidCharModes = {
            "Unicodes": "unicode",
            "Dashes (-)": "-",
            "Underlines (_)": "_",
            "None (remove)": ""
        };

        return UIC.createSettingOverlay(
            UIC.section("General",
                UIC.row("Playback speed",           UIC.slider("playbackSpeed", { min: 1, max: 50, step: 1, formatter: val => val + "x" }, onChange)),
                UIC.row("Output format",            UIC.select("outputFormat", Object.getOwnPropertyNames(defaultFormats), onFormatChange)),
                customFormatSection,
                UIC.row("Skip downloaded tracks",   UIC.toggle("skipDownloadedTracks", onChange)),
                UIC.row("Skip ignored tracks",      UIC.toggle("skipIgnoredTracks", onChange)),
                UIC.row("Embed cover art",          UIC.toggle("embedCoverArt", onChange)),
                UIC.row("Save cover art in album folder", UIC.toggle("saveCoverArt", onChange)),
                UIC.row("Embed lyrics",             UIC.toggle("embedLyrics", onChange)),
                UIC.row("Save lyrics as .lrc/.txt", UIC.toggle("saveLyrics", onChange)),
                UIC.row("Save canvas",              UIC.toggle("saveCanvas", (k, v) => { 
                    v = onChange(k, v);
                    canvasPathTxt.style.display = v ? "block" : "none";
                    return v;
                }))
            ),
            UIC.section("Paths",
                UIC.rows("Base path",           UIC.colSection(basePathTextInput, UIC.button(null, Icons.Folder, browseBasePath))),
                UIC.rows("Track template",      UIC.textInput("savePaths.track", onChange)),
                UIC.rows("Podcast template",    UIC.textInput("savePaths.episode", onChange)),
                canvasPathTxt,
                UIC.row("Replace invalid characters with", UIC.select("savePaths.invalidCharRepl", invalidCharModes, onChange)),
                UIC.rows(UIC.collapsible("Variables", ...pathVarTags))
            ),
            UIC.section("Misc",
                UIC.row("Block telemetry",      UIC.toggle("blockAds", onChange)),
                UIC.row("Move 'Add to Queue' to top", UIC.toggle("liftAddToQueueMenuItem", onChange)),
            )
        );
    }

    public showNotification(icon: string, text: string) {
        let anchor = document.querySelector(".Root__now-playing-bar, [data-testid='now-playing-bar']");

        let node = UIC.parse(`
<div class="sgf-notification-wrapper">
    ${icon}
    <span>${text}</span>
</div>`);
        UIC.notification(node, anchor, "up", false, 3);
    }

    private onContextMenuOpened(menuList: Element) {
        const HookDescs = [
            (contextUri, trackUris) => ({
                text: "Generate M3U",
                icon: Icons.SaveAs,
                onClick: () => this.createM3U(contextUri, trackUris)
            }),
            (contextUri, trackUris) => {
                let uris = trackUris?.length > 0 ? trackUris : [contextUri];
                let ignored = uris.some(uri => config.ignorelist[uri]);
                
                return {
                    text: `${ignored ? "Unignore" : "Ignore"} ${uris[0].split(':')[1]}${uris.length > 1 ? "s" : ""}`,
                    icon: Icons.Block,
                    onClick: () => {
                        for (let uri of uris) {
                            ignored ? delete config.ignorelist[uri] : config.ignorelist[uri] = 1;
                        }
                        if (isTrackIgnored(Player.getState().item)) {
                            //server will resync status, updateRows() will be called then
                            this.conn.send(MessageType.DOWNLOAD_STATUS, {
                                playbackId: Player.getState().playbackId,
                                ignore: true
                            });
                        } else {
                            this.statusIndicator.updateRows({});
                        }
                        this.updateConfig("ignorelist", config.ignorelist);
                    }
                }
            }
        ];

        let menuItem = menuList.querySelector("li:has(path[d*='M16 15H2v'])"); // "Add to Queue" button
        
        if (!menuItem) return;

        menuList["_sgf_handled"] = true; //add flag to prevent infinite loop
        
        //Move "Add to Queue" to top.
        if (config["liftAddToQueueMenuItem"]) {
            menuList.insertBefore(menuItem, menuList.firstChild);
        }
        
        //Add new entries
        let props = Utils.getReactProps(menuList.parentElement, menuList);
        let contextUri = props.contextUri ?? props.uri ?? props.reference?.uri ?? props.context?.uri;
        let trackUris = props.contextUri && props.uri ? [props.uri] : (props.uris ?? (props.item ? [props.item?.uri] : []));

        for (let descFactory of HookDescs.reverse()) {
            let desc = descFactory(contextUri, trackUris);
            let item = menuItem.cloneNode(true) as HTMLLIElement;
            item.querySelector("span").innerText = desc.text;
            item.querySelector("button").classList?.remove("QgtQw2NJz7giDZxap2BB"); //separator class
            item.querySelector("button").onclick = () => {
                desc.onClick();
                menuList.parentElement.parentElement["_tippy"]?.props?.onClickOutside();
            };

            //Replace icon
            let icon = item.querySelector("svg");
            let newIcon = UIC.parse(desc.icon) as SVGSVGElement;
            icon.innerHTML = newIcon.innerHTML;
            icon.setAttribute("viewBox", newIcon.getAttribute("viewBox"));

            menuItem.insertAdjacentElement("afterend", item);
        }
    }

    private updateConfig(key: string, newValue?: any) {
        let finalValue = Utils.accessObjectPath(config, key.split('.'), newValue);

        if (newValue !== undefined) {
            let field = key.split('.')[0]; //sync only supports topmost field
            let delta = { [field]: config[field] };
            this.syncConfig(delta);
            this.conn.send(MessageType.SYNC_CONFIG, delta);
        }
        return finalValue;
    }
    private watchConfig(key: string, cb: (newValue: any) => void) {
        this.configWatchers.push({ key, cb });
    }

    public syncConfig(updatedEntries: Record<string, any>) {
        for (let watcher of this.configWatchers) {
            if (Object.hasOwn(updatedEntries, watcher.key)) {
                watcher.cb(updatedEntries[watcher.key]);
            }
        }
    }
}
