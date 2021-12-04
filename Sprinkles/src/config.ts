let config = {
    outputFormat: {
        args: "-c copy",
        ext: ""
    },
    downloadLyrics: true,
    embedCoverArt: true,
    playbackSpeed: 1.0,
    savePaths: {
        track: "{user_home}/Music/Soggfy/{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.ogg",
        episode: "{user_home}/Music/SoggfyPodcasts/{artist_name}/{album_name}/{track_name}.ogg",
    },
    saveCoverArt: true
};
export default config;