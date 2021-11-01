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
        Write-Host "Downloading $($asset.name)..."

        # Invoke-WebRequest is too slow - https://stackoverflow.com/questions/28682642/powershell-why-is-using-invoke-webrequest-much-slower-than-a-browser-download
        (New-Object Net.WebClient).DownloadFile($asset.browser_download_url, $asset.name)
        
        # Remove previous installation
        if (Test-Path "./ffmpeg/") {
            Remove-Item -Path "./ffmpeg/" -Recurse -Force
        }
        New-Item -ItemType Directory -Path "./ffmpeg/" -Force
        
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

Write-Host "Could not find any matching ffmpeg builds, please open an issue or check https://ffmpeg.org/download.html"
pause