# Soggfy - SpotifyOggDumper

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN.

This project takes a completely different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any quality loss. The resulting files are automatically tagged with metadata fetched from Spotify's public API.

# Installation
1. Go to the [releases page](https://github.com/Rafiuth/Soggfy/releases) and download the latest version of Soggfy from the `.7z` file
2. Download and install [7-zip](https://7-zip.org), right click your downloaded `.7z`, then go to 7-zip in the right-click toolbar, and click the "Extract" button.
3. Disable your antivirus or Windows Defender because it may flag the `Injector.exe` file you find in the extracted folder. Antiviruses detect this file because they think its some sort of virus that changes a program to make it malicious, but don't worry, this doesn't do that.
4. After you disable your Antivirus, open up Spotify, and make sure its updated to the latest version and then run the `Injector.exe` file in the Soggfy folder.
5. If you want, you can also run `BlockSpotifyUpdates.bat` to stop Spotify updates. This is optional.
6. Once you ran the Injector, a new Spotify window should open up with `SOGGFY` in it, and everything should look fine.
7. After that, play the music you want to download and enjoy! Read [Notes](https://github.com/Rafiuth/Soggfy#Notes) for more info.

# Notes
- This program is only for Windows as of now.
- Tracks are dumped in real time, seeking or skipping will cancel the dump.
- Default save path is `%userprofile%/Music/Soggfy`. It can be changed in `config.json`.
- Quality depends on account type: 160Kb/s for free accounts, and 320Kb/s for premium accounts. It may also depend on the client settings.
- There could be a non-zero chance of getting banned by using this (tracked in [#1](https://github.com/Rafiuth/Soggfy/issues/1))
- Last supported version: [1.1.69.612.gb7409abc](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.69.612.gb7409abc-16.exe)
