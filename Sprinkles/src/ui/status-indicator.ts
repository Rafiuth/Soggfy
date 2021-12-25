import config from "../config"
import { Connection, MessageType } from "../connection";
import { Selectors } from "./ui";
import Utils from "../utils";
import { PathTemplate, TemplatedSearchTree } from "../path-template";

export enum DownloadStatus
{
    ERROR       = "ERROR",
    IN_PROGRESS = "IN_PROGRESS",
    CONVERTING  = "CONVERTING",
    WARN        = "WARN",
    DONE        = "DONE"
}
export interface TrackStatus
{
    status: DownloadStatus,
    path?: string; //if status == DONE
    message?: string;
}
//From https://fonts.google.com/icons
const icons = {
    [DownloadStatus.ERROR]: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#f22"><path d="M14.354 2.353l-.708-.707L8 7.293 2.353 1.646l-.707.707L7.293 8l-5.647 5.646.707.708L8 8.707l5.646 5.647.708-.708L8.707 8z"/></svg>`,
    [DownloadStatus.IN_PROGRESS]: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#29f"><path d="M18.32,4.26C16.84,3.05,15.01,2.25,13,2.05v2.02c1.46,0.18,2.79,0.76,3.9,1.62L18.32,4.26z M19.93,11h2.02 c-0.2-2.01-1-3.84-2.21-5.32L18.31,7.1C19.17,8.21,19.75,9.54,19.93,11z M18.31,16.9l1.43,1.43c1.21-1.48,2.01-3.32,2.21-5.32 h-2.02C19.75,14.46,19.17,15.79,18.31,16.9z M13,19.93v2.02c2.01-0.2,3.84-1,5.32-2.21l-1.43-1.43 C15.79,19.17,14.46,19.75,13,19.93z M15.59,10.59L13,13.17V7h-2v6.17l-2.59-2.59L7,12l5,5l5-5L15.59,10.59z M11,19.93v2.02 c-5.05-0.5-9-4.76-9-9.95s3.95-9.45,9-9.95v2.02C7.05,4.56,4,7.92,4,12S7.05,19.44,11,19.93z"/></svg>`,
    [DownloadStatus.CONVERTING]: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#ddd"><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"><animateTransform attributeName="transform" attributeType="XML" type="rotate" from="360 12 12" to="0 12 12" dur="3s" repeatCount="indefinite"/></path></svg>`,
    [DownloadStatus.WARN]: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#fbd935"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>`,
    [DownloadStatus.DONE]: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#3f3"><path d="M13.985 2.383L5.127 12.754 1.388 8.375l-.658.77 4.397 5.149 9.618-11.262z"/></svg>`
};

export class StatusIndicator
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
                this.sendUpdateRequest(dirtyRows);
            }
        });
        let container = document.querySelector(".main-view-container__scroll-node-child");
        this._obs.observe(container, {
            subtree: true,
            childList: true
        });
    }

    updateRows(map: { [uri: string]: TrackStatus })
    {
        let listSection = document.querySelector('section[data-testid="playlist-page"],[data-testid="album-page"]');
        let listDiv = listSection.querySelector('div[tabindex="0"]');
        let listDivRows = listDiv.querySelector('div[role="presentation"]:not([class])');

        for (let rowWrapper of listDivRows.children) {
            let row = rowWrapper.firstElementChild;
            let trackInfo = this.getRowTrackInfo(row, listSection);
            let info = map[trackInfo.uri];
            if (!info) continue;
            
            let node = row["__sgf_status_ind"] ??= document.createElement("div");
            if (!node.parentElement) {
                let infoColDiv = row.lastElementChild;
                infoColDiv.prepend(node);
            }
            
            node.className = "sgf-status-indicator";
            node.innerHTML = `
<div class="sgf-status-indicator-card">
${info.status == DownloadStatus.DONE
    ? `
    <div class="sgf-status-browse-button">
        <svg width="24" height="24" viewBox="0 0 24 24" fill="#ddd">
            <path xmlns="http://www.w3.org/2000/svg" d="M20 6h-8l-2-2H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2zm0 12H4V8h16v10z"></path>
        </svg>
        <span style="padding-left: 4px; font-size: 16px; color: #ddd;">Open Folder</span>
    </div>`
    : `
    <span style="font-size: 16px; color: #ddd;">${info.message}</span>
`}
</div>
${icons[info.status]}`;

            let browseBtn: HTMLDivElement = node.querySelector(".sgf-status-browse-button");
            if (browseBtn) {
                browseBtn.onclick = () => {
                    this._conn.send(MessageType.OPEN_FOLDER, { path: info.path });
                };
            }
        }
    }
    private async sendUpdateRequest(dirtyRows: HTMLDivElement[])
    {
        let listSection = document.querySelector('section[data-testid="playlist-page"],[data-testid="album-page"]');
        let tree = new TemplatedSearchTree(config.savePaths.track);

        for (let row of dirtyRows) {
            let trackInfo = this.getRowTrackInfo(row, listSection);
            tree.add(trackInfo.uri, {
                track_name: trackInfo.name,
                artist_name: trackInfo.artistName,
                album_name: trackInfo.albumName,
            });
        }
        let data = await this._conn.request(MessageType.DOWNLOAD_STATUS, {
            searchTree: tree.root,
            basePath: config.savePaths.basePath
        });
        let results = data.payload.results;
        //split multiple tracks
        for (let key in results) {
            if (!key.includes(",")) continue;

            let val = {
                ...results[key],
                status: DownloadStatus.WARN,
                message: "Different tracks mapping to the same file name"
            };
            for (let subkey of key.split(',')) {
                results[subkey] = val;
            }
        }

        this.updateRows(results);
    }
    private getRowTrackInfo(row: Element, listSection: Element)
    {
        let albumName =
            (row.querySelector('a[href^="/album"]') as HTMLElement)?.innerText ??
            (listSection.querySelector('section[data-testid="album-page"] span h1') as HTMLElement).innerText;
        
        let menuBtn = row.querySelector(Selectors.rowMoreButton);
        let extraProps = Utils.getReactProps(row, menuBtn).menu.props;

        return {
            uri: extraProps.uri,
            name: row.querySelector(Selectors.rowTitle).innerText,
            artistName: row.querySelector(Selectors.rowSubTitle).firstChild.innerText,
            albumName: albumName
        };
    }
}