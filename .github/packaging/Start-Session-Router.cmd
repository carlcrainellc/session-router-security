@echo off
setlocal
REM Session Router one-click starter. Double-click this file.
REM Windows asks for Administrator (UAC). Creating the sr-tun0 adapter and
REM setting its DNS to 127.0.0.1 need admin. This starter does both.
REM You do not run netsh by hand.
REM Empty upstream= in session-router.ini is valid and does not block start.
REM Exit stays off in the packaged ini. Do not turn Exit on.

cd /d "%~dp0"

net session >nul 2>&1
if errorlevel 1 (
  echo Requesting Administrator permission. Approve the UAC prompt.
  powershell -NoProfile -ExecutionPolicy Bypass -Command "Start-Process -FilePath \"%~f0\" -Verb RunAs"
  exit /b
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Start-Session-Router.ps1"
set RC=%ERRORLEVEL%
if not "%RC%"=="0" (
  echo Starter failed with code %RC%.
  pause
  exit /b %RC%
)
echo.
echo Session Router is running in its own window. You can close this window.
echo Do not close the Session Router window.
pause
endlocal
