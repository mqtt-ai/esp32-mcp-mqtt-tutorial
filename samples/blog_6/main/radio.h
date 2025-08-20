#ifndef ESP32_DEMO_RADIO_H
#define ESP32_DEMO_RADIO_H

#include "driver/i2s_std.h"
#include "esp_err.h"

#define MAX98357_BCLK_GPIO 15 // 位时钟引脚
#define MAX98357_WS_GPIO 16   // 字选择引脚（左右声道选择）
#define MAX98357_DIN_GPIO 7   // 数据输入引脚
#define MAX98357_GAIN_GPIO 38 // GAIN 控制引脚

#define I2S_SAMPLE_RATE 44100
#define I2S_SAMPLE_BITS 16
#define I2S_CHANNELS 2

// 函数声明
esp_err_t max98357_init(void);
esp_err_t max98357_set_volume_percent(uint8_t volume);

#endif