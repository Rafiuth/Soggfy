import { Connection, MessageType } from "./connection";
import PlayerStateTracker from "./player-state-tracker";
import { StatusIndicator, DownloadStatus, TrackStatus } from "./ui/status-indicator";
import UI from "./ui/ui";
import config from "./config";
import { PlayerState } from "./spotify-apis";
import Utils from "./utils";

let conn = new Connection(onMessage);
let playbackTracker = new PlayerStateTracker(conn, onPlayerStateChanged);
let ui = new UI(conn);
let statusIndicator = new StatusIndicator(conn);
ui.install();

function onPlayerStateChanged(newState: PlayerState, oldState?: PlayerState)
{
    if (newState.playbackId !== oldState?.playbackId) {
        conn.send(MessageType.DOWNLOAD_STATUS, { playbackId: newState.playbackId });
    }
}
function onMessage(type: MessageType, payload: any)
{
    switch (type) {
        case MessageType.READY: {
            ui.setEnabled(true);
            break;
        }
        case MessageType.CLOSED: {
            if (PRODUCTION) {
                ui.setEnabled(false);
            }
            break;
        }
        case MessageType.SYNC_CONFIG: {
            Utils.deepMerge(config, payload);
            conn.send(MessageType.SYNC_CONFIG, config);
            break;
        }
        case MessageType.TRACK_META: {
            playbackTracker.getMetadata(payload.playbackId)
                .then(data => {
                    conn.send(MessageType.TRACK_META, data.info, data.coverData);
                })
                .catch(ex => {
                    conn.send(MessageType.TRACK_META, {
                        playbackId: payload.playbackId,
                        failed: true,
                        message: ex.message
                    });
                    setPlaybackStatusInd(payload.playbackId, { status: DownloadStatus.ERROR, message: ex.message });
                })
                .finally(() => {
                    playbackTracker.remove(payload.playbackId);
                });
            break;
        }
        case MessageType.DOWNLOAD_STATUS: {
            if (payload.playbackId) {
                setPlaybackStatusInd(payload.playbackId, payload);
            } else {
                let results = payload.results;
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
                statusIndicator.updateRows(results);
            }
            break;
        }
    }
}

function setPlaybackStatusInd(playbackId: string, data: TrackStatus)
{
    let info = playbackTracker.getTrackInfo(playbackId);
    if (info) {
        statusIndicator.updateRows({ [info.uri]: data });
    }
    if (data.status === DownloadStatus.ERROR || data.status === DownloadStatus.DONE) {
        playbackTracker.remove(playbackId);
    }
}