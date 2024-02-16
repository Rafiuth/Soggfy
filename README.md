<div align="center">

# Soggfy - SpotifyOggDumper

A music downloader mod for the Windows Spotify client

<img align="right" src="https://user-images.githubusercontent.com/53208252/147526053-a62850c2-9ee9-471f-83c1-481f2f0dca32.png" width="250" />
</div>

# Features
- Download tracks directly from Spotify
- Download and embed metadata, lyrics and canvas
- Generate M3U for albums and playlists
- Automatic conversion to MP3 and many other formats

# Installation and Usage
1. Download and extract the `.zip` package of the [latest release](https://github.com/Rafiuth/Soggfy/releases/latest).
2. Double click the `Install.cmd` file. It will run the Install.ps1 script with Execution Policy Bypass. Wait for it to finish.
3. Open Spotify and play the songs you want to download.

Tracks are saved in the Music folder by default. The settings panel can be accessed by hovering next to the download button in the navigation bar.  
Hovering the check mark drawn on each individual track will display a popup offering to open the folder containing them.

You may need to disable or whitelist Soggfy in your anti-virus for it to work.

If the Spotify client crashes because of missing DLLs, you may need to install the [MSVC Redistributable package](https://aka.ms/vs/17/release/vc_redist.x86.exe).

# Notes
- Songs are only downloaded if played from start to finish, without seeking (pausing is fine).
- Quality depends on the account being used: _160Kb/s_ or _320Kb/s_ for _free_ and _premium_ plans respectively. You may also need to change the streaming quality to "Very high" on Spotify settings to get _320Kb/s_ files.
- Podcast support is very hit or miss and will only work with audio-only OGG podcasts (usually the exclusive ones).
- **This mod breaks [Spotify's Guidelines](https://www.spotify.com/us/legal/user-guidelines/) and using it could get your account banned. Consider using alt accounts or keeping backups (see [Exportify](https://github.com/watsonbox/exportify) and [SpotMyBackup](http://www.spotmybackup.com)).**

# How it works
Soggfy works by intercepting Spotify's OGG parser and capturing the unencrypted data during playback. This process is similar to recording, but it results in an exact copy of the original files served by Spotify, without ever extracting keys or actually re-downloading them.  
Conversion and metadata is then applied according to user settings.

# Manual Installation
If you are having issues with the install script, try following the steps below for a manual installation:

1. Download and install the _correct_ Spotify client version using the link inside the Install.ps1 script.
2. Copy and rename `SpotifyOggDumper.dll` to `%appdata%/Spotify/dpapi.dll`
3. Copy `SoggfyUIC.js` to `%appdata%/Spotify/SoggfyUIC.js`
4. Download and extract [FFmpeg binaries](https://github.com/AnimMouse/ffmpeg-autobuild/releases) to `%localappdata%/Soggfy/ffmpeg/ffmpeg.exe` (or add them to `%PATH%`).

Alternatively, `Injector.exe` can be used to launch _or_ inject Soggfy into an already running Spotify instance. A portable and self-contained install can be made by copying Spotify binaries from `%appdata%/Spotify/` to `Soggfy/Spotify/`.

# Credits
- [XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify) and spotifykeydumper - Inspiration for this project
- [Spicetify](https://github.com/khanhas/spicetify-cli), [Ghidra](https://ghidra-sre.org/) and [x64dbg](https://x64dbg.com/) - Tools for reversing and debugging the client
- [abba23's spotify-adblock](https://github.com/abba23/spotify-adblock) - The built-in telemetry/update blocker is based on this
