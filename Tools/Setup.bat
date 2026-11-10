@echo off
setlocal EnableExtensions
title Nawat Kernel Setup

rem Double-click            : compile the plugin against Unreal Engine 5.7.
rem Drag a .uproject onto it: copy the plugin into that project.

set "HERE=%~dp0"
set "PLUGIN=%HERE%Nawat"
if not exist "%PLUGIN%\Nawat.uplugin" set "PLUGIN=%HERE%..\Plugins\Nawat"
if not exist "%PLUGIN%\Nawat.uplugin" goto :noplugin
for %%I in ("%PLUGIN%") do set "PLUGIN=%%~fI"

if not "%~1"=="" goto :install

rem ---- Locate Unreal Engine 5.7 (UE_ROOT overrides) ----
set "UE="
if defined UE_ROOT set "UE=%UE_ROOT%"
if not defined UE for /f "tokens=2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\5.7" /v InstalledDirectory 2^>nul ^| find "InstalledDirectory"') do set "UE=%%B"
if not defined UE if exist "%ProgramFiles%\Epic Games\UE_5.7\Engine" set "UE=%ProgramFiles%\Epic Games\UE_5.7"
if not defined UE goto :noengine

set "UAT=%UE%\Engine\Build\BatchFiles\RunUAT.bat"
if not exist "%UAT%" goto :noengine

set "OUTROOT=%HERE%Build"
set "OUT=%OUTROOT%\Nawat"
set "LOG=%OUTROOT%\build-log.txt"
if not exist "%OUTROOT%" mkdir "%OUTROOT%"
if exist "%OUT%" rmdir /s /q "%OUT%"

echo Engine : %UE%
echo Plugin : %PLUGIN%
echo.
echo Compiling. This takes several minutes and prints nothing until it ends...
call "%UAT%" BuildPlugin -Plugin="%PLUGIN%\Nawat.uplugin" -Package="%OUT%" -TargetPlatforms=Win64 > "%LOG%" 2>&1
if errorlevel 1 goto :buildfail

echo.
echo [OK] Build succeeded.
echo Compiled plugin : %OUT%
echo Next: drag your .uproject file onto Setup.bat to install it into your project.
goto :done

:install
if /i not "%~x1"==".uproject" goto :notproject
set "DEST=%~dp1Plugins\Nawat"
echo Installing into: %DEST%
robocopy "%PLUGIN%" "%DEST%" /E /XD Binaries Intermediate >nul
if errorlevel 8 goto :copyfail
echo.
echo [OK] Plugin installed.
echo Open the project. Unreal will offer to rebuild the Nawat modules; answer Yes.
echo The project must be a C++ project. For a Blueprint-only project, first add
echo any C++ class from Tools - New C++ Class.
goto :done

:noplugin
echo [ERROR] Nawat.uplugin was not found next to this script.
goto :fail

:noengine
echo [ERROR] Unreal Engine 5.7 was not found.
echo Install it from the Epic Games Launcher, or set UE_ROOT to the engine folder,
echo for example:  set UE_ROOT=D:\Epic\UE_5.7
goto :fail

:notproject
echo [ERROR] Drag a .uproject file onto this script. Received: %~1
goto :fail

:copyfail
echo [ERROR] Copy failed. Close Unreal Editor and try again.
goto :fail

:buildfail
echo.
echo [FAILED] The build failed. Error lines:
echo ------------------------------------------------------------
findstr /i /c:"error" "%LOG%"
echo ------------------------------------------------------------
echo Full log: %LOG%
echo Send that file back so the errors can be fixed.
echo If the log mentions a missing compiler, install Visual Studio 2022 with the
echo "Game development with C++" workload.
goto :fail

:fail
echo.
pause
exit /b 1

:done
echo.
pause
exit /b 0
