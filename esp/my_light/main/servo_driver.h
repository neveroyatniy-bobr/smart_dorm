#pragma once

#include <esp_err.h>
#include <stdbool.h>

typedef struct servo_driver *servo_driver_handle_t;

typedef struct {
    int gpio_num;
} servo_driver_config_t;

servo_driver_handle_t servo_driver_init(const servo_driver_config_t *config);
esp_err_t servo_driver_set_angle(servo_driver_handle_t handle, int angle_deg);
