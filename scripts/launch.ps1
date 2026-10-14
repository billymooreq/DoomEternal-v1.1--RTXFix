<#
.SYNOPSIS
  Starts DOOM Eternal with the doomfix layer enabled. Used by run_doom_fixed.bat and run_fix_*.bat.

.DESCRIPTION
  On the first run the game folder is looked up in the Steam libraries (libraryfolders.vdf) or asked
  for, and saved to a small config file next to the .bat file. Then the game is started from its own
  folder with DOOMFIX_ENABLE=1, SteamAppId=782330 (keeps the game from relaunching itself through
  Steam, which would drop these variables) and the chosen DOOMFIX_INDIRECT_FIX mode.

.PARAMETER Mode
  DOOMFIX_INDIRECT_FIX mode: on (default), off (no fix, for comparison) or aggressive.
.PARAMETER ConfigFile
  Where the game folder is remembered. The .bat files pass a file next to themselves.
.PARAMETER Reconfigure
  Forget the saved folder and look for the game again.
.PARAMETER DryRun
  Find the game and check the installation, but do not start the game.
#>
param(
    [ValidateSet('on', 'off', 'aggressive')][string]$Mode = 'on',
    [string]$ConfigFile = (Join-Path (Split-Path $PSScriptRoot -Parent) 'doomfix_game_path.txt'),
    [switch]$Reconfigure,
    [switch]$DryRun
)
$ErrorActionPreference = 'Stop'

$ExeName = 'DOOMEternalx64vk.exe'
$SteamAppId = '782330'
$LayerManifest = 'VK_LAYER_DOOMFIX.json'

function Test-GameDir([string]$Dir) {
    return $Dir -and (Test-Path -LiteralPath (Join-Path $Dir $ExeName) -PathType Leaf)
}

function Get-SteamLibraries {
    $roots = @()
    foreach ($key in 'HKCU:\Software\Valve\Steam', 'HKLM:\SOFTWARE\WOW6432Node\Valve\Steam', 'HKLM:\SOFTWARE\Valve\Steam') {
        $props = Get-ItemProperty -Path $key -ErrorAction SilentlyContinue
        if (-not $props) { continue }
        foreach ($name in 'SteamPath', 'InstallPath') {
            if ($props.$name) { $roots += ($props.$name -replace '/', '\') }
        }
    }
    $libraries = @()
    foreach ($root in ($roots | Select-Object -Unique)) {
        $libraries += $root
        $vdf = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $vdf) {
            # Entries look like:  "path"   "D:\\SteamLibrary"
            foreach ($m in [regex]::Matches((Get-Content -LiteralPath $vdf -Raw), '"path"\s+"([^"]+)"')) {
                $libraries += ($m.Groups[1].Value -replace '\\\\', '\')
            }
        }
    }
    return $libraries | Select-Object -Unique
}

function Find-GameInSteam {
    foreach ($library in Get-SteamLibraries) {
        $dir = Join-Path $library 'steamapps\common\DOOMEternal'
        if (Test-GameDir $dir) { return $dir }
    }
    return $null
}

function Read-GameDir {
    while ($true) {
        $answer = (Read-Host "Enter the DOOM Eternal folder (the one that contains $ExeName)").Trim().Trim('"')
        if (-not $answer) { throw 'No folder given. Run the launcher again when you know where the game is installed.' }
        if ((Split-Path $answer -Leaf) -ieq $ExeName) { $answer = Split-Path $answer -Parent }
        if (Test-GameDir $answer) { return (Resolve-Path -LiteralPath $answer).Path }
        Write-Warning "$ExeName was not found in '$answer'."
    }
}

# --- 1. Game folder --------------------------------------------------------------------------
$gameDir = $null
if (-not $Reconfigure -and (Test-Path -LiteralPath $ConfigFile)) {
    $gameDir = (Get-Content -LiteralPath $ConfigFile -Raw).Trim()
    if (-not (Test-GameDir $gameDir)) {
        Write-Warning "The saved game folder no longer contains $ExeName`: $gameDir"
        $gameDir = $null
    }
}
if (-not $gameDir) {
    $found = Find-GameInSteam
    if ($found) {
        Write-Host "Found DOOM Eternal in your Steam library: $found"
        if ((Read-Host 'Use this folder? [Y/n]') -match '^\s*n') { $found = $null }
    } else {
        Write-Host 'DOOM Eternal was not found in the Steam libraries.'
    }
    $gameDir = if ($found) { $found } else { Read-GameDir }
    Set-Content -LiteralPath $ConfigFile -Value $gameDir -Encoding UTF8
    Write-Host "Saved the game folder to $ConfigFile (delete this file to choose again)."
}

# --- 2. Is the layer installed? ----------------------------------------------------------------
$implicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$registered = @()
if (Test-Path $implicitKey) {
    $registered = @((Get-Item $implicitKey).Property | Where-Object { (Split-Path $_ -Leaf) -ieq $LayerManifest -and (Test-Path -LiteralPath $_) })
}
if (-not $registered) {
    throw 'The doomfix layer is not installed. Run install.bat first.'
}
if (-not (Get-Process -Name steam -ErrorAction SilentlyContinue)) {
    Write-Warning 'Steam is not running. Start Steam first if the game asks for it.'
}

# --- 3. Start the game -------------------------------------------------------------------------
if ($DryRun) {
    Write-Host "Dry run: would start $(Join-Path $gameDir $ExeName) with DOOMFIX_INDIRECT_FIX=$Mode (layer: $($registered[0]))."
    return
}
$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = Join-Path $gameDir $ExeName
$psi.WorkingDirectory = $gameDir
$psi.UseShellExecute = $false
$psi.EnvironmentVariables['DOOMFIX_ENABLE'] = '1'
$psi.EnvironmentVariables['SteamAppId'] = $SteamAppId
$psi.EnvironmentVariables['DOOMFIX_INDIRECT_FIX'] = $Mode
[System.Diagnostics.Process]::Start($psi) | Out-Null

Write-Host "Started DOOM Eternal with doomfix (DOOMFIX_INDIRECT_FIX=$Mode)."
Write-Host "Log files: $(Join-Path $env:LOCALAPPDATA 'doomfix\logs')"
