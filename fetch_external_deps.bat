mkdir external
curl https://cef-builds.spotifycdn.com/cef_binary_95.7.18+g0d6005e+chromium-95.0.4638.69_windows32_minimal.tar.bz2 --output external/cef_bin.tar.bz2
7z x "external/cef_bin.tar.bz2" -so | 7z x -aoa -si -ttar "-xr!*.dll" "-xr!cef_sandbox.lib" -o"external/"
move /y "external\cef_binary_*" "external\cef_bin"