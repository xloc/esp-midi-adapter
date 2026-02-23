# Slice 03: USB MIDI to BLE MIDI Bridge

## Goal

Receive MIDI from USB keyboard, forward to connected BLE MIDI device. Complete the bridge.

## Scope

1. **USB Host MIDI** - Receive Note On/Off from USB keyboard (from Slice 01)
2. **BLE MIDI Output** - Send notes to paired device (from Slice 02)
3. **Bridge** - Forward USB MIDI events to BLE MIDI
4. **Feedback** - LCD backlight indicates MIDI activity

## Architecture

```
USB MIDI Keyboard → [USB Host] → parse_midi() → send_midi_note() → [BLE] → GarageBand
                                      ↓
                              LCD backlight flash
```

## Acceptance Test

1. Flash firmware to AtomS3
2. Connect USB MIDI keyboard to AtomS3 USB-C port
3. Open serial monitor, verify "USB Host installed", "Advertising started"
4. Connect from GarageBand (Settings → Advanced → Bluetooth MIDI Devices → "ESP-MIDI")
5. Verify "BLE connected" and "BLE subscribe: notify=1" in console
6. Press key on USB keyboard
7. Note plays in GarageBand
8. LCD backlight flashes on note

## Notes

- Only forwarding Note On/Off for now
- Single BLE connection only

## Future

- Support Control Change (CC) messages (mod wheel, sustain pedal, volume, etc.)
- Support pitch bend
