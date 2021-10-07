@echo off
set loc="%userprofile%\AppData\Local\Spotify\Update"

if exist %loc%\ (
    rmdir /s /q %loc%
)
if not exist %loc% (
    copy nul %loc%
    attrib +r %loc%
    echo Blocked Spotify updates. Run this again to re-enable them.
) else (
    del /f /s /q %loc%
    echo Re-enabled Spotify updates.
)
pause