# FoxHunter Receiver Firmware

LoRa receiver for fox-hunting (radio direction finding) events.
Displays the signal strength of the selected beacon on an OLED screen.
Runs on the **Seeed Wio Tracker L1** (nRF52840 + SX1262).

## Table of Contents

1. [Hardware](#hardware)
2. [Prerequisites](#prerequisites)
3. [Building the Firmware](#building-the-firmware)
4. [Flashing to the Device](#flashing-to-the-device)
5. [Configuring the Receiver](#configuring-the-receiver)
6. [Operation](#operation)
7. [Display Layout](#display-layout)
8. [Serial Monitor](#serial-monitor)

---

## Hardware

| Component  | Part |
|------------|------|
| MCU board  | Seeed Wio Tracker L1 |
| Radio      | SX1262 (built-in) |
| Display    | 1.3" SH1106 OLED 128×64 (built-in, I2C 0x3D) |
| Input      | 5-way joystick (built-in) |

**SX1262 pin mapping:**

| Signal   | Pin | nRF52 pad |
|----------|-----|-----------|
| NSS/CS   | D4  | P1.14     |
| DIO1     | D1  | P0.07     |
| RESET    | D2  | P1.07     |
| BUSY     | D3  | P1.10     |
| RXEN     | D5  | P1.08     |

---

## Prerequisites

### Toolchain

Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html):

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
cd foxhunter/receiver
pio run
```

A successful build ends with:

```
Linking .pio/build/foxhunter_receiver/firmware.elf
=== [SUCCESS] ===
```

The post-build script also produces `firmware.uf2` in the same directory.

---

## Flashing to the Device

### UF2 drag-and-drop (first flash)

1. Double-press **RESET** on the Wio Tracker L1 to enter UF2 bootloader mode.
   A USB mass storage device appears.
2. Copy `firmware.uf2` onto it:

```bash
pio run  # builds and produces firmware.uf2
cp .pio/build/foxhunter_receiver/firmware.uf2 /media/$USER/<BOOTLOADER_DRIVE>/
```

### Subsequent uploads (nrfutil over USB)

```bash
pio run --target upload
```

Specify the port explicitly if auto-detection fails:

```bash
pio run --target upload --upload-port /dev/ttyACM0
```

---

## Configuring the Receiver

Connect via USB serial (115200 baud) and use the `tools/configure_receiver.py`
helper, or send commands manually with any serial terminal.

### Commands

| Command             | Description |
|---------------------|-------------|
| `SET SYSID <value>` | System identifier (must match the beacons), 1-8 chars |
| `SET FREQ <MHz>`    | LoRa frequency (e.g. `869.45`) |
| `SET BW <kHz>`      | LoRa bandwidth (e.g. `125`) |
| `SET SF <5-12>`     | Spreading factor |
| `SET CR <5-8>`      | Coding rate |
| `GET ALL`           | Print current parameters |
| `SAVE`              | Persist config to flash |
| `RESET`             | Erase config and reboot |

### Minimal setup

```
SET SYSID MYFOX
SAVE
```

The LoRa parameters default to the same values used by the beacon firmware
(`FREQ=869.45, BW=125, SF=9, CR=5`), so normally only `SYSID` needs to be set.

---

## Operation

- **Joystick UP**: increment selected fox number (wraps 1 → 255 → 1)
- **Joystick DOWN**: decrement selected fox number (wraps 1 → 255 → 1)

Only packets whose `system_id` matches the configured value **and** whose fox
number matches the currently selected fox are shown.  All other packets are
silently discarded.

---

## Display Layout

```
┌────────────────────────┐
│ SYS: MYFOX             │  ← system identifier
│ FOX: 3                 │  ← selected fox number
│                        │
│ -87 dBm                │  ← RSSI (large text, or "---" when no signal)
│                        │
│ ████████░░░░░░░░░░░░░░ │  ← signal bar (-120 dBm=empty, -30 dBm=full)
└────────────────────────┘
```

The displayed RSSI value is held for **300 ms** after the last received packet.
If no packet arrives within that window the value resets to `---` and the bar
clears.

---

## Serial Monitor

```bash
pio device monitor
```

Each accepted packet prints one line:

```
RX fox=3 rssi=-87.50 dBm
```
