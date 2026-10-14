<#
.SYNOPSIS
  Turns "Steam mode" on or off: lets the game pick up doomfix when it is started from Steam.
  Used by enable_steam_mode.bat and disable_steam_mode.bat.

.DESCRIPTION
  The layer only activates in processes that have DOOMFIX_ENABLE=1. run_doom_fixed.bat sets it for the
  game alone. Steam mode instead stores DOOMFIX_ENABLE=1 as a variable of the current Windows user
  (HKCU\Environment), so the game gets it when Steam starts it. Steam only reads its environment when
  it starts, so after a change Steam has to be restarted; the script offers to do that.

  With the variable set, every Vulkan program of this user loads the layer. Outside DOOM Eternal it does
  not change anything, but disable_steam_mode.bat removes the variable if you no longer need it.

.PARAMETER Enable
  Set DOOMFIX_ENABLE=1 for the current user.
.PARAMETER Disable
  Remove DOOMFIX_ENABLE from the current user's variables.
.PARAMETER RestartSteam
  ask (default): ask before restarting a running Steam; yes: restart without asking; no: do not restart.
#>
param(
    [switch]$Enable,
    [switch]$Disable,
    [ValidateSet('ask', 'yes', 'no')][string]$RestartSteam = 'ask'
)
$ErrorActionPreference = 'Stop'

$VarName = 'DOOMFIX_ENABLE'
$GameProcess = 'DOOMEternalx64vk'
$LayerManifest = 'VK_LAYER_DOOMFIX.json'

if ($Enable -eq $Disable) { throw 'Pass exactly one of -Enable or -Disable.' }

function Get-SteamExe {
    $props = Get-ItemProperty -Path 'HKCU:\Software\Valve\Steam' -ErrorAction SilentlyContinue
    $candidates = @()
    if ($props -and $props.SteamExe) { $candidates += ($props.SteamExe -replace '/', '\') }
    if ($props -and $props.SteamPath) { $candidates += (Join-Path ($props.SteamPath -replace '/', '\') 'steam.exe') }
    return $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}

function Restart-Steam {
    $steamExe = Get-SteamExe
    if (-not $steamExe) {
        Write-Warning 'steam.exe was not found. Restart Steam yourself: exit it from the tray icon and start it again.'
        return
    }
    Write-Host 'Closing Steam...'
    & $steamExe -shutdown
    $deadline = (Get-Date).AddSeconds(60)
    while ((Get-Process -Name steam -ErrorAction SilentlyContinue) -and (Get-Date) -lt $deadline) {
        Start-Sleep -Milliseconds 500
    }
    if (Get-Process -Name steam -ErrorAction SilentlyContinue) {
        Write-Warning 'Steam did not close within a minute. Exit it from the tray icon and start it again yourself.'
        return
    }
    # Steam inherits the environment of this process, which already has the new value (see below).
    Start-Process -FilePath $steamExe
    Write-Host 'Steam is starting again.'
}

# --- 1. Change the user variable --------------------------------------------------------------
if ($Enable) {
    $implicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
    $installed = (Test-Path $implicitKey) -and
        @((Get-Item $implicitKey).Property | Where-Object { (Split-Path $_ -Leaf) -ieq $LayerManifest }).Count -gt 0
    if (-not $installed) {
        Write-Warning 'The doomfix layer is not installed yet. Run install.bat too, otherwise Steam mode does nothing.'
    }
    # Also notifies running programs (WM_SETTINGCHANGE), so Explorer passes it to programs started later.
    [Environment]::SetEnvironmentVariable($VarName, '1', 'User')
    $env:DOOMFIX_ENABLE = '1'
    Write-Host "Steam mode is ON: $VarName=1 is set for your Windows user."
} else {
    $had = [Environment]::GetEnvironmentVariable($VarName, 'User')
    [Environment]::SetEnvironmentVariable($VarName, $null, 'User')
    Remove-Item Env:\DOOMFIX_ENABLE -ErrorAction SilentlyContinue
    if ($had) {
        Write-Host "Steam mode is OFF: $VarName was removed from your Windows user."
    } else {
        Write-Host "Steam mode was already off ($VarName is not set for your Windows user)."
        return
    }
}

# --- 2. Restart Steam so that it sees the change ----------------------------------------------
if (-not (Get-Process -Name steam -ErrorAction SilentlyContinue)) {
    Write-Host 'Steam is not running; the change applies the next time you start it.'
    return
}
if (Get-Process -Name $GameProcess -ErrorAction SilentlyContinue) {
    Write-Warning 'DOOM Eternal is running, so Steam is not restarted now. Close the game, then restart Steam (exit it from the tray icon).'
    return
}
$restart = switch ($RestartSteam) {
    'yes' { $true }
    'no' { $false }
    default { -not ((Read-Host 'Steam must be restarted to see the change. Restart it now? [Y/n]') -match '^\s*n') }
}
if ($restart) {
    Restart-Steam
} else {
    Write-Host 'Restart Steam yourself for the change to take effect: exit it from the tray icon and start it again.'
}
