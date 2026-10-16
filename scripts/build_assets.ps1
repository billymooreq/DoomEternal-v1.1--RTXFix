<#
.SYNOPSIS
  Regenerates the installer artwork in installer\assets (wizard backgrounds and doomfix.ico).

.DESCRIPTION
  Creates a Python virtual environment in build-assets\.venv (ignored by git) with the versions from
  installer\assets\requirements.txt and runs installer\assets\generate.py. The generated PNG and ICO files are
  committed, so building the installer does not need Python; run this only after changing generate.py.
  Needs Python 3 on PATH (python.exe or the py launcher).

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\build_assets.ps1
#>
param(
    [string]$VenvDir = (Join-Path $PSScriptRoot '..\build-assets\.venv')
)
$ErrorActionPreference = 'Stop'
$assets = (Resolve-Path (Join-Path $PSScriptRoot '..\installer\assets')).Path
$python = Join-Path $VenvDir 'Scripts\python.exe'

if (-not (Test-Path $python)) {
    $base = Get-Command python -ErrorAction SilentlyContinue
    if ($base) { & $base.Source -m venv $VenvDir } else { & py -3 -m venv $VenvDir }
    if ($LASTEXITCODE -or -not (Test-Path $python)) { throw "Could not create the virtual environment in $VenvDir (is Python 3 installed?)" }
}
& $python -m pip install --disable-pip-version-check --quiet -r (Join-Path $assets 'requirements.txt')
if ($LASTEXITCODE) { throw "pip install failed with exit code $LASTEXITCODE" }

& $python (Join-Path $assets 'generate.py')
if ($LASTEXITCODE) { throw "generate.py failed with exit code $LASTEXITCODE" }
