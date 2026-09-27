#include <esp_err.h>
#include <esp_log.h>
#include <esp_matter.h>
#include <esp_matter_console.h>
#include <nvs_flash.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "app_priv.h"

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static const char *TAG = "app_main";

uint16_t light_endpoint_id = 0;

static void app_event_cb(const chip::DeviceLayer::ChipDeviceEvent *event, intptr_t arg)
{
    (void)arg;

    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStarted:
        ESP_LOGI(TAG, "Commissioning session started");
        break;
    case chip::DeviceLayer::DeviceEventType::kCommissioningSessionStopped:
        ESP_LOGI(TAG, "Commissioning session stopped");
        break;
    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type,
                                       uint16_t endpoint_id,
                                       uint8_t effect_id,
                                       uint8_t effect_variant,
                                       void *priv_data)
{
    (void)priv_data;
    ESP_LOGI(TAG,
             "Identification callback: type=%u endpoint=%u effect=%u variant=%u",
             static_cast<unsigned>(type),
             static_cast<unsigned>(endpoint_id),
             static_cast<unsigned>(effect_id),
             static_cast<unsigned>(effect_variant));
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type,
                                         uint16_t endpoint_id,
                                         uint32_t cluster_id,
                                         uint32_t attribute_id,
                                         esp_matter_attr_val_t *val,
                                         void *priv_data)
{
    if (type == esp_matter::attribute::PRE_UPDATE) {
        auto driver_handle = static_cast<app_driver_handle_t>(priv_data);
        return app_driver_attribute_update(driver_handle, endpoint_id, cluster_id, attribute_id, val);
    }

    return ESP_OK;
}

extern "C" void app_main()
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    /* Physical driver first, same separation as in Espressif's light example. */
    app_driver_handle_t light_handle = app_driver_light_init();
    if (light_handle == nullptr) {
        ESP_LOGE(TAG, "Failed to initialize SG90 driver");
        return;
    }

    /* Matter root node. */
    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (node == nullptr) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    endpoint_t *root_endpoint = endpoint::get(node, 0);

    cluster_t *basic_info_cluster =
        cluster::get(root_endpoint, chip::app::Clusters::BasicInformation::Id);

    if (basic_info_cluster) {
        cluster::basic_information::attribute::create_serial_number(
            basic_info_cluster,
            "SMART-DORM-001",
            sizeof("SMART-DORM-001") - 1
        );
    }

    /* One application endpoint only: standard Matter On/Off Light. */
    on_off_light::config_t light_config;
    light_config.on_off.on_off = DEFAULT_POWER;

    endpoint_t *endpoint = on_off_light::create(node, &light_config, ENDPOINT_FLAG_NONE, light_handle);
    if (endpoint == nullptr) {
        ESP_LOGE(TAG, "Failed to create On/Off Light endpoint");
        return;
    }

    light_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "On/Off Light created on endpoint %u", static_cast<unsigned>(light_endpoint_id));

    err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter: %s", esp_err_to_name(err));
        return;
    }

    /* Apply the Matter attribute's current value to the real servo after Matter starts. */
    err = app_driver_light_set_defaults(light_endpoint_id);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to apply initial servo state: %s", esp_err_to_name(err));
    }

#if CONFIG_ENABLE_CHIP_SHELL
    esp_matter::console::diagnostics_register_commands();
    esp_matter::console::wifi_register_commands();
    esp_matter::console::factoryreset_register_commands();
    esp_matter::console::attribute_register_commands();
    esp_matter::console::init();
#endif

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}
