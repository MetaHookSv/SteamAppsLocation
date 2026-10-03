@echo off
set "Configuration=Release"
call "%~dp0build-SteamAppsLocation-x86.bat" %*
exit /b %errorlevel%
