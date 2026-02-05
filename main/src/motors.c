#include "motors.h"

#include <string.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

static const char *TAG = "MOTORS";

#define MOTOR_PWM_FREQ_HZ 20000
#define MOTOR_PWM_RESOLUTION LEDC_TIMER_10_BIT
#define MOTOR_SPEED_MODE LEDC_LOW_SPEED_MODE
#define MOTOR_TIMER LEDC_TIMER_0

#define MOTOR_STBY_GPIO 7 // พิน STBY สำหรับ TB6612FNG ทั้ง 2 ตัว (ใช้ร่วมกัน)

static const int motor_pwm_gpio[MOTORS_COUNT] = {
    4, // Motor 0 PWM pin (TB6612 #1 ซ้าย - PWMA/B ร่วมกัน)
    4, // Motor 1 PWM pin (TB6612 #1 ซ้าย - PWMA/B ร่วมกัน)
    8, // Motor 2 PWM pin (TB6612 #2 ขวา - PWMA/B ร่วมกัน)
    8  // Motor 3 PWM pin (TB6612 #2 ขวา - PWMA/B ร่วมกัน)
};

static const int motor_in1_gpio[MOTORS_COUNT] = {
    5, // Motor 0 IN1 pin (TB6612 #1 ซ้าย - AIN1/BIN1 ร่วมกัน)
    5, // Motor 1 IN1 pin (TB6612 #1 ซ้าย - AIN1/BIN1 ร่วมกัน)
    9, // Motor 2 IN1 pin (TB6612 #2 ขวา - AIN1/BIN1 ร่วมกัน)
    9  // Motor 3 IN1 pin (TB6612 #2 ขวา - AIN1/BIN1 ร่วมกัน)
};

static const int motor_in2_gpio[MOTORS_COUNT] = {
    6, // Motor 0 IN2 pin (TB6612 #1 ซ้าย - AIN2/BIN2 ร่วมกัน)
    6, // Motor 1 IN2 pin (TB6612 #1 ซ้าย - AIN2/BIN2 ร่วมกัน)
    10, // Motor 2 IN2 pin (TB6612 #2 ขวา - AIN2/BIN2 ร่วมกัน)
    10  // Motor 3 IN2 pin (TB6612 #2 ขวา - AIN2/BIN2 ร่วมกัน)
};

static const ledc_channel_t motor_ledc_channel[MOTORS_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3
};

static bool motors_ready = false;

static bool pins_configured(void) {
    for (int i = 0; i < MOTORS_COUNT; ++i) {
        if (motor_pwm_gpio[i] < 0 || motor_in1_gpio[i] < 0 || motor_in2_gpio[i] < 0) {
            return false;
        }
    }
    return true;
}

static esp_err_t configure_gpio_output(int gpio_num) {
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ULL << gpio_num),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };

    return gpio_config(&io_conf);
}

static void set_direction(uint8_t motor_id, int16_t speed) {
    if (speed > 0) {
        gpio_set_level(motor_in1_gpio[motor_id], 1);
        gpio_set_level(motor_in2_gpio[motor_id], 0);
    } else if (speed < 0) {
        gpio_set_level(motor_in1_gpio[motor_id], 0);
        gpio_set_level(motor_in2_gpio[motor_id], 1);
    } else {
        gpio_set_level(motor_in1_gpio[motor_id], 0);
        gpio_set_level(motor_in2_gpio[motor_id], 0);
    }
}

static esp_err_t set_pwm(uint8_t motor_id, uint32_t duty) {
    esp_err_t err = ledc_set_duty(MOTOR_SPEED_MODE, motor_ledc_channel[motor_id], duty);
    if (err != ESP_OK) {
        return err;
    }
    return ledc_update_duty(MOTOR_SPEED_MODE, motor_ledc_channel[motor_id]);
}

esp_err_t motors_init(void) {
    if (!pins_configured()) {
        ESP_LOGE(TAG, "Motor pins not configured. Update motor pin mapping in motors.c");
        return ESP_ERR_INVALID_ARG;
    }

    ledc_timer_config_t ledc_timer = {
        .speed_mode = MOTOR_SPEED_MODE,
        .timer_num = MOTOR_TIMER,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    for (int i = 0; i < MOTORS_COUNT; ++i) {
        ESP_ERROR_CHECK(configure_gpio_output(motor_in1_gpio[i]));
        ESP_ERROR_CHECK(configure_gpio_output(motor_in2_gpio[i]));

        ledc_channel_config_t ledc_channel = {
            .gpio_num = motor_pwm_gpio[i],
            .speed_mode = MOTOR_SPEED_MODE,
            .channel = motor_ledc_channel[i],
            .timer_sel = MOTOR_TIMER,
            .duty = 0,
            .hpoint = 0
        };
        ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
    }

    if (MOTOR_STBY_GPIO >= 0) {
        ESP_ERROR_CHECK(configure_gpio_output(MOTOR_STBY_GPIO));
        gpio_set_level(MOTOR_STBY_GPIO, 0);
    }

    motors_ready = true;
    motors_stop_all();
    return ESP_OK;
}

void motors_enable(bool enable) {
    if (MOTOR_STBY_GPIO >= 0) {
        gpio_set_level(MOTOR_STBY_GPIO, enable ? 1 : 0);
    }
}

void motors_stop_all(void) {
    if (!motors_ready) {
        return;
    }

    for (int i = 0; i < MOTORS_COUNT; ++i) {
        set_direction(i, 0);
        set_pwm(i, 0);
    }
}

esp_err_t motors_set_speed(uint8_t motor_id, int16_t speed) {
    if (!motors_ready) {
        return ESP_ERR_INVALID_STATE;
    }
    if (motor_id >= MOTORS_COUNT) {
        return ESP_ERR_INVALID_ARG;
    }

    if (speed > MOTORS_MAX_DUTY) {
        speed = MOTORS_MAX_DUTY;
    } else if (speed < -MOTORS_MAX_DUTY) {
        speed = -MOTORS_MAX_DUTY;
    }

    set_direction(motor_id, speed);
    uint32_t duty = (speed < 0) ? (uint32_t)(-speed) : (uint32_t)(speed);
    return set_pwm(motor_id, duty);
}
