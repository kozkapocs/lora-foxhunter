# FoxHunter Beacon Firmware

Periodic LoRa beacon for fox-hunting (radio direction finding) events.
Runs on the **Seeed XIAO nRF52840** with an SX1262 LoRa radio module.

## Table of Contents

1. [Hardware](#hardware)
2. [Prerequisites](#prerequisites)
3. [Building the Firmware](#building-the-firmware)
4. [Flashing to the Device](#flashing-to-the-device)
5. [Configuring the Beacon](#configuring-the-beacon)
6. [Packet Format](#packet-format)
7. [Transmission Timing](#transmission-timing)
8. [LED Indicators](#led-indicators)
9. [Serial Monitor](#serial-monitor)

---

## Hardware

| Component | Part |
|-----------|------|
| MCU board | Seeed XIAO nRF52840 |
| Radio | SX1262 LoRa module wired to the XIAO |

**SX1262 pin connections (fixed in firmware):**

| Signal  | XIAO pin | nRF52 pad |
|---------|----------|-----------|
| NSS/CS  | D4       | P0.04     |
| DIO1    | D1       | P0.03     |
| RESET   | D2       | P0.28     |
| BUSY    | D3       | P0.29     |
| RXEN    | D5       | P0.05     |

---

## Prerequisites

### Toolchain

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html)
(CLI is sufficient; the IDE plugin works too):

```bash
pip install platformio
```

### Python configurator dependencies

```bash
pip install pyserial
```

---

## Building the Firmware

Clone the **entire repository** first — the build relies on shared `boards/` and
`tools/` directories at the repo root:

```bash
git clone <repo-url> foxhunter
cd foxhunter/beacon
pio run
```

A successful build ends with output similar to:

```
Linking .pio/build/foxhunter_beacon/firmware.elf
Checking size .pio/build/foxhunter_beacon/firmware.elf
RAM:   [=         ]   9.8% (used 6396 bytes from 65536 bytes)
Flash: [===       ]  27.4% (used 71456 bytes from 261120 bytes)
=== [SUCCESS] ===
```

---

## Flashing to the Device

### First-time flashing (UF2 drag-and-drop)

1. Double-press the **RESET** button on the XIAO to enter UF2 bootloader mode.
   A USB mass storage device called **XIAO-SENSE** (or similar) appears.
2. Run PlatformIO to produce a UF2 file and copy it:

```bash
pio run --target upload
```

PlatformIO uses `nrfutil` to upload automatically when the device is in
bootloader mode. If auto-detection fails, specify the port explicitly:

```bash
pio run --target upload --upload-port /dev/ttyACM0
```

### Subsequent uploads (nrfutil over USB)

Once the firmware has been flashed at least once, normal uploads work without
entering bootloader mode manually:

```bash
pio run --target upload
```

### Finding the serial port

```bash
# Linux
ls /dev/ttyACM*

# macOS
ls /dev/cu.usbmodem*
```

---

## Configuring the Beacon

Configuration is written over USB serial using the Python helper script located
at `tools/configure_beacon.py`.

The script requires the `pyserial` package (`pip install pyserial`).

### Show current configuration

```bash
python3 tools/configure_beacon.py --port /dev/ttyACM0
```

Example output:

```
Current config on device:
--- FoxHunter Beacon Config ---
STATUS: NO CONFIG (set SYSID and FOX, then SAVE)
SYSID:   (not set)
FOX:     (not set)
TXCOUNT: 10
TXINT:   1
PERIOD:  20
FREQ:    869.450
BW:      125.00
SF:      9
CR:      5
TXPOWER: 18
-------------------------------
```

### Set mandatory fields and save

```bash
python3 tools/configure_beacon.py --port /dev/ttyACM0 --sysid TEAM1 --fox 1
```

### Full configuration in one command

```bash
python3 tools/configure_beacon.py --port /dev/ttyACM0 \
    --sysid TEAM1 --fox 2 \
    --txcount 10 --txint 1 --period 20 \
    --freq 869.45 --bw 125 --sf 9 --cr 5 --txpower 18
```

### All available parameters

| Flag        | Description                                      | Range / default |
|-------------|--------------------------------------------------|-----------------|
| `--sysid`   | System identifier (group name)                   | 1–8 chars, **mandatory** |
| `--fox`     | Fox number within the group                      | 1–255, **mandatory** |
| `--txcount` | Packets transmitted per burst                    | 1–100 (default 10) |
| `--txint`   | Seconds between packets within a burst           | 1–255 s (default 1) |
| `--period`  | Full cycle duration (burst + silence)            | 1–255 s (default 20) |
| `--freq`    | LoRa carrier frequency in MHz                    | 150–960 MHz (default 869.45) |
| `--bw`      | LoRa bandwidth in kHz                            | 7.8 / 10.4 / 15.6 / 20.8 / 31.25 / 41.7 / 62.5 / **125** / 250 / 500 |
| `--sf`      | Spreading factor                                  | 5–12 (default 9) |
| `--cr`      | Coding rate                                       | 5–8 (default 5) |
| `--txpower` | TX power in dBm                                  | -9–22 (default 18) |

### Erase stored configuration

```bash
python3 tools/configure_beacon.py --port /dev/ttyACM0 --reset
```

The device erases the config file from flash and reboots. It will then blink
the red LED until a new configuration is saved.

### Configuring without the script (any serial terminal)

Connect with any terminal at **115200 baud** (e.g. `screen /dev/ttyACM0 115200`)
and type commands manually:

```
SET SYSID TEAM1
SET FOX 1
SET TXPOWER 18
GET ALL
SAVE
```

Every command returns `OK` or `ERR: <reason>`.

---

## Packet Format

Each transmitted LoRa packet is **9 bytes**:

```
Byte 0–7  system_id   Group identifier, null-padded to 8 bytes (e.g. "TEAM1\0\0")
Byte 8    fox_num     Fox index within the group (1–255)
```

Hardware CRC is appended automatically by the SX1262. No application-level
checksum is needed.

---

## Transmission Timing

The beacon operates in repeating cycles:

```
|<--------- period_s seconds --------->|
|  TX TX TX … TX  |      silence       |
   ↑tx_count pkts
   ↑ spaced tx_interval_s seconds apart
```

**Default settings** (SF9 / BW125):

| Parameter   | Value | Meaning |
|-------------|-------|---------|
| tx_count    | 10    | 10 packets per burst |
| tx_interval | 1 s   | one packet per second |
| period      | 20 s  | 10 s transmitting, 10 s quiet |
| Air time    | ~144 ms/packet | |
| Duty cycle  | 7.2 % | within the 10 % legal limit for 869.4–869.65 MHz |

Receivers see a fresh RSSI reading every second during the burst, allowing
direction-finding by rotating the antenna.

> **Warning:** high SF combined with a short period may exceed the 10 % duty
> cycle limit in the 869.4–869.65 MHz sub-band. The firmware will refuse to
> save a configuration where the burst does not fit inside the period.

---

## LED Indicators

| LED   | Colour | Behaviour | Meaning |
|-------|--------|-----------|---------|
| D11   | Red    | Fast blink (4 Hz) | No valid configuration stored |
| D12   | Blue   | Brief flash per TX | Packet being transmitted |

Both LEDs are **active LOW** (standard for XIAO nRF52840).

---

## Serial Monitor

Live status messages are printed at 115200 baud during normal operation:

```
Config loaded from flash.
Config valid — starting beacon.
TX #1 [TEAM1/1] RSSI floor: -95.00 dBm
TX #2 [TEAM1/1] RSSI floor: -96.00 dBm
…
TX #10 [TEAM1/1] RSSI floor: -95.00 dBm
(10 s silence)
TX #11 [TEAM1/1] RSSI floor: -95.00 dBm
…
```

To monitor without the configurator script:

```bash
# PlatformIO built-in monitor
pio device monitor --port /dev/ttyACM0 --baud 115200
```
