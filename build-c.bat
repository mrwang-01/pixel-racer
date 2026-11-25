@echo off
REM Build the pure C / Win32 GDI edition of Pixel Racer.
REM Prefers the Visual Studio C compiler; falls back to MinGW gcc.
REM If you changed game-config.json, run "node sync-config.js" first.
setlocal
cd /d "%~dp0"

set "VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvarsall.bat"
if not exist "%VCVARS%" set "VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"

if exist "%VCVARS%" (
  echo Using Visual Studio C compiler...
  call "%VCVARS%" x64 >nul
  cl /nologo /O2 /W3 /D_CRT_SECURE_NO_WARNINGS /Fe:PixelRacerC.exe main.c /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib kernel32.lib
  if not errorlevel 1 goto success
  echo.
  echo MSVC build FAILED. See messages above.
  pause
  exit /b 1
)

echo Visual Studio C toolchain not found; trying MinGW gcc...
where gcc >nul 2>nul
if errorlevel 1 (
  echo ERROR: Neither Visual Studio C toolchain nor gcc is available.
  pause
  exit /b 1
)
gcc -O2 -std=c99 -mwindows -o PixelRacerC.exe main.c -lgdi32 -luser32 -lm
if errorlevel 1 (
  echo.
  echo gcc build FAILED. See messages above.
  pause
  exit /b 1
)

:success
echo.
echo Build OK - PixelRacerC.exe created.
echo Run it directly, or open PixelRacerC.sln in Visual Studio and press F5.
pause
