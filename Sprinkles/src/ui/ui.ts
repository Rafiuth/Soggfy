import config from "../config";
import { Connection, MessageType } from "../connection";
import Utils from "../utils";
import UIC from "./components";
import { Platform, SpotifyUtils } from "../spotify-apis";

import ComponentsStyle from "./css/components.css";
import SettingsStyle from "./css/settings.css";
import StatusIndicatorStyle from "./css/status-indicator.css";
import { PathVars } from "../metadata";
const MergedStyles = [ComponentsStyle, SettingsStyle, StatusIndicatorStyle].join('\n'); //TODO: find a better way to do this

const Icons = {
    Apps: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M4 8h4V4H4v4zm6 12h4v-4h-4v4zm-6 0h4v-4H4v4zm0-6h4v-4H4v4zm6 0h4v-4h-4v4zm6-10v4h4V4h-4zm-6 4h4V4h-4v4zm6 6h4v-4h-4v4zm0 6h4v-4h-4v4z"/></svg>`,
    Album: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M12 2C6.48 2 2 6.48 2 12s4.48 10 10 10 10-4.48 10-10S17.52 2 12 2zm0 14.5c-2.49 0-4.5-2.01-4.5-4.5S9.51 7.5 12 7.5s4.5 2.01 4.5 4.5-2.01 4.5-4.5 4.5zm0-5.5c-.55 0-1 .45-1 1s.45 1 1 1 1-.45 1-1-.45-1-1-1z"/></svg>`,
    List: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M2 17h2v.5H3v1h1v.5H2v1h3v-4H2v1zm1-9h1V4H2v1h1v3zm-1 3h1.8L2 13.1v.9h3v-1H3.2L5 10.9V10H2v1zm5-6v2h14V5H7zm0 14h14v-2H7v2zm0-6h14v-2H7v2z"/></svg>`,
    MagicWand: `<svg width="24" height="24" viewBox="-2 -2 28 28" fill="currentColor"><path d="M7.5 5.6L10 7 8.6 4.5 10 2 7.5 3.4 5 2l1.4 2.5L5 7zm12 9.8L17 14l1.4 2.5L17 19l2.5-1.4L22 19l-1.4-2.5L22 14zM22 2l-2.5 1.4L17 2l1.4 2.5L17 7l2.5-1.4L22 7l-1.4-2.5zm-7.63 5.29c-.39-.39-1.02-.39-1.41 0L1.29 18.96c-.39.39-.39 1.02 0 1.41l2.34 2.34c.39.39 1.02.39 1.41 0L16.7 11.05c.39-.39.39-1.02 0-1.41l-2.33-2.35zm-1.03 5.49l-2.12-2.12 2.44-2.44 2.12 2.12-2.44 2.44z"></path></svg>`,
    Folder: `<svg width="24px" height="24px" viewBox="0 0 24 24" fill="currentColor"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path></svg>`,
    Sliders: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"></path></svg>`
};

export default class UI
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
            () => this.openQuickSettingsPopup()
        );
    }
    setEnabled(enabled: boolean)
    {
        this._settingsButton.disabled = !enabled;
    }

    private openQuickSettingsPopup()
    {
        let pathTemplates = [
            UIC.parse(`${Icons.Apps}<span>Single</span>`),
            UIC.parse(`${Icons.Album}<span>Album</span>`),
            UIC.parse(`${Icons.List}<span>Playlist</span>`),
            UIC.parse(`${Icons.MagicWand}<span>Auto</span>`)
        ];
        let popup = this.openPopup(
            "sgf-quick-settings-popup", this._settingsButton,
            UIC.rows(
                UIC.colDesc("Path template"),
                UIC.switchField("activePathTemplate", pathTemplates, (key, newIndex) => { console.log(key, newIndex); return 0; }),
                UIC.row("Playback speed", this.createSpeedSlider()),
                UIC.button("More settings...", Icons.Sliders, () => {
                    popup.remove();
                    document.body.append(this.createSettingsDialog());
                })
            )
        );
    }

    private openPopup(className: string, anchor: Element, content: Node)
    {
        let anchorRect = anchor.getBoundingClientRect();
        let popup = UIC.parse(`
<div class="${className}" 
        style="left: ${anchorRect.left}px; top: ${anchorRect.bottom}px"
        tabindex="-1">
</div>`) as HTMLDivElement;

        popup.appendChild(content);

        popup.addEventListener("focusout", ev => {
            if (ev.relatedTarget ? !popup.contains(ev.relatedTarget as Element) : ev.target === popup) {
                popup.remove();
            }
        });
        popup.addEventListener("keydown", ev => {
            if (ev.key === "Escape") {
                popup.remove();
            }
        });
        document.body.appendChild(popup);
        popup.focus();

        return popup;
    }

    private createSettingsDialog()
    {
        let onChange = this.configAccessor;

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
            UIC.rows("FFmpeg arguments", UIC.textInput("outputFormat.args", onChange)),
            //TODO: allow this to be editable
            UIC.row("Extension", UIC.select("outputFormat.ext", ["mp3", "m4a", "mp4", "ogg", "opus"], onChange))
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
            let resp = await this._conn.request(MessageType.BROWSE_FOLDER, {
                initialPath: config.savePaths.basePath
            }, null, -1);
            basePathTextInput.value = resp.payload.path;
            onChange("savePaths.basePath", resp.payload.path);
        };

        let pathVarTags = [];
        for (let pv of PathVars) {
            let name = `{${pv.name}}`;
            let tag = UIC.tagButton(name, () => {
                Platform.getClipboardAPI().copy(name);
                UIC.notification("Copied", tag, 1);
            });
            tag.title = pv.desc;
            pathVarTags.push(tag);
        }
        
        return UIC.createSettingOverlay(
            UIC.section("General",
                UIC.row("Output format",        UIC.select("outputFormat", Object.getOwnPropertyNames(defaultFormats), onFormatChange)),
                customFormatSection,
                UIC.row("Embed cover art",      UIC.toggle("embedCoverArt", onChange)),
                UIC.row("Download lyrics",      UIC.toggle("downloadLyrics", onChange)),
                UIC.row("Playback speed",       this.createSpeedSlider())
            ),
            UIC.section("Download Paths",
                UIC.rows("Base Path",       UIC.colSection(basePathTextInput, UIC.button(null, Icons.Folder, browseBasePath))),
                UIC.rows("Singles",         UIC.textInput("savePaths.track", onChange)),
                UIC.rows("Albums",          UIC.textInput("savePaths.album", onChange)),
                UIC.rows("Playlists",       UIC.textInput("savePaths.playlist", onChange)),
                UIC.rows("Podcasts",        UIC.textInput("savePaths.podcast", onChange)),
                UIC.row("Save cover art",   UIC.toggle("saveCoverArt", onChange)),
                UIC.rows(UIC.collapsible("Variables", ...pathVarTags)),
            )
        );
    }

    private createSpeedSlider()
    {
        return UIC.slider(
            "playbackSpeed",
            { min: 1, max: 20, step: 1, formatter: val => val + "x" },
            (key, newValue) => {
                if (newValue) {
                    SpotifyUtils.resetCurrentTrack(false);
                }
                return this.configAccessor(key, newValue);
            }
        );
    }

    private configAccessor(key: string, newValue?: any)
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