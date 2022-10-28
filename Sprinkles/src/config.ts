let config = {
    playbackSpeed: 1.0,
    downloaderEnabled: true,
    skipDownloadedTracks: false,
    skipIgnoredTracks: false,
    embedLyrics: true,
    saveLyrics: true,
    embedCoverArt: true,
    saveCoverArt: true,
    saveCanvas: false,
    outputFormat: {
        args: "-c copy",
        ext: ""
    },
    savePaths: {
        basePath: "",
        track: "{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.ogg",
        episode: "Podcasts/{artist_name}/{album_name}/{release_date} - {track_name}.ogg",
        canvas: "{artist_name}/{album_name}{multi_disc_path}/Canvas/{track_num}. {track_name}.mp4",
        invalidCharRepl: "unicode", //nasty global, used in PathTemplate.escapePath()
    },
    //Resource URIs to be ignored by the downloader.
    ignorelist: {
    },
    blockAds: true
};
export default config;

export function isTrackIgnored(track) {
    let tieUris = [
        track.uri,
        track.album?.uri ?? track.albumUri,
        track.metadata?.context_uri ?? track.contextUri,
        ...(track.artists ?? [])
    ];
    return tieUris.some(res => config.ignorelist[res?.uri ?? res]);
}