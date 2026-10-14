@echo off
rem Starts DOOM Eternal with the doomfix layer: indirect barrier fix ON (recommended).
rem On the first run the game folder is found in your Steam libraries or asked for, and saved
rem to doomfix_game_path.txt next to this file. Extra arguments go to scripts\launch.ps1 (e.g. -Reconfigure).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\launch.ps1" -Mode on -ConfigFile "%~dp0doomfix_game_path.txt" %*
if errorlevel 1 pause
