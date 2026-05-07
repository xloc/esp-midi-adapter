# ESP MIDI Adapter

USB Host MIDI to BLE MIDI bridge for M5Stack AtomS3.

## Build

```fish
# setup esp-idf
uv python install python3.12
~/.espressif/v6.0/esp-idf/install.fish

# build
. ~/.espressif/v6.0/esp-idf/export.fish
idf.py set-target esp32s3
idf.py build flash monitor
idf.py fullclean
```

## Connecting

BLE MIDI devices connect from **within the app**, not system Bluetooth settings.

**GarageBand (iOS):** Settings → Advanced → Bluetooth MIDI Devices → tap "ESP-MIDI"
