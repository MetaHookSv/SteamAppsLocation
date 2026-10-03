@echo off
setlocal
if not "%Configuration%"=="Debug" if not "%Configuration%"=="Release" exit /b 2
set "SteamAppsLocationRoot=%~dp0.."
for %%I in ("%SteamAppsLocationRoot%") do set "SteamAppsLocationRoot=%%~fI"

cmake -G "Visual Studio 17 2022" -A Win32 -S "%SteamAppsLocationRoot%" -B "%SteamAppsLocationRoot%\build\x86\%Configuration%" -DCMAKE_INSTALL_PREFIX="%SteamAppsLocationRoot%\install\x86\%Configuration%" -DBUILD_TESTING=ON %*
if errorlevel 1 exit /b %errorlevel%
cmake --build "%SteamAppsLocationRoot%\build\x86\%Configuration%" --config %Configuration% --parallel
if errorlevel 1 exit /b %errorlevel%
ctest --test-dir "%SteamAppsLocationRoot%\build\x86\%Configuration%" -C %Configuration% --output-on-failure
if errorlevel 1 exit /b %errorlevel%
cmake --install "%SteamAppsLocationRoot%\build\x86\%Configuration%" --config %Configuration%
exit /b %errorlevel%
