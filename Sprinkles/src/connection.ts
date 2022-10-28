import { DeferredPromise } from "./utils";

//Client: JS, Server: C++
enum MessageType {
    /**
     * [S->C] Once the connection is established.
     * [C->S] To update a toplevel config field.
     */
    SYNC_CONFIG = 1,
    /**
     * [S->C] To request metadata for a given playbackId.
     * [C->S] In response for a request with all metadata necessary to save a track.
     * Note that the client also removes the playbackId from the tracking state.
     */
    TRACK_META = 2,
    /** 
     * [C->S] To request the download status of a list of tracks (uri, name, album, artist).
     * [S->C] In response to a request with a map of uri -> { path }.
     * [S->C] Whenever a download is aborted or completed with a playbackId.
     */
    DOWNLOAD_STATUS = 3,
    /**
     * [C->S] To open the file explorer with the specified file selected.
     */
    OPEN_FOLDER = 4,

    /**
     * [C->S] To open a path browser dialog. { type: 1 = OPEN_FILE | 2 = SAVE_FILE | 3 = SELECT_FOLDER }
     */
    OPEN_FILE_PICKER = 5,

    /**
     * [C->S] To write data in a file.
     * Format: { path: string, mode: "replace" (default) | "append" | "keep" }
     * Data to write is either { text: string } or binaryPayload.
     */
    WRITE_FILE = 6,

    READY = -1, //Internal
    CLOSED = -2, //Internal
}
type MessageHandler = (type: MessageType, payload: any, payloadPayload?: ArrayBuffer) => void;

type Message = {
    type: MessageType,
    payload: any,
    binaryPayload?: Uint8Array
}

type RequestInfo = {
    id: number;
    promise?: DeferredPromise<Message>;
}

class Connection {
    private ws: WebSocket;
    private textEnc = new TextEncoder();
    private textDec = new TextDecoder();
    private msgHandler: MessageHandler;
    private connAttempts = 0;

    private pendingReqs: RequestInfo[] = [];
    private nextReqId = 1;

    get isConnected() {
        return this.ws.readyState === WebSocket.OPEN;
    }

    constructor(msgHandler: MessageHandler) {
        this.msgHandler = msgHandler;
        this.reconnect();
    }

    /**
     * @param {number} type
     * @param {any} payload
     * @param {ArrayBuffer} binaryPayload
     */
    send(type: MessageType, payload: any, binaryPayload?: ArrayBuffer) {
        if (!this.isConnected) return;

        let payloadStr = JSON.stringify(payload);
        let payloadData = this.textEnc.encode(payloadStr);
        let data = new Uint8Array(5 + payloadData.length + (binaryPayload?.byteLength ?? 0));
        let view = new DataView(data.buffer);
        view.setUint8(0, type);
        view.setInt32(1, payloadData.length, true);
        data.set(payloadData, 5);
        if (binaryPayload) {
            data.set(new Uint8Array(binaryPayload), 5 + payloadData.length);
        }
        this.ws.send(data);
    }

    /**
     * Sends an message with an "reqId" field and waits for the response.
     * @param payload The message to send. Will be mutated with the "reqId" field.
     * @param timeoutMs How long to wait before failing the promise. Values <= 0 indicate infinite.
     */
    async request(type: MessageType, payload: any, binaryPayload?: ArrayBuffer, timeoutMs = 15000) {
        if (!this.isConnected) throw Error("Can't send request in disconnected state.");

        let info: RequestInfo = {
            id: this.nextReqId++
        };
        let index = this.pendingReqs.push(info);
        info.promise = new DeferredPromise<Message>(() => {
            this.pendingReqs.splice(index);
        }, timeoutMs);

        payload.reqId = info.id;
        this.send(type, payload, binaryPayload);

        return info.promise;
    }
    private handleResponse(id: number, msg: Message) {
        let reqs = this.pendingReqs;
        for (let i = 0; i < reqs.length; i++) {
            if (reqs[i].id === id) {
                reqs[i].promise.resolve(msg);
                reqs.splice(i);
                return;
            }
        }
        console.warn(`Response for unknown request '${id}'`, msg.type, msg.payload);
    }

    private onMessage(ev: MessageEvent<ArrayBuffer>) {
        let view = new DataView(ev.data);
        let type = view.getUint8(0);
        let payloadLen = view.getInt32(1, true);
        let payloadData = new Uint8Array(ev.data, 5, payloadLen);
        let binaryPayload = new Uint8Array(ev.data, 5 + payloadLen);

        let payloadStr = this.textDec.decode(payloadData);
        let payload = JSON.parse(payloadStr);

        let reqId = payload["reqId"];
        if (reqId) {
            this.handleResponse(reqId, {
                type: type,
                payload: payload,
                binaryPayload: binaryPayload
            });
            return;
        }
        this.msgHandler(type, payload, binaryPayload);
    }

    private reconnect() {
        this.ws = new WebSocket("ws://127.0.0.1:28653/sgf_ctrl");
        this.ws.binaryType = "arraybuffer";
        this.ws.onmessage = this.onMessage.bind(this);
        this.ws.onopen = () => {
            this.msgHandler(MessageType.READY, {});
            this.connAttempts = 0;
        };
        this.ws.onclose = (ev) => {
            for (let req of this.pendingReqs) {
                req.promise.reject("Connection lost");
            }
            this.pendingReqs = [];

            this.msgHandler(MessageType.CLOSED, {
                code: ev.code,
                reason: ev.reason
            });
            //stop trying to reconnect after a few attempts to prevent
            //spamming the console with errors, because its annoying af
            if (this.connAttempts++ < 3) {
                setTimeout(this.reconnect.bind(this), 15000);
            }
        };
    }
}

export {
    Connection,
    MessageType
}