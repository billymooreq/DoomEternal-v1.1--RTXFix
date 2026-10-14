@echo off
rem Optional: lets the game pick up doomfix when you start it from Steam (instead of run_doom_fixed.bat).
rem Sets DOOMFIX_ENABLE=1 for your Windows user and offers to restart Steam. Undo with disable_steam_mode.bat.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\steam_mode.ps1" -Enable %*
pause
