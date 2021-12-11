let config = {
    outputFormat: {
        args: "-c copy",
        ext: ""
    },
    downloadLyrics: true,
    embedCoverArt: true,
    generatePlaylistM3U: false,
    playbackSpeed: 1.0,
    savePaths: {
        basePath: "{user_home}/Music/Soggfy",
        track: "Songs/{artist_name} - {track_name}",
        album: "Albums/{artist_name}/{album_name}{multi_disc_path}/{track_num}. {track_name}",
        playlist: "Playlists/{playlist_name}/{artist_name} - {track_name}",
        podcast: "Podcasts/{artist_name}/{album_name}/{release_date} - {track_name}",
    }
};
export default config;