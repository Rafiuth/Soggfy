import config from "../config";
import { Connection, MessageType } from "../connection";
import Utils from "../utils";
import UIC from "./components";
import { Platform, SpotifyUtils } from "../spotify-apis";
import Resources from "../resources";

import ComponentsStyle from "./css/components.css";
import SettingsStyle from "./css/settings.css";
import StatusIndicatorStyle from "./css/status-indicator.css";
import { PathTemplate, TemplatedSearchTree } from "../path-template";

const MergedStyles = [ComponentsStyle, SettingsStyle, StatusIndicatorStyle].join('\n'); //TODO: find a better way to do this

export const Icons = {
    Folder: `<svg width="24px" height="24px" viewBox="0 0 24 24" fill="currentColor"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path></svg>`,
    Sliders: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"></path></svg>`,
    
    Done: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#3f3"><path d="M13.985 2.383L5.127 12.754 1.388 8.375l-.658.77 4.397 5.149 9.618-11.262z"/></svg>`,
    Error: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#f22"><path d="M14.354 2.353l-.708-.707L8 7.293 2.353 1.646l-.707.707L7.293 8l-5.647 5.646.707.708L8 8.707l5.646 5.647.708-.708L8.707 8z"/></svg>`,
    InProgress: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#29f"><path d="M18.32,4.26C16.84,3.05,15.01,2.25,13,2.05v2.02c1.46,0.18,2.79,0.76,3.9,1.62L18.32,4.26z M19.93,11h2.02 c-0.2-2.01-1-3.84-2.21-5.32L18.31,7.1C19.17,8.21,19.75,9.54,19.93,11z M18.31,16.9l1.43,1.43c1.21-1.48,2.01-3.32,2.21-5.32 h-2.02C19.75,14.46,19.17,15.79,18.31,16.9z M13,19.93v2.02c2.01-0.2,3.84-1,5.32-2.21l-1.43-1.43 C15.79,19.17,14.46,19.75,13,19.93z M15.59,10.59L13,13.17V7h-2v6.17l-2.59-2.59L7,12l5,5l5-5L15.59,10.59z M11,19.93v2.02 c-5.05-0.5-9-4.76-9-9.95s3.95-9.45,9-9.95v2.02C7.05,4.56,4,7.92,4,12S7.05,19.44,11,19.93z"/></svg>`,
    Processing: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#ddd"><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"><animateTransform attributeName="transform" attributeType="XML" type="rotate" from="360 12 12" to="0 12 12" dur="3s" repeatCount="indefinite"/></path></svg>`,
    Warning: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#fbd935"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>`,
    
    DoneBig: `<svg width="24" height="24" viewBox="0 0 24 24" fill="#3f3"><path d="M9 16.2L4.8 12l-1.4 1.4L9 19 21 7l-1.4-1.4L9 16.2z"/></svg>`,
    ErrorBig: `<svg width="24" height="24" viewBox="0 0 24 24" fill="#f22"><path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12 19 6.41z"/></svg>`,

    FileDownload: `<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M18,15v3H6v-3H4v3c0,1.1,0.9,2,2,2h12c1.1,0,2-0.9,2-2v-3H18z M17,11l-1.41-1.41L13,12.17V4h-2v8.17L8.41,9.59L7,11l5,5 L17,11z"></path></svg>`,
    FileDownloadOff: `<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M16 18 17.15 20H6Q5.175 20 4.588 19.413 4 18.825 4 18V15H6V18M12.575 15.425 12 16 7 11 7.575 10.425ZM15.6 9.55 17 11 15.425 12.575 14 11.15ZM13 4V10.15L11 8.15V4Z"/><path d="M2.8 2.8 21.2 21.2 19.775 22.625 1.375 4.225Z" fill="#e91429"/></svg>`,
};
export default class UI
{
    private _conn: Connection;

    private _adblockStyleAdded = false;

    constructor(conn: Connection)
    {
        this._conn = conn;
    }

    install()
    {
        let style = document.createElement("style");
        style.innerHTML = MergedStyles;
        document.head.appendChild(style);

        this.addTopbarButtons();

        let bodyObs = new MutationObserver(this.onDocBodyChanged.bind(this));
        bodyObs.observe(document.body, { childList: true });
    }

    private addTopbarButtons()
    {
        let backButton = document.querySelector(".Root__top-bar").querySelector("button");
        let topbarContainer = backButton.parentElement;
        let buttonClass = backButton.classList[0];

        let div = document.createElement("div");
        div.className = "sgf-topbar-retractor";
        div.innerHTML = `
<button class=${buttonClass}>${config.downloaderEnabled ? Icons.FileDownload : Icons.FileDownloadOff}</button>
<button class=${buttonClass}>${Icons.Sliders}</button>`;
        
        //@ts-ignore
        div.children[0].onclick = () => {
            this.updateConfig("downloaderEnabled", !config.downloaderEnabled);
            div.children[0].innerHTML = config.downloaderEnabled ? Icons.FileDownload : Icons.FileDownloadOff;
            //reset track (when enabling), or speed (when disabling)
            SpotifyUtils.resetCurrentTrack(!config.downloaderEnabled && config.playbackSpeed === 1.0);
        };
        //@ts-ignore
        div.children[1].onclick = () => {
            document.body.append(this.createSettingsDialog());
        };
        topbarContainer.append(div);
        return div;
    }

    private async createM3U(uri: string)
    {
        let info = await Resources.getTracks(uri);
        let tracks = info.tracks;
        
        let saveResult = await this._conn.request(MessageType.OPEN_FILE_PICKER, {
            type: 2 /* SAVE_FILE */,
            initialPath: config.savePaths.basePath + `/${PathTemplate.escapePath(info.name)}.m3u8`,
            fileTypes: [ "M3U Playlist|*.m3u8" ]
        }, null, -1);
        let savePath = saveResult.payload.path;
        if (!saveResult.payload.success) return;

        let tree = new TemplatedSearchTree(config.savePaths.track);
        for (let track of tracks) {
            tree.add(track.uri, track.vars);
        }

        let data = await this._conn.request(MessageType.DOWNLOAD_STATUS, {
            searchTree: tree.root,
            basePath: config.savePaths.basePath
        }, null, -1);
        let results = data.payload.results;
        let plData = `#EXTM3U\n#PLAYLIST:${info.name}\n\n`;
        let numExported = 0;

        for (let track of tracks) {
            let loc = results[track.uri];
            if (!loc) continue;
            
            plData += `#EXTINF:${(track.durationMs / 1000).toFixed(0)},${track.vars.artist_name} - ${track.vars.track_name}\n`;
            plData += `${loc.path}\n\n`;
            numExported++;
        }
        this._conn.send(MessageType.WRITE_FILE, { path: savePath, mode: "replace", text: plData });
        this.showNotification(Icons.DoneBig, `Exported ${numExported} tracks`);
    }

    private createSettingsDialog()
    {
        //TODO: refactor (+ react port?)
        let onChange = this.updateConfig.bind(this);

        let speedSlider = UIC.slider(
            "playbackSpeed",
            { min: 1, max: 20, step: 1, formatter: val => val + "x" },
            (key, newValue) => {
                if (newValue) {
                    SpotifyUtils.resetCurrentTrack(false);
                }
                return onChange(key, newValue);
            }
        );
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
            let resp = await this._conn.request(MessageType.OPEN_FILE_PICKER, {
                type: 3 /* SELECT_FOLDER */,
                initialPath: config.savePaths.basePath
            }, null, -1);
            basePathTextInput.value = resp.payload.path;
            onChange("savePaths.basePath", resp.payload.path);
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
                UIC.row("Playback speed",           speedSlider),
                UIC.row("Output format",            UIC.select("outputFormat", Object.getOwnPropertyNames(defaultFormats), onFormatChange)),
                customFormatSection,
                UIC.row("Skip downloaded tracks",   UIC.toggle("skipDownloadedTracks", onChange)),
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
            )
        );
    }

    public showNotification(icon: string, text: string)
    {
        let anchor = document.querySelector(".Root__now-playing-bar");

        let node = UIC.parse(`
<div class="sgf-notification-wrapper">
    ${icon}
    <span>${text}</span>
</div>`);
        UIC.notification(node, anchor, "up", false, 3);
    }

    private onDocBodyChanged()
    {
        let menuList = document.querySelector("#context-menu ul");
        if (menuList !== null && !menuList["_sgf_handled"]) {
            this.onContextMenuOpened(menuList);
        }
        let adLeaderboardDom = Platform.getAdManagers()?.leaderboard?.domTarget;
        if (adLeaderboardDom && !this._adblockStyleAdded) {
            this._adblockStyleAdded = true;
            
            let style = document.createElement("style");
            style.innerHTML = `.${adLeaderboardDom.className} { display: none !important; }`;
            document.body.appendChild(style);
        }
    }

    private onContextMenuOpened(menuList: Element)
    {
        const HookDescs = [
            (uri) => ({
                text: "Export M3U",
                offset: "beforebegin",
                onClick: () => this.createM3U(uri)
            })
        ];

        for (let menuItem of menuList.children) {
            let props = Utils.getReactProps(menuList, menuItem);
            let isTarget = props && (
                (props.contextUri && props.highlightedUri) ||               //Track: Show credits
                (props.uri && props.hasOwnProperty("onRemoveCallback")) ||  //Album: Add/remove to library
                (props.uri && props.description != null)                    //Playlist: Go to playlist radio
            );
            if (isTarget) {
                let uri = props.contextUri ?? props.uri;

                for (let descFactory of HookDescs) {
                    var desc = descFactory(uri);

                    let itemTemplate = menuList.querySelector("li button[aria-disabled='false'] span").parentElement.parentElement;
                    let item = itemTemplate.cloneNode(true) as HTMLLIElement;
                    item.querySelector("span").innerText = desc.text;
                    item.querySelector("button").onclick = () => {
                        desc.onClick();
                        menuList.parentElement.parentElement["_tippy"]?.props?.onClickOutside();
                    };
                    menuItem.insertAdjacentElement(desc.offset as any ?? "afterend", item);
                }
                menuList["_sgf_handled"] = true; //add mark to prevent this method from being fired multiple times
                break;
            }
        }
    }

    private updateConfig(key: string, newValue?: any)
    {
        let finalValue = Utils.accessObjectPath(config, key.split('.'), newValue);

        if (newValue !== undefined) {
            let delta = {};
            let field = key.split('.')[0]; //sync only supports topmost field
            delta[field] = config[field];
            this._conn.send(MessageType.SYNC_CONFIG, delta);
        }
        return finalValue;
    }
}

/**
 * Extracts the specified CSS selectors from xpui.js.
 * Note: This function is expansive and results should be cached.
 */
async function extractSelectors(...names: string[])
{
    let pattern = `(${names.join('|')}):\\s*"(.+?)"`;
    let regex = new RegExp(pattern, "g");
    let results: any = {};

    let req = await fetch("/xpui.js");
    let js = await req.text();

    let match: RegExpExecArray;
    while (match = regex.exec(js)) {
        let key = match[1];
        let val = match[2];
        results[key] = "." + val;
    }
    return results;
}

let selectors = await extractSelectors(
    "trackListRow", "rowTitle", "rowSubTitle", "rowSectionEnd",
    "rowMoreButton"
);

export {
    selectors as Selectors
}