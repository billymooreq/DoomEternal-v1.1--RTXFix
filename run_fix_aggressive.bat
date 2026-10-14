@echo off
rem DOOM Eternal 1.1 with doomfix layer, DOOMFIX_INDIRECT_FIX=aggressive (no RenderDoc).
setlocal
set "GAME_DIR=C:\Steam\steamapps\common\DOOMEternal"
set "GAME_EXE=DOOMEternalx64vk.exe"
if not exist "%GAME_DIR%\%GAME_EXE%" (
  echo Not found: %GAME_DIR%\%GAME_EXE%
  pause
  exit /b 1
)
set "DOOMFIX_ENABLE=1"
set "SteamAppId=782330"
set "DOOMFIX_INDIRECT_FIX=aggressive"
cd /d "%GAME_DIR%"
start "" "%GAME_DIR%\%GAME_EXE%"
endlocal
