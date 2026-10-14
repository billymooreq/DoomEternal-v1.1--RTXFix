@echo off
rem Installs the doomfix Vulkan layer for the current user (no administrator rights needed).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\install.ps1" %*
pause
