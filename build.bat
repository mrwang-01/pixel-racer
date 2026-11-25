@echo off
REM Fallback build for Pixel Racer.
REM Uses the .NET Framework C# compiler that ships with Windows.
REM No Visual Studio workload and no .NET SDK are required.
REM If you changed game-config.json, run "node sync-config.js" first.
setlocal
cd /d "%~dp0"
set "CSC=%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
if not exist "%CSC%" set "CSC=%WINDIR%\Microsoft.NET\Framework\v4.0.30319\csc.exe"
if not exist "%CSC%" (
  echo ERROR: Windows C# compiler not found.
  pause
  exit /b 1
)
"%CSC%" /nologo /target:winexe /optimize+ /out:PixelRacer.exe ^
  /r:System.dll /r:System.Core.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll ^
  Program.cs Config.cs
if errorlevel 1 (
  echo.
  echo Build FAILED.
  pause
  exit /b 1
)
echo.
echo Build OK - PixelRacer.exe created.
echo Double click PixelRacer.exe to play.
pause
