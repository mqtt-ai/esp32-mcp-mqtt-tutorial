#include <stdio.h>

#include "freertos/FreeRTOS.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "mcp.h"
#include "mcp_server.h"
#include "wifi.h"

const char *set_volume(int n_args, property_t *args) {
  if (n_args < 2) {
    return "At least two arguments are required";
  }

  for (int i = 0; i < n_args; i++) {
    if (args[i].type != PROPERTY_INTEGER) {
      return "All arguments must be integers";
    }
    ESP_LOGI("mcp server", "Setting volume to: %lld\n",
             args[i].value.integer_value);
  }
  return "Volume set successfully";
}

const char *display(int n_args, property_t *args) {
  if (n_args < 1) {
    return "At least one argument is required";
  }
  for (int i = 0; i < n_args; i++) {
    if (args[i].type != PROPERTY_STRING) {
      return "All arguments must be strings";
    }
    ESP_LOGI("mcp server", "Display: %s\n", args[i].value.string_value);
  }
  return "Message displayed successfully";
}

void app_main(void) {
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);

  wifi_station_init("wifi_ssid", "wifi_password");

  vTaskDelay(pdMS_TO_TICKS(1000));
  mcp_server_t *server = mcp_server_init(
      "ESP32 Demo Server", "A demo server for ESP32 using MCP over MQTT",
      "mqtt://broker.emqx.io", "esp32-demo-server-client", NULL, NULL, NULL);

  mcp_tool_t tools[] = {
      {.name = "set_volume",
       .description = "Set the volume of the device, range 1 to 100",
       .property_count = 1,
       .properties =
           (property_t[]){
               {.name = "volume",
                .description = "Volume level",
                .type = PROPERTY_INTEGER,
                .value.integer_value = 30},
           },
       .call = set_volume},
      {.name = "display",
       .description = "Display a message on the device",
       .property_count = 1,
       .properties =
           (property_t[]){
               {.name = "message",
                .description = "Message to display",
                .type = PROPERTY_STRING,
                .value.string_value = "Hello, MCP!"},
           },
       .call = display},
  };

  mcp_server_register_tool(server, sizeof(tools) / sizeof(mcp_tool_t), tools);

  mcp_server_run(server);
}
