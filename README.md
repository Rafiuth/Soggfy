# Soggfy - SpotifyOggDumper
[![Discord](https://discord.com/api/guilds/897274718942531594/widget.png)](https://discord.gg/syc9aMDVBf)

Previous Spotify downloader projects ([XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify), spotifykeydumper) worked by recovering encryption keys and re-downloading tracks manually from Spotify's CDN.

This project takes a completely different approach - by hooking directly into functions that demux the (already decrypted) OGG stream, a replica of the track can be reconstructed without any quality loss. The resulting files are automatically tagged with metadata fetched from Spotify's public API.

**Soggfy does not download from YouTube, or other sources unlike most other "Spotify Downloading Tools." It actually dumps from Spotify!**

# Installation
1. Go to the [releases page](https://github.com/Rafiuth/Soggfy/releases) and download the latest version of Soggfy from the `.zip` file
2. Right click your downloaded `.zip`, then select "Extract All" or "Extract Here" if you have WinRAR or 7-zip installed.
3. Disable your antivirus or Windows Defender because it may flag the `Injector.exe` file you find in the extracted folder. Antiviruses detect this file because they think its some sort of virus that changes a program to make it malicious, but don't worry, this doesn't do that.
4. Open up Spotify, make sure it's updated to the latest version and then run the `Injector.exe` file in the Soggfy folder.
5. Once you ran the Injector, a new Spotify window should open up with `SOGGFY` in it, and everything should look fine.
6. Play the music you want to download and enjoy! Read [Notes](https://github.com/Rafiuth/Soggfy#Notes) for more info.

## Enabling tagging and conversion
1. Run `DownloadFFmpeg.ps1` (right click it and select `Run with PowerShell`). If you see any prompts about execution policy, press A.
2. Drag and drop the `config.json` file into notepad;
3. Change the value of `convert_to` to the format you want, one listed in `formats`. e.g: `"convert_to": "MP3 320K"`
4. Save the file and inject Soggfy as described above.

_Step 1 is not necessary if you already have ffmpeg installed and set on `%PATH%`_

## Blocking Spotify updates
Spotify auto updates itself every about two weeks. Soggfy will stop working because the function addresses that need to be intercepted changes.
You can optionally prevent this by running `BlockSpotifyUpdates.bat`.
   
# Notes
- Tracks are dumped in real time, seeking or skipping will cancel the dump / download.
- Podcasts can be downloaded as long as they are audio only. They may also have lower bitrate: OGG 96Kb/s or MP3 128Kb/s.
- Video podcasts such as The Joe Rogan Experience, Misfits, Storytime with Seth Rogan, and others cannot be downloaded yet.
- Default save path is `%userprofile%/Music/Soggfy`. It can be changed in `config.json`.
- Quality depends on account type: 160Kb/s for free accounts, and 320Kb/s for premium accounts. It may also depend on the client settings.
- **You could get banned by using this. Please consider using alt accounts or keeping backups (see [Exportify](https://watsonbox.github.io/exportify) or [SpotMyBackup](http://www.spotmybackup.com)).**
- Last supported Spotify client version: [1.1.72.439.gc253025e](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.72.439.gc253025e-33.exe)
