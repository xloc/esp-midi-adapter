# USB Host MIDI + Backlight Feedback Implementation Plan

## Overview

Implement USB Host MIDI input with LCD backlight feedback on M5Stack AtomS3 (ESP32-S3, ESP-IDF v5.2).

## Hardware Findings

**M5Stack AtomS3 (with 0.85" LCD screen):**
- **NO RGB LED** - the AtomS3 has pads for WS2812 but they are not populated
- **LCD Backlight**: GPIO16 (simple GPIO on/off, tested and working)
- **Front Button**: GPIO41
- **USB-C Port**: Can be configured as USB Host (OTG)

## Current Status

| Task | Status |
|------|--------|
| Dev environment (ESP-IDF v5.2) | ✅ Working |
| Project scaffold | ✅ Working |
| Build system | ✅ Working (`./build.fish build`) |
| LCD Backlight control | ✅ Tested and working |
| USB Host MIDI | ✅ Working |

## Files

| File | Status |
|------|--------|
| `main/main.c` | Created - USB host + MIDI + backlight |
| `main/led_encoder.c` | Created but NOT NEEDED (no RGB LED) |
| `main/led_encoder.h` | Created but NOT NEEDED |
| `main/CMakeLists.txt` | Updated |
| `build.fish` | Created - build helper script |

## Architecture

```
app_main()
  ├── init_backlight()     # GPIO16 output
  ├── usb_host_task        # Daemon: USB library events
  └── midi_task            # Client: MIDI device handling
```

## Implementation

### Backlight Control (Working)
```c
#define BACKLIGHT_GPIO 16

static void init_backlight(void) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BACKLIGHT_GPIO),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&cfg);
}

static void set_backlight(bool on) {
    gpio_set_level(BACKLIGHT_GPIO, on ? 1 : 0);
}
```

### USB Host MIDI (Working)

**Implementation:**
1. `usb_host_install()` - installs USB host library
2. `usb_host_client_register()` - registers MIDI client
3. Wait for `USB_HOST_CLIENT_EVENT_NEW_DEV`
4. Open device, get config descriptor
5. Find MIDI interface (class 0x01, subclass 0x03)
6. Find bulk IN endpoint
7. Claim interface, allocate transfer, submit

### MIDI Parsing
USB MIDI packet = 4 bytes:
- Byte 0: Cable | CIN (0x09=NoteOn, 0x08=NoteOff)
- Byte 1: Status
- Byte 2: Note number
- Byte 3: Velocity

## Next Steps

Slice 01 complete. Proceed to Slice 02 (BLE MIDI output).

## Reference Files

- `/Users/olir/esp/esp-idf/examples/peripherals/usb/host/usb_host_lib/main/class_driver.c`
- `/Users/olir/esp/esp-idf/examples/peripherals/usb/host/usb_host_lib/main/usb_host_lib_main.c`
- `/Users/olir/esp/esp-idf/components/usb/include/usb/usb_host.h`

## Verification

1. Build: `./build.fish build`
2. Flash: `./build.fish flash monitor`
3. Connect USB MIDI keyboard to AtomS3 USB-C port
4. Check serial output for device detection messages
5. Press keys - should see "Note On: XX, velocity: XX"
6. Backlight should turn on/off with notes

## Document Updates Needed

Update `design/01-vertical-slice.md`:
- Change "RGB LED: GPIO35" to "LCD Backlight: GPIO16"
- Note that AtomS3 has no RGB LED (only AtomS3 Lite has one)
