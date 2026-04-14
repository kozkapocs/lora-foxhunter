#!/usr/bin/env python3
"""
configure_receiver.py — FoxHunter receiver configuration tool

Usage examples:

  # Show current config
  python3 configure_receiver.py --port /dev/ttyACM0

  # Set mandatory field and save
  python3 configure_receiver.py --port /dev/ttyACM0 --sysid OVODAS

  # Full configuration in one call
  python3 configure_receiver.py --port /dev/ttyACM0 \\
      --sysid ISKOLASOK --freq 869.45 --bw 125 --sf 9 --cr 5

  # Erase stored config
  python3 configure_receiver.py --port /dev/ttyACM0 --reset
"""

import argparse
import sys
import time
import serial


BAUD = 115200
TIMEOUT = 3.0  # seconds to wait for a response line


def send_cmd(ser: serial.Serial, cmd: str) -> str:
    """Send one command line and return the response line (stripped)."""
    ser.write((cmd + "\n").encode())
    ser.flush()
    deadline = time.time() + TIMEOUT
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            return line
    return "(no response)"


def read_until_separator(ser: serial.Serial, separator: str = "---", timeout: float = 5.0) -> list[str]:
    lines = []
    deadline = time.time() + timeout
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            lines.append(line)
            if line.startswith(separator) and len(lines) > 1:
                break
    return lines


def open_port(port: str) -> serial.Serial:
    try:
        ser = serial.Serial(port, BAUD, timeout=0.5)
    except serial.SerialException as exc:
        print(f"ERROR: cannot open {port}: {exc}", file=sys.stderr)
        sys.exit(1)
    # Wait for the device boot banner, up to 5 s.  Needed when run right
    # after DFU: first-boot LittleFS format + radio init can take several
    # seconds.  Falls through immediately if device is already running.
    deadline = time.time() + 5.0
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            break
    time.sleep(0.5)
    ser.reset_input_buffer()
    return ser


def show_config(ser: serial.Serial) -> None:
    ser.write(b"GET ALL\n")
    ser.flush()
    lines = read_until_separator(ser, separator="---")
    for line in lines:
        print(line)


def apply_settings(ser: serial.Serial, args: argparse.Namespace) -> bool:
    mapping = [
        ("sysid", "SET SYSID {}"),
        ("freq",  "SET FREQ {}"),
        ("bw",    "SET BW {}"),
        ("sf",    "SET SF {}"),
        ("cr",    "SET CR {}"),
    ]

    any_sent = False
    for attr, template in mapping:
        value = getattr(args, attr, None)
        if value is None:
            continue
        cmd = template.format(value)
        response = send_cmd(ser, cmd)
        status = "OK" if response.startswith("OK") else f"FAILED ({response})"
        print(f"  {cmd}  →  {status}")
        if not response.startswith("OK"):
            print("Aborting due to error.", file=sys.stderr)
            return False
        any_sent = True
    return any_sent


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Configure a FoxHunter receiver over USB serial."
    )
    parser.add_argument("--port",  required=True, help="Serial port (e.g. /dev/ttyACM0)")
    parser.add_argument("--sysid", help="System identifier, 1-8 chars (must match beacons)")
    parser.add_argument("--freq",  type=float, help="LoRa frequency in MHz (default 869.45)")
    parser.add_argument("--bw",    type=float, help="LoRa bandwidth in kHz (default 125)")
    parser.add_argument("--sf",    type=int,   help="Spreading factor 5-12 (default 9)")
    parser.add_argument("--cr",    type=int,   help="Coding rate 5-8 (default 5)")
    parser.add_argument("--reset", action="store_true", help="Erase stored config and reboot")
    args = parser.parse_args()

    ser = open_port(args.port)

    if args.reset:
        print("Erasing config…")
        response = send_cmd(ser, "RESET")
        print(response)
        ser.close()
        return

    has_set_args = any(
        getattr(args, a) is not None
        for a in ("sysid", "freq", "bw", "sf", "cr")
    )
    if not has_set_args:
        print("Current config on device:")
        show_config(ser)
        ser.close()
        return

    print(f"Configuring receiver on {args.port}…")
    success = apply_settings(ser, args)
    if not success:
        ser.close()
        sys.exit(1)

    print("Saving to flash…")
    response = send_cmd(ser, "SAVE")
    print(f"  SAVE  →  {response}")
    if not response.startswith("OK"):
        print("Save failed.", file=sys.stderr)
        ser.close()
        sys.exit(1)

    print("Done.")
    ser.close()


if __name__ == "__main__":
    main()
