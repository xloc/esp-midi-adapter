# Build Contract: ESP MIDI Adapter

## Goal

Build a USB MIDI to BLE MIDI adapter using ESP32-S3 that lets musicians wirelessly connect their piano keyboard to any BLE MIDI-capable device.

## User Stories

As a user, I can:

1. Pair the adapter with my iPad/Mac/PC via Bluetooth and it appears as a standard BLE MIDI device
2. Play notes on my keyboard and they appear in my DAW/music app with minimal latency (<20ms)
3. See an LED indicator showing connection status (powered/paired/active)

## Definition of Done

- [ ] Firmware compiles with `idf.py build`
- [ ] Can flash to ESP32-S3 dev board with `idf.py flash`
- [ ] USB MIDI keyboard recognized when plugged in
- [ ] BLE MIDI device visible from iPhone/iPad/Mac
- [ ] MIDI notes sent from keyboard appear in GarageBand or similar app
- [ ] Round-trip latency under 20ms

## Constraints

- **MCU:** ESP32-S3 (native USB OTG + BLE)
- **Dev Board:** M5Stack Atom S3
  - Front button (below screen): GPIO41
  - RGB LED: GPIO35 (SK6812 addressable)
- **Framework:** ESP-IDF (not Arduino)
- **USB:** Act as USB Host to receive MIDI from keyboard
- **BLE:** Standard BLE MIDI spec (Apple/MIDI Association)
- **Power:** External 5V (keyboard doesn't provide power)

## Not Doing (v1)

- DIN MIDI (5-pin) support
- MIDI output back to keyboard (one-way only)
- Multi-device pairing
- Configuration UI/app
- Custom PCB (dev board only for now)
- WiFi MIDI / RTP-MIDI
