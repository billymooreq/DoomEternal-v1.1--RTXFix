<#
.SYNOPSIS
  Checks the installer end to end without a GPU or the game: upgrade over an install.bat installation,
  reinstall, removal of the old Steam mode and uninstall. Used by the GitHub Actions workflows.

.DESCRIPTION
  The "Steam mode" of v0.3.1 is simulated by setting DOOMFIX_ENABLE=1 for the user; install.ps1, the installer
  and the uninstaller must each remove it.

  1. Installs the layer the old way (scripts\install.ps1) and adds a stale registration from another folder.
  2. Runs the installer silently with a fake game folder and checks files, registry values, the "Installed
     apps" entry, shortcuts and "doomfix_launcher.exe --dry-run".
  3. Runs it again without the desktop shortcut and checks that nothing is registered twice.
  4. Uninstalls silently and checks that everything is gone except the logs folder.

  On a developer machine this replaces the installed doomfix and leaves it UNINSTALLED at the end, and it
  removes a DOOMFIX_ENABLE user variable. Install doomfix again afterwards.

.PARAMETER Setup
  The installer to test. Default: the newest dist\doomfix-*-setup.exe.
.PARAMETER BinDir
  Build output for the old-style installation. Default: ..\build-release\bin.
#>
param(
    [string]$Setup = '',
    [string]$BinDir = (Join-Path $PSScriptRoot '..\build-release\bin')
)
$ErrorActionPreference = 'Stop'

$AppId = '{B3F23E39-9C8E-4D02-B62B-4A2E262905A6}'   # installer\doomfix.iss
$UninstallKey = "HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\${AppId}_is1"
$ImplicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$SettingsKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\LoaderSettings'
$AppDir = Join-Path $env:LOCALAPPDATA 'doomfix'
$Manifest = Join-Path $AppDir 'layer\VK_LAYER_DOOMFIX.json'
$SettingsFile = Join-Path $AppDir 'loader\vk_loader_settings.json'
$LogsDir = Join-Path $AppDir 'logs'
$ShortcutName = 'DOOM Eternal (doomfix).lnk'
$StartMenuShortcut = Join-Path ([Environment]::GetFolderPath('Programs')) $ShortcutName
$DesktopShortcut = Join-Path ([Environment]::GetFolderPath('Desktop')) $ShortcutName

if (-not $Setup) {
    $Setup = Get-ChildItem (Join-Path $PSScriptRoot '..\dist') -Filter 'doomfix-*-setup.exe' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending | Select-Object -First 1 -ExpandProperty FullName
    if (-not $Setup) { throw 'No dist\doomfix-*-setup.exe found. Build it with scripts\build_installer.ps1 first.' }
}
$Setup = (Resolve-Path $Setup).Path
Write-Host "Installer: $Setup"

$failures = New-Object System.Collections.Generic.List[string]
function Check([bool]$Condition, [string]$What) {
    if ($Condition) { Write-Host "  ok   $What" } else { Write-Host "  FAIL $What"; $failures.Add($What) }
}

function Get-DoomfixRegistrations {
    if (-not (Test-Path $ImplicitKey)) { return @() }
    return @((Get-Item $ImplicitKey).Property | Where-Object { (Split-Path $_ -Leaf) -ieq 'VK_LAYER_DOOMFIX.json' })
}

function Test-RegistryValue([string]$Key, [string]$Name) {
    return (Test-Path $Key) -and ((Get-Item $Key).Property -contains $Name)
}

function Invoke-Setup([string[]]$Arguments) {
    $log = Join-Path $work "setup_$(Get-Random).log"
    $p = Start-Process -FilePath $Setup -ArgumentList ($Arguments + @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', "/LOG=`"$log`"")) -Wait -PassThru
    if ($p.ExitCode -ne 0) {
        if (Test-Path $log) { Get-Content $log | Select-Object -Last 40 | Write-Host }
        throw "The installer exited with code $($p.ExitCode)."
    }
}

# What enable_steam_mode.bat of v0.3.1 did.
function Set-OldSteamMode {
    [Environment]::SetEnvironmentVariable('DOOMFIX_ENABLE', '1', 'User')
}

function Test-OldSteamModeRemoved {
    return -not [Environment]::GetEnvironmentVariable('DOOMFIX_ENABLE', 'User')
}

function Invoke-Launcher([string[]]$Arguments) {
    return (Start-Process -FilePath (Join-Path $AppDir 'doomfix_launcher.exe') -ArgumentList $Arguments -Wait -PassThru).ExitCode
}

$work = Join-Path ([System.IO.Path]::GetTempPath()) "doomfix_installer_test_$PID"
$gameDir = Join-Path $work 'DOOMEternal'
New-Item -ItemType Directory -Force $gameDir | Out-Null
New-Item -ItemType File -Force (Join-Path $gameDir 'DOOMEternalx64vk.exe') | Out-Null
$keepLog = Join-Path $LogsDir "installer_test_$PID.log"
$stale = Join-Path $work 'old\VK_LAYER_DOOMFIX.json'

try {
    # --- 1. An installation made with install.bat, plus a stale registration --------------------
    Write-Host '1. Old-style installation (install.ps1) with the old Steam mode on'
    Set-OldSteamMode
    & (Join-Path $PSScriptRoot 'install.ps1') -SourceDir $BinDir 3>$null | Out-Null
    Check (Test-OldSteamModeRemoved) 'install.ps1 removes the old Steam mode variable'
    Set-OldSteamMode
    New-ItemProperty -Path $ImplicitKey -Name $stale -Value 0 -PropertyType DWord -Force | Out-Null
    New-Item -ItemType Directory -Force $LogsDir | Out-Null
    Set-Content -LiteralPath $keepLog -Value 'must survive the uninstall'

    # --- 2. Install over it ----------------------------------------------------------------------
    Write-Host '2. Silent install over it'
    Invoke-Setup @("/GAMEDIR=`"$gameDir`"", '/TASKS=desktopicon')
    foreach ($f in 'layer\doomfix_layer.dll', 'layer\VK_LAYER_DOOMFIX.json', 'doomfix_launcher.exe', 'PRIVACY.md') {
        Check (Test-Path (Join-Path $AppDir $f)) "installed $f"
    }
    $registrations = @(Get-DoomfixRegistrations)
    Check ($registrations.Count -eq 1 -and $registrations[0] -ieq $Manifest) "exactly one layer registration ($($registrations -join '; '))"
    Check (Test-RegistryValue $SettingsKey $SettingsFile) 'loader settings registered'
    $settings = Get-Content -LiteralPath $SettingsFile -Raw | ConvertFrom-Json
    Check ($settings.settings_array[0].layers[0].path -ieq $Manifest) 'loader settings point to the installed manifest'
    $gamePath = (Get-Content -LiteralPath (Join-Path $AppDir 'doomfix_game_path.txt') -Raw).Trim()
    Check ($gamePath -eq $gameDir) "game folder saved ($gamePath)"
    $entry = Get-ItemProperty -Path $UninstallKey -ErrorAction SilentlyContinue
    Check ($null -ne $entry) 'entry in Installed apps'
    Check (Test-Path -LiteralPath $StartMenuShortcut) 'Start menu shortcut'
    Check (Test-Path -LiteralPath $DesktopShortcut) 'desktop shortcut'
    Check (Test-OldSteamModeRemoved) 'the installer removes the old Steam mode variable'
    Check ((Invoke-Launcher @('--dry-run')) -eq 0) 'doomfix_launcher.exe --dry-run finds the game and the layer'
    Check (Test-OldSteamModeRemoved) 'the launcher does not set DOOMFIX_ENABLE for the user'
    $flags = Invoke-Launcher @('--check-processes')
    Check (($flags -band 1) -eq 0) "doomfix_launcher.exe --check-processes: the game is not running (flags $flags)"

    # --- 3. Reinstall without the desktop shortcut -------------------------------------------------
    Write-Host '3. Silent reinstall without the desktop shortcut'
    Invoke-Setup @('/TASKS=')
    Check (@(Get-DoomfixRegistrations).Count -eq 1) 'still exactly one layer registration'
    Check (-not (Test-Path -LiteralPath $DesktopShortcut)) 'desktop shortcut removed when not selected'
    $gamePath = (Get-Content -LiteralPath (Join-Path $AppDir 'doomfix_game_path.txt') -Raw).Trim()
    Check ($gamePath -eq $gameDir) 'game folder kept on reinstall'

    # --- 4. Uninstall ----------------------------------------------------------------------------
    Write-Host '4. Silent uninstall with the old Steam mode on'
    Set-OldSteamMode
    $uninstaller = (Get-ItemProperty -Path $UninstallKey).UninstallString.Trim('"')
    Start-Process -FilePath $uninstaller -ArgumentList '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART' -Wait | Out-Null
    # The uninstaller finishes in a temporary copy of itself; wait until its entry disappears.
    $deadline = (Get-Date).AddSeconds(60)
    while ((Test-Path $UninstallKey) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 500 }
    Start-Sleep -Seconds 2
    Check (-not (Test-Path $UninstallKey)) 'entry in Installed apps removed'
    Check (@(Get-DoomfixRegistrations).Count -eq 0) 'no layer registrations left'
    Check (-not (Test-RegistryValue $SettingsKey $SettingsFile)) 'loader settings registration removed'
    Check (Test-OldSteamModeRemoved) 'the uninstaller removes the old Steam mode variable'
    foreach ($f in 'layer', 'loader', 'doomfix_launcher.exe', 'doomfix_game_path.txt', 'PRIVACY.md') {
        Check (-not (Test-Path (Join-Path $AppDir $f))) "removed $f"
    }
    Check (-not (Test-Path -LiteralPath $StartMenuShortcut)) 'Start menu shortcut removed'
    Check (Test-Path -LiteralPath $keepLog) 'logs kept'
} finally {
    [Environment]::SetEnvironmentVariable('DOOMFIX_ENABLE', $null, 'User')
    Remove-Item -LiteralPath $keepLog -ErrorAction SilentlyContinue
    if (Test-Path $ImplicitKey) { Remove-ItemProperty -Path $ImplicitKey -Name $stale -ErrorAction SilentlyContinue }
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

if ($failures.Count) { throw "Installer test: $($failures.Count) check(s) failed: $($failures -join '; ')" }
Write-Host 'Installer test: ALL PASSED'
