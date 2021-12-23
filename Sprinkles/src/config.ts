let config = {
    outputFormat: {
        args: "-c copy",
        ext: ""
    },
    embedLyrics: true,
    saveLyrics: true,
    embedCoverArt: true,
    saveCoverArt: true,
    generatePlaylistM3U: true,
    playbackSpeed: 1.0,
    savePaths: {
        basePath: "{user_home}/Music/Soggfy",
        track: "{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}",
        episode: "Podcasts/{artist_name}/{album_name}/{release_date} - {track_name}",
        playlistM3U: "{playlist_name}.m3u8",
    }
};
export default config;