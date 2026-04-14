# FoxHunter

LoRa-based fox-hunting (radio direction finding) system.

Fox-hunting is a radio sport where participants use directional antennas to locate hidden transmitters ("foxes") by following the signal strength.
This project implements the transmitter and receiver firmware using affordable off-the-shelf hardware and the LoRa radio protocol.

---

## Components

### Beacon (transmitter)
- **Hardware:** Seeed XIAO nRF52840 + SX1262 LoRa module
- **Role:** Periodically broadcasts a short LoRa packet containing a system identifier and a fox number. Multiple beacons can operate simultaneously on the same frequency.
- **Build & documentation:** [beacon/README.md](beacon/README.md)

### Receiver
- **Hardware:** Seeed Wio Tracker L1 (nRF52840 + SX1262 + 1.3" OLED + joystick)
- **Role:** Listens for beacon packets, filters by system identifier, and displays the signal strength (RSSI) of the selected fox on the built-in OLED screen. The player navigates with the joystick to select which fox to measure.
- **Build & documentation:** [receiver/README.md](receiver/README.md)

### Configuration tool
- **Location:** `tools/`
- `configure_beacon.py` — configures a beacon over USB serial
- `configure_receiver.py` — configures a receiver over USB serial

---

## Radio protocol

Both devices use LoRa (SX1262) with matching parameters (default: 869.45 MHz, BW 125 kHz, SF 9, CR 5).
Each beacon packet is 9 bytes: an 8-byte null-padded ASCII system identifier followed by a 1-byte fox number.

---

## Acknowledgements

### MeshCore

The board definition files, linker scripts, and Wio Tracker L1 variant files (pin mappings) in this repository are derived from the [MeshCore](https://github.com/ripplebiz/MeshCore) project.
MeshCore is an open, packet-based LoRa mesh networking firmware — we borrowed their hardware support layer as a solid foundation so we didn't have to reinvent it.

> We love MeshCore ❤️

### MeshCore Hungary

This project is technically supported by the KözKapocs Association, the umbrella organization of Meshcore Hungary community.
🌐 [kozkapocs.hu](https://kozkapocs.hu)
🌐 [mc868.hu](https://mc868.hu)
