# Slice 02: BLE MIDI Output

## Goal

Advertise as BLE MIDI device, pair with phone/computer, send dummy note on button press.

## Scope

1. **BLE Advertising** - Device appears as "ESP-MIDI" in Bluetooth settings
2. **Pairing** - Can connect from iPhone/iPad/Mac/PC
3. **Send Note** - Press button on devboard → MIDI note sent over BLE
4. **Feedback** - LED indicates connection status

## Tasks

### 1. BLE MIDI Service
- [ ] Initialize BLE stack
- [ ] Configure BLE MIDI service UUID (03B80E5A-EDE8-4B33-A751-6CE34EC4C700)
- [ ] Configure MIDI characteristic (7772E5DB-3868-4112-A1A9-F2669D106BF3)
- [ ] Advertise device name "ESP-MIDI"

### 2. Connection Management
- [ ] Handle pairing request
- [ ] Track connection state (disconnected/connected)
- [ ] RGB LED via GPIO35 (SK6812 addressable LED, needs RMT driver)
- [ ] LED red = disconnected, LED green = connected

### 3. Button Input
- [ ] Configure GPIO41 for front button (below the screen on Atom S3)
- [ ] Detect button press

### 4. Send MIDI Note
- [ ] Format BLE MIDI packet (timestamp + Note On)
- [ ] Send hardcoded note (e.g., middle C, velocity 100) on button press
- [ ] Send Note Off on button release (or after short delay)

## Acceptance Test

1. Flash firmware to ESP32-S3
2. Open Bluetooth settings on iPhone/Mac
3. See "ESP-MIDI" in device list
4. Pair with device
5. LED turns on
6. Open GarageBand or MIDI monitor app
7. Press the front button (below screen) on devboard
8. Note appears in app

## Notes

- No USB yet - that's the integration slice (03)
- Single connection only
- Hardcoded note values
- Hardware: M5Stack Atom S3
  - Front button (below screen): GPIO41
  - RGB LED: GPIO35 (SK6812 addressable)
