# Usage
Notes on some features

## Playback Speed
The speed slider is limited to 20x max, but the textbox on the left side will accept any value.
Notes:
- Speed >= ~30x will cause the player to shutter/glitch and silently spam requests to a Spotify API, which could increase the changes of your account getting banned. (~50x seems to work fine, so you may want to test different values).
- Speed >= ~200x will saturate CPU. Altough it seems to work fine, this may increase the chance of crashes or unexpected behavior.
- Shuttering may happen just after changing the value, but it might stop on the next song. There also seems to be a memory leak, but it goes away after some time. (?)

## Config Files
Since `v2.2.0`, Soggfy saves configuration files in `%appdata%/Soggfy` so you don't have to copy config.json on each update. They can be deleted using the `Uninstall.bat` script, or manually.

## High quality AAC
FFmpeg's native AAC encoder is known to generate poor quality files[^1][^2], the alternative encoder _libfdk_aac_ is not distributed in popular builds due to licensing issues (also because PowerShell doesn't support .7z); you need to either build it yourself or download from somewhere else:
- https://github.com/AnimMouse/ffmpeg-autobuild/releases
- https://github.com/marierose147/ffmpeg_windows_exe_with_fdk_aac/releases

To install it, add the binary folder in your `%PATH%` or copy it to the Soggfy folder (i.e. `Soggfy/ffmpeg/ffmpeg.exe`). Then select the "Custom" option in the format setting, and use these values:
- Arguments: `-c:a libfdk_aac -b:a 192k -disposition:v attached_pic -c:v copy` (you may change `192k` to your preferred bitrate)
- Extension: either `m4a` or `mp4`

# Refs
[^1]: https://trac.ffmpeg.org/wiki/Encode/HighQualityAudio
[^2]: https://trac.ffmpeg.org/wiki/Encode/AAC
