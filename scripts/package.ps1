<#
.SYNOPSIS
  Assembles the release zip from a Release build (layout: docs/BUILDING.md, "Release zip").
  Used by the GitHub Actions workflows; works the same locally.

.DESCRIPTION
  Creates <OutDir>\doomfix-<Version>.zip with a doomfix-<Version>\ folder inside, and next to it
  doomfix-<Version>.zip.sha256 in the format of sha256sum ("<hash>  <file name>").
  On GitHub Actions it also sets the step outputs name, version, zip and sha256.

.PARAMETER BuildDir
  Build folder whose bin\ has doomfix_layer.dll and VK_LAYER_DOOMFIX.json. Default: ..\build-release.
.PARAMETER OutDir
  Where the zip goes. Default: ..\dist.
.PARAMETER Version
  Version in the file name, for example 0.3.1 or 0.3.1-rc1. It must start with the version from
  CMakeLists.txt (the one the DLL reports in its log). Default: exactly the CMakeLists.txt version.
#>
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build-release'),
    [string]$OutDir = (Join-Path $PSScriptRoot '..\dist'),
    [string]$Version = ''
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

$bin = Join-Path $BuildDir 'bin'
$name = "doomfix-$Version"
$stage = Join-Path $OutDir $name
$zip = Join-Path $OutDir "$name.zip"

if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force (Join-Path $stage 'bin'), (Join-Path $stage 'scripts') | Out-Null

$files = @{
    'bin'     = @('doomfix_layer.dll', 'VK_LAYER_DOOMFIX.json') | ForEach-Object { Join-Path $bin $_ }
    'scripts' = @('install.ps1', 'uninstall.ps1', 'launch.ps1') |
        ForEach-Object { Join-Path $root "scripts\$_" }
    '.'       = @('install.bat', 'uninstall.bat', 'run_doom_fixed.bat', 'run_fix_strict.bat', 'run_fix_off.bat',
                  'run_fix_aggressive.bat',
                  'README.md', 'README.ru.md', 'PRIVACY.md', 'LICENSE') | ForEach-Object { Join-Path $root $_ }
}
foreach ($dir in $files.Keys) {
    foreach ($f in $files[$dir]) {
        if (-not (Test-Path -LiteralPath $f)) { throw "Not found: $f (build with scripts\build.ps1 -Config Release first?)" }
        Copy-Item -LiteralPath $f (Join-Path $stage $dir)
    }
}

# Entries are added one by one with '/' separators: Compress-Archive and ZipFile.CreateFromDirectory in
# Windows PowerShell 5.1 write '\', which some unzip tools do not treat as folders.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
if (Test-Path $zip) { Remove-Item -Force $zip }
$stageFull = (Resolve-Path $stage).Path
$archive = [System.IO.Compression.ZipFile]::Open($zip, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    foreach ($file in Get-ChildItem $stageFull -Recurse -File | Sort-Object FullName) {
        $entry = "$name/" + $file.FullName.Substring($stageFull.Length + 1).Replace('\', '/')
        [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, $file.FullName, $entry,
            [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
    }
} finally {
    $archive.Dispose()
}
Remove-Item -Recurse -Force $stage

$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLowerInvariant()
[System.IO.File]::WriteAllText("$zip.sha256", "$hash  $name.zip`n")

Write-Host "Package: $zip"
Write-Host "SHA256:  $hash"
if ($env:GITHUB_OUTPUT) {
    $lines = "name=$name`nversion=$Version`nzip=$zip`nsha256=$hash`n"
    [System.IO.File]::AppendAllText($env:GITHUB_OUTPUT, $lines, (New-Object System.Text.UTF8Encoding $false))
}
