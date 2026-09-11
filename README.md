# ESP32 Flight Data Logger

Working copy of the project from the "Technical Documentation: ESP32 Flight Data Logger" Google Doc.
Everything here is runnable as-is; the doc's code blocks had been mangled by the doc's table formatting.

```
flight-data-logger/
  flight_logger/flight_logger.ino     firmware for the airborne board
  ground_station/ground_station.ino   firmware for the receiving board (optional)
  dashboard/flightdeck.html           live browser dashboard
  tools/setup.bat                     one-time toolchain install
  tools/build.bat                     compile + upload
  tools/log_serial.py                 headless CSV logger (fallback)
```

## Quick start

1. `tools\setup.bat` once. Installs arduino-cli, the ESP32 core, and the three libraries.
   Open a new terminal afterwards so PATH picks up arduino-cli.
2. Plug the ESP32 in. `tools\build.bat` with no arguments lists the detected boards and compiles.
3. `tools\build.bat COM5` (use whatever port step 2 printed) to upload.
4. Close every serial monitor. Open `dashboard\flightdeck.html` in Chrome or Edge, click CONNECT,
   pick the ESP32 port. F11 for fullscreen.

No hardware to hand: open the dashboard and click SIM.

## Wiring

| Component | Module pin | ESP32 GPIO | Notes |
|---|---|---|---|
| MPU6050 | VCC / GND | 3V3 / GND | 3.3V |
| | SDA | GPIO 21 | I2C data |
| | SCL | GPIO 22 | I2C clock |
| BMP280 | VIN / GND | 3V3 / GND | 3.3V |
| | SDA | GPIO 21 | shared I2C bus |
| | SCL | GPIO 22 | shared I2C bus |
| NRF24L01+ | VCC / GND | 3V3 / GND | 3.3V ONLY, plus the capacitor below |
| | MOSI | GPIO 23 | |
| | MISO | GPIO 19 | |
| | SCK | GPIO 18 | |
| | CSN | GPIO 5 | |
| | CE | GPIO 4 | |
| | IRQ | not connected | |

**Hardware rule that is not optional:** a 10-100 uF electrolytic capacitor directly across
the NRF24's VCC and GND pins on the breadboard. The PA+LNA module's TX current spikes will
otherwise brown out the ESP32. Attach the SMA antenna before powering on, every time.

## Changes made to the doc's code

Three things in the doc version would have bitten you on the bench:

1. **Units did not match the CSV header.** Adafruit's MPU6050 driver returns acceleration in
   m/s^2 and gyro in rad/s, but the header advertises `ax_g` and `gx_dps`. On a desk the
   dashboard's TOTAL G-FORCE would have read ~9.81 and sat permanently in the red warn state,
   and the rotation gauge would have read ~1 instead of ~57. The firmware now converts to g
   and deg/s before printing.
2. **The radio packet was too big.** `TelemetryPacket` was 44 bytes; the NRF24L01+ payload
   limit is 32, so `radio.write()` would have silently truncated it. The packet is now 24
   bytes with the floats scaled to int16 (accel x1000 g, gyro in whole deg/s, temp x100).
   `ground_station.ino` carries the identical struct, so keep the two in sync if you edit it.
3. **`#if RADIO_ENABLED` with `#define RADIO_ENABLED true`** is fragile in the preprocessor.
   It is `1` / `0` now. There is also a `radioOk` flag so a missing radio no longer means
   every loop attempts a write.

Minor: `while (!Serial)` replaced with a fixed delay (it can hang on some CP210x setups),
I2C bumped to 400 kHz, `setRetries(1,3)` so a failing radio does not stall the 20 Hz loop,
BMP address lifted to a `#define` at the top for the 0x76/0x77 swap, and the dashboard now
says so plainly if the browser has no Web Serial rather than throwing.

## Troubleshooting

- **NRF24 not responding** - the capacitor. Then check CE=4, CSN=5. Then try
  `RF24_PA_MIN` while testing on a desk.
- **BMP280 not found** - change `BMP_ADDRESS` to `0x77` at the top of the firmware.
- **Dashboard will not connect** - something else holds the port. Close the Arduino serial
  monitor and any running `log_serial.py`. Web Serial is Chrome/Edge only; it will not work
  in Firefox or Safari.
- **Upload fails at "Connecting..."** - hold the BOOT button on the devkit until it starts.
- **Altitude drifts** - the BMP280 is picking up heat from the ESP32 regulator. Move it, or
  re-run the baseline calibration by resetting the board once it has thermally settled.
- **No COM port at all** - CP210x driver:
  https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers

## Demo run order

1. Upload firmware, leave the board on USB.
2. Open `flightdeck.html`, CONNECT, F11.
3. REC to start capturing, tilt the board around, SAVE CSV to drop a file for analysis.

## Working in VS Code

Two ways, pick one.

**Terminal only, no extension.** Open this folder in VS Code, Ctrl+` for the terminal, and use
`tools\setup.bat` then `tools\build.bat COM5`. VS Code is just the editor here.

**PlatformIO** (build/upload buttons, IntelliSense, installs the libraries for you).
Install the "PlatformIO IDE" extension, reopen this folder, wait for it to finish initialising,
then use the checkmark (build), arrow (upload) and plug icon (serial monitor) in the blue status
bar. `platformio.ini` is already set up. To flash the receiver instead, swap the `src_dir` line
in `platformio.ini` to `ground_station`.

The first PlatformIO build downloads a few hundred MB of toolchain. It is slow exactly once.

Note that the two toolchains are independent: PlatformIO ignores `setup.bat` entirely and keeps
its own copy of the ESP32 core and libraries. Do not bother running both.
