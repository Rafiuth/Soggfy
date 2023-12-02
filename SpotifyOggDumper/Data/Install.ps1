$base = $PWD.Path
$SpotifyInstallerUrl = "https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.2.25.1011.g0348b2ea-701.exe"
$SpotifyVersion = $SpotifyInstallerUrl -replace '.+installer-(.+)\.g.+', '$1'

Set-Location -Path "$base\"

function InstallSpotify {
    DownloadFile -Url $SpotifyInstallerUrl -DestPath "$base\SpotifyInstaller-$SpotifyVersion.exe"

    Write-Host "Extracting..."
    Start-Process -FilePath "$base\SpotifyInstaller-$SpotifyVersion.exe" -ArgumentList "/extract" -Wait
    Remove-Item "$base\SpotifyInstaller-$SpotifyVersion.exe"
    
    if (Test-Path "$base\Spotify/") { 
        Remove-Item "$base\Spotify" -Force -Recurse
    }
	
    $spotifyFolder = Get-ChildItem -Filter ".\spotify-update-*"
    Rename-Item -Path $spotifyFolder.FullName -NewName "$base\Spotify"

    if ((Read-Host -Prompt "Do you want to install SpotX to block ads and enable extra client features? Y/N") -eq "y") {
        InstallSpotX
    }
    Remove-Item -Path "$base\Spotify\crash_reporter.cfg"
}
function InstallSpotX {
    $baseUrl = "https://raw.githubusercontent.com/SpotX-Official/SpotX/5f85bf124a1f459b4016d775e4219b8ebdf135fa"
    $src = (Invoke-WebRequest "$baseUrl/run.ps1" -UseBasicParsing).Content

    # Patch the script so it runs on our install dir, and at a fixed commit so that it hopefully won't break easily
    $src = $src.Replace('Join-Path $env:APPDATA ''Spotify', "Join-Path '$base\Spotify' '");
    $src = $src.Replace('https://spotx-official.github.io/SpotX', $baseUrl);
    $src = $src.Replace('[System.Text.Encoding]::UTF8.GetString($response)', '$response');

    # Set-Content -Path "spotx_patched.ps1" -Value $src
    Invoke-Expression "&{ $src } -new_theme -block_update_on"
}

function InstallFFmpeg {
    if ([Environment]::Is64BitOperatingSystem) {
        $repoUrl = "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/latest"
        $platform = "win64"
    } else {
        $repoUrl = "https://api.github.com/repos/sudo-nautilus/FFmpeg-Builds-Win32/releases/latest"
        $platform = "win32"
    }
    $release = Invoke-WebRequest $repoUrl -UseBasicParsing | ConvertFrom-Json

    foreach ($asset in $release.assets) {
        if ($asset.name -cmatch "ffmpeg-n.+$platform-gpl-shared") {
            DownloadFile -Url $asset.browser_download_url -DestPath "$base\$($asset.name)"
            
            # Remove previous installation
            if (Test-Path "$base\ffmpeg\") {
                Remove-Item -Path "$base\ffmpeg\" -Recurse -Force
            }
            New-Item -ItemType Directory -Path "$base\ffmpeg\" -Force | Out-Null
            
            # Extract bin/ folder to ffmpeg/
            Add-Type -Assembly System.IO.Compression.FileSystem
            $zip = [IO.Compression.ZipFile]::OpenRead("$base\$($asset.name)")

            foreach ($entry in $zip.Entries) {
                if ($entry.FullName -notlike "*/bin/*.*") { continue; }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, "$base\ffmpeg\" + $entry.Name)
            }
            $zip.Dispose()

            # Delete zip
            Remove-Item "$base\$($asset.name)"
            
            return
        }
    }

    Write-Host "Failed to install ffmpeg. Try downloading it from https://ffmpeg.org/download.html and extract the binaries into the 'Soggfy/ffmpeg/' directory."
}

function DownloadFile($Url, $DestPath) {
    $req = [System.Net.WebRequest]::CreateHttp($Url)
    $resp = $req.GetResponse()
    $is = $resp.GetResponseStream()
    $os = [System.IO.File]::Create($DestPath)
    try {
        $buffer = New-Object byte[] (1024 * 512)
        $lastProgUpdate = 0
        while ($true) {
            $bytesRead = $is.Read($buffer, 0, $buffer.Length);
            if ($bytesRead -le 0) { break; }
            $os.Write($buffer, 0, $bytesRead);
            
            # Delay progress updates because they are awfully slow
            if ([Environment]::TickCount - $lastProgUpdate -lt 100) { continue; }
            $lastProgUpdate = [Environment]::TickCount;

            $totalReceived = $os.Position / 1048576
            $totalLength = $resp.ContentLength / 1048576
            Write-Progress `
                -Activity "Downloading $DestPath" `
                -Status ('{0:0.00}MB of {1:0.00}MB' -f $totalReceived, $totalLength) `
                -PercentComplete ($totalReceived * 100 / $totalLength)
        }
        Write-Progress -Activity "Downloading $DestPath" -Completed
    } finally {
        $os.Dispose()
        $resp.Dispose()
    }
}

if (-not (Test-Path '$base\Spotify\Spotify.exe') -or ((Get-Item "$base\Spotify\Spotify.exe").VersionInfo.FileVersion -ne $SpotifyVersion)) {
    Write-Host "Installing Spotify..."
    InstallSpotify
}
where.exe /q ffmpeg
if (-not (Test-Path '$base\ffmpeg\ffmpeg.exe') -and ($LastExitCode -eq 1)) {
    Write-Host "Installing ffmpeg..."
    InstallFFmpeg
}
Write-Host "Everything done. You can now run Injector.exe"
Pause
