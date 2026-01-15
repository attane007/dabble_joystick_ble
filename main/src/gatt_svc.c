#include "gatt_svc.h"
#include "host/ble_hs.h"
#include "services/gatt/ble_svc_gatt.h"

static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(DABBLE_SERVICE_UUID),
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = BLE_UUID16_DECLARE(DABBLE_CH_UUID),
                .access_cb = gatt_svr_chr_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE | 
                         BLE_GATT_CHR_F_NOTIFY | BLE_GATT_CHR_F_WRITE_NO_RSP,
            }, {
                0, // No more characteristics in this service
            }
        },
    },
    {
        0, // No more services
    },
};

static int gatt_svr_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg) {
    return 0;
}

int gatt_svc_init(void) {
    ble_svc_gatt_init();
    return ble_gatts_count_cfg(gatt_svr_svcs) || ble_gatts_add_svcs(gatt_svr_svcs);
}