#include "servo_driver.h"

#include <driver/ledc.h>
#include <esp_log.h>
#include <stdint.h>

static const char *TAG = "servo_driver";

/* Typical RC-servo timing used by Espressif's servo example. */
static constexpr uint32_t SERVO_PWM_FREQUENCY_HZ = 50;
static constexpr uint32_t SERVO_PERIOD_US = 1000000UL / SERVO_PWM_FREQUENCY_HZ;
static constexpr uint32_t SERVO_MIN_PULSE_US = 500;
static constexpr uint32_t SERVO_MAX_PULSE_US = 2500;
static constexpr int SERVO_MIN_ANGLE_DEG = -90;
static constexpr int SERVO_MAX_ANGLE_DEG = 90;

static constexpr ledc_mode_t SERVO_LEDC_MODE = LEDC_LOW_SPEED_MODE;
static constexpr ledc_timer_t SERVO_LEDC_TIMER = LEDC_TIMER_0;
static constexpr ledc_channel_t SERVO_LEDC_CHANNEL = LEDC_CHANNEL_0;
static constexpr ledc_timer_bit_t SERVO_LEDC_RESOLUTION = LEDC_TIMER_14_BIT;
static constexpr uint32_t SERVO_MAX_DUTY = (1UL << 14) - 1;

struct servo_driver {
    int gpio_num;
    bool initialized;
};

static servo_driver s_servo = {};

static uint32_t angle_to_pulse_us(int angle_deg)
{
    if (angle_deg < SERVO_MIN_ANGLE_DEG) {
        angle_deg = SERVO_MIN_ANGLE_DEG;
    } else if (angle_deg > SERVO_MAX_ANGLE_DEG) {
        angle_deg = SERVO_MAX_ANGLE_DEG;
    }

    const int angle_range = SERVO_MAX_ANGLE_DEG - SERVO_MIN_ANGLE_DEG;
    const uint32_t pulse_range = SERVO_MAX_PULSE_US - SERVO_MIN_PULSE_US;

    return SERVO_MIN_PULSE_US +
           static_cast<uint32_t>(angle_deg - SERVO_MIN_ANGLE_DEG) * pulse_range / angle_range;
}

static uint32_t pulse_us_to_duty(uint32_t pulse_us)
{
    return static_cast<uint32_t>((static_cast<uint64_t>(pulse_us) * SERVO_MAX_DUTY) / SERVO_PERIOD_US);
}

servo_driver_handle_t servo_driver_init(const servo_driver_config_t *config)
{
    if (config == nullptr || config->gpio_num < 0) {
        ESP_LOGE(TAG, "Invalid servo config");
        return nullptr;
    }

    ledc_timer_config_t timer_config = {};
    timer_config.speed_mode = SERVO_LEDC_MODE;
    timer_config.duty_resolution = SERVO_LEDC_RESOLUTION;
    timer_config.timer_num = SERVO_LEDC_TIMER;
    timer_config.freq_hz = SERVO_PWM_FREQUENCY_HZ;
    timer_config.clk_cfg = LEDC_AUTO_CLK;

    esp_err_t err = ledc_timer_config(&timer_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_timer_config failed: %s", esp_err_to_name(err));
        return nullptr;
    }

    ledc_channel_config_t channel_config = {};
    channel_config.gpio_num = config->gpio_num;
    channel_config.speed_mode = SERVO_LEDC_MODE;
    channel_config.channel = SERVO_LEDC_CHANNEL;
    channel_config.intr_type = LEDC_INTR_DISABLE;
    channel_config.timer_sel = SERVO_LEDC_TIMER;
    channel_config.duty = 0;
    channel_config.hpoint = 0;

    err = ledc_channel_config(&channel_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ledc_channel_config failed: %s", esp_err_to_name(err));
        return nullptr;
    }

    s_servo.gpio_num = config->gpio_num;
    s_servo.initialized = true;

    ESP_LOGI(TAG, "SG90 PWM initialized on GPIO %d at %lu Hz",
             s_servo.gpio_num, static_cast<unsigned long>(SERVO_PWM_FREQUENCY_HZ));

    return &s_servo;
}

esp_err_t servo_driver_set_angle(servo_driver_handle_t handle, int angle_deg)
{
    if (handle == nullptr || !handle->initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    const uint32_t pulse_us = angle_to_pulse_us(angle_deg);
    const uint32_t duty = pulse_us_to_duty(pulse_us);

    esp_err_t err = ledc_set_duty(
    SERVO_LEDC_MODE,
    SERVO_LEDC_CHANNEL,
    duty
    );

    if (err == ESP_OK) {
        err = ledc_update_duty(
            SERVO_LEDC_MODE,
            SERVO_LEDC_CHANNEL
        );
    }
    
    return err;
}
