$base = $PWD.Path
$temp = "$base\temp";
$SpotifyDir = "$env:APPDATA\Spotify"

$SpotifyInstallerUrl = "https://upgrade.scdn.co/upgrade/client/win32-x86/spotify_installer-1.2.26.1187.g36b715a1-269.exe"
$SpotifyVersion = $SpotifyInstallerUrl -replace '.+installer-(.+)\.g.+', '$1'
$SpotifyVersionWithCommit = $SpotifyInstallerUrl -replace '.+installer-(.+\.g.+)\.exe', '$1'

Set-Location -Path "$base\"

function New-Hyperlink {
  <#
        .SYNOPSIS
            Creates a VT Hyperlink in a supported terminal such as Windows Terminal 1.4+
        .NOTES
            There's a more powerful version of this, with color support and more, in PANSIES
        .EXAMPLE
            New-Hyperlink https://github.com/Jaykul/PANSIES PANSIES
            Creates a hyperlink with the text PANSIES which links to the github project
    #>
  [Alias("Url")]
  [CmdletBinding()]
  param(
    # The Uri the hyperlink should point to
    [Parameter(Mandatory, ValueFromPipeline, ValueFromPipelineByPropertyName)]
    [string]$Uri,

    # The text of the hyperlink (if not specified, defaults to the URI)
    [ValidateNotNullOrEmpty()]
    [Parameter(ValueFromRemainingArguments)]
    [String]$InputObject = $Uri
  )
  $8 = [char]27 + "]8;;"
  "$8{0}`a{1}$8`a" -f $Uri, $InputObject
}

function CheckOrInstallSpotify {
    if (Test-Path "$SpotifyDir\Spotify.exe") {
        $installedVersion = (Get-Item "$SpotifyDir\Spotify.exe").VersionInfo.FileVersion;

        if ($installedVersion -ne $SpotifyVersion) {
            Write-Host "The currently installed Spotify version $installedVersion may not work with this version of Soggfy."

            if ((Read-Host -Prompt "Do you want to reinstall to the recommended version ($SpotifyVersion)? Y/N") -ne "y") { return; }
        }
        else {
            return;
        }

        Stop-Process -Name "Spotify" -ErrorAction SilentlyContinue
    }
    DownloadFile -Url $SpotifyInstallerUrl -DestPath "$temp\SpotifyInstaller-$SpotifyVersion.exe"

    Write-Host "Installing..."

    # Remove everything but user folders, to avoid conflicts with Spicetify extracted files
    Remove-Item -Path $SpotifyDir -Recurse -Exclude ("Users\", "prefs") -ErrorAction SilentlyContinue

    # Other undocumented switches: /extract /log-file
    Start-Process -FilePath "$temp\SpotifyInstaller-$SpotifyVersion.exe" -ArgumentList "/silent /skip-app-launch" -Wait
    Remove-Item -Path "$SpotifyDir\crash_reporter.cfg" -ErrorAction SilentlyContinue

    if ((Read-Host -Prompt "Do you want to install SpotX to block ads, updates, and enable extra client features? Y/N") -eq "y") {
        $flags = Read-Host -Prompt "Input any desired $(New-Hyperlink 'https://github.com/SpotX-Official/SpotX/discussions/60' 'SpotX parameters') (forced flags: -new_theme -block_update_on)"
        $src = (Invoke-WebRequest "https://spotx-official.github.io/run.ps1" -UseBasicParsing).Content
        $src = [System.Text.Encoding]::UTF8.GetString($src);
        Invoke-Expression "& { $src } $flags -new_theme -block_update_on -version $SpotifyVersionWithCommit"
    }
}
function InstallFFmpeg {
    where.exe /q ffmpeg
    if ($LastExitCode -eq 0) {
        Write-Host "Will use FFmpeg binaries found in %PATH% at '$(where.exe ffmpeg)'."
        return;
    }
    if ((Test-Path "$env:LOCALAPPDATA\Soggfy\ffmpeg\ffmpeg.exe")) {
        if ((Read-Host -Prompt "Do you want to re-install or update FFmpeg? Y/N") -ne "y") { return; }

        Remove-Item -Path "$env:LOCALAPPDATA\Soggfy\ffmpeg\" -Recurse -Force
    }
    $arch = $(if ([Environment]::Is64BitOperatingSystem) { "win64" } else { "win32" })
    $release = Invoke-WebRequest "https://api.github.com/repos/AnimMouse/ffmpeg-autobuild/releases/latest" -UseBasicParsing | ConvertFrom-Json
    $asset = $release.assets | Where-Object { $_.name.Contains($arch) } | Select-Object -First 1

    DownloadFile -Url $asset.browser_download_url -DestPath "$temp/$($asset.name)"
    DownloadFile -Url "https://7-zip.org/a/7zr.exe" -DestPath "$temp/7zr.exe"

    & "$temp\7zr.exe" e "$temp\$($asset.name)" -y -o"$env:LOCALAPPDATA\Soggfy\ffmpeg\" ffmpeg.exe
}
function InstallSoggfy {
    Write-Host "Copying Soggfy files..."
    Copy-Item -Path "$base\SpotifyOggDumper.dll" -Destination "$SpotifyDir\dpapi.dll"
    Copy-Item -Path "$base\SoggfyUIC.js" -Destination "$SpotifyDir\SoggfyUIC.js"
    Write-Host "Done."
}

function DownloadFile($Url, $DestPath) {
    $req = [System.Net.WebRequest]::CreateHttp($Url)
    $resp = $req.GetResponse()
    $is = $resp.GetResponseStream()

    $name = [System.IO.Path]::GetFileName($DestPath)
    [System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($DestPath))
    $os = [System.IO.File]::Create($DestPath)

    try {
        $buffer = New-Object byte[] (1024 * 512)
        $lastProgUpdate = 0
        while ($true) {
            $bytesRead = $is.Read($buffer, 0, $buffer.Length);
            if ($bytesRead -le 0) { break; }
            $os.Write($buffer, 0, $bytesRead);

            # Throttle progress updates because they slowdown download too much
            if ([Environment]::TickCount - $lastProgUpdate -lt 100) { continue; }
            $lastProgUpdate = [Environment]::TickCount;

            $totalReceived = $os.Position / 1048576
            $totalLength = $resp.ContentLength / 1048576
            Write-Progress `
                -Activity "Downloading $name" `
                -Status ('{0:0.00}MB of {1:0.00}MB' -f $totalReceived, $totalLength) `
                -PercentComplete ($totalReceived * 100 / $totalLength)
        }
        Write-Progress -Activity "Downloading $name" -Completed
    } finally {
        $os.Dispose()
        $resp.Dispose()
    }
}

CheckOrInstallSpotify
InstallSoggfy
InstallFFmpeg

Write-Host "Everything done. Soggfy will be enabled on the next Spotify launch.";
Pause
