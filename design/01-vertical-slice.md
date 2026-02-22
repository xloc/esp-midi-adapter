# Slice 01: USB Host MIDI Input

## Goal

Configure ESP32-S3 as USB Host, receive MIDI from a keyboard, print to console, blink LED.

## Scope

1. **USB Host** - ESP32-S3 recognizes a USB MIDI device
2. **MIDI Parse** - Extract Note On/Off messages
3. **Output** - Print note info to serial console
4. **Feedback** - Blink LED on each note received

## Tasks

### 1. Dev Environment Setup
- [ ] ESP-IDF installed and working
- [ ] Project scaffold with CMakeLists.txt
- [ ] `idf.py build` succeeds (empty main)
- [ ] Can flash to ESP32-S3 dev board

### 2. USB Host MIDI
- [ ] Initialize USB Host stack
- [ ] Detect when MIDI device is plugged in
- [ ] Read raw USB packets from MIDI device
- [ ] Parse Note On/Off from USB MIDI packet
- [ ] Log note number and velocity to serial console

### 3. LED Feedback
- [ ] Configure RGB LED via GPIO35 (SK6812 addressable LED, needs RMT driver)
- [ ] Blink LED when note received

## Acceptance Test

1. Flash firmware to ESP32-S3
2. Connect USB MIDI keyboard to ESP32-S3 USB port
3. Open serial monitor
4. Press key on keyboard
5. See note info printed (e.g., "Note On: 60, velocity: 100")
6. LED blinks

## Notes

- No BLE yet - that's slice 02
- Just Note On/Off for now, ignore CC, pitch bend, etc.
- Hard-code everything possible
- Hardware: M5Stack Atom S3
  - Front button (below screen): GPIO41
  - RGB LED: GPIO35 (SK6812 addressable)
