#include <stdlib.h>
#include <string.h>

#include "mbedtls/base64.h"

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define MAX_HTTP_OUTPUT_BUFFER 2048

extern const uint8_t image_jpg_start[] asm("_binary_image_jpg_start");
extern const uint8_t image_jpg_end[] asm("_binary_image_jpg_end");

static const char *TAG = "HTTP_CLIENT";

esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
    static int output_len;
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (output_len == 0 && evt->user_data) {
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            if (evt->user_data && evt->data_len > 0 && output_len < MAX_HTTP_OUTPUT_BUFFER) {
                int copy_len = evt->data_len;
                if (output_len + copy_len > MAX_HTTP_OUTPUT_BUFFER) {
                    copy_len = MAX_HTTP_OUTPUT_BUFFER - output_len;
                }
                memcpy(evt->user_data + output_len, evt->data, copy_len);
                output_len += copy_len;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

char *http_send_image(char* vision_explain_address, char* question) {
    const size_t image_len = image_jpg_end - image_jpg_start;

    size_t b64_len;
    mbedtls_base64_encode(NULL, 0, &b64_len, (const unsigned char *)image_jpg_start, image_len);
    unsigned char *b64_buf = malloc(b64_len);
    if (b64_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for base64 buffer");
        return NULL;
    }
    mbedtls_base64_encode(b64_buf, b64_len, &b64_len, (const unsigned char *)image_jpg_start, image_len);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "role", "user");
    cJSON *content_array = cJSON_AddArrayToObject(root, "content");
    
    // 第一个内容项：图片
    cJSON *content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(content_item, "type", "image_url");
    cJSON *image_url_obj = cJSON_CreateObject();

    char *image_url_str;
    const char *prefix = "data:image/jpg;base64,";
    image_url_str = malloc(strlen(prefix) + b64_len + 1);
    strcpy(image_url_str, prefix);
    strcat(image_url_str, (char *)b64_buf);
    cJSON_AddStringToObject(image_url_obj, "url", image_url_str);

    cJSON_AddItemToObject(content_item, "image_url", image_url_obj);
    cJSON_AddItemToArray(content_array, content_item);
    
    // 第二个内容项：文本问题
    cJSON *text_content_item = cJSON_CreateObject();
    cJSON_AddStringToObject(text_content_item, "type", "text");
    cJSON_AddStringToObject(text_content_item, "text", question);
    cJSON_AddItemToArray(content_array, text_content_item);
    

    char *post_data = cJSON_Print(root);

    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER] = {0};

    esp_http_client_config_t config = {
        .url = vision_explain_address,
        .event_handler = _http_event_handler,
        .user_data = local_response_buffer,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    char *response_string = NULL;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
        ESP_LOGI(TAG, "Response: %s", local_response_buffer);

        cJSON *response_json = cJSON_Parse(local_response_buffer);
        if (response_json != NULL) {
            cJSON *response_item = cJSON_GetObjectItem(response_json, "response");
            if (cJSON_IsString(response_item) && (response_item->valuestring != NULL)) {
                response_string = strdup(response_item->valuestring);
            }
            cJSON_Delete(response_json);
        } else {
            ESP_LOGE(TAG, "Failed to parse response JSON");
        }
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    cJSON_Delete(root);
    free(post_data);
    free(b64_buf);
    free(image_url_str);

    return response_string;
}