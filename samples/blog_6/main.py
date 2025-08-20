import asyncio
import anyio
import logging
import os
from typing import List, Optional, Union, cast
from dataclasses import dataclass

from llama_index.core.agent.workflow import (
    AgentOutput,
    AgentStream,
    AgentWorkflow,
    ToolCallResult,
)

import mcp.client.mqtt as mcp_mqtt
from mcp.shared.mqtt import configure_logging
import mcp.types as types

from llama_index.llms.siliconflow import SiliconFlow
from llama_index.core.llms import ChatMessage, MessageRole
from llama_index.core.tools import BaseTool, FunctionTool
from llama_index.core.settings import Settings

configure_logging(level="DEBUG")
logger = logging.getLogger(__name__)


async def on_mcp_server_discovered(client: mcp_mqtt.MqttTransportClient, server_name):
    logger.info(f"Discovered {server_name}, connecting ...")
    await client.initialize_mcp_server(server_name)


async def on_mcp_connect(client, server_name, connect_result):
    capabilities = client.get_session(server_name).server_info.capabilities
    logger.info(f"Capabilities of {server_name}: {capabilities}")
    if capabilities.prompts:
        prompts = await client.list_prompts(server_name)
        logger.info(f"Prompts of {server_name}: {prompts}")
    if capabilities.resources:
        resources = await client.list_resources(server_name)
        logger.info(f"Resources of {server_name}: {resources}")
        resource_templates = await client.list_resource_templates(server_name)
        logger.info(f"Resources templates of {server_name}: {resource_templates}")
    if capabilities.tools:
        toolsResult = await client.list_tools(server_name)
        tools = toolsResult.tools
        logger.info(f"Tools of {server_name}: {tools}")


async def on_mcp_disconnect(client, server_name):
    logger.info(f"Disconnected from {server_name}")


client = None
api_key = os.getenv("YOUR_API_KEY_FROM_CLOUD_SILICONFLOW_CN")


async def get_mcp_tools(mcp_client: mcp_mqtt.MqttTransportClient) -> List[BaseTool]:
    all_tools = []
    try:
        try:
            tools_result = await mcp_client.list_tools("ESP32 Demo Server")

            if tools_result is False:
                return all_tools

            list_tools_result = cast(types.ListToolsResult, tools_result)
            tools = list_tools_result.tools

            for tool in tools:
                logger.info(f"tool: {tool.name} - {tool.description}")

                def create_mcp_tool_wrapper(client_ref, server_name, tool_name):
                    async def mcp_tool_wrapper(**kwargs):
                        try:
                            result = await client_ref.call_tool(
                                server_name, tool_name, kwargs
                            )
                            if result is False:
                                return f"call {tool_name} failed"

                            call_result = cast(types.CallToolResult, result)

                            if hasattr(call_result, "content") and call_result.content:
                                content_parts = []
                                for content_item in call_result.content:
                                    if hasattr(content_item, "type"):
                                        if content_item.type == "text":
                                            text_content = cast(
                                                types.TextContent, content_item
                                            )
                                            content_parts.append(text_content.text)
                                        elif content_item.type == "image":
                                            image_content = cast(
                                                types.ImageContent, content_item
                                            )
                                            content_parts.append(
                                                f"[image: {image_content.mimeType}]"
                                            )
                                        elif content_item.type == "resource":
                                            resource_content = cast(
                                                types.EmbeddedResource, content_item
                                            )
                                            content_parts.append(
                                                f"[resource: {resource_content.resource}]"
                                            )
                                        else:
                                            content_parts.append(str(content_item))
                                    else:
                                        content_parts.append(str(content_item))

                                result_text = "\n".join(content_parts)

                                if (
                                    hasattr(call_result, "isError")
                                    and call_result.isError
                                ):
                                    return f"tool return error: {result_text}"
                                else:
                                    return result_text
                            else:
                                return str(call_result)

                        except Exception as e:
                            error_msg = f"call {tool_name} error: {e}"
                            logger.error(error_msg)
                            return error_msg

                    return mcp_tool_wrapper

                wrapper_func = create_mcp_tool_wrapper(
                    mcp_client, "ESP32 Demo Server", tool.name
                )

                try:
                    llamaindex_tool = FunctionTool.from_defaults(
                        fn=wrapper_func,
                        name=f"mcp_{tool.name}",
                        description=tool.description or f"MCP tool: {tool.name}",
                        async_fn=wrapper_func,
                    )
                    all_tools.append(llamaindex_tool)
                    # logger.info(f"call tool success: mcp_{tool.name}")

                except Exception as e:
                    logger.error(f"create tool {tool.name} error: {e}")

        except Exception as e:
            logger.error(f"Get tool list error: {e}")

    except Exception as e:
        logger.error(f"Get tool list error: {e}")

    return all_tools


def process_tool_output(response_text):
    if hasattr(response_text, "content"):
        response_text = response_text.content
        return response_text
    return None


class ConversationalAgent:
    def __init__(self, mcp_client: Optional[mcp_mqtt.MqttTransportClient] = None):
        self.llm = SiliconFlow(
            api_key=api_key,
            model="deepseek-ai/DeepSeek-V3",
            temperature=0.6,
            max_tokens=4000,
            timeout=180,
        )
        Settings.llm = self.llm

        self.mcp_client = mcp_client
        self.tools = []

        # self.agent = AgentRunner.from_llm(llm=self.llm, tools=self.tools, verbose=True)

        self.mcp_tools_loaded = False

        self.conversation_history = []

        self.max_history_length = 20

        self.system_prompt = """
                在这个对话中，你将扮演一个简单的情感助手。
                你有视觉能力，当你被问 “你看看我今天打扮得怎么样”、“你看看我这件衣服是什么牌子的” 等视觉相关问题时，你可以调用 "explain_photo" 这个工具，
                这个工具可以给主人拍摄一张照片，然后针对此照片做出评价，并将评价返回给你。
                根据我提供的问题，生成一个简洁的回应。
                请注意，回应的长度不应超过20个字符，内容应是对我的问题的情感分析或回应。
                """

    def _build_chat_messages(self, new_message: str) -> list:
        """Build structured chat message array"""
        messages = []

        # Add system prompt
        messages.append(
            ChatMessage(
                role=MessageRole.SYSTEM,
                content=self.system_prompt,
                additional_kwargs={},
            )
        )

        # Add conversation history (keep recent N rounds of conversation)
        recent_history = (
            self.conversation_history[-self.max_history_length :]
            if len(self.conversation_history) > self.max_history_length
            else self.conversation_history
        )

        # Clean history messages, completely remove tool_calls field and filter empty messages
        for i, msg in enumerate(recent_history):
            # Ensure content is valid (not None and not empty string)
            content = msg.content or ""

            if not content.strip():  # Skip empty messages
                continue

            # Use model_construct to create cleanest messages
            clean_msg = ChatMessage.model_construct(
                role=msg.role,
                content=content,
                additional_kwargs={},
                blocks=[],  # Ensure no extra block data
            )
            messages.append(clean_msg)

        # Add current user message
        messages.append(
            ChatMessage(
                role=MessageRole.USER, content=new_message, additional_kwargs={}
            )
        )

        # Final cleanup: ensure all messages have valid content
        final_messages = []
        for msg in messages:
            # Check if content is valid
            content = msg.content
            if content is None or not str(content).strip():
                continue  # Skip invalid messages
            else:
                # Add messages with valid content directly
                final_messages.append(msg)

        return final_messages

    async def load_mcp_tools(self):
        if not self.mcp_tools_loaded and self.mcp_client:
            try:
                mcp_tools = await get_mcp_tools(self.mcp_client)
                if mcp_tools:
                    self.tools.extend(mcp_tools)
                    # self.agent = AgentRunner.from_llm(
                    #     llm=self.llm, tools=self.tools, verbose=True
                    # )
                    logger.info(f"load {len(mcp_tools)} tools")
                    self.mcp_tools_loaded = True
            except Exception as e:
                logger.error(f"load tool error: {e}")

    async def chat(self, message: str) -> str:
        try:
            if not self.mcp_tools_loaded:
                await self.load_mcp_tools()

            query_info = AgentWorkflow.from_tools_or_functions(
                tools_or_functions=self.tools,
                llm=self.llm,
                system_prompt=self.system_prompt,
                verbose=False,
                timeout=180,
            )

            message = self._build_chat_messages(message)
            handler = query_info.run(chat_history=message)

            output = None
            async for event in handler.stream_events():
                if isinstance(event, AgentOutput):
                    output = event.response
            response = process_tool_output(output)

            logger.info(f"Agent response: {response}")
            return str(response)

        except Exception as e:
            error_msg = f"error: {e}"
            logger.error(error_msg)
            return error_msg


async def main():
    try:
        async with mcp_mqtt.MqttTransportClient(
            "test_client",
            auto_connect_to_mcp_server=True,
            on_mcp_server_discovered=on_mcp_server_discovered,
            on_mcp_connect=on_mcp_connect,
            on_mcp_disconnect=on_mcp_disconnect,
            mqtt_options=mcp_mqtt.MqttOptions(
                host="localhost",
            ),
        ) as mcp_client:
            await mcp_client.start()
            await anyio.sleep(3)

            agent = ConversationalAgent(mcp_client)
            if not agent.mcp_tools_loaded:
                await agent.load_mcp_tools()

            print("input 'exit' or 'quit' exit")
            print("input 'tools' show available tools")
            print("=" * 50)

            while True:
                try:
                    user_input = input("\nuser: ").strip()

                    if user_input.lower() in ["exit", "quit"]:
                        break

                    if user_input.lower() == "tools":
                        print(f"available tools: {len(agent.tools)}")
                        for tool in agent.tools:
                            tool_name = getattr(tool.metadata, "name", str(tool))
                            tool_desc = getattr(
                                tool.metadata, "description", "No description"
                            )
                            print(f"- {tool_name}: {tool_desc}")
                        continue

                    if not user_input:
                        continue

                    response = await agent.chat(user_input)
                    print(f"\nAgent: {response}")

                except KeyboardInterrupt:
                    break
                except Exception as e:
                    print(f"error: {e}")

    except Exception as e:
        print(f"agent init error: {e}")


if __name__ == "__main__":
    anyio.run(main)
