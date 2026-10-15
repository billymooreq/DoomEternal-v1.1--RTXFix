<#
.SYNOPSIS
  Builds the installer doomfix-<Version>-setup.exe from ready files with Inno Setup 6 (installer\doomfix.iss).
  Used by the GitHub Actions workflows; works the same locally.

.DESCRIPTION
  This is a separate step after scripts\build.ps1: it takes doomfix_layer.dll, VK_LAYER_DOOMFIX.json and
  doomfix_launcher.exe from BinDir as they are and does not compile anything, so these files can be signed
  in between. Creates <OutDir>\doomfix-<Version>-setup.exe and next to it a .sha256 file in the format of
  sha256sum ("<hash>  <file name>"). On GitHub Actions it also sets the step outputs setup and setup_sha256.

.PARAMETER BinDir
  Folder with the files to install. Default: ..\build-release\bin.
.PARAMETER OutDir
  Where the installer goes. Default: ..\dist.
.PARAMETER Version
  Version in the file name and in "Installed apps", for example 0.3.1 or 0.3.1-rc1. It must start with the
  version from CMakeLists.txt. Default: exactly the CMakeLists.txt version.
.PARAMETER Iscc
  Path to ISCC.exe. Default: found in the usual Inno Setup 6 install locations.
#>
param(
    [string]$BinDir = (Join-Path $PSScriptRoot '..\build-release\bin'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$Version = '',
    [string]$Iscc = ''
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$cmakeVersion = [regex]::Match((Get-Content (Join-Path $root 'CMakeLists.txt') -Raw),
    'project\(doomfix VERSION (\d+\.\d+\.\d+)').Groups[1].Value
if (-not $cmakeVersion) { throw 'project(doomfix VERSION x.y.z) not found in CMakeLists.txt.' }
if (-not $Version) { $Version = $cmakeVersion }
if ($Version -notmatch "^$([regex]::Escape($cmakeVersion))(-[0-9A-Za-z.]+)?$") {
    throw "Version '$Version' does not match the version in CMakeLists.txt ($cmakeVersion)."
}

foreach ($f in 'doomfix_layer.dll', 'VK_LAYER_DOOMFIX.json', 'doomfix_launcher.exe') {
    if (-not (Test-Path -LiteralPath (Join-Path $BinDir $f))) {
        throw "Not found: $(Join-Path $BinDir $f) (build with scripts\build.ps1 -Config Release first?)"
    }
}

if (-not $Iscc) {
    $candidates = @()
    foreach ($key in 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
                     'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1',
                     'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\Inno Setup 6_is1') {
        $location = (Get-ItemProperty -Path $key -ErrorAction SilentlyContinue).InstallLocation
        if ($location) { $candidates += Join-Path $location 'ISCC.exe' }
    }
    $candidates += @("${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe", "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
                     "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe")
    $Iscc = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $Iscc) {
        throw 'Inno Setup 6 not found (ISCC.exe). Install it, for example: winget install JRSoftware.InnoSetup, or pass -Iscc.'
    }
}
Write-Host "Inno Setup: $Iscc"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$binFull = (Resolve-Path $BinDir).Path
$outFull = (Resolve-Path $OutDir).Path
& $Iscc /Q "/DAppVersion=$Version" "/DFileVersion=$cmakeVersion" "/DBinDir=$binFull" "/DOutputDir=$outFull" (Join-Path $root 'installer\doomfix.iss')
if ($LASTEXITCODE) { throw "ISCC failed with exit code $LASTEXITCODE" }

$name = "doomfix-$Version-setup.exe"
$setup = Join-Path $outFull $name
if (-not (Test-Path -LiteralPath $setup)) { throw "ISCC did not create $setup" }
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $setup).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText("$setup.sha256", "$hash  $name`n")

Write-Host "Installer: $setup"
Write-Host "SHA256:    $hash"
if ($env:GITHUB_OUTPUT) {
    $lines = "setup=$setup`nsetup_name=$name`nsetup_sha256=$hash`n"
    [System.IO.File]::AppendAllText($env:GITHUB_OUTPUT, $lines, (New-Object System.Text.UTF8Encoding $false))
}
