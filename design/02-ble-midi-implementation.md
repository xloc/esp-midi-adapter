# Slice 02: BLE MIDI Output Implementation Plan

## Goal
Standalone BLE MIDI output. Press button on AtomS3 → send MIDI note over BLE to paired device.

## Current Status

| Step | Status | Notes |
|------|--------|-------|
| Move Slice 1 code to backup | ✅ Done | `main/usb_midi.c.bak` |
| Create sdkconfig.defaults | ✅ Done | NimBLE enabled |
| Create BLE MIDI main.c | ✅ Done | ~220 lines |
| Build successfully | ✅ Done | |
| Device boots without crash | ✅ Done | Fixed stack overflow (2048→4096) |
| Console shows "Advertising started" | ✅ Done | |
| Device visible in GarageBand | ✅ Done | Settings → Advanced → Bluetooth MIDI Devices |
| Connect from GarageBand | ✅ Done | |
| Send MIDI notes | ✅ Done | Button press sends Middle C |

**Slice 02 Complete!**

## Important Notes

### Connecting from iOS
BLE MIDI devices connect through the **app**, not system Bluetooth settings:
1. Open GarageBand
2. Settings → Advanced → Bluetooth MIDI Devices
3. Tap "ESP-MIDI" to connect

### BLE vs Classic Bluetooth Scanning
System Bluetooth settings on iOS/Mac are for Classic Bluetooth and may not show BLE-only peripherals. Use a BLE scanner app (e.g., nRF Connect) for debugging, or connect directly from a MIDI app.

### Console Log Interpretation
The `adv_channel_map=0` in NimBLE logs is misleading - it shows the input parameter value, but NimBLE internally converts 0 to the default channel map (all channels enabled). This is NOT the cause of discovery issues.

## Architecture

```
Button GPIO41 → [AtomS3 BLE] → GarageBand / MIDI App
```

## Files

| File | Purpose |
|------|---------|
| `main/main.c` | BLE MIDI service + button handling (~220 lines) |
| `sdkconfig.defaults` | NimBLE configuration |

## Key Implementation Details

- **MIDI Service UUID**: `03B80E5A-EDE8-4B33-A751-6CE34EC4C700`
- **MIDI Characteristic UUID**: `7772E5DB-3868-4112-A1A9-F2669D106BF3`
- **Device Name**: "ESP-MIDI"
- **Button**: GPIO41 (active low, internal pull-up)
- **MIDI Output**: Note On/Off for Middle C (note 60) on button press/release

## Verification (All Passed)

1. ✅ `./build.fish build` - compiles without errors
2. ✅ `./build.fish flash monitor` - flash and watch logs
3. ✅ Console shows: "Advertising started"
4. ✅ Open GarageBand → Settings → Advanced → Bluetooth MIDI Devices
5. ✅ "ESP-MIDI" appears - tap to connect
6. ✅ Console shows "BLE connected" then "BLE subscribe: notify=1"
7. ✅ Press front button on AtomS3 - note plays in GarageBand
8. ✅ Console shows "Button pressed", "Sent NoteOn"
9. ✅ Release button - "Button released", "Sent NoteOff"

## Reference Files

- ESP-IDF BLE HR example: `/Users/olir/esp/esp-idf/examples/bluetooth/nimble/blehr/`
- ESP-IDF BLE peripheral: `/Users/olir/esp/esp-idf/examples/bluetooth/nimble/bleprph/`

## Next Steps

Proceed to Slice 03: Integration (USB Host MIDI → BLE MIDI bridge)
