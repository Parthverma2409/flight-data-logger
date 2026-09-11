@echo off
REM ============================================================
REM  Flight Data Logger - one-time toolchain setup (Windows)
REM  Installs arduino-cli, the ESP32 board core, and the three
REM  required libraries. Run once, from anywhere.
REM ============================================================
setlocal

echo.
echo [1/4] Checking for arduino-cli...
where arduino-cli >nul 2>&1
if %errorlevel%==0 (
  echo       already installed.
) else (
  echo       installing via winget...
  winget install --id ArduinoSA.CLI -e --accept-package-agreements --accept-source-agreements
  if errorlevel 1 (
    echo.
    echo   winget failed. Install manually from:
    echo   https://arduino.github.io/arduino-cli/latest/installation/
    exit /b 1
  )
  echo       NOTE: you may need to open a NEW terminal for PATH to update.
)

echo.
echo [2/4] Initialising config and adding the ESP32 board index...
arduino-cli config init --overwrite
arduino-cli config add board_manager.additional_urls https://espressif.github.io/arduino-esp32/package_esp32_index.json
arduino-cli core update-index

echo.
echo [3/4] Installing the ESP32 core (this one is a few hundred MB)...
arduino-cli core install esp32:esp32

echo.
echo [4/4] Installing libraries...
arduino-cli lib install "Adafruit MPU6050"
arduino-cli lib install "Adafruit BMP280 Library"
arduino-cli lib install "RF24"

echo.
echo Done. Plug the ESP32 in and run:  tools\build.bat
echo If the board does not show up as a COM port, install the CP210x
echo USB driver: https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers
echo.
endlocal
