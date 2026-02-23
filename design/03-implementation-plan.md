# USB MIDI to BLE MIDI Bridge Implementation Plan

## Overview

Merge USB Host MIDI (Slice 01) with BLE MIDI Output (Slice 02) into a single firmware.

## Current Status

| Task | Status |
|------|--------|
| Merge USB host code | ✅ Done |
| Merge BLE MIDI code | ✅ Done |
| Bridge parse_midi → send_midi_note | ✅ Done |
| Build | ✅ Done |
| Test end-to-end | ✅ Done |

## Files to Merge

| Source | Content |
|--------|---------|
| `main/main.c` (current) | BLE MIDI service, advertising, `send_midi_note()` |
| `main/usb_midi.c.bak` | USB host, MIDI parsing, backlight, blink patterns |

## Key Changes

### `parse_midi()` - Before (Slice 01)
```c
if (cin == 0x09 && velocity > 0) {
    ESP_LOGI(TAG, "Note On: %d, velocity: %d", note, velocity);
    set_backlight(true);
}
```

### `parse_midi()` - After (Slice 03)
```c
if (cin == 0x09 && velocity > 0) {
    ESP_LOGI(TAG, "Note On: %d, velocity: %d", note, velocity);
    set_backlight(true);
    send_midi_note(note, velocity, true);  // Forward to BLE
}
```

### `app_main()` - Combined
```c
void app_main(void) {
    // NVS (required for BLE)
    nvs_flash_init();

    // Backlight
    init_backlight();

    // BLE MIDI
    nimble_port_init();
    gatt_svr_init();
    nimble_port_freertos_init(ble_host_task);

    // USB Host MIDI
    SemaphoreHandle_t usb_ready = xSemaphoreCreateBinary();
    xTaskCreate(blink_task, "blink", 2048, NULL, 1, NULL);
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, usb_ready, 2, NULL, 0);
    xTaskCreatePinnedToCore(midi_task, "midi", 4096, usb_ready, 3, NULL, 0);
}
```

## Removals

- Button task from Slice 02 (was only for testing BLE output)
