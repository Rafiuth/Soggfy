# Usage
Misc features and other details

## High quality AAC
FFmpeg's native AAC encoder is known to generate poor quality files[^1][^2], the alternative encoder _libfdk_aac_ is not distributed in popular builds due to licensing issues (also because PowerShell doesn't support .7z); you need to either build it yourself or download from somewhere else:
- https://github.com/AnimMouse/ffmpeg-autobuild/releases
- https://github.com/marierose147/ffmpeg_windows_exe_with_fdk_aac/releases

To install it, add the binary folder in your `%PATH%` or copy it to the Soggfy folder (it must look like: `Soggfy/ffmpeg/ffmpeg.exe`). Then select the "Custom" option in the format setting, and use these values:
- Arguments: `-c:a libfdk_aac -b:a 192k -disposition:v attached_pic -c:v copy` (you may change `192k` to your preferred bitrate)
- Extension: either `m4a` or `mp4`

# Refs
[^1]: https://trac.ffmpeg.org/wiki/Encode/HighQualityAudio
[^2]: https://trac.ffmpeg.org/wiki/Encode/AAC
