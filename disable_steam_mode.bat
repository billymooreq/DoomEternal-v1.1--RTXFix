@echo off
rem Turns Steam mode off: removes DOOMFIX_ENABLE from your Windows user and offers to restart Steam.
rem run_doom_fixed.bat keeps working without it.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\steam_mode.ps1" -Disable %*
pause
