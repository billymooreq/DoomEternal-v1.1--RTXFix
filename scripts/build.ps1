<#
.SYNOPSIS
  Собирает doomfix_layer.dll из окружения Visual Studio (Build Tools) с помощью CMake и Ninja.
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
    throw "Visual Studio Build Tools не найдены (нет vswhere.exe). См. README.md, раздел 'Что установить'."
}
$vs = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) {
    throw "Visual Studio найдена, но без компонента 'MSVC x64/x86 build tools'. Добавьте рабочую нагрузку 'Desktop development with C++'."
}

# Окружение компилятора x64 (cl, link, Windows SDK, а также cmake и ninja из состава VS).
& (Join-Path $vs 'Common7\Tools\Launch-VsDevShell.ps1') -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

foreach ($tool in 'cmake', 'ninja') {
    if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
        throw "$tool не найден. Установите компонент 'C++ CMake tools for Windows' в Build Tools."
    }
}

cmake -S $root -B $BuildDir -G Ninja "-DCMAKE_BUILD_TYPE=$Config"
if ($LASTEXITCODE) { throw "cmake configure завершился с кодом $LASTEXITCODE" }
cmake --build $BuildDir
if ($LASTEXITCODE) { throw "cmake build завершился с кодом $LASTEXITCODE" }

Write-Host ""
Write-Host "Готово: $(Join-Path (Resolve-Path $BuildDir) 'bin')"
