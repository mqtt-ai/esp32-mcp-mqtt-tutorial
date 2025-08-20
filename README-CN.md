
[English](README.md) | [简体中文](README-CN.md)
ESP32 + MCP over MQTT 构建智能体
这是系列文章「ESP32 + MCP over MQTT：从 0 到 1 打造情感陪伴智能体」的样例代码库。该系列文章介绍了如何使用 MCP over MQTT、ESP32 硬件（或者类似的硬件）以及各种外设、LLM、VLM、语音识别和语音合成等技术端到端构建一个智能体，让物可以感知世界、听得到、看得见，说得出，它可以理解人类的自然语言，甚至注入情感和人格，在情感类陪伴玩具、具身智能等各类场景下都有广泛的应用场景。

## 系列文章
- 第一篇：[ESP32 + MCP over MQTT：从 0 到 1 打造情感陪伴智能体](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt) ｜ [代码](samples/blog_1/)
- 第二篇：[ESP32 + MCP over MQTT：硬件设备能力封装](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt-2) ｜ [代码](samples/blog_2/)
- 第三篇：[ESP32 + MCP over MQTT：通过大模型控制智能硬件设备](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt-3) ｜ [代码](samples/blog_3/)
- 第四篇：[ESP32 + MCP over MQTT：实现智能设备语音交互](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt-4) ｜ [代码](samples/blog_4/)
- 第五篇：[ESP32 + MCP over MQTT：基于大模型打造人格化情感智能体](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt-5)｜ [代码](samples/blog_5/)
- 第六篇：[ESP32 + MCP over MQTT：给智能体增加“眼睛”：图像采集 + 多模态理解](https://www.emqx.com/zh/blog/esp32-and-mcp-over-mqtt-6) ｜ [代码](samples/blog_6/)

## **总体系统架构设计**
![image-20250722-034615.png](pics/arch.png)

- 硬件
  - ESP32：运行基于 [ESP32 MCP over MQTT](https://github.com/mqtt-ai/esp-mcp-over-mqtt) 开发的 MCP Server Tools，封装了硬件的能力
  - 外部设备：麦克风、扬声器、摄像头等
- 连接层和传输层
  - MQTT Broker（EMQX） + MCP 服务发现、管理和权限控制
  - Multimedia Stream Server 用于处理音频流数据
- App + AI 服务层
  - 语音识别、语音合成、大模型推理、多模态大模型识别照片
  - 情感、人格支持，以及记忆系统

## 相关软件和服务
- [ESP32 MCP over MQTT SDK](https://github.com/mqtt-ai/esp-mcp-over-mqtt)
- [MQTT 协议相关的基本材料：开发者指南 | EMQX 文档](https://docs.emqx.com/zh/emqx/latest/connect-emqx/developer-guide.html)
- [EMQX Serverless 免费注册 - 安全、可伸缩的 Serverless MQTT 消息服务](https://www.emqx.com/zh/cloud/serverless-mqtt)
- [MCP over MQTT 的 Python SDK 代码，以及样例](https://github.com/emqx/mcp-python-sdk)
- [阿里模型服务灵积 DashScope](https://dashscope.aliyun.com/)
- [大模型服务平台百炼_企业级大模型开发平台_百炼AI应用构建-阿里云](https://www.aliyun.com/product/bailian)
- [MQTT 客户端工具](https://mqttx.app/)
- [ESP32 官方网站](https://www.espressif.com.cn/en/products/socs/esp32)