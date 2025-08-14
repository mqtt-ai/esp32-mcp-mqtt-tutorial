# ESP32 AI Voice Assistant

An intelligent voice assistant system based on ESP32, EMQX, and AI large language models.

## Project Overview

This project implements a complete voice conversation system with voice recording, AI processing, and audio playback capabilities. The system communicates via MQTT protocol and supports real-time voice interaction.

### System Architecture

```
ESP32 → EMQX → Webhook → AI Model → EMQX → ESP32
  ↓        ↓        ↓        ↓         ↓       ↓
Record → Message Queue → Data Integration → Voice Generation → Message Queue → Playback
```

## Features

- **Voice Detection**: Real-time voice detection based on energy threshold
- **Audio Recording**: High-quality WAV format audio recording
- **AI Voice Generation**: Integration with Alibaba Qwen model
- **Audio Playback**: MP3 format audio playback
- **MQTT Communication**: Reliable message queue communication
- **Connection Management**: Automatic reconnection mechanism

## Hardware Requirements

### ESP32 Development Board
- ESP32 development board (ESP32-S3 recommended)
- I2S microphone module
- I2S audio amplifier/speaker

### Pin Connections

#### Recording Pins (I2S_NUM_0)
- BCLK: GPIO 7
- LRCL: GPIO 8
- DOUT: GPIO 9

#### Playback Pins (I2S_NUM_1)
- BCLK: GPIO 2
- LRCL: GPIO 1
- DOUT: GPIO 42

## Dependencies

### ESP32 Libraries
```cpp
#include <WiFi.h>
#include <PubSubClient.h>
#include <SPIFFS.h>
#include <AudioFileSourceSPIFFS.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
```

### Python Dependencies
```bash
pip install fastapi uvicorn httpx openai numpy lameenc
```

## Configuration

### ESP32 Configuration

1. **WiFi Settings**
```cpp
const char *ssid = "YOUR_WIFI_SSID";
const char *password = "YOUR_WIFI_PASSWORD";
```

2. **MQTT Settings**
```cpp
const char *mqtt_broker = "broker.emqx.io";
const char *mqtt_upload_topic = "emqx/esp32/audio";
const char *mqtt_download_topic = "emqx/esp32/playaudio";
```

### Python Server Configuration

1. **Environment Variables**
```bash
export OPENAI_API_KEY="your_api_key_here"
export EMQX_USERNAME="admin"
export EMQX_PASSWORD="public"
export PORT=5005
```

2. **EMQX Data Integration Configuration**
- Create rule: Monitor `emqx/esp32/audio` topic
- Add action: HTTP request to Webhook server
- Configure URL: `http://your-server:5005/process_audio`

## Quick Start

### 1. Hardware Setup
Connect ESP32 and audio modules according to the pin connection diagram

### 2. ESP32 Code Deployment
```bash
# Using Arduino IDE or PlatformIO
# 1. Install required libraries
# 2. Modify WiFi and MQTT configuration
# 3. Compile and upload code
```

### 3. Python Server Deployment
```bash
# Clone project
git clone https://github.com/your-username/esp32-ai-voice-assistant.git
cd esp32-ai-voice-assistant

# Install dependencies
pip install -r requirements.txt

# Set environment variables
export OPENAI_API_KEY="your_api_key_here"

# Start server
python webhook.py
```

### 4. EMQX Configuration
1. Login to EMQX Dashboard
2. Create data integration rule
3. Configure Webhook action pointing to your server

## Configuration Parameters

### Voice Detection Parameters
```cpp
#define ENERGY_THRESHOLD  350    // Voice detection threshold
#define DETECTION_WINDOW  2      // Consecutive detection window
#define SILENCE_RESET     5      // Silence reset count
#define RECORD_SECONDS    3      // Recording duration (seconds)
```

### Sampling Parameters
```cpp
#define I2S_SAMPLE_RATE   8000   // Sample rate
#define I2S_SAMPLE_BITS   16     // Sample bits
#define I2S_CHANNEL_NUM   1      // Number of channels
```

## API Documentation

### Webhook Endpoints

#### POST /process_audio
Process audio data and generate AI response

**Request Body:**
```json
{
  "audio": "base64_encoded_audio_data"
}
```

**Response:**
```json
{
  "success": true,
  "message": "Audio processing task started"
}
```

#### GET /ping
Health check endpoint

**Response:**
```json
{
  "message": "pong",
  "status": "healthy"
}
```

## Troubleshooting

### Common Issues

1. **Connection Issues**
   - Check WiFi credentials
   - Verify MQTT broker address
   - Confirm firewall settings

2. **Audio Issues**
   - Check I2S pin connections
   - Verify microphone power supply
   - Adjust voice detection threshold

3. **AI Response Issues**
   - Verify API key
   - Check network connection
   - Review server logs

### Debug Mode

Enable verbose logging:
```cpp
Serial.printf("Energy: %u, Threshold: %d\n", energy, ENERGY_THRESHOLD);
```

Python server logging:
```bash
LOG_LEVEL=debug python webhook.py
```

## Support

If you encounter issues or have questions, please:

1. Check [Issues](https://github.com/your-username/esp32-ai-voice-assistant/issues)
2. Create a new Issue
3. Refer to documentation and examples

---

**Enjoy building your AI voice assistant!** Push to the branch (`git push origin feature/AmazingFeature`)
