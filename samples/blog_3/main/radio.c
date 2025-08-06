#include <stdio.h>
#include <string.h>

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_system.h"

#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_codec_dev_vol.h"

#include "radio.h"

static const char             *TAG         = "MAX98357";
static i2s_chan_handle_t       tx_chan     = NULL;
static esp_codec_dev_handle_t *output_dev_ = NULL;

esp_err_t max98357_init(void)
{
    esp_err_t ret = ESP_OK;

    gpio_config_t gain_cfg = {
        .pin_bit_mask = (1ULL << MAX98357_GAIN_GPIO),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&gain_cfg));

    i2s_chan_config_t chan_cfg =
        I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    chan_cfg.auto_clear = true;

    ret = i2s_new_channel(&chan_cfg, &tx_chan, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S channel create failed: %s", esp_err_to_name(ret));
        return ret;
    }

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_SAMPLE_BITS, I2S_CHANNELS),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = MAX98357_BCLK_GPIO,
            .ws = MAX98357_WS_GPIO,
            .dout = MAX98357_DIN_GPIO,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };

    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    audio_codec_i2s_cfg_t i2s_cfg = {
        .port      = I2S_NUM_AUTO,
        .rx_handle = NULL,
        .tx_handle = tx_chan,
    };
    const audio_codec_data_if_t *data_if_ = audio_codec_new_i2s_data(&i2s_cfg);

    esp_codec_dev_cfg_t dev_cfg = {
        .dev_type = ESP_CODEC_DEV_TYPE_OUT,
        .codec_if = NULL,
        .data_if  = data_if_,
    };
    output_dev_ = esp_codec_dev_new(&dev_cfg);
    if (output_dev_ == NULL) {
        ESP_LOGE(TAG, "Failed to create codec device");
        return ESP_ERR_NO_MEM;
    }

    ESP_ERROR_CHECK(esp_codec_dev_open(
        output_dev_,
        &(esp_codec_dev_sample_info_t) {
            .sample_rate     = I2S_SAMPLE_RATE,
            .bits_per_sample = I2S_SAMPLE_BITS,
            .channel         = I2S_CHANNELS,
            .channel_mask    = ESP_CODEC_DEV_MAKE_CHANNEL_MASK(0),
        }));

    ESP_LOGI(TAG, "MAX98357 init success");

    return ESP_OK;
}

esp_err_t max98357_set_volume_percent(uint8_t volume)
{
    esp_err_t ret = ESP_OK;

    if (volume > 100) {
        ESP_LOGE(TAG, "Volume percent must be in range [0, 100]");
        return ESP_ERR_INVALID_ARG;
    }

    ret = esp_codec_dev_set_out_vol(output_dev_, volume);

    ESP_LOGI(TAG, "Set volume to %d%%, %d", volume, ret);
    return ret;
}