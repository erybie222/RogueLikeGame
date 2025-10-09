@echo off
setlocal

rem U¿ycie: run.bat [Debug|Release]
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

call "%~dp0build.bat" %CONFIG%
if errorlevel 1 exit /b %errorlevel%

set EXE1=%~dp0build\RogueLikeGame.exe
set EXE2=%~dp0build\%CONFIG%\RogueLikeGame.exe

if exist "%EXE1%" (
  "%EXE1%"
  exit /b %ERRORLEVEL%
)

if exist "%EXE2%" (
  "%EXE2%"
  exit /b %ERRORLEVEL%
)

echo [RUN] Nie znaleziono pliku wykonywalnego (sprawdzono: "%EXE1%" i "%EXE2%").
exit /b 1
