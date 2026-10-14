<#
.SYNOPSIS
  Installs doomfix as an implicit Vulkan layer for the current user (no administrator rights needed).

.DESCRIPTION
  1. Copies doomfix_layer.dll and its manifest to %LOCALAPPDATA%\doomfix\layer.
  2. Registers the manifest under HKCU\SOFTWARE\Khronos\Vulkan\ImplicitLayers.
  3. Sets the layer order through a loader settings file (HKCU\SOFTWARE\Khronos\Vulkan\LoaderSettings ->
     vk_loader_settings.json): doomfix goes first, i.e. closest to the game and above tools such as
     RenderDoc. Without this step the loader reads HKLM implicit layers before HKCU ones, and a layer
     registered in HKLM (RenderDoc does that) would sit above doomfix.

  The layer only activates in processes started with DOOMFIX_ENABLE=1, so other applications are not affected.

.PARAMETER SourceDir
  Folder with doomfix_layer.dll and VK_LAYER_DOOMFIX.json. Default: ..\bin (release zip), then
  ..\build\bin (built from source), relative to this script.

.PARAMETER GameExe
  If set, the layer-order rule applies only to this exe (loader settings "app_keys"). The loader compares
  the path case-sensitively, so it must match exactly how the game is started. Default: the rule is global.

.PARAMETER NoOrdering
  Skip the loader settings file and only register the layer (doomfix may then end up BELOW RenderDoc).
#>
param(
    [string]$SourceDir = '',
    [string]$InstallDir = (Join-Path $env:LOCALAPPDATA 'doomfix\layer'),
    [string]$GameExe = '',
    [switch]$NoOrdering
)
$ErrorActionPreference = 'Stop'

$LayerName = 'VK_LAYER_DOOMFIX'
$ImplicitKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$SettingsKey = 'HKCU:\SOFTWARE\Khronos\Vulkan\LoaderSettings'
$SettingsDir = Join-Path $env:LOCALAPPDATA 'doomfix\loader'
$SettingsFile = Join-Path $SettingsDir 'vk_loader_settings.json'   # the file name is fixed by the loader

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

# --- 1. Copy the files ------------------------------------------------------------------------
if (-not $SourceDir) {
    $candidates = @((Join-Path $PSScriptRoot '..\bin'), (Join-Path $PSScriptRoot '..\build\bin'))
    $SourceDir = $candidates | Where-Object { Test-Path (Join-Path $_ 'doomfix_layer.dll') } | Select-Object -First 1
    if (-not $SourceDir) {
        throw "doomfix_layer.dll not found in $($candidates -join ' or '). Download a release zip, or build " +
              "the project first (see docs/BUILDING.md), or pass -SourceDir."
    }
}
$dll = Join-Path $SourceDir 'doomfix_layer.dll'
$manifest = Join-Path $SourceDir "$LayerName.json"
foreach ($f in $dll, $manifest) {
    if (-not (Test-Path $f)) { throw "Not found: $f" }
}
New-Item -ItemType Directory -Force -Path $InstallDir | Out-Null
foreach ($f in @($dll, $manifest) + @(Get-ChildItem $SourceDir -Filter 'doomfix_layer.pdb' | ForEach-Object FullName)) {
    Copy-Item $f $InstallDir -Force
}
$installedManifest = (Resolve-Path (Join-Path $InstallDir "$LayerName.json")).Path
Write-Host "Layer files: $InstallDir"

# --- 2. ImplicitLayers ------------------------------------------------------------------------
# Remove older doomfix registrations from other folders to avoid duplicates.
foreach ($name in Get-RegistryValueNames $ImplicitKey) {
    if ((Split-Path $name -Leaf) -ieq "$LayerName.json" -and $name -ne $installedManifest) {
        Remove-ItemProperty -Path $ImplicitKey -Name $name
        Write-Host "Removed old registration: $name"
    }
}
Set-RegistryDword $ImplicitKey $installedManifest
Write-Host "Registered implicit layer: $ImplicitKey -> $installedManifest"

# --- 3. Layer order (loader settings) ---------------------------------------------------------
if ($NoOrdering) {
    Write-Warning "Layer order not configured (-NoOrdering): tools registered in HKLM (e.g. RenderDoc) will sit above doomfix."
} else {
    $foreign = Get-RegistryValueNames $SettingsKey | Where-Object {
        $_ -ne $SettingsFile -and (Split-Path $_ -Leaf) -ieq 'vk_loader_settings.json' -and (Test-Path $_)
    }
    if ($foreign) {
        Write-Warning ("Another loader settings file is already registered in $SettingsKey (probably by Vulkan Configurator):`n  " +
            ($foreign -join "`n  ") +
            "`nThe loader uses only one such file, so doomfix leaves it alone. Close vkconfig (or turn off its " +
            "'Vulkan Loader Management') and run install.bat again.")
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
        $scope = if ($GameExe) { "only for $GameExe" } else { 'for all applications (the layer is active only with DOOMFIX_ENABLE=1)' }
        Write-Host "Layer order: $LayerName first, closest to the application; $scope"
        Write-Host "  $SettingsKey -> $SettingsFile"
    }
}

Write-Host ""
Write-Host "Done. Start the game with run_doom_fixed.bat (see README.md)."
Write-Host "Logs: $(Join-Path $env:LOCALAPPDATA 'doomfix\logs')"
