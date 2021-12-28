<div align="center">

# Soggfy - SpotifyOggDumper

A downloader mod for the Windows Spotify client
[![Discord](https://discord.com/api/guilds/897274718942531594/widget.png)](https://discord.gg/syc9aMDVBf)

<img align="right" src="https://user-images.githubusercontent.com/53208252/147526053-a62850c2-9ee9-471f-83c1-481f2f0dca32.png" width="250" />
</div>

# Features
- Integration with Spotify's UI
- Download and/or embed lyrics
- Generate M3U for albums and playlists
- Controllable playback speed
- Automatic conversion to MP3 and many other formats

# Installation and Usage
1. Download and extract the `.zip` package of the [latest release](https://github.com/Rafiuth/Soggfy/releases/latest)
2. Open Spotify, then run `Injector.exe`. If it works, a download button will appear on Spotify's top bar (you may click on it to change settings)
3. After that, any song you play will be downloaded to the selected save path

You may need to disable or whitelist Soggfy in your anti-virus for it to work.

If the injector crashes because missing DLLs, you need to install the [MSVC Redistributable package](https://aka.ms/vs/17/release/vc_redist.x86.exe).

# Notes
- Songs are only downloaded if you play them from start to finish, without seeking (pausing is fine).
- Quality depends on the account you are using: _160Kb/s_ or _320Kb/s_ for _free_ and _premium_ accounts respectively. Note that you may need to change the streaming quality to "Very high" on Spotify settings.
_This only applies to the original OGGs._
- If you are converting to AAC and care about quality, read this: [High quality AAC](/USAGE.md#high-quality-aac)
- **You could get banned by using this. Please consider using alt accounts or keeping backups (see [Exportify](https://watsonbox.github.io/exportify) and [SpotMyBackup](http://www.spotmybackup.com)).**
- Last supported Spotify client version: [1.1.74.631.g0b24d9ad-16](https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.74.631.g0b24d9ad-16.exe)

# How it works
Soggfy works by intercepting and dumping OGG streams in real time (hence _ogg dumper_ -> s_ogg_fy). Note that this is different from recording the audio output - this actually gets you a copy of the original OGG files, with no loss in quality. They are then optionally converted and embedded with metadata, depending on settings.

# Credits/Tools
- [XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify) and spotifykeydumper - The main source of inspiration for this project
- [Spicetify](https://github.com/khanhas/spicetify-cli) - Used during development of the UI integration code
- [abba23's spotify-adblock](https://github.com/abba23/spotify-adblock) - For the built-in ad/telemetry blocker
- [Ghidra](https://ghidra-sre.org/) and [x64dbg](https://x64dbg.com/) - Tools for reversing and debugging the client
