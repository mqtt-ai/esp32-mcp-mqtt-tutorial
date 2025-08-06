import asyncio
import anyio
import logging
import os
from typing import List, Optional, Union, cast
from dataclasses import dataclass

import mcp.client.mqtt as mcp_mqtt
from mcp.shared.mqtt import configure_logging
import mcp.types as types

from llama_index.llms.siliconflow import SiliconFlow
from llama_index.core.llms import ChatMessage, MessageRole
from llama_index.core.agent import AgentRunner
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
api_key = "sk-*"

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
                            result = await client_ref.call_tool(server_name, tool_name, kwargs)
                            if result is False:
                                return f"call {tool_name} failed"
                            
                            call_result = cast(types.CallToolResult, result)
                            
                            if hasattr(call_result, 'content') and call_result.content:
                                content_parts = []
                                for content_item in call_result.content:
                                    if hasattr(content_item, 'type'):
                                        if content_item.type == 'text':
                                            text_content = cast(types.TextContent, content_item)
                                            content_parts.append(text_content.text)
                                        elif content_item.type == 'image':
                                            image_content = cast(types.ImageContent, content_item)
                                            content_parts.append(f"[image: {image_content.mimeType}]")
                                        elif content_item.type == 'resource':
                                            resource_content = cast(types.EmbeddedResource, content_item)
                                            content_parts.append(f"[resource: {resource_content.resource}]")
                                        else:
                                            content_parts.append(str(content_item))
                                    else:
                                        content_parts.append(str(content_item))
                                
                                result_text = '\n'.join(content_parts)
                                
                                if hasattr(call_result, 'isError') and call_result.isError:
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
                
                wrapper_func = create_mcp_tool_wrapper(mcp_client, "ESP32 Demo Server", tool.name)
                
                try:
                    llamaindex_tool = FunctionTool.from_defaults(
                        fn=wrapper_func,
                        name=f"mcp_{tool.name}",
                        description=tool.description or f"MCP tool: {tool.name}",
                        async_fn=wrapper_func
                    )
                    all_tools.append(llamaindex_tool)
                    logger.info(f"call tool success: mcp_{tool.name}")
                    
                except Exception as e:
                    logger.error(f"create tool {tool.name} error: {e}")
                    
        except Exception as e:
            logger.error(f"Get tool list error: {e}")
                
    except Exception as e:
        logger.error(f"Get tool list error: {e}")
    
    return all_tools

class ConversationalAgent:
    def __init__(self, mcp_client: Optional[mcp_mqtt.MqttTransportClient] = None):
        self.llm = SiliconFlow(api_key=api_key, model="deepseek-ai/DeepSeek-R1", temperature=0.6, max_tokens=4000, timeout=180)
        Settings.llm = self.llm
        
        self.mcp_client = mcp_client
        self.tools = []
        
        self.agent = AgentRunner.from_llm(
            llm=self.llm,
            tools=self.tools,
            verbose=True
        )
        
        self.mcp_tools_loaded = False
        
    async def load_mcp_tools(self):
        if not self.mcp_tools_loaded and self.mcp_client:
            try:
                mcp_tools = await get_mcp_tools(self.mcp_client)
                if mcp_tools:
                    self.tools.extend(mcp_tools)
                    self.agent = AgentRunner.from_llm(
                        llm=self.llm,
                        tools=self.tools,
                        verbose=True
                    )
                    logger.info(f"load {len(mcp_tools)} tools")
                    self.mcp_tools_loaded = True
            except Exception as e:
                logger.error(f"load tool error: {e}")
        
    async def chat(self, message: str) -> str:
        try:
            if not self.mcp_tools_loaded:
                await self.load_mcp_tools()
            
            logger.info(f"user input: {message}")
            user_message = ChatMessage(role=MessageRole.USER, content=message)
            
            response = await self.agent.achat(message)
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
                host="broker.emqx.io",
            )
        ) as mcp_client:
            await mcp_client.start()
            await anyio.sleep(3)
            
            agent = ConversationalAgent(mcp_client)
            
            print("input 'exit' or 'quit' exit")
            print("input 'tools' show available tools")
            print("="*50)
            
            while True:
                try:
                    user_input = input("\nuser: ").strip()
                    
                    if user_input.lower() in ['exit', 'quit']:
                        break
                    
                    if user_input.lower() == 'tools':
                        print(f"available tools: {len(agent.tools)}")
                        for tool in agent.tools:
                            tool_name = getattr(tool.metadata, 'name', str(tool))
                            tool_desc = getattr(tool.metadata, 'description', 'No description')
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
