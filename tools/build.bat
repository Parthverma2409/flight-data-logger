@echo off
REM ============================================================
REM  Compile and upload the flight logger firmware.
REM    build.bat              -> compile only
REM    build.bat COM5         -> compile + upload to COM5
REM    build.bat COM5 ground  -> compile + upload the ground station
REM ============================================================
setlocal
set ROOT=%~dp0..
set FQBN=esp32:esp32:esp32
set SKETCH=%ROOT%\flight_logger
if /I "%~2"=="ground" set SKETCH=%ROOT%\ground_station

echo.
echo Detected boards:
arduino-cli board list
echo.
echo Compiling %SKETCH% ...
arduino-cli compile --fqbn %FQBN% "%SKETCH%"
if errorlevel 1 (
  echo COMPILE FAILED
  exit /b 1
)

if "%~1"=="" (
  echo.
  echo Compile OK. To upload:  build.bat COM3
  exit /b 0
)

echo.
echo Uploading to %~1 ...
arduino-cli upload -p %~1 --fqbn %FQBN% "%SKETCH%"
if errorlevel 1 (
  echo UPLOAD FAILED - hold the BOOT button while it says "Connecting..."
  exit /b 1
)

echo.
echo Uploaded. Close every serial monitor, then open dashboard\flightdeck.html
echo in Chrome or Edge and click CONNECT.
endlocal
