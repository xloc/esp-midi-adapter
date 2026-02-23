# Slice 01: USB Host MIDI Input

## Goal

Configure ESP32-S3 as USB Host, receive MIDI from a keyboard, print to console, toggle LCD backlight.

## Scope

1. **USB Host** - ESP32-S3 recognizes a USB MIDI device
2. **MIDI Parse** - Extract Note On/Off messages
3. **Output** - Print note info to serial console
4. **Feedback** - Toggle LCD backlight on each note received

## Tasks

### 1. Dev Environment Setup
- [x] ESP-IDF v5.2 installed and working
- [x] Project scaffold with CMakeLists.txt
- [x] `idf.py build` succeeds
- [x] Can flash to AtomS3 dev board

### 2. USB Host MIDI
- [x] Initialize USB Host stack
- [x] Detect when MIDI device is plugged in
- [x] Read raw USB packets from MIDI device
- [x] Parse Note On/Off from USB MIDI packet
- [x] Log note number and velocity to serial console

### 3. Backlight Feedback
- [x] Configure LCD backlight via GPIO16 (simple GPIO output)
- [x] Toggle backlight when note received

## Acceptance Test

1. Flash firmware to AtomS3
2. Connect USB MIDI keyboard to AtomS3 USB-C port
3. Open serial monitor
4. Press key on keyboard
5. See note info printed (e.g., "Note On: 60, velocity: 100")
6. LCD backlight toggles on/off

## Notes

- No BLE yet - that's slice 02
- Just Note On/Off for now, ignore CC, pitch bend, etc.
- Hard-code everything possible
- Hardware: M5Stack AtomS3 (with 0.85" LCD)
  - Front button: GPIO41
  - LCD Backlight: GPIO16 (simple GPIO, no RGB LED on this model)
  - USB-C: Can be configured as USB Host (OTG)
