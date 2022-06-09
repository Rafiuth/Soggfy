<div align="center">

# Soggfy - SpotifyOggDumper

A music downloader mod for the Windows Spotify client
[![Discord](https://discord.com/api/guilds/897274718942531594/widget.png)](https://discord.gg/syc9aMDVBf)

<img align="right" src="https://user-images.githubusercontent.com/53208252/147526053-a62850c2-9ee9-471f-83c1-481f2f0dca32.png" width="250" />
</div>

# Features
- Download tracks directly from Spotify
- Download and embed metadata, lyrics and canvas
- Generate M3U for albums and playlists
- Automatic conversion to MP3 and many other formats

# Usage
1. Download and extract the `.zip` package of the [latest release](https://github.com/Rafiuth/Soggfy/releases/latest)
2. Right click the `DownloadFFmpeg.ps1` file, and select "Run with PowerShell". If it prompts about execution policy, press A to allow, then wait for it to finish.
3. Open Spotify, then run `Injector.exe`. Shortly after it loads, a download button should appear on Spotify's navigation bar (you may click on it to change settings).
4. After that, any song you play will be downloaded to the selected save path.

Last tested Spotify version: [1.1.87.612](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.87.612.gf8d110e2-19.exe)  
_If tracks are not being downloaded or Spotify is crashing, try downgrading to this version and running the `BlockSpotifyUpdates.bat` script._

You may need to disable or whitelist Soggfy in your anti-virus for it to work.

If the injector crashes because of missing DLLs, you need to install the [MSVC Redistributable package](https://aka.ms/vs/17/release/vc_redist.x86.exe).

# Notes
- Songs are only downloaded if you play them from start to finish, without seeking (pausing is fine).
- Quality depends on the account you are using: _160Kb/s_ or _320Kb/s_ for _free_ and _premium_ plans respectively. You may need to change the streaming quality to "Very high" on Spotify settings to get _320Kb/s_ files.
- If you are _converting_ to AAC and care about quality, read this: [High quality AAC](/USAGE.md#high-quality-aac)
- **Your account could get banned by using this. Consider using alt accounts or keeping backups (see [Exportify](https://github.com/watsonbox/exportify) and [SpotMyBackup](http://www.spotmybackup.com)).**

# How it works
Soggfy works by hijacking/intercepting functions used by Spotify to decode OGG files, which receives the data in plaintext chunks throughout playback.
By merging all chunks together, an exact copy of the original (unenc​‌r​‌y​‌pted) OGG file served by Spotify can be obtained.
The resulting files are then optionally converted and embedded with metadata, depending on settings.

# Credits
- [XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify) and spotifykeydumper - Inspiration for this project
- [abba23's spotify-adblock](https://github.com/abba23/spotify-adblock) - For the built-in telemetry blocker
- [Spicetify](https://github.com/khanhas/spicetify-cli), [Ghidra](https://ghidra-sre.org/) and [x64dbg](https://x64dbg.com/) - Tools for reversing and debugging the client
