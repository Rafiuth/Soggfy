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
    
export {
    platform as Platform,
    player as Player,
    cosmos as CosmosAsync,
    webApi as WebAPI,

    PlayerAPI,
    PlayerState,
    TrackInfo
};

interface PlayerAPI
{
    _cosmos: any;
    _events: any;
    _state: PlayerState;
}
interface PlayerState
{
    playbackId: string;
    timestamp: number,
    context: {
        uri: string;
        metadata: any;
    },
    item: TrackInfo;
}
interface TrackInfo
{
    type: string,
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