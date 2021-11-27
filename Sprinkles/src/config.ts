let config = {
    enabled: true,
    downloadLyrics: true,
    embedCoverArt: true,
    outputFormat: {
        args: "-c copy",
        ext: ""
    },
    playbackSpeed: 1.0,
    savePaths: {
        track: {
            audio: "{user_home}/Music/Soggfy/{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}.ogg",
            cover: "{user_home}/Music/Soggfy/{artist_name}/{album_name}{multi_disc_path}/cover.jpg"
        },
        podcast: {
            audio: "{user_home}/Music/SoggfyPodcasts/{artist_name}/{album_name}/{track_name}.ogg",
            cover: ""
        }
    }
};
export default config;