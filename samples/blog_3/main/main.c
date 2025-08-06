#include <math.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "mcp.h"
#include "mcp_server.h"
#include "radio.h"
#include "wifi.h"

const char *set_volume(int n_args, property_t *args)
{
    if (n_args < 1) {
        return "At least one argument is required";
    }

    if (args[0].type != PROPERTY_INTEGER) {
        return "Volume argument must be an integer";
    }

    int volume = (int) args[0].value.integer_value;
    if (volume < 0 || volume > 100) {
        return "Volume must be between 0 and 100";
    }

    esp_err_t ret = max98357_set_volume_percent((uint8_t) volume);
    if (ret != ESP_OK) {
        return "Failed to set volume";
    }

    ESP_LOGI("mcp server", "Setting volume to: %d%%", volume);
    return "Volume set successfully";
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = max98357_init();
    if (ret != ESP_OK) {
        ESP_LOGE("main", "MAX98357 init error: %s", esp_err_to_name(ret));
    }

    max98357_set_volume_percent(50);

    wifi_station_init("wifi_ssid", "wifi_password");

    vTaskDelay(pdMS_TO_TICKS(1000));
    mcp_server_t *server = mcp_server_init(
        "ESP32 Demo Server", "A demo server for ESP32 using MCP over MQTT",
        "mqtt://broker.emqx.io", "esp32-demo-server-client", NULL, NULL, NULL);

    mcp_tool_t tools[] = {
        { .name           = "set_volume",
          .description    = "Set the volume of the device, range 0 to 100",
          .property_count = 1,
          .properties =
              (property_t[]) {
                  { .name                = "volume",
                    .description         = "Volume level (0-100)",
                    .type                = PROPERTY_INTEGER,
                    .value.integer_value = 50 },
              },
          .call = set_volume },
    };

    mcp_server_register_tool(server, sizeof(tools) / sizeof(mcp_tool_t), tools);

    mcp_server_run(server);
}
