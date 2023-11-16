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
3. Run `Injector.exe`, and wait for Spotify to open.
4. Play the songs you want to download.

Tracks are saved on the Music folder by default, this can be changed on the settings pane, which can be accessed through the controls button shown after hovering the download toggle button in the navigation bar.  
Hovering the check mark drawn on each individual track will display a popup offering to open the folder containing them.

You may need to disable or whitelist Soggfy in your anti-virus for it to work.

If the injector crashes because of missing DLLs, you need to install the [MSVC Redistributable package](https://aka.ms/vs/17/release/vc_redist.x86.exe).

# Notes
- Songs are only downloaded if played from start to finish, without seeking (pausing is fine).
- Quality depends on the account being used: _160Kb/s_ or _320Kb/s_ for _free_ and _premium_ plans respectively. You may also need to change the streaming quality to "Very high" on Spotify settings to get _320Kb/s_ files.
- If you are _converting_ to AAC and care about quality, see [High quality AAC](/USAGE.md#high-quality-aac).
- **This tool breaks [Spotify's Guidelines](https://www.spotify.com/us/legal/user-guidelines/) and using it could get your account banned. Consider using alt accounts or keeping backups (see [Exportify](https://github.com/watsonbox/exportify) and [SpotMyBackup](http://www.spotmybackup.com)).**

# How it works
Soggfy works by intercepting Spotify's OGG parser and capturing the unencr​ypted data during playback. This process is similar to recording, but it results in an exact copy of the original file served by Spotify, without ever extracting k​eys or actually re-downloading it.  
Conversion and metadata is then applied according to user settings.

# Manual Install
If you are having issues with the install script, try following the steps below for a manual installation:
1. Download and install the _correct_ Spotify client version using the link inside the install script.
2. Go to `%appdata%` and copy the `Spotify` folder to the `Soggfy` folder (such that the final structure looks like `Soggfy/Spotify/Spotify.exe`).  
_Note that you can also install other mods such as Spicetify or SpotX before this step._
3. [Download and install FFmpeg](/USAGE.md#high-quality-aac).

# Credits
- [XSpotify](https://web.archive.org/web/20200303145624/https://github.com/meik97/XSpotify) and spotifykeydumper - Inspiration for this project
- [Spicetify](https://github.com/khanhas/spicetify-cli), [Ghidra](https://ghidra-sre.org/) and [x64dbg](https://x64dbg.com/) - Tools for reversing and debugging the client
- [abba23's spotify-adblock](https://github.com/abba23/spotify-adblock) - The built-in telemetry/update blocker is based on this
