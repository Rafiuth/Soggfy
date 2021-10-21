# Soggfy - SpotifyOggDumper
[![Discord](https://discord.com/api/guilds/897274718942531594/widget.png)](https://discord.gg/syc9aMDVBf)

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN.

This project takes a completely different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any quality loss. The resulting files are automatically tagged with metadata fetched from Spotify's public API.

**Soggfy does not download from YouTube, or other sources unlike most other "Spotify Downloading Tools." It actually dumps from Spotify!**

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
- Tracks are dumped in real time, seeking or skipping will cancel the dump / download.
- Podcasts are downloadable as long as they are on OGG format. If a podcast is in MP3, then it will not be downloaded. This means some podcasts such as The Joe Rogan Experience, Misfits, Storytime with Seth Rogan, and others cannot be downloaded. Note that some podcasts aren't 160Kb/s or 320Kb/s either, they can be 96Kb/s too.
- Default save path is `%userprofile%/Music/Soggfy`. It can be changed in `config.json`.
- Quality depends on account type: 160Kb/s for free accounts, and 320Kb/s for premium accounts. It may also depend on the client settings.
- **You could get banned by using this. Please consider using alt accounts or keeping backups (see [Exportify](https://watsonbox.github.io/exportify) or [SpotMyBackup](http://www.spotmybackup.com)).**
- Last supported Spotify client version: [1.1.70.610.g4585142b](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.70.610.g4585142b-8.exe)
