@echo off
rem Removes the doomfix Vulkan layer registration and its installed files.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\uninstall.ps1" %*
pause
