[English](README.md) | [简体中文](README-CN.md)
## ESP32 + MCP over MQTT: Building Intelligent Agents

This is the sample code repository for the blog series "ESP32 + MCP over MQTT: Building Emotional Companion Agents from 0 to 1". This series introduces how to use MCP over MQTT, ESP32 hardware (or similar hardware), and various peripherals, LLMs, VLMs, ASR (Automatic Speech Recognition), and TTS (Text-to-Speech) technologies to build an intelligent agent end-to-end. It enables devices to perceive the world, hear, see, and speak. It can understand human natural language and even inject emotions and personality, with broad applications in emotional companion toys, embodied intelligence, and various other scenarios.

## Blog Series
- Part 1: [ESP32 + MCP over MQTT: Building Emotional Companion Agents from 0 to 1](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt) | [Code](samples/blog_1/)
- Part 2: [ESP32 + MCP over MQTT: Hardware Device Capability Encapsulation](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt-2) | [Code](samples/blog_2/)
- Part 3: [ESP32 + MCP over MQTT: Controlling Smart Hardware Devices through Large Models](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt-3) | [Code](samples/blog_3/)
- Part 4: [ESP32 + MCP over MQTT: Implementing Voice Interaction for Smart Devices](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt-4) | [Code](samples/blog_4/)
- Part 5: [ESP32 + MCP over MQTT: Building Personalized Emotional Agents Based on Large Models](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt-5) | [Code](samples/blog_5/)
- Part 6: [ESP32 + MCP over MQTT: Giving Agents "Eyes": Image Capture + Multimodal Understanding](https://www.emqx.com/en/blog/esp32-and-mcp-over-mqtt-6) | [Code](samples/blog_6/)

## **Overall System Architecture Design**
![image-20250722-034615.png](pics/arch.png)

- **Hardware**
  - ESP32: Runs MCP Server Tools developed based on [ESP32 MCP over MQTT](https://github.com/mqtt-ai/esp-mcp-over-mqtt), encapsulating hardware capabilities
  - Peripherals: Microphone, speaker, camera, etc.
- **Connection and Transport Layer**
  - MQTT Broker (EMQX) + MCP service discovery, management, and access control
  - Multimedia Stream Server for processing audio stream data
- **App + AI Service Layer**
  - ASR (Automatic Speech Recognition), TTS (Text-to-Speech), large model inference, multimodal large model photo recognition
  - Emotion, personality support, and memory system

## Related Software and Services
- [ESP32 MCP over MQTT SDK](https://github.com/mqtt-ai/esp-mcp-over-mqtt)
- [MQTT Protocol Related Materials: Developer Guide | EMQX Documentation](https://docs.emqx.com/zh/emqx/latest/connect-emqx/developer-guide.html)
- [EMQX Serverless Free Registration - Secure, Scalable Serverless MQTT Messaging Service](https://www.emqx.com/zh/cloud/serverless-mqtt)
- [MCP over MQTT Python SDK Code and Examples](https://github.com/emqx/mcp-python-sdk)
- [Alibaba Model Service Lingji DashScope](https://dashscope.aliyun.com/)
- [Large Model Service Platform Bailian - Enterprise Large Model Development Platform - Bailian AI Application Building - Alibaba Cloud](https://www.aliyun.com/product/bailian)
- [MQTT Client Tools](https://mqttx.app/)
- [ESP32 Official Website](https://www.espressif.com.cn/en/products/socs/esp32)