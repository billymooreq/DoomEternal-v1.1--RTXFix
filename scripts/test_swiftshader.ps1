<#
.SYNOPSIS
  Runs doomfix_barrier_test on SwiftShader, the software Vulkan driver that ships with Google Chrome and
  Microsoft Edge. For machines without a GPU, such as the GitHub Actions runners.

.DESCRIPTION
  1. Finds vk_swiftshader_icd.json and vulkan-1.dll in the newest installed Chrome or Edge.
  2. Installs the layer from BuildDir\bin for the current user (scripts\install.ps1): the test checks the
     installed layer. On a developer machine this replaces the installed doomfix with this build.
  3. Copies the test next to the browser's vulkan-1.dll, so it uses that loader and not whatever is in
     System32, and runs it with VK_DRIVER_FILES (and the older VK_ICD_FILENAMES) pointing at SwiftShader
     only. The test never submits work to the GPU, so a software driver is enough.

.PARAMETER BuildDir
  Build folder with bin\doomfix_layer.dll and bin\doomfix_barrier_test.exe. Default: ..\build-release.
.PARAMETER Soak
  Pass --soak to the test (an extra ~12 s multithreaded run).
#>
param(
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build-release'),
    [switch]$Soak
)
$ErrorActionPreference = 'Stop'

$bin = (Resolve-Path (Join-Path $BuildDir 'bin')).Path
$test = Join-Path $bin 'doomfix_barrier_test.exe'
if (-not (Test-Path $test)) { throw "Not found: $test (build first)." }

# --- 1. SwiftShader --------------------------------------------------------------------------
$browserDirs = @(
    "${env:ProgramFiles}\Google\Chrome\Application",
    "${env:ProgramFiles(x86)}\Google\Chrome\Application",
    "${env:ProgramFiles(x86)}\Microsoft\Edge\Application",
    "${env:ProgramFiles}\Microsoft\Edge\Application"
)
$swiftshader = foreach ($dir in $browserDirs) {
    if (-not (Test-Path $dir)) { continue }
    Get-ChildItem $dir -Directory | Where-Object {
        $_.Name -match '^\d+(\.\d+)+$' -and
        (Test-Path (Join-Path $_.FullName 'vk_swiftshader_icd.json')) -and
        (Test-Path (Join-Path $_.FullName 'vulkan-1.dll'))
    } | Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
}
$swiftshader = $swiftshader | Select-Object -First 1
if (-not $swiftshader) {
    throw 'SwiftShader not found: no Chrome or Edge with vk_swiftshader_icd.json and vulkan-1.dll is installed.'
}
$icd = Join-Path $swiftshader.FullName 'vk_swiftshader_icd.json'
Write-Host "SwiftShader: $icd"

# --- 2. Install the layer --------------------------------------------------------------------
& (Join-Path $PSScriptRoot 'install.ps1') -SourceDir $bin

# An elevated process has high integrity, and then the Vulkan loader ignores layers registered under HKCU
# (loader_windows.c: use_secondary_hive). The GitHub runners run everything as an elevated administrator, so
# there the installed manifest is also registered under HKLM for the duration of the test.
$hklmKey = 'HKLM:\SOFTWARE\Khronos\Vulkan\ImplicitLayers'
$manifest = Join-Path $env:LOCALAPPDATA 'doomfix\layer\VK_LAYER_DOOMFIX.json'
$elevated = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
$addedHklm = $false
if ($elevated) {
    if (-not (Test-Path $hklmKey)) { New-Item -Path $hklmKey -Force | Out-Null }
    if (-not ((Get-Item $hklmKey).Property -contains $manifest)) {
        New-ItemProperty -Path $hklmKey -Name $manifest -Value 0 -PropertyType DWord -Force | Out-Null
        $addedHklm = $true
    }
    Write-Host "Elevated process: the loader skips HKCU layers, registered the manifest under $hklmKey for the test."
}

# --- 3. Run the test -------------------------------------------------------------------------
$work = Join-Path ([System.IO.Path]::GetTempPath()) "doomfix_swiftshader_$PID"
New-Item -ItemType Directory -Force $work | Out-Null
Copy-Item $test, (Join-Path $swiftshader.FullName 'vulkan-1.dll') $work -Force
$env:VK_DRIVER_FILES = $icd
$env:VK_ICD_FILENAMES = $icd
try {
    $testArgs = @()
    if ($Soak) { $testArgs += '--soak' }
    & (Join-Path $work 'doomfix_barrier_test.exe') @testArgs
    $code = $LASTEXITCODE
} finally {
    Remove-Item Env:\VK_DRIVER_FILES, Env:\VK_ICD_FILENAMES -ErrorAction SilentlyContinue
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
    if ($addedHklm) { Remove-ItemProperty -Path $hklmKey -Name $manifest -ErrorAction SilentlyContinue }
}
if ($code -ne 0) { throw "doomfix_barrier_test failed with exit code $code." }
