import { Connection, MessageType } from "./connection";
import PlayerStateTracker from "./player-state-tracker";
import { DownloadStatus, TrackStatus } from "./ui/status-indicator";
import UI from "./ui/ui";
import config from "./config";
import Utils from "./utils";

//Hack so we can navigate into the source on devtools
if (!PRODUCTION) console.log(() => { "Go to source" });

let conn = new Connection(onMessage);
let playbackTracker = new PlayerStateTracker(conn);
let ui = new UI(conn);

function onMessage(type: MessageType, payload: any) {
    switch (type) {
        case MessageType.SYNC_CONFIG: {
            Utils.deepMerge(config, payload);
            ui.syncConfig(payload);
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
                    setPlaybackStatusInd(payload.playbackId, { status: DownloadStatus.Error, message: ex.message });
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
                        status: DownloadStatus.Warn,
                        message: "Different tracks mapping to the same file name"
                    };
                    for (let subkey of key.split(',')) {
                        results[subkey] = val;
                    }
                }
                ui.statusIndicator.updateRows(results);
            }
            break;
        }
    }
}

function setPlaybackStatusInd(playbackId: string, data: TrackStatus) {
    let info = playbackTracker.getTrackInfo(playbackId);
    if (info) {
        ui.statusIndicator.updateRows({ [info.uri]: data });
    }
    if (data.status === DownloadStatus.Error || data.status === DownloadStatus.Done) {
        playbackTracker.remove(playbackId);
    }
}