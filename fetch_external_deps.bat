rmdir /s /q external
mkdir external
curl https://cef-builds.spotifycdn.com/cef_binary_119.4.4+g5d1e039+chromium-119.0.6045.199_windows32_minimal.tar.bz2 --output external/cef_bin.tar.bz2
7z x "external/cef_bin.tar.bz2" -so | 7z x -aoa -si -ttar "-xr!*.dll" "-xr!cef_sandbox.lib" -o"external/"
move /y "external\cef_binary_*" "external\cef_bin"