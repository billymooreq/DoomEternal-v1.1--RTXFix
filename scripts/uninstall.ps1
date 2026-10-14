<#
.SYNOPSIS
  Removes the doomfix registration (ImplicitLayers, LoaderSettings) and the installed layer files.
  Log files in %LOCALAPPDATA%\doomfix\logs are kept; delete that folder by hand if you do not need them.
#>
param(
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'doomfix\layer')
)
$ErrorActionPreference = 'Stop'

$LayerName = 'VK_LAYER_DOOMFIX'
$ImplicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$SettingsKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\LoaderSettings'
$SettingsDir = Join-Path $env:LOCALAPPDATA 'doomfix\loader'
$SettingsFile = Join-Path $SettingsDir 'vk_loader_settings.json'

if (Test-Path $ImplicitKey) {
    foreach ($name in (Get-Item $ImplicitKey).Property) {
        if ((Split-Path $name -Leaf) -ieq "$LayerName.json") {
            Remove-ItemProperty -Path $ImplicitKey -Name $name
            Write-Host "Removed: $ImplicitKey -> $name"
        }
    }
}

if ((Test-Path $SettingsKey) -and ((Get-Item $SettingsKey).Property -contains $SettingsFile)) {
    Remove-ItemProperty -Path $SettingsKey -Name $SettingsFile
    Write-Host "Removed: $SettingsKey -> $SettingsFile"
}
foreach ($dir in $SettingsDir, $InstallDir) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
        Write-Host "Removed folder: $dir"
    }
}
Write-Host "doomfix is uninstalled."
