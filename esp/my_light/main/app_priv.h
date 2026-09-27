#pragma once

#include <esp_err.h>
#include <esp_matter.h>
#include <stdint.h>

/* Hardware configuration */
#define SERVO_PWM_GPIO 4

/* Logical light positions */
#define SERVO_ON_ANGLE_DEG 15
#define SERVO_OFF_ANGLE_DEG -15

/* Matter default state */
#define DEFAULT_POWER false

typedef void *app_driver_handle_t;

app_driver_handle_t app_driver_light_init();

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle,
                                      uint16_t endpoint_id,
                                      uint32_t cluster_id,
                                      uint32_t attribute_id,
                                      esp_matter_attr_val_t *val);

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id);
