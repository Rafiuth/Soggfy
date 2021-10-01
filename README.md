# Soggfy - SpotifyOggDumper

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN. This project takes a completely different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any quality loss. The resulting files are automatically tagged with metadata fetched from Spotify's public API.

# Notes
- Tracks are dumped in real time, seeking or skipping a song will cancel the dump.
- Default save path is `%userprofile%/Music/Soggfy`. It can be changed in `config.json`.
- This has not been tested on premium accounts. Try at your own risk.
- Last supported version: 1.1.69.612.gb7409abc