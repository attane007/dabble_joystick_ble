#ifndef GATT_SVC_H
#define GATT_SVC_H

#include "host/ble_gatt.h"

// UUID ที่ Dabble ต้องการ
#define DABBLE_SERVICE_UUID 0xFFE0
#define DABBLE_CH_UUID      0xFFE1

int gatt_svc_init(void);

#endif