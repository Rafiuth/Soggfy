# Building Soggfy

For end users, the easiest way to build Soggfy is by forking (or reuploading) the repository into GitHub and manually triggering the _Release_ workflow in GitHub Actions. Alternatively, the steps below describe how to build locally.

## Requirements
- [Visual Studio 2019+](https://visualstudio.microsoft.com/downloads) or [Build Tools for VS](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022)
- [Node.js 16+](https://nodejs.org/en/download/)

Soggfy is split into three components: 
- _Injector_: Responsible for launching and/or loading the _Core DLL_ into Spotify.
- _Core DLL_ (SpotifyOggDumper): Audio/player hooks, state and file management.
- _Sprinkles_: UI integration and track metadata provider. This is a single JS bundle injected into the main CEF window by the _Core DLL_.

To build the project, open a "Native Tools CMD for VS", cd into the repository directory and run the following commands:

```
fetch_external_deps.bat

msbuild /p:Configuration=Release

cd Sprinkles
npm run build
copy dist\bundle.js ..\build\Release\Soggfy.js /y
```

Note that the Core DLL depends on some CEF headers, they are downloaded by the `fetch_external_deps.bat` script, which assumes that curl and 7z are available. You can alternatively manually download the `.tar` file (link in the .bat) and extract it into `external/cef_bin/` folder.

# Development Guide
Client reverse engineering is done with [Ghidra](https://ghidra-sre.org/), [x64dbg](https://github.com/x64dbg/x64dbg), and [CheatEngine](https://github.com/cheat-engine/cheat-engine/) ([Frida](https://frida.re/docs/installation/) might be a better alternative). Most functions can be found via strings, but the _audio decode dispatch_ function is tricky. You need to place a breakpoint on e.g. the ogg decoder function and find the (virtual) dispatcher on the call stack.

Debugging the Core DLL is be easily done by attaching the VS debugger into the main Spotify process after injecting the DLL (there are no anti-debugging tricks). Change cycles are slow, you'll need to rebuild and reinject the DLL, but in most cases, you won't need to restart Spotify - just "uninject" the DLL by typing `u` in the console, then build and reinject.

The easiest way to develop Sprinkles is/was via [Spicetify](https://spicetify.app/docs/getting-started) after adding it as an extension. Because Spotify locked the builtin DevTools in v1.1.80 (and `spicetify enable-devtools` is pretty bad now), you can instead launch Spotify with `Spotify.exe --remote-debugging-port=9222` and use the remote debugger in `chrome://inspect`.

After adding Soggfy as a Spicetify extension, the `dev_build.bat` will build and copy it to the extension folder, but Spotify must be reloaded by running `location.reload()` on the DevTools console (Ctrl+R no longer works).