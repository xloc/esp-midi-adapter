#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#define BUTTON_GPIO 41

static const char *TAG = "BLE_MIDI";

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

// Forward declarations
static int gap_event_cb(struct ble_gap_event *event, void *arg);
static void ble_advertise(void);

// GATT access callback (minimal - reads return empty, writes ignored)
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

static void button_task(void *arg) {
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&cfg);

    bool last_pressed = false;

    while (1) {
        bool pressed = !gpio_get_level(BUTTON_GPIO);  // Active low

        if (pressed != last_pressed) {
            vTaskDelay(pdMS_TO_TICKS(50));  // Debounce
            pressed = !gpio_get_level(BUTTON_GPIO);

            if (pressed != last_pressed) {
                last_pressed = pressed;
                if (pressed) {
                    ESP_LOGI(TAG, "Button pressed");
                    send_midi_note(60, 100, true);   // Middle C
                } else {
                    ESP_LOGI(TAG, "Button released");
                    send_midi_note(60, 0, false);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "ESP MIDI Adapter - BLE MIDI");

    // Initialize NVS (required for BLE)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE
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

    // Start button task
    xTaskCreate(button_task, "button", 4096, NULL, 1, NULL);
}
