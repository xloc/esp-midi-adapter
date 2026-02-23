# ESP MIDI Adapter

USB Host MIDI to BLE MIDI bridge for M5Stack AtomS3.

## Build

```fish
./build.fish build flash monitor
./build.fish fullclean
```

## Connecting

BLE MIDI devices connect from **within the app**, not system Bluetooth settings.

**GarageBand (iOS):** Settings → Advanced → Bluetooth MIDI Devices → tap "ESP-MIDI"
