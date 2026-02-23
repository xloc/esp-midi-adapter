#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "usb/usb_host.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BACKLIGHT_GPIO 16

static const char *TAG = "MIDI";

// Blink count: 1=init, 2=waiting, 3=device found, 0=MIDI ready
static volatile int blink_count = 1;

// BLE MIDI Service: 03B80E5A-EDE8-4B33-A751-6CE34EC4C700 (little-endian)
static const ble_uuid128_t midi_svc_uuid =
    BLE_UUID128_INIT(0x00, 0xC7, 0xC4, 0x4E, 0xE3, 0x6C, 0x51, 0xA7,
                     0x33, 0x4B, 0xE8, 0xED, 0x5A, 0x0E, 0xB8, 0x03);

// MIDI Characteristic: 7772E5DB-3868-4112-A1A9-F2669D106BF3
static const ble_uuid128_t midi_chr_uuid =
    BLE_UUID128_INIT(0xF3, 0x6B, 0x10, 0x9D, 0x66, 0xF2, 0xA9, 0xA1,
                     0x12, 0x41, 0x68, 0x38, 0xDB, 0xE5, 0x72, 0x77);

// BLE state
static uint16_t midi_chr_handle;
static uint16_t ble_conn_handle;
static bool ble_connected;
static bool ble_subscribed;
static uint8_t ble_addr_type;

// USB MIDI state
typedef struct {
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t dev_hdl;
    usb_transfer_t *transfer;
    uint8_t dev_addr;
    uint8_t ep_addr;
    bool connected;
} usb_midi_state_t;

static usb_midi_state_t usb_midi;

// Forward declarations
static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_advertise(void);

// ----------------------------------------------------------------------------
// Backlight
// ----------------------------------------------------------------------------

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

static void blink_pattern(int count) {
    for (int i = 0; i < count; i++) {
        set_backlight(true);
        vTaskDelay(pdMS_TO_TICKS(100));
        set_backlight(false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelay(pdMS_TO_TICKS(600));
}

static void blink_task(void *arg) {
    while (1) {
        if (blink_count > 0) {
            blink_pattern(blink_count);
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }
    }
}

// ----------------------------------------------------------------------------
// BLE MIDI
// ----------------------------------------------------------------------------

static int midi_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                           struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

// GATT service definition
static const struct ble_gatt_svc_def midi_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &midi_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &midi_chr_uuid.u,
                .access_cb = midi_chr_access,
                .val_handle = &midi_chr_handle,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

static int gatt_svr_init(void) {
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_gatts_count_cfg(midi_svcs);
    ble_gatts_add_svcs(midi_svcs);
    return 0;
}

static void ble_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Advertising data: flags + MIDI service UUID
    memset(&fields, 0, sizeof(fields));
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.uuids128 = &midi_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
        return;
    }

    // Scan response: device name
    memset(&rsp_fields, 0, sizeof(rsp_fields));
    rsp_fields.name = (uint8_t *)"ESP-MIDI";
    rsp_fields.name_len = 8;
    rsp_fields.name_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_rsp_set_fields failed: %d", rc);
        return;
    }

    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(ble_addr_type, NULL, BLE_HS_FOREVER, &adv_params, gap_event_cb, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Advertising started");
}

static int gap_event_cb(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status == 0) {
            ble_conn_handle = event->connect.conn_handle;
            ble_connected = true;
            ESP_LOGI(TAG, "BLE connected");
        } else {
            ESP_LOGI(TAG, "BLE connection failed, restarting advertising");
            ble_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ble_connected = false;
        ble_subscribed = false;
        ESP_LOGI(TAG, "BLE disconnected, restarting advertising");
        ble_advertise();
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ble_subscribed = event->subscribe.cur_notify;
        ESP_LOGI(TAG, "BLE subscribe: notify=%d", ble_subscribed);
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ESP_LOGI(TAG, "Advertising complete, restarting");
        ble_advertise();
        break;
    }
    return 0;
}

static void ble_on_sync(void) {
    int rc = ble_hs_id_infer_auto(0, &ble_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "ble_hs_id_infer_auto failed: %d", rc);
        return;
    }

    uint8_t addr[6] = {0};
    ble_hs_id_copy_addr(ble_addr_type, addr, NULL);
    ESP_LOGI(TAG, "Device address: %02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    ble_advertise();
}

static void ble_on_reset(int reason) {
    ESP_LOGE(TAG, "BLE reset, reason=%d", reason);
}

static void ble_host_task(void *arg) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void send_midi_note(uint8_t note, uint8_t velocity, bool on) {
    if (!ble_subscribed) {
        ESP_LOGW(TAG, "Not subscribed, skipping note");
        return;
    }

    uint8_t packet[5] = {
        0x80, 0x80,                    // Timestamp (0)
        on ? 0x90 : 0x80,              // Note On/Off channel 0
        note, velocity
    };

    struct os_mbuf *om = ble_hs_mbuf_from_flat(packet, sizeof(packet));
    int rc = ble_gatts_notify_custom(ble_conn_handle, midi_chr_handle, om);
    ESP_LOGI(TAG, "Sent %s note=%d vel=%d rc=%d", on ? "NoteOn" : "NoteOff", note, velocity, rc);
}

// ----------------------------------------------------------------------------
// USB Host MIDI
// ----------------------------------------------------------------------------

static void parse_midi(const uint8_t *data, int len) {
    for (int i = 0; i + 3 < len; i += 4) {
        uint8_t cin = data[i] & 0x0F;
        uint8_t note = data[i + 2];
        uint8_t velocity = data[i + 3];

        if (cin == 0x09 && velocity > 0) {
            ESP_LOGI(TAG, "Note On: %d, velocity: %d", note, velocity);
            set_backlight(true);
            send_midi_note(note, velocity, true);
        } else if (cin == 0x08 || (cin == 0x09 && velocity == 0)) {
            ESP_LOGI(TAG, "Note Off: %d", note);
            set_backlight(false);
            send_midi_note(note, 0, false);
        }
    }
}

static void transfer_cb(usb_transfer_t *transfer) {
    if (transfer->status == USB_TRANSFER_STATUS_COMPLETED && transfer->actual_num_bytes > 0) {
        parse_midi(transfer->data_buffer, transfer->actual_num_bytes);
    }
    if (usb_midi.connected) {
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

            if (intf->bInterfaceClass == 0x01 &&
                intf->bInterfaceSubClass == 0x03 &&
                intf->bNumEndpoints > 0) {
                in_midi_interface = true;
                current_intf_num = intf->bInterfaceNumber;
                current_alt = intf->bAlternateSetting;
            }
        } else if (type == USB_B_DESCRIPTOR_TYPE_ENDPOINT && len >= 7 && in_midi_interface) {
            const usb_ep_desc_t *ep = (const usb_ep_desc_t *)p;
            if (ep->bEndpointAddress & 0x80) {
                *intf_num = current_intf_num;
                *alt_setting = current_alt;
                *ep_addr = ep->bEndpointAddress;
                return true;
            }
        }

        p += len;
    }

    return false;
}

static void client_event_cb(const usb_host_client_event_msg_t *msg, void *arg) {
    usb_midi_state_t *state = (usb_midi_state_t *)arg;
    if (msg->event == USB_HOST_CLIENT_EVENT_NEW_DEV) {
        state->dev_addr = msg->new_dev.address;
        blink_count = 3;
    } else if (msg->event == USB_HOST_CLIENT_EVENT_DEV_GONE) {
        state->connected = false;
        blink_count = 2;
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
            .callback_arg = &usb_midi,
        },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_cfg, &usb_midi.client_hdl));
    ESP_LOGI(TAG, "USB client registered, waiting for MIDI device...");

    while (1) {
        usb_host_client_handle_events(usb_midi.client_hdl, portMAX_DELAY);

        if (usb_midi.dev_addr && !usb_midi.connected) {
            if (usb_host_device_open(usb_midi.client_hdl, usb_midi.dev_addr, &usb_midi.dev_hdl) != ESP_OK) {
                usb_midi.dev_addr = 0;
                continue;
            }

            const usb_config_desc_t *config_desc;
            if (usb_host_get_active_config_descriptor(usb_midi.dev_hdl, &config_desc) != ESP_OK) {
                usb_host_device_close(usb_midi.client_hdl, usb_midi.dev_hdl);
                usb_midi.dev_addr = 0;
                continue;
            }

            uint8_t intf_num, alt_setting;
            if (!find_midi_endpoint(config_desc, &intf_num, &alt_setting, &usb_midi.ep_addr)) {
                ESP_LOGW(TAG, "Not a MIDI device");
                usb_host_device_close(usb_midi.client_hdl, usb_midi.dev_hdl);
                usb_midi.dev_addr = 0;
                continue;
            }

            if (usb_host_interface_claim(usb_midi.client_hdl, usb_midi.dev_hdl, intf_num, alt_setting) != ESP_OK) {
                usb_host_device_close(usb_midi.client_hdl, usb_midi.dev_hdl);
                usb_midi.dev_addr = 0;
                continue;
            }

            ESP_ERROR_CHECK(usb_host_transfer_alloc(64, 0, &usb_midi.transfer));
            usb_midi.transfer->device_handle = usb_midi.dev_hdl;
            usb_midi.transfer->bEndpointAddress = usb_midi.ep_addr;
            usb_midi.transfer->callback = transfer_cb;
            usb_midi.transfer->num_bytes = 64;

            usb_midi.connected = true;
            blink_count = 0;
            ESP_LOGI(TAG, "MIDI device connected!");

            usb_host_transfer_submit(usb_midi.transfer);
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
    blink_count = 2;

    xSemaphoreGive(ready);

    while (1) {
        uint32_t event_flags;
        usb_host_lib_handle_events(portMAX_DELAY, &event_flags);
    }
}

// ----------------------------------------------------------------------------
// Main
// ----------------------------------------------------------------------------

void app_main(void) {
    ESP_LOGI(TAG, "ESP MIDI Adapter - USB to BLE Bridge");

    // NVS (required for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Backlight
    init_backlight();

    // BLE MIDI
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return;
    }
    
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.reset_cb = ble_on_reset;

    // Initialize GATT services
    gatt_svr_init();
    ble_svc_gap_device_name_set("ESP-MIDI");

    // Start BLE host task
    nimble_port_freertos_init(ble_host_task);

    // USB Host MIDI
    SemaphoreHandle_t usb_ready = xSemaphoreCreateBinary();
    xTaskCreate(blink_task, "blink", 2048, NULL, 1, NULL);
    xTaskCreatePinnedToCore(usb_host_task, "usb_host", 4096, usb_ready, 2, NULL, 0);
    xTaskCreatePinnedToCore(midi_task, "midi", 4096, usb_ready, 3, NULL, 0);
}
