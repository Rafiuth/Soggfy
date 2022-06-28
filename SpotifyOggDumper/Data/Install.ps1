$SpotifyInstallerUrl = "https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.1.87.612.gf8d110e2-19.exe"
$SpotifyVersion = "1.1.87.612"
$BtsUrl = "https://github.com/mrpond/BlockTheSpot/releases/download/2021.10.29.44/chrome_elf.zip"

function InstallSpotify
{
    DownloadFile -Url $SpotifyInstallerUrl -DestPath "SpotifyInstaller-$SpotifyVersion.exe"

    Write-Host "Extracting..."
    Start-Process -FilePath "SpotifyInstaller-$SpotifyVersion.exe" -ArgumentList "/extract" -Wait
    Remove-Item "SpotifyInstaller-$SpotifyVersion.exe"
    
    if (Test-Path "Spotify/") { 
        Remove-Item "Spotify" -Force -Recurse
    }
    $spotifyFolder = Get-ChildItem -Filter "spotify-update-*"
    Rename-Item -Path $spotifyFolder.FullName -NewName "Spotify"

    if ((Read-Host -Prompt "Do you want to install BlockTheSpot to block ads? Y/N") -eq "y") {
        DownloadFile -Url $BtsUrl -DestPath "bts_patch.zip"

        Rename-Item -Path "Spotify/chrome_elf.dll" -NewName "chrome_elf_bak.dll"
        Expand-Archive -Path "bts_patch.zip" -DestinationPath "Spotify/" -Force
        Remove-Item "bts_patch.zip"
    }
    Remove-Item -Path "Spotify/crash_reporter.cfg"
}
function InstallFFmpeg
{
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
            DownloadFile -Url $asset.browser_download_url -DestPath $asset.name
            
            # Remove previous installation
            if (Test-Path "./ffmpeg/") {
                Remove-Item -Path "./ffmpeg/" -Recurse -Force
            }
            New-Item -ItemType Directory -Path "./ffmpeg/" -Force | Out-Null
            
            # Extract bin/ folder to ffmpeg/
            Add-Type -Assembly System.IO.Compression.FileSystem
            $zip = [IO.Compression.ZipFile]::OpenRead($asset.name)

            foreach ($entry in $zip.Entries) {
                if ($entry.FullName -notlike "*/bin/*.*") { continue; }
                [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, "./ffmpeg/" + $entry.Name)
            }
            $zip.Dispose()

            # Delete zip
            Remove-Item $asset.name
            
            return
        }
    }

    Write-Host "Failed to install ffmpeg. Try downloading it from https://ffmpeg.org/download.html and extract the binaries into the 'Soggfy/ffmpeg/' directory."
}

function DownloadFile {
    param ( [string] $Url, [string] $DestPath )
    
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

if (-not (Test-Path './Spotify/Spotify.exe') -or ((Get-Item ".\Spotify\Spotify.exe").VersionInfo.FileVersion -ne $SpotifyVersion)) {
    Write-Host "Installing Spotify..."
    InstallSpotify
}
where.exe /q ffmpeg
if (-not (Test-Path './ffmpeg/ffmpeg.exe') -and ($LastExitCode -eq 1)) {
    Write-Host "Installing ffmpeg..."
    InstallFFmpeg
}
Write-Host "Everything done. You can now run Injector.exe"
Pause