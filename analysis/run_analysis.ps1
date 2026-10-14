<#
.SYNOPSIS
  Runs one of the analysis scripts (analysis/scripts/*.py) inside RenderDoc's qrenderdoc.exe.

.DESCRIPTION
  qrenderdoc --python has no console and gives scripts neither __file__ nor argv, so this wrapper
  checks the inputs, passes them through environment variables and prints the script's log afterwards.
  The capture is only opened for reading; nothing is written into it.

.PARAMETER Script
  Script name, with or without .py, e.g. s1_explore or s25_lists.py.
.PARAMETER Capture
  Path to the RenderDoc capture (.rdc).
.PARAMETER Out
  Optional subfolder of analysis/ for the results (e.g. c3_fixed). Default: analysis/ itself.
.PARAMETER RenderDoc
  RenderDoc install folder (the one containing qrenderdoc.exe). Default: $env:DOOMFIX_RENDERDOC,
  then "$env:ProgramFiles\RenderDoc".
.PARAMETER Set
  Extra variables for scripts that take parameters, e.g. @{ DOOMFIX_DRAW = '18764'; DOOMFIX_TARGET = '53374' }.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File analysis\run_analysis.ps1 -Script s20_overview -Capture D:\captures\doom.rdc -Out mycapture
#>
param(
    [Parameter(Mandatory = $true)][string]$Script,
    [Parameter(Mandatory = $true)][string]$Capture,
    [string]$Out = '',
    [string]$RenderDoc = $env:DOOMFIX_RENDERDOC,
    [hashtable]$Set = @{},
    [int]$TimeoutSec = 3600
)
$ErrorActionPreference = 'Stop'

$scriptsDir = Join-Path $PSScriptRoot 'scripts'
$name = if ($Script.EndsWith('.py')) { $Script } else { "$Script.py" }
$scriptPath = Join-Path $scriptsDir $name
if (-not (Test-Path -LiteralPath $scriptPath)) {
    throw "Script not found: $scriptPath. Available: $((Get-ChildItem $scriptsDir -Filter 's*.py' | ForEach-Object BaseName) -join ', ')"
}
if (-not (Test-Path -LiteralPath $Capture -PathType Leaf)) {
    throw "Capture not found: '$Capture'. Pass the path to a RenderDoc .rdc file with -Capture."
}
$Capture = (Resolve-Path -LiteralPath $Capture).Path

if (-not $RenderDoc) { $RenderDoc = Join-Path $env:ProgramFiles 'RenderDoc' }
$qrenderdoc = Join-Path $RenderDoc 'qrenderdoc.exe'
if (-not (Test-Path -LiteralPath $qrenderdoc)) {
    throw "qrenderdoc.exe not found in '$RenderDoc'. Install RenderDoc (https://renderdoc.org, tested with 1.46) " +
          "and pass its folder with -RenderDoc or set DOOMFIX_RENDERDOC."
}

$outDir = if ($Out) { Join-Path $PSScriptRoot $Out } else { $PSScriptRoot }
New-Item -ItemType Directory -Force -Path $outDir | Out-Null
$errorFile = Join-Path $env:TEMP 'doomfix_analysis_error.txt'
Remove-Item -LiteralPath $errorFile -ErrorAction SilentlyContinue

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = $qrenderdoc
$psi.Arguments = "--python `"$scriptPath`""
$psi.UseShellExecute = $false
$psi.EnvironmentVariables['DOOMFIX_SCRIPTS'] = $scriptsDir
$psi.EnvironmentVariables['DOOMFIX_CAPTURE'] = $Capture
$psi.EnvironmentVariables['DOOMFIX_OUT'] = $Out
$psi.EnvironmentVariables['DOOMFIX_RENDERDOC'] = $RenderDoc
foreach ($k in $Set.Keys) { $psi.EnvironmentVariables[[string]$k] = [string]$Set[$k] }

Write-Host "Running $name on $Capture (results in $outDir) ..."
$start = Get-Date
$proc = [System.Diagnostics.Process]::Start($psi)
if (-not $proc.WaitForExit($TimeoutSec * 1000)) {
    $proc.Kill()
    throw "qrenderdoc did not finish within $TimeoutSec s and was stopped."
}

if (Test-Path -LiteralPath $errorFile) {
    throw "The script stopped: $(Get-Content -LiteralPath $errorFile -Raw)"
}
$log = Get-ChildItem -LiteralPath $outDir -Filter '*.log' | Where-Object { $_.LastWriteTime -ge $start } |
    Sort-Object LastWriteTime | Select-Object -Last 1
if ($log) {
    Write-Host "--- $($log.FullName)"
    Get-Content -LiteralPath $log.FullName
} else {
    Write-Warning "No log was written to $outDir. The script may have failed to start inside qrenderdoc."
}
