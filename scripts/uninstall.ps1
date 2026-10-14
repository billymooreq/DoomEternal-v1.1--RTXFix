<#
.SYNOPSIS
  Удаляет регистрацию doomfix (ImplicitLayers, LoaderSettings) и установленные файлы слоя.
  Логи в %LOCALAPPDATA%\doomfix\logs остаются.
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
            Write-Host "Удалено: $ImplicitKey -> $name"
        }
    }
}

if ((Test-Path $SettingsKey) -and ((Get-Item $SettingsKey).Property -contains $SettingsFile)) {
    Remove-ItemProperty -Path $SettingsKey -Name $SettingsFile
    Write-Host "Удалено: $SettingsKey -> $SettingsFile"
}
foreach ($dir in $SettingsDir, $InstallDir) {
    if (Test-Path $dir) {
        Remove-Item -Recurse -Force $dir
        Write-Host "Удалён каталог: $dir"
    }
}
