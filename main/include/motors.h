#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MOTORS_COUNT 4
#define MOTORS_MAX_DUTY 1023

esp_err_t motors_init(void);
void motors_enable(bool enable);
void motors_stop_all(void);
esp_err_t motors_set_speed(uint8_t motor_id, int16_t speed);

#ifdef __cplusplus
}
#endif
