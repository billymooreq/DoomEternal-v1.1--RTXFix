<#
.SYNOPSIS
  Устанавливает doomfix как implicit Vulkan layer для текущего пользователя (без прав администратора).

.DESCRIPTION
  1. Копирует doomfix_layer.dll и манифест из build\bin в %LOCALAPPDATA%\doomfix\layer.
  2. Регистрирует манифест в HKCU\SOFTWARE\Khronos\Vulkan\ImplicitLayers.
  3. Задаёт порядок слоёв через loader settings (HKCU\SOFTWARE\Khronos\Vulkan\LoaderSettings ->
     vk_loader_settings.json): doomfix ставится первым, то есть ближе всех к приложению и выше
     RenderDoc. Без этого шага loader читает implicit-слои из HKLM раньше, чем из HKCU, и
     RenderDoc (зарегистрирован в HKLM) оказался бы выше doomfix.

  Слой включается только при DOOMFIX_ENABLE=1, поэтому для остальных приложений он неактивен.

.PARAMETER GameExe
  Если задан, правило порядка слоёв применяется только к этому exe (поле app_keys).
  Loader сравнивает путь посимвольно с учётом регистра, поэтому путь должен совпадать
  с тем, как процесс запускается. По умолчанию правило глобальное.

.PARAMETER NoOrdering
  Не создавать loader settings: только регистрация в ImplicitLayers (doomfix окажется НИЖЕ RenderDoc).
#>
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build'),
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'doomfix\layer'),
    [string]$GameExe = '',
    [switch]$NoOrdering
)
$ErrorActionPreference = 'Stop'

$LayerName = 'VK_LAYER_DOOMFIX'
$ImplicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$SettingsKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\LoaderSettings'
$SettingsDir = Join-Path $env:LOCALAPPDATA 'doomfix\loader'
$SettingsFile = Join-Path $SettingsDir 'vk_loader_settings.json'   # имя файла фиксировано loader'ом

function Set-RegistryDword([string]$Key, [string]$Name) {
    if (-not (Test-Path $Key)) { New-Item -Path $Key -Force | Out-Null }
    New-ItemProperty -Path $Key -Name $Name -Value 0 -PropertyType DWord -Force | Out-Null
}

function Get-RegistryValueNames([string]$Key) {
    if (-not (Test-Path $Key)) { return @() }
    return @((Get-Item $Key).Property)
}

function Write-Utf8NoBom([string]$Path, [string]$Text) {
    [System.IO.File]::WriteAllText($Path, $Text, (New-Object System.Text.UTF8Encoding $false))
}

# --- 1. Копирование файлов ---------------------------------------------------------------
$bin = Join-Path $BuildDir 'bin'
$dll = Join-Path $bin 'doomfix_layer.dll'
$manifest = Join-Path $bin "$LayerName.json"
foreach ($f in $dll, $manifest) {
    if (-not (Test-Path $f)) { throw "Не найден $f. Сначала соберите проект: scripts\build.ps1" }
}
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
foreach ($f in @($dll, $manifest) + @(Get-ChildItem $bin -Filter 'doomfix_layer.pdb' | ForEach-Object FullName)) {
    Copy-Item $f $InstallDir -Force
}
$installedManifest = (Resolve-Path (Join-Path $InstallDir "$LayerName.json")).Path
Write-Host "Файлы слоя: $InstallDir"

# --- 2. ImplicitLayers ------------------------------------------------------------------
# Удаляем старые регистрации doomfix из других каталогов, чтобы не было дублей.
foreach ($name in Get-RegistryValueNames $ImplicitKey) {
    if ((Split-Path $name -Leaf) -ieq "$LayerName.json" -and $name -ne $installedManifest) {
        Remove-ItemProperty -Path $ImplicitKey -Name $name
        Write-Host "Удалена старая регистрация: $name"
    }
}
Set-RegistryDword $ImplicitKey $installedManifest
Write-Host "Зарегистрирован implicit layer: $ImplicitKey -> $installedManifest"

# --- 3. Порядок слоёв (loader settings) ---------------------------------------------------
if ($NoOrdering) {
    Write-Warning "Порядок слоёв не настроен (-NoOrdering): RenderDoc будет ближе к приложению, чем doomfix."
} else {
    $foreign = Get-RegistryValueNames $SettingsKey | Where-Object {
        $_ -ne $SettingsFile -and (Split-Path $_ -Leaf) -ieq 'vk_loader_settings.json' -and (Test-Path $_)
    }
    if ($foreign) {
        Write-Warning ("В $SettingsKey уже зарегистрирован чужой файл настроек loader'а (вероятно, от Vulkan Configurator):`n  " +
            ($foreign -join "`n  ") +
            "`nLoader использует только один такой файл, поэтому doomfix его не трогает. Закройте vkconfig " +
            "(или выключите в нём 'Vulkan Loader Management') и запустите install.ps1 снова.")
    } else {
        $layers = @(
            [ordered]@{ control = 'auto'; name = $LayerName; path = $installedManifest; treat_as_implicit_manifest = $true },
            [ordered]@{ control = 'unordered_layer_location' }
        )
        $entry = [ordered]@{}
        if ($GameExe) { $entry.app_keys = @($GameExe) }
        $entry.layers = $layers
        $doc = [ordered]@{ file_format_version = '1.0.1'; settings_array = @($entry) }

        New-Item -ItemType Directory -Force -Path $SettingsDir | Out-Null
        Write-Utf8NoBom $SettingsFile ($doc | ConvertTo-Json -Depth 10)
        Set-RegistryDword $SettingsKey $SettingsFile
        $scope = if ($GameExe) { "только для $GameExe" } else { 'для всех приложений (слой активен только при DOOMFIX_ENABLE=1)' }
        Write-Host "Порядок слоёв: $LayerName первым, ближе к приложению; $scope"
        Write-Host "  $SettingsKey -> $SettingsFile"
    }
}

Write-Host ""
Write-Host "Включение: переменная окружения DOOMFIX_ENABLE=1 у процесса игры (см. README.md)."
Write-Host "Логи: $(Join-Path $env:LOCALAPPDATA 'doomfix\logs')"
