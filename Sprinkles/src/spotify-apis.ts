async function getPlatform(): Promise<any>
{
    function tryGet()
    {
        let reactRoot = (document.querySelector("#main") as any)?._reactRootContainer?._internalRoot;
        if (!reactRoot) return null;
        
        let platform = reactRoot.current?.child?.child?.stateNode?.props?.children?.props?.children?.props?.children?.props?.platform;
        if (!platform) throw Error("Can't find Spotify platform object");
        
        return platform;
    }
    function callback(resolve)
    {
        let apis = tryGet();
        if (apis) {
            resolve(apis);
            return;
        }
        setTimeout(callback, 50, resolve);
    }
    return new Promise(callback);
}

let platform = await getPlatform();
let player: PlayerAPI = platform.getPlayerAPI();
let cosmos = player._cosmos;
let webApi = platform?.getAdManagers()?.hpto?.hptoApi?.webApi; //TODO: this api reports telemetry, is it a good idea to use it?
let user = await platform.getUserAPI().getUser();

export {
    platform as Platform,
    player as Player,
    cosmos as CosmosAsync,
    webApi as WebAPI,

    PlayerAPI,
    PlayerState,
    TrackInfo
};

export class SpotifyUtils
{
    /** Resets the current track (this method creates a new playback id) */
    static async resetCurrentTrack(preservePosition = true)
    {
        let state = player.getState();
        let position = (Date.now() - state.timestamp) * state.speed + state.positionAsOfTimestamp;

        let queue = player._queue;
        let queuedTracks = queue.getQueue().queued;

        let tracks = [{ uri: state.item.uri }];
        if (queuedTracks.length > 0) {
            await queue.insertIntoQueue(tracks, { before: queuedTracks[0] }); 
        } else {
            await queue.addToQueue(tracks);
        }
        await player.skipToNext();
        
        if (preservePosition) {
            await player.seekTo(position);
        }
    }
    static getLocalStorageItem(key: string, prependUsername = true)
    {
        if (prependUsername) {
            key = `${user.username}:${key}`;
        }
        return JSON.parse(localStorage.getItem(key));
    }
    static getPlaylistSortState(uri: string)
    {
        const SortOrders = {
            0: "NONE",
            1: "ASC",
            2: "DESC",
            3: "SECONDARY_ASC",
            4: "SECONDARY_DESC"
        };
        let state = this.getLocalStorageItem("sortedState")?.[uri];
        return !state ? undefined : {
            field: state.column as string,
            order: SortOrders[state.order] as string
        };
    }
}

interface PlayerAPI
{
    _cosmos: any;
    _events: any;
    _client: any;
    _queue: any;
    
    getState(): PlayerState;
    skipToNext(): Promise<void>;
    skipToPrevious(): Promise<void>;
    seekTo(position: number): Promise<void>;
}
interface PlayerState
{
    playbackId: string;
    timestamp: number,
    positionAsOfTimestamp: number,
    speed: number,
    context: {
        uri: string;
        metadata: any;
    },
    item: TrackInfo;
    index: {
        pageURI?: any,
        pageIndex?: number,
        itemIndex?: number;
    }
}
interface TrackInfo
{
    type: "track" | "episode",
    uri: string,
    isLocal: boolean,
    isExplicit: boolean,
    is19PlusOnly: boolean,
    metadata: {
        album_title: string,
        duration: string,
        popularity: string,
        image_url: string,
        image_large_url: string,
        image_small_url: string,
        image_xlarge_url: string,
        context_uri: string,
        album_track_count: string,
        entity_uri: string,
        album_artist_name: string,
        album_disc_number: string,
        artist_uri: string,
        artist_name: string,
        album_disc_count: string,
        title: string,
        album_track_number: string,
        has_lyrics: string,
        album_uri: string,
        is_explicit?: string;
    }
}