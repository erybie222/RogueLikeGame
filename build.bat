@echo off
setlocal

rem U¿ycie: build.bat [Debug|Release]
set CONFIG=%1
if "%CONFIG%"=="" set CONFIG=Debug

rem Pierwsza konfiguracja (tylko raz)
if not exist build (
  where ninja >nul 2>&1
  if %ERRORLEVEL%==0 (
    cmake -S . -B build -G "Ninja"
  ) else (
    cmake -S . -B build -G "Visual Studio 17 2022" -A x64
  )
)

rem Build (Ninja nie potrzebuje --config; VS generator tak)
if exist build\build.ninja (
  cmake --build build
) else (
  cmake --build build --config %CONFIG%
)

exit /b %ERRORLEVEL%
