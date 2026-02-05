#include <stdio.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* NimBLE Headers */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

/* อ้างอิงไฟล์จากตัวอย่างเดิม */
#include "common.h"
#include "gap.h"
#include "led.h"
#include "gatt_svc.h"
#include "motors.h"

// ลบ TAG ออกถ้าใน common.h มีประกาศไว้แล้ว เพื่อเลี่ยง warning redefined
// #define TAG "DABBLE_ESP32" 

/* --- 1. Function Prototypes (ประกาศไว้ก่อนใช้งาน) --- */
void ble_store_config_init(void);
static int gatt_svr_dabble_access(uint16_t conn_handle, uint16_t attr_handle,
                                  struct ble_gatt_access_ctxt *ctxt, void *arg);

/* --- 2. Dabble UUID Definitions --- */
static const ble_uuid128_t dabble_svc_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xE0, 0xFF, 0x00, 0x00}
};

static const ble_uuid128_t dabble_chr_uuid = {
    .u.type = BLE_UUID_TYPE_128,
    .value = {0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xE1, 0xFF, 0x00, 0x00}
};

/* --- 3. GATT Service Definition (ประกาศแค่ครั้งเดียว) --- */
static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &dabble_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) { {
            .uuid = &dabble_chr_uuid.u,
            .access_cb = gatt_svr_dabble_access,
            .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_NOTIFY,
        }, {
            0, /* No more characteristics */
        } },
    },
    {
        0, /* No more services */
    },
};

static uint16_t prev_btn_group = 0; 
static int8_t prev_axis_x = 0;
static int8_t prev_axis_y = 0;

#define JOYSTICK_DEADBAND 6

static int16_t clamp_i16(int16_t value, int16_t min_value, int16_t max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

static int16_t axis_to_duty(int16_t axis) {
    axis = clamp_i16(axis, -127, 127);
    return (int16_t)((axis * MOTORS_MAX_DUTY) / 127);
}

static void apply_joystick(int8_t axis_x, int8_t axis_y) {
    int16_t x = axis_x;
    int16_t y = axis_y;

    if (x > -JOYSTICK_DEADBAND && x < JOYSTICK_DEADBAND) {
        x = 0;
    }
    if (y > -JOYSTICK_DEADBAND && y < JOYSTICK_DEADBAND) {
        y = 0;
    }

    int16_t left = clamp_i16(y + x, -127, 127);
    int16_t right = clamp_i16(y - x, -127, 127);

    int16_t left_duty = axis_to_duty(left);
    int16_t right_duty = axis_to_duty(right);

    motors_set_speed(0, left_duty);
    motors_set_speed(1, left_duty);
    motors_set_speed(2, right_duty);
    motors_set_speed(3, right_duty);
}

static int gatt_svr_dabble_access(uint16_t conn_handle, uint16_t attr_handle,
                                 struct ble_gatt_access_ctxt *ctxt, void *arg) 
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint8_t data[64];
        uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
        os_mbuf_copydata(ctxt->om, 0, len, data);

        // Debug Raw Data
        printf("Dabble RX [%d bytes]: ", len);
        for(int i=0; i<len; i++) printf("%02X ", data[i]);
        printf("\n");

        // ตรวจสอบ Header สำหรับ Gamepad (Module 02)
        if (len >= 8 && data[0] == 0xFF && data[4] == 0x02) {
            
            // --- 1. ส่วนของปุ่มกด (Start, Select, Triangle, Circle, Cross, Square) ---
            uint8_t current_btns = data[5];
            if (current_btns != prev_btn_group) {
                uint8_t pressed = current_btns & ~prev_btn_group;
                uint8_t released = ~current_btns & prev_btn_group;

                if (pressed) {
                    if (pressed & 0x01) ESP_LOGI(TAG, "Button: START (Pressed)");
                    if (pressed & 0x02) ESP_LOGI(TAG, "Button: SELECT (Pressed)");
                    if (pressed & 0x04) ESP_LOGI(TAG, "Button: TRIANGLE (Pressed)");
                    if (pressed & 0x08) ESP_LOGI(TAG, "Button: CIRCLE (Pressed)");
                    if (pressed & 0x10) ESP_LOGI(TAG, "Button: CROSS (Pressed)");
                    if (pressed & 0x20) ESP_LOGI(TAG, "Button: SQUARE (Pressed)");
                }
                if (released) {
                    if (released & 0x01) ESP_LOGI(TAG, "Button: START (Released)");
                    if (released & 0x02) ESP_LOGI(TAG, "Button: SELECT (Released)");
                    // ... เพิ่ม Released ของปุ่มอื่นๆ ตามต้องการ
                }
                prev_btn_group = current_btns;
            }

            // --- 2. ส่วนของแกนควบคุม (Digital Arrows / Joystick / Accelerometer) ---
            int8_t axis_x = (int8_t)data[6];
            int8_t axis_y = (int8_t)data[7];

            if (axis_x != prev_axis_x || axis_y != prev_axis_y) {
                // ถ้าเป็นโหมด Digital (ค่าจะเป็น 1, 2, 4, 8)
                if (len == 8 && axis_y == 0) { 
                    int8_t mapped_x = 0;
                    int8_t mapped_y = 0;
                    if (axis_x == 0x01) ESP_LOGI(TAG, "Direction: UP");
                    else if (axis_x == 0x02) ESP_LOGI(TAG, "Direction: DOWN");
                    else if (axis_x == 0x04) ESP_LOGI(TAG, "Direction: LEFT");
                    else if (axis_x == 0x08) ESP_LOGI(TAG, "Direction: RIGHT");
                    else if (axis_x == 0x00) ESP_LOGI(TAG, "Direction: CENTER/RELEASED");

                    if (axis_x == 0x01) mapped_y = 127;
                    else if (axis_x == 0x02) mapped_y = -127;
                    else if (axis_x == 0x04) mapped_x = -127;
                    else if (axis_x == 0x08) mapped_x = 127;

                    apply_joystick(mapped_x, mapped_y);
                } 
                // ถ้าเป็นโหมด Joystick หรือ Accelerometer (ค่าจะแปรผันต่อเนื่อง)
                else {
                    ESP_LOGI(TAG, "Control Axis -> X: %d, Y: %d", axis_x, axis_y);
                    apply_joystick(axis_x, axis_y);
                }

                prev_axis_x = axis_x;
                prev_axis_y = axis_y;
            }
        }
    }
    return 0;
}
/* --- 5. NimBLE Stack Event Handlers --- */
static void on_stack_reset(int reason) {
    ESP_LOGI(TAG, "NimBLE stack reset, reason: %d", reason);
}

static void on_stack_sync(void) {
    adv_init();
}

static void nimble_host_task(void *param) {
    ESP_LOGI(TAG, "NimBLE host task started!");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* --- 6. Main Application --- */
void app_main(void) {
    esp_err_t ret;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    led_init();

    ret = motors_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Motor init failed: %d", ret);
    } else {
        motors_enable(true);
        motors_stop_all();
    }

    ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to init NimBLE: %d", ret);
        return;
    }

    /* Initialize GATT Server */
    ble_svc_gatt_init();
    ble_gatts_count_cfg(gatt_svr_svcs);
    ble_gatts_add_svcs(gatt_svr_svcs);

#if CONFIG_BT_NIMBLE_GAP_SERVICE
    gap_init();
#endif

    ble_hs_cfg.reset_cb = on_stack_reset;
    ble_hs_cfg.sync_cb = on_stack_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    nimble_port_freertos_init(nimble_host_task);
}