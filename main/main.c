#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"
#include "usb/usb_helpers.h"

#define BACKLIGHT_GPIO 16

static const char *TAG = "MIDI";

// Blink count: 1=init, 2=waiting, 3=device found, 4=not MIDI, 0=MIDI ready
static volatile int blink_count = 1;  // init

// Backlight
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

// Blink N times, then pause
static void blink_pattern(int count) {
    for (int i = 0; i < count; i++) {
        set_backlight(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_backlight(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(600));  // pause between patterns
}

static void blink_task(void *arg) {
    while (1) {
        if (blink_count > 0) {
            blink_pattern(blink_count);
        } else {
            // MIDI ready - let parse_midi control backlight
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// USB MIDI
typedef struct {
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t dev_hdl;
    usb_transfer_t *transfer;
    uint8_t dev_addr;
    uint8_t ep_addr;
    bool connected;
} midi_state_t;

static midi_state_t midi;

static void parse_midi(const uint8_t *data, int len) {
    for (int i = 0; i + 3 < len; i += 4) {
        uint8_t cin = data[i] & 0x0F;
        uint8_t note = data[i + 2];
        uint8_t velocity = data[i + 3];

        if (cin == 0x09 && velocity > 0) {
            ESP_LOGI(TAG, "Note On: %d, velocity: %d", note, velocity);
            set_backlight(true);
        } else if (cin == 0x08 || (cin == 0x09 && velocity == 0)) {
            ESP_LOGI(TAG, "Note Off: %d", note);
            set_backlight(false);
        }
    }
}

static void transfer_cb(usb_transfer_t *transfer) {
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes > 0) {
        parse_midi(transfer->data_buffer, transfer->actual_num_bytes);
    }
    // Re-submit for continuous reading
    if (midi.connected) {
        usb_host_transfer_submit(transfer);
    }
}

static bool find_midi_endpoint(const usb_config_desc_t *config_desc, uint8_t *intf_num, uint8_t *alt_setting, uint8_t *ep_addr) {
    const uint8_t *p = (const uint8_t *)config_desc;
    const uint8_t *end = p + config_desc->wTotalLength;
    bool in_midi_interface = false;
    uint8_t current_intf_num = 0;
    uint8_t current_alt = 0;

    while (p < end && p[0] > 0) {
        uint8_t len = p[0];
        uint8_t type = p[1];

        if (type == USB_B_DESCRIPTOR_TYPE_INTERFACE && len >= 9) {
            const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;
            in_midi_interface = false;

            // Audio class (0x01), MIDI Streaming subclass (0x03), with endpoints
            if (intf->bInterfaceClass == 0x01 &&
                intf->bInterfaceSubClass == 0x03 &&
                intf->bNumEndpoints > 0) {
                in_midi_interface = true;
                current_intf_num = intf->bInterfaceNumber;
                current_alt = intf->bAlternateSetting;
            }
        } else if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= 7 && in_midi_interface) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;
            if (ep->bEndpointAddress & 0x80) {  // IN endpoint
                *intf_num = current_intf_num;
                *alt_setting = current_alt;
                *ep_addr = ep->bEndpointAddress;
                return true;
            }
        }

        p += len;
    }

    blink_count = 4;  // not MIDI
    return false;
}

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *arg) {
    midi_state_t *state = (midi_state_t *)arg;
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        state->dev_addr = msg->new_dev.address;
        blink_count = 3;  // device found
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        state->connected = false;
        blink_count = 2;  // waiting
    }
}

static void midi_task(void *arg) {
    SemaphoreHandle_t ready = (SemaphoreHandle_t)arg;
    xSemaphoreTake(ready, portMAX_DELAY);

    usb_host_client_config_t client_cfg = {
        .is_synchronous = false,
        .max_num_event_msg = 5,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = &midi,
        },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_cfg, &midi.client_hdl));
    ESP_LOGI(TAG, "USB client registered, waiting for MIDI device...");

    while (1) {
        usb_host_client_handle_events(midi.client_hdl, portMAX_DELAY);

        if (midi.dev_addr && !midi.connected) {
            // Open device
            if (usb_host_device_open(midi.client_hdl, midi.dev_addr, &midi.dev_hdl) != ESP_OK) {
                midi.dev_addr = 0;
                continue;
            }

            // Get config descriptor
            const usb_config_desc_t *config_desc;
            if (usb_host_get_active_config_descriptor(midi.dev_hdl, &config_desc) != ESP_OK) {
                usb_host_device_close(midi.client_hdl, midi.dev_hdl);
                midi.dev_addr = 0;
                continue;
            }

            // Find MIDI interface and endpoint
            uint8_t intf_num, alt_setting;
            if (!find_midi_endpoint(config_desc, &intf_num, &alt_setting, &midi.ep_addr)) {
                ESP_LOGW(TAG, "Not a MIDI device");
                // debug_state already set by find_midi_endpoint
                usb_host_device_close(midi.client_hdl, midi.dev_hdl);
                midi.dev_addr = 0;
                continue;
            }

            // Claim interface with the correct alternate setting
            if (usb_host_interface_claim(midi.client_hdl, midi.dev_hdl, intf_num, alt_setting) != ESP_OK) {
                usb_host_device_close(midi.client_hdl, midi.dev_hdl);
                midi.dev_addr = 0;
                continue;
            }

            // Allocate transfer
            ESP_ERROR_CHECK(usb_host_transfer_alloc(64, 0, &midi.transfer));
            midi.transfer->device_handle = midi.dev_hdl;
            midi.transfer->bEndpointAddress = midi.ep_addr;
            midi.transfer->callback = transfer_cb;
            midi.transfer->num_bytes = 64;

            midi.connected = true;
            blink_count = 0;  // MIDI ready
            ESP_LOGI(TAG, "MIDI device connected!");

            // Start reading
            usb_host_transfer_submit(midi.transfer);
        }
    }
}

static void usb_host_task(void *arg) {
    SemaphoreHandle_t ready = (SemaphoreHandle_t)arg;

    usb_host_config_t host_cfg = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_cfg));
    ESP_LOGI(TAG, "USB Host installed");
    blink_count = 2;  // waiting

    xSemaphoreGive(ready);

    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP MIDI Adapter starting");

    init_backlight();

    SemaphoreHandle_t usb_ready = xSemaphoreCreateBinary();

    xTaskCreate(blink_task, "blink", 2048, NULL, 1, NULL);
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, usb_ready, 2, NULL, 0);
    xTaskCreatePinnedToCore(midi_task, "midi", 4096, usb_ready, 3, NULL, 0);
}
