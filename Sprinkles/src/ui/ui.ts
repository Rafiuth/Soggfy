import config from "../config";
import { Connection, MessageType } from "../connection";
import Utils from "../utils";
import UIC from "./components";

import SettingsStyle from "./css/settings.css";
import StatusIndicatorStyle from "./css/status-indicator.css";
const MergedStyles = SettingsStyle + "\n" + StatusIndicatorStyle; //TODO: find a better way to do this

class UI
{
    private _styleElement: HTMLStyleElement;
    private _settingsButton: HTMLButtonElement;
    private _conn: Connection;

    constructor(conn: Connection)
    {
        this._conn = conn;
        this._styleElement = document.createElement("style");
        this._styleElement.innerHTML = MergedStyles;
    }

    install()
    {
        document.head.appendChild(this._styleElement);

        this._settingsButton = UIC.addTopbarButton(
            "Soggfy",
            //TODO: design a icon for this
            //https://fonts.google.com/icons?selected=Material+Icons:settings&icon.query=down
            `<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M18,15v3H6v-3H4v3c0,1.1,0.9,2,2,2h12c1.1,0,2-0.9,2-2v-3H18z M17,11l-1.41-1.41L13,12.17V4h-2v8.17L8.41,9.59L7,11l5,5 L17,11z"></path></svg>`,
            () => document.body.append(this.createSettingsDialog())
        );
    }
    setEnabled(enabled: boolean)
    {
        this._settingsButton.disabled = !enabled;
    }

    private createSettingsDialog()
    {
        let onChange = (key: string, newValue?: any) => {
            let finalValue = Utils.accessObjectPath(config, key.split('.'), newValue);

            if (newValue !== undefined) {
                let delta = {};
                let field = key.split('.')[0]; //sync only supports topmost field
                delta[field] = config[field];
                this._conn.send(MessageType.SYNC_CONFIG, delta);
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
        let customFormatSection = UIC.subSection(
            UIC.rowSection("FFmpeg arguments",  UIC.textInput("outputFormat.args", onChange)),
            //TODO: allow this to be editable
            UIC.row("Extension",                UIC.select("outputFormat.ext", ["mp3","m4a","mp4","ogg","opus"], onChange))
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
        let updateCoverPath = (key: string, save?: boolean) => {
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
        
        return UIC.createSettingOverlay(
            UIC.section("General",
                UIC.row("Output format",        UIC.select("outputFormat", Object.getOwnPropertyNames(defaultFormats), onFormatChange)),
                customFormatSection,
                UIC.row("Embed cover art",      UIC.toggle("embedCoverArt", onChange)),
                UIC.row("Download lyrics",      UIC.toggle("downloadLyrics", onChange)),
                UIC.row("Playback speed",       UIC.slider("playbackSpeed", { min: 1, max: 20, step: 1, formatter: val => val + "x" }, onChange))
            ),
            UIC.section("Download Paths",
                UIC.rowSection("Songs",         UIC.textInput("savePaths.track.audio", onChange)),
                UIC.rowSection("Podcasts",      UIC.textInput("savePaths.podcast.audio", onChange)),
                UIC.row("Save cover art separately", UIC.toggle("saveCoverArt", (key, newValue) => {
                    return updateCoverPath("savePaths.track.cover", newValue) || 
                           updateCoverPath("savePaths.podcast.cover", newValue);
                })),
            )
        );
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

export default UI;

export {
    selectors as Selectors
}