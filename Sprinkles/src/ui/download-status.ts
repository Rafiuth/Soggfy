import config from "../config"
import { Connection, MessageType } from "../connection";
import { Selectors } from "./ui";
import Utils from "../utils";

interface DownloadStatusMap
{
    [uri: string]: {
        path?: string;
        errorMessage?: string;
    }
}

export default class DownloadStatus
{
    private _conn: Connection;
    private _obs: MutationObserver;

    constructor(con: Connection)
    {
        this._conn = con;

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

    updateRows(tracks: DownloadStatusMap)
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

                let browseBtn: HTMLDivElement = node.querySelector(".sgf-status-browse-button");
                if (browseBtn) {
                    browseBtn.onclick = () => {
                        this._conn.send(MessageType.OPEN_FOLDER, { path: status.path });
                    };
                }
                row["__sgf_state_elem"] = node;
            }
        }
    }
    _sendUpdateRequest(dirtyRows: HTMLDivElement[])
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
        this._conn.send(MessageType.DOWNLOAD_STATUS, {
            pathTemplate: config.savePaths.track.audio.replace(/\.\w+$/, "{_ext}"),
            tracks: tracks
        });
    }
    _getRowTrackInfo(row: Element, listSection: Element)
    {
        let albumName =
            (row.querySelector('a[href^="/album"]') as HTMLElement)?.innerText ??
            (listSection.querySelector('section[data-testid="album-page"] span h1') as HTMLElement).innerText;
        
        let menuBtn = row.querySelector(Selectors.rowMoreButton);
        let extraProps = Utils.getReactProps(row, menuBtn).menu.props;

        return {
            trackUri: extraProps.uri,
            trackName: row.querySelector(Selectors.rowTitle).innerText,
            artistName: row.querySelector(Selectors.rowSubTitle).firstChild.innerText,
            albumName: albumName
        };
    }
}