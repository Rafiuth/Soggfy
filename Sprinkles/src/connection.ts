//Client: JS, Server: C++
enum MessageType
{
    /**
     * [S->C] Once the connection is established.
     * [C->S] To update a toplevel config field.
     */
    SYNC_CONFIG      = 1,
    /**
     * [S->C] To request metadata for a given playbackId.
     * [C->S] In response for a request with all metadata necessary to save a track.
     * Note that the client also removes the playbackId from the tracking state.
     */
    TRACK_META       = 2,
    /** 
     * [C->S] To request the download status of a list of tracks (uri, name, album, artist).
     * [S->C] In response to a request with a map of uri -> { path }.
     * [S->C] Whenever a download is aborted or completed with a playbackId.
     */
    DOWNLOAD_STATUS  = 3,
    /**
     * [C->S] To open the file explorer with the specified file selected.
     */
    OPEN_FOLDER      = 4,

    /**
     * [C->S] To open a directory browse dialog and update config.savePaths.basePath accordingly
     */
    BROWSE_FOLDER    = 5,
    
    READY            = -1, //Internal
    CLOSED           = -2, //Internal
}
type MessageHandler = (type: MessageType, payload: any, payloadPayload?: ArrayBuffer) => void;

class Connection
{
    private _ws: WebSocket;
    private _textEnc = new TextEncoder();
    private _textDec = new TextDecoder();
    private _msgHandler: MessageHandler;
    private _connAttempts = 0;
    
    get isConnected()
    {
        return this._ws.readyState === WebSocket.OPEN;
    }
    
    constructor(msgHandler: MessageHandler)
    {
        this._msgHandler = msgHandler;
        this.reconnect();
    }

    /**
     * @param {number} type
     * @param {any} payload
     * @param {ArrayBuffer} binaryPayload
     */
    send(type: MessageType, payload: any, binaryPayload?: ArrayBuffer)
    {
        if (!this.isConnected) return;
        
        let payloadStr = JSON.stringify(payload);
        let payloadData = this._textEnc.encode(payloadStr);
        let data = new Uint8Array(5 + payloadData.length + (binaryPayload?.byteLength ?? 0));
        let view = new DataView(data.buffer);
        view.setUint8(0, type);
        view.setInt32(1, payloadData.length, true);
        data.set(payloadData, 5);
        if (binaryPayload) {
            data.set(new Uint8Array(binaryPayload), 5 + payloadData.length);
        }
        this._ws.send(data);
    }

    private reconnect()
    {
        this._ws = new WebSocket("ws://127.0.0.1:28653/sgf_ctrl");
        this._ws.binaryType = "arraybuffer";
        this._ws.onmessage = this.onMessage.bind(this);
        this._ws.onopen = () => {
            this._msgHandler(MessageType.READY, {});
            this._connAttempts = 0;
        };
        this._ws.onclose = (ev) => {
            this._msgHandler(MessageType.CLOSED, {
                code: ev.code,
                reason: ev.reason
            });
            //stop trying to reconnect after a few attempts to prevent
            //spamming the console with errors, because its annoying af
            if (this._connAttempts++ < 3) {
                setTimeout(this.reconnect.bind(this), 15000);
            }
        };
    }
    private onMessage(ev: MessageEvent<ArrayBuffer>)
    {
        let view = new DataView(ev.data);
        let type = view.getUint8(0);
        let payloadLen = view.getInt32(1, true);
        let payloadData = new Uint8Array(ev.data, 5, payloadLen);
        let binaryPayload = new Uint8Array(ev.data, 5 + payloadLen);

        let payloadStr = this._textDec.decode(payloadData);
        let payload = JSON.parse(payloadStr);

        this._msgHandler(type, payload, binaryPayload);
    }
}

export {
    Connection,
    MessageType
}