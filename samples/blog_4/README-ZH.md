# ESP32 AI 语音助手

一个基于 ESP32、EMQX 和 AI 大模型的智能语音助手系统。

## 项目概述

本项目实现了一个完整的语音对话系统，包含语音录制、AI 处理和语音播放功能。系统通过 MQTT 协议进行通信，支持实时语音交互。

### 系统架构

```
ESP32 → EMQX → Webhook → AI 模型 → EMQX → ESP32
  ↓        ↓        ↓        ↓         ↓       ↓
录音 → 消息队列 → 数据集成 → 语音生成 → 消息队列 → 播放
```

## 功能特性

- **语音检测**: 基于能量阈值的实时语音检测
- **音频录制**: 高质量 WAV 格式音频录制
- **AI 语音生成**: 集成阿里云通义千问模型
- **音频播放**: MP3 格式音频播放
- **MQTT 通信**: 可靠的消息队列通信
- **连接管理**: 自动重连机制

## 硬件要求

### ESP32 开发板
- ESP32 开发板 (推荐 ESP32-S3)
- I2S 麦克风模块
- I2S 音频放大器/扬声器

### 引脚连接

#### 录音引脚 (I2S_NUM_0)
- BCLK: GPIO 7
- LRCL: GPIO 8
- DOUT: GPIO 9

#### 播放引脚 (I2S_NUM_1)
- BCLK: GPIO 2
- LRCL: GPIO 1
- DOUT: GPIO 42

## 依赖库

### ESP32 库
```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
```

### Python 依赖
```bash
pip install fastapi uvicorn httpx openai numpy lameenc
```

## 配置说明

### ESP32 配置

1. **WiFi 设置**
```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
```

2. **MQTT 设置**
```cpp
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_upload_topic = "emqx/esp32/audio";
const char *mqtt_download_topic = "emqx/esp32/playaudio";
```

### Python 服务器配置

1. **环境变量设置**
```bash
export OPENAI_API_KEY="your_api_key_here"
export EMQX_USERNAME="admin"
export EMQX_PASSWORD="public"
export PORT=5005
```

2. **EMQX 数据集成配置**
- 创建规则：监听 `emqx/esp32/audio` 主题
- 添加动作：HTTP 请求到 Webhook 服务器
- 配置 URL：`http://your-server:5005/process_audio`

## 快速开始

### 1. 硬件准备
按照引脚连接图连接 ESP32 和音频模块

### 2. ESP32 代码部署
```bash
# 使用 Arduino IDE 或 PlatformIO
# 1. 安装所需库
# 2. 修改 WiFi 和 MQTT 配置
# 3. 编译并上传代码
```

### 3. Python 服务器部署
```bash
# 克隆项目
git clone https://github.com/your-username/esp32-ai-voice-assistant.git
cd esp32-ai-voice-assistant

# 安装依赖
pip install -r requirements.txt

# 设置环境变量
export OPENAI_API_KEY="your_api_key_here"

# 启动服务器
python webhook.py
```

### 4. EMQX 配置
1. 登录 EMQX Dashboard
2. 创建数据集成规则
3. 配置 Webhook 动作指向你的服务器

## 配置参数

### 语音检测参数
```cpp
#define ENERGY_THRESHOLD  350    // 语音检测阈值
#define DETECTION_WINDOW  2      // 连续检测窗口
#define SILENCE_RESET     5      // 静音重置计数
#define RECORD_SECONDS    3      // 录音时长(秒)
```

### 采样参数
```cpp
#define I2S_SAMPLE_RATE   8000   // 采样率
#define I2S_SAMPLE_BITS   16     // 采样位数
#define I2S_CHANNEL_NUM   1      // 声道数
```

## API 文档

### Webhook 端点

#### POST /process_audio
处理音频数据并生成 AI 响应

**请求体:**
```json
{
  "audio": "base64_encoded_audio_data"
}
```

**响应:**
```json
{
  "success": true,
  "message": "Audio processing task started"
}
```

#### GET /ping
健康检查端点

**响应:**
```json
{
  "message": "pong",
  "status": "healthy"
}
```

## 故障排除

### 常见问题

1. **连接问题**
   - 检查 WiFi 凭据
   - 验证 MQTT 代理地址
   - 确认防火墙设置

2. **音频问题**
   - 检查 I2S 引脚连接
   - 验证麦克风供电
   - 调整语音检测阈值

3. **AI 响应问题**
   - 验证 API 密钥
   - 检查网络连接
   - 查看服务器日志

### 调试模式

启用详细日志输出：
```cpp
Serial.printf("Energy: %u, Threshold: %d\n", energy, ENERGY_THRESHOLD);
```

Python 服务器日志：
```bash
LOG_LEVEL=debug python webhook.py
```

## 支持

如果您遇到问题或有疑问，请：

1. 查看 [Issues](https://github.com/your-username/esp32-ai-voice-assistant/issues)
2. 创建新的 Issue
3. 参考文档和示例

---

**享受构建您的 AI 语音助手！**