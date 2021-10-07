# Soggfy - SpotifyOggDumper

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN. This project takes a completely different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any quality loss. The resulting files are automatically tagged with metadata fetched from Spotify's public API.

# Notes
- Tracks are dumped in real time, seeking or skipping will cancel the dump.
- Default save path is `%userprofile%/Music/Soggfy`. It can be changed in `config.json`.
- Quality depends on account type: 160Kb/s for free accounts, and 320Kb/s for premium accounts. It may also depend on the client settings.
- There could be a non-zero chance of getting banned by using this (tracked in [#1](https://github.com/Rafiuth/Soggfy/issues/1))
- Last supported version: [1.1.69.612.gb7409abc](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.69.612.gb7409abc-16.exe)