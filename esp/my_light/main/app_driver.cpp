#include <esp_log.h>
#include <esp_matter.h>

#include "app_priv.h"
#include "servo_driver.h"

using namespace chip::app::Clusters;
using namespace esp_matter;

static const char *TAG = "app_driver";

extern uint16_t light_endpoint_id;

static esp_err_t app_driver_light_set_power(servo_driver_handle_t handle, esp_matter_attr_val_t *val)
{
    if (handle == nullptr || val == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    const bool on = val->val.b;
    const int angle = on ? SERVO_ON_ANGLE_DEG : SERVO_OFF_ANGLE_DEG;

    ESP_LOGI(TAG, "Matter light -> %s; SG90 -> %+d deg", on ? "ON" : "OFF", angle);
    return servo_driver_set_angle(handle, angle);
}

esp_err_t app_driver_attribute_update(app_driver_handle_t driver_handle,
                                      uint16_t endpoint_id,
                                      uint32_t cluster_id,
                                      uint32_t attribute_id,
                                      esp_matter_attr_val_t *val)
{
    if (endpoint_id != light_endpoint_id) {
        return ESP_OK;
    }

    if (cluster_id == OnOff::Id && attribute_id == OnOff::Attributes::OnOff::Id) {
        return app_driver_light_set_power(static_cast<servo_driver_handle_t>(driver_handle), val);
    }

    return ESP_OK;
}

esp_err_t app_driver_light_set_defaults(uint16_t endpoint_id)
{
    void *priv_data = endpoint::get_priv_data(endpoint_id);
    auto handle = static_cast<servo_driver_handle_t>(priv_data);
    if (handle == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    attribute_t *attribute = attribute::get(endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id);
    if (attribute == nullptr) {
        ESP_LOGE(TAG, "OnOff attribute not found");
        return ESP_ERR_NOT_FOUND;
    }

    esp_matter_attr_val_t val = esp_matter_invalid(nullptr);
    esp_err_t err = attribute::get_val(attribute, &val);
    if (err != ESP_OK) {
        return err;
    }

    return app_driver_light_set_power(handle, &val);
}

app_driver_handle_t app_driver_light_init()
{
    servo_driver_config_t config = {};
    config.gpio_num = SERVO_PWM_GPIO;

    servo_driver_handle_t handle = servo_driver_init(&config);
    return static_cast<app_driver_handle_t>(handle);
}
