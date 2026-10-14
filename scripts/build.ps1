<#
.SYNOPSIS
  Builds doomfix_layer.dll inside a Visual Studio (Build Tools) environment with CMake and Ninja.
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build.ps1
#>
param(
    [ValidateSet('Debug', 'Release', 'RelWithDebInfo')]
    [string]$Config = 'RelWithDebInfo',
    [string]$BuildDir = (Join-Path $PSScriptRoot '..\build')
)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path $vswhere)) {
    throw "Visual Studio Build Tools not found (no vswhere.exe). See docs/BUILDING.md, 'Prerequisites'."
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "Visual Studio was found, but without 'MSVC x64/x86 build tools'. Add the 'Desktop development with C++' workload."
}

# x64 compiler environment (cl, link, Windows SDK, plus the cmake and ninja that ship with VS).
& (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

foreach ($tool in 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool not found. Install the 'C++ CMake tools for Windows' component of the Build Tools."
    }
}

cmake -S $root -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
if ($LASTEXITCODE) { throw "cmake configure failed with exit code $LASTEXITCODE" }
cmake --build $BuildDir
if ($LASTEXITCODE) { throw "cmake build failed with exit code $LASTEXITCODE" }

Write-Host ""
Write-Host "Done: $(Join-Path (Resolve-Path $BuildDir) 'bin')"
