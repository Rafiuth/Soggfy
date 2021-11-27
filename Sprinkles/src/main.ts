import { Connection, MessageType } from "./connection";
import PlayerStateTracker from "./player-state-tracker";
import DownloadStatusHook from "./ui/download-status";
import UI from "./ui/ui";
import config from "./config";

const IS_DEBUG = true;

let conn = new Connection(messageHandler);

let playerState = new PlayerStateTracker();
let ui = new UI(conn);
let statusIndicator = new DownloadStatusHook(conn);
ui.install();

function messageHandler(type: MessageType, payload: any)
{
    switch (type) {
        case MessageType.READY: {
            ui.setEnabled(true);
            break;
        }
        case MessageType.CLOSED: {
            if (!IS_DEBUG) {
                ui.setEnabled(false);
            }
            break;
        }
        case MessageType.SYNC_CONFIG: {
            for (let key in payload) {
                config[key] = payload[key];
            }
            break;
        }
        case MessageType.TRACK_META: {
            playerState.getPlaybackMetadata(payload.playbackId).then(data => {
                conn.send(MessageType.TRACK_META, data.info, data.coverData);
            });
            break;
        }
        case MessageType.DOWNLOAD_STATUS: {
            if (payload.playbackId) {
                let info = playerState.getPlaybackTrack(payload.playbackId);
                if (info) {
                    statusIndicator.updateRows({
                        [info.uri]: {
                            errorMessage: payload.message
                        }
                    });
                }
            } else {
                statusIndicator.updateRows(payload.tracks);
            }
            break;
        }
    }
}