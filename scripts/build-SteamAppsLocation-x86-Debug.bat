@echo off
set "Configuration=Debug"
call "%~dp0build-SteamAppsLocation-x86.bat" %*
exit /b %errorlevel%
