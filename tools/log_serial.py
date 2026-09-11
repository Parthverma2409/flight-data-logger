#!/usr/bin/env python3
"""
Headless CSV logger - a fallback for when the browser dashboard cannot take
the COM port (or when you want a file without pressing REC).

    pip install pyserial
    python log_serial.py                 # auto-detect the ESP32 port
    python log_serial.py COM5            # explicit port
    python log_serial.py COM5 flight.csv # explicit port + output file

Only one program can hold the COM port, so close the dashboard and the
Arduino serial monitor before running this.
"""
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial is missing.  Run:  pip install pyserial")

BAUD = 115200
HEADER = ("timestamp_ms,ax_g,ay_g,az_g,gx_dps,gy_dps,gz_dps,"
          "pressure_pa,temp_c,altitude_m,packet_id")


def autodetect():
    hits = []
    for p in list_ports.comports():
        blob = f"{p.description} {p.manufacturer} {p.hwid}".lower()
        if any(k in blob for k in ("cp210", "ch340", "silicon labs", "usb-serial", "esp32")):
            hits.append(p.device)
    if not hits:
        sys.exit("No ESP32-looking port found. Pass the port explicitly, e.g. "
                 "python log_serial.py COM5")
    if len(hits) > 1:
        print(f"Multiple candidates {hits}, using {hits[0]}")
    return hits[0]


def main():
    port = sys.argv[1] if len(sys.argv) > 1 else autodetect()
    out = sys.argv[2] if len(sys.argv) > 2 else f"flight_{int(time.time())}.csv"

    print(f"Port {port} @ {BAUD}  ->  {out}")
    print("Ctrl+C to stop.\n")

    n = 0
    with serial.Serial(port, BAUD, timeout=1) as ser, open(out, "w", newline="") as f:
        f.write(HEADER + "\n")
        try:
            while True:
                raw = ser.readline().decode("utf-8", "replace").strip()
                if not raw:
                    continue
                if not raw[0].isdigit():        # banner / error lines
                    print(f"  [esp32] {raw}")
                    continue
                if len(raw.split(",")) < 10:
                    continue
                f.write(raw + "\n")
                n += 1
                if n % 20 == 0:
                    f.flush()
                    print(f"\r  {n} samples", end="", flush=True)
        except KeyboardInterrupt:
            pass

    print(f"\nStopped. {n} samples written to {out}")


if __name__ == "__main__":
    main()
