if (![Environment]::Is64BitOperatingSystem) {
    Write-Host "This script only supports 64-bit OS."
    pause
    return
}

$release = Invoke-WebRequest "https://api.github.com/repos/BtbN/FFmpeg-Builds/releases/latest" | ConvertFrom-Json

foreach ($asset in $release.assets) {
    if ($asset.name -cmatch 'ffmpeg-n.+win64-gpl-shared') {
        Write-Host "Downloading $($asset.name)..."

        # Invoke-WebRequest is too slow, had to use WebClient manually. 
        # https://stackoverflow.com/questions/28682642/powershell-why-is-using-invoke-webrequest-much-slower-than-a-browser-download
        (New-Object Net.WebClient).DownloadFile($asset.browser_download_url, $asset.name)

        # Remove previous installation
        Remove-Item -Path "./ffmpeg/" -Recurse -Force
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

Write-Host "Could not find any matching ffmpeg builds from BtbN"
pause