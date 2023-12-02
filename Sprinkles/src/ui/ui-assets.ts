import ComponentsStyle from "./css/components.css";
import SettingsStyle from "./css/settings.css";
import StatusIndicatorStyle from "./css/status-indicator.css";

export const MergedStyles = [
    ComponentsStyle, SettingsStyle, StatusIndicatorStyle,
    ".X871RxPwx9V0MqpQdMom { display: none !important; }" //hide ad leaderboard
].join('\n'); //TODO: find a better way to do this

//From https://fonts.google.com/icons
export const Icons = {
    Folder: `<svg width="24px" height="24px" viewBox="0 0 24 24" fill="currentColor"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path></svg>`,
    Sliders: `<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"></path></svg>`,

    Done: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#3f3"><path d="M13.985 2.383L5.127 12.754 1.388 8.375l-.658.77 4.397 5.149 9.618-11.262z"/></svg>`,
    Error: `<svg width="16" height="16" viewBox="0 0 16 16" fill="#f22"><path d="M14.354 2.353l-.708-.707L8 7.293 2.353 1.646l-.707.707L7.293 8l-5.647 5.646.707.708L8 8.707l5.646 5.647.708-.708L8.707 8z"/></svg>`,
    InProgress: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#29f"><path d="M18.32,4.26C16.84,3.05,15.01,2.25,13,2.05v2.02c1.46,0.18,2.79,0.76,3.9,1.62L18.32,4.26z M19.93,11h2.02 c-0.2-2.01-1-3.84-2.21-5.32L18.31,7.1C19.17,8.21,19.75,9.54,19.93,11z M18.31,16.9l1.43,1.43c1.21-1.48,2.01-3.32,2.21-5.32 h-2.02C19.75,14.46,19.17,15.79,18.31,16.9z M13,19.93v2.02c2.01-0.2,3.84-1,5.32-2.21l-1.43-1.43 C15.79,19.17,14.46,19.75,13,19.93z M15.59,10.59L13,13.17V7h-2v6.17l-2.59-2.59L7,12l5,5l5-5L15.59,10.59z M11,19.93v2.02 c-5.05-0.5-9-4.76-9-9.95s3.95-9.45,9-9.95v2.02C7.05,4.56,4,7.92,4,12S7.05,19.44,11,19.93z"/></svg>`,
    Processing: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#ddd"><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"><animateTransform attributeName="transform" attributeType="XML" type="rotate" from="360 12 12" to="0 12 12" dur="3s" repeatCount="indefinite"/></path></svg>`,
    Warning: `<svg width="18" height="18" viewBox="0 0 24 24" fill="#fbd935"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>`,
    SyncDisabled: `<svg height="20" width="20" viewBox="0 0 24 25" fill="#bbb"><path d="m19.8 22.6-3.725-3.725q-.475.275-.987.5-.513.225-1.088.375v-2.1q.15-.05.3-.112.15-.063.3-.138l-8-8q-.275.625-.438 1.288Q6 11.35 6 12.05q0 1.125.425 2.187Q6.85 15.3 7.75 16.2l.25.25V14h2v6H4v-2h2.75l-.4-.35q-1.225-1.225-1.788-2.662Q4 13.55 4 12.05q0-1.125.287-2.163.288-1.037.838-1.962L1.4 4.2l1.425-1.425 18.4 18.4Zm-.875-6.575-1.5-1.5q.275-.6.425-1.25.15-.65.15-1.325 0-1.125-.425-2.188Q17.15 8.7 16.25 7.8L16 7.55V10h-2V4h6v2h-2.75l.4.35q1.225 1.225 1.788 2.662Q20 10.45 20 11.95q0 1.125-.288 2.137-.287 1.013-.787 1.938Zm-9.45-9.45-1.5-1.5Q8.45 4.8 8.95 4.6q.5-.2 1.05-.35v2.1q-.125.05-.262.1-.138.05-.263.125Z"/></svg>`,

    DoneBig: `<svg width="24" height="24" viewBox="0 0 24 24" fill="#3f3"><path d="M9 16.2L4.8 12l-1.4 1.4L9 19 21 7l-1.4-1.4L9 16.2z"/></svg>`,
    ErrorBig: `<svg width="24" height="24" viewBox="0 0 24 24" fill="#f22"><path d="M19 6.41L17.59 5 12 10.59 6.41 5 5 6.41 10.59 12 5 17.59 6.41 19 12 13.41 17.59 19 19 17.59 13.41 12 19 6.41z"/></svg>`,

    FileDownload: `<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M18,15v3H6v-3H4v3c0,1.1,0.9,2,2,2h12c1.1,0,2-0.9,2-2v-3H18z M17,11l-1.41-1.41L13,12.17V4h-2v8.17L8.41,9.59L7,11l5,5 L17,11z"></path></svg>`,
    FileDownloadOff: `<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M16 18 17.15 20H6Q5.175 20 4.588 19.413 4 18.825 4 18V15H6V18M12.575 15.425 12 16 7 11 7.575 10.425ZM15.6 9.55 17 11 15.425 12.575 14 11.15ZM13 4V10.15L11 8.15V4Z"/><path d="M2.8 2.8 21.2 21.2 19.775 22.625 1.375 4.225Z" fill="#e91429"/></svg>`,

    //Cropped to fit context-menu scale https://svgcrop.com
    SaveAs: `<svg viewBox="140 -820 720 760"><path d="M212.309-140.001q-29.923 0-51.115-21.193-21.193-21.192-21.193-51.115v-535.382q0-29.923 21.193-51.115 21.192-21.193 51.115-21.193h459.229l148.461 148.461v196.614q-14.385-5.692-29.692-7.23-15.307-1.539-30.307.308v-164.769L646.615-760H212.309q-5.385 0-8.847 3.462-3.462 3.462-3.462 8.847v535.382q0 5.385 3.462 8.847 3.462 3.462 8.847 3.462h224.614V-140.001H212.309ZM200-760v560V-760ZM524.616-60.001v-105.692l217.153-216.153q7.462-7.461 16.154-10.5 8.692-3.038 17.384-3.038 9.308 0 18.192 3.538 8.885 3.539 15.961 10.615l37 37.385q6.462 7.461 10 16.153 3.539 8.693 3.539 17.385 0 8.692-3.231 17.692t-10.308 16.461L630.307-60.002H524.616Zm287.691-250.307-37-37.385 37 37.385Zm-240 202.615h38l129.847-130.462-18.385-19-18.615-18.769-130.847 130.231v38Zm149.462-149.462-18.615-18.769 37 37.769-18.385-19ZM255.386-564.616h328.459v-139.998H255.386v139.998ZM480-269.233q4.077 0 7.77-.769 3.692-.769 7.385-2.693l81.382-80.382q1.923-4.308 2.692-7.885.77-3.577.77-8.269 0-41.538-29.231-70.769-29.23-29.23-70.768-29.23T409.232-440q-29.231 29.231-29.231 70.769 0 41.537 29.231 70.768 29.23 29.23 70.768 29.23Z"></path></svg>`,
    Block: `<svg viewBox="80 -880 800 800"><path d="M480-80q-83 0-156-31.5T197-197q-54-54-85.5-127T80-480q0-83 31.5-156T197-763q54-54 127-85.5T480-880q83 0 156 31.5T763-763q54 54 85.5 127T880-480q0 83-31.5 156T763-197q-54 54-127 85.5T480-80Zm0-80q54 0 104-17.5t92-50.5L228-676q-33 42-50.5 92T160-480q0 134 93 227t227 93Zm252-124q33-42 50.5-92T800-480q0-134-93-227t-227-93q-54 0-104 17.5T284-732l448 448Z"></path></svg>`
};

export const Selectors = await extractSelectors(
    "trackListRow", "rowTitle", "rowSubTitle", "rowSectionEnd",
    "rowMoreButton"
);

/**
 * Extracts the specified CSS selectors from xpui.js.
 * Note: This function is expansive and results should be cached.
 */
async function extractSelectors(...names: string[]) {
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