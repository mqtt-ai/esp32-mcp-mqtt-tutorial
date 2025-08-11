"""
ChatBot class - extracted from main.py
Chatbot implementation based on LlamaIndex and Qwen model
"""

import os
from typing import Optional
from llama_index.core import Settings
from llama_index.llms.dashscope import DashScope
from llama_index.core.llms import ChatMessage, MessageRole
from config import ChatConfig
from roles import (
    get_preset_roles,
    get_role_description,
    get_tts_constraints,
    get_role_prompt,
    get_role_info,
    ROLE_RESPONSIBILITIES,
)
from personality import personality_manager


class ChatBot:
    """Chatbot based on LlamaIndex and Qwen model"""

    def __init__(
        self,
        config: Optional[ChatConfig] = None,
        model_name: Optional[str] = None,
        api_key: Optional[str] = None,
        api_base: Optional[str] = None,
        temperature: Optional[float] = None,
        max_tokens: Optional[int] = None,
    ):
        """
        Initialize chatbot

        Args:
            config: Configuration object, takes priority if provided
            model_name: Qwen model name (e.g.: qwen-turbo, qwen-plus, qwen-max)
            api_key: API key
            api_base: API base URL
            temperature: Temperature parameter, controls randomness (0.0-2.0)
            max_tokens: Maximum token count, controls response length
        """
        # Configuration initialization - simplified version
        try:
            if config:
                self.config = config
            elif any([model_name, api_key, api_base, temperature, max_tokens]):
                # Create configuration using passed parameters
                self.config = ChatConfig(
                    api_key=api_key or os.getenv("DASHSCOPE_API_KEY", ""),
                    api_base=api_base,
                    model_name=model_name or "qwen-turbo",
                    temperature=temperature if temperature is not None else 0.7,
                    max_tokens=max_tokens if max_tokens is not None else 2048,
                )
            else:
                # Load from environment variables
                self.config = ChatConfig.from_env()
        except Exception as e:
            raise Exception(f"Configuration loading failed: {str(e)}")

        # Set simple debug mode
        self.debug_mode = getattr(self.config, "enable_debug", False)

        # Use default role and add TTS constraints
        default_role_prompt = get_role_prompt("default")
        self.system_prompt = self._build_complete_prompt(default_role_prompt)

        # Track current role and personality
        self.current_role = "default"
        self.current_personality_id = (
            get_role_info("default")["personality_id"]
            if get_role_info("default")
            else None
        )

        # For storing conversation history (using structured ChatMessage)
        self.conversation_history = []

        # Initialize model
        try:
            self.llm = DashScope(
                model_name=self.config.model_name,
                api_key=self.config.api_key,
                temperature=self.config.temperature,
                max_tokens=self.config.max_tokens,
                is_function_calling_model=False,  # Disable function calling feature
            )
        except Exception as e:
            raise Exception(f"Model initialization failed: {str(e)}")

        # Set global LLM
        Settings.llm = self.llm

    def _get_role_description(self) -> str:
        """Extract role description from system prompt"""
        # Check if matches preset roles (now need to consider complete prompt with TTS constraints)
        preset_roles = get_preset_roles()
        for role_name, role_prompt in preset_roles.items():
            complete_prompt = self._build_complete_prompt(role_prompt)
            if complete_prompt == self.system_prompt:
                return get_role_description(role_name)
        # If no preset role matched, return custom voice assistant
        return "Custom voice assistant"

    def _build_complete_prompt(self, role_prompt: str) -> str:
        """Build complete system prompt with TTS constraints"""
        # Format role name
        formatted_prompt = role_prompt.format(self.config.assistant_name)
        tts_constraints = get_tts_constraints()
        return f"{formatted_prompt}\n\n{tts_constraints}"

    def stream_chat(self, message: str):
        """
        Stream chat with LLM - generator function for web interface

        Args:
            message: User input message

        Yields:
            str: Streaming response chunks
        """
        try:
            # Build structured conversation messages
            messages = self._build_chat_messages(message)

            # Stream response from LLM
            response_text = ""
            for chunk in self.llm.stream_chat(messages):
                chunk_text = chunk.delta
                if chunk_text:
                    response_text += chunk_text
                    yield chunk_text

            # Record conversation history after streaming is complete
            user_msg = ChatMessage(
                role=MessageRole.USER, content=message, additional_kwargs={}
            )
            assistant_msg = ChatMessage(
                role=MessageRole.ASSISTANT, content=response_text, additional_kwargs={}
            )

            self.conversation_history.append(user_msg)
            self.conversation_history.append(assistant_msg)

        except Exception as e:
            yield f"Error occurred: {str(e)}"

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
            self.conversation_history[-self.config.max_history_length :]
            if len(self.conversation_history) > self.config.max_history_length
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

    def reset_conversation(self):
        """Reset conversation history"""
        self.conversation_history = []

    def get_role_info(self) -> dict:
        """Get current role information as dictionary"""
        role_desc = self._get_role_description()
        current_personality = (
            personality_manager.get_personality(self.current_personality_id)
            if self.current_personality_id
            else None
        )
        return {
            "current_role": self.current_role,  # Use role identifier, not description
            "current_role_description": role_desc,  # Add separate field for description
            "current_personality_id": self.current_personality_id,
            "current_personality_name": current_personality.name
            if current_personality
            else None,
            "system_prompt": self.system_prompt,
            "config": self.config.to_dict(),
        }

    def get_conversation_history(self) -> list:
        """Get conversation history as list of dictionaries"""
        history = []
        for message in self.conversation_history:
            history.append(
                {
                    "role": message.role.value,
                    "content": message.content,
                    "timestamp": None,  # Can be added later if needed
                }
            )
        return history

    def change_role(self, role_name: str) -> dict:
        """
        Switch chatbot role

        Returns:
            dict: Result with success status and message
        """
        preset_roles = get_preset_roles()
        if role_name in preset_roles:
            role_prompt = get_role_prompt(role_name)
            self.system_prompt = self._build_complete_prompt(role_prompt)
            self.current_role = role_name
            # Update current personality to match the role's default personality
            role_info = get_role_info(role_name)
            if role_info:
                self.current_personality_id = role_info["personality_id"]
            self.reset_conversation()  # Reset conversation history when switching roles
            return {
                "success": True,
                "message": f"Role switched to: {self._get_role_description()}",
                "current_role": self._get_role_description(),
            }
        else:
            available_roles = ", ".join(preset_roles.keys())
            return {
                "success": False,
                "message": f"Unknown role '{role_name}', available roles: {available_roles}",
                "available_roles": list(preset_roles.keys()),
            }

    def list_available_roles(self) -> dict:
        """List all available preset roles as dictionary"""
        preset_roles = get_preset_roles()
        roles_list = []
        for role_name in preset_roles.keys():
            role_info_data = get_role_info(role_name)
            personality_id = (
                role_info_data["personality_id"] if role_info_data else None
            )
            personality = (
                personality_manager.get_personality(personality_id)
                if personality_id
                else None
            )

            role_info = {
                "name": role_name,
                "description": get_role_description(role_name),
                "personality_name": personality.name if personality else None,
                "is_current": self.current_role == role_name,
            }
            roles_list.append(role_info)

        return {"roles": roles_list, "current_role": self._get_role_description()}

    def get_config_info(self) -> dict:
        """Get configuration information"""
        return {
            "model_name": self.config.model_name,
            "temperature": self.config.temperature,
            "max_tokens": self.config.max_tokens,
            "max_history_length": self.config.max_history_length,
            "assistant_name": self.config.assistant_name,
            "debug_mode": self.debug_mode,
            "current_role": self._get_role_description(),
        }

    def apply_custom_pairing(self, role_name: str, personality_id: str) -> dict:
        """
        Apply custom role-personality pairing

        Args:
            role_name: Role identifier
            personality_id: Personality identifier

        Returns:
            dict: Result with success status and message
        """
        # Validate role exists
        if role_name not in ROLE_RESPONSIBILITIES:
            available_roles = ", ".join(ROLE_RESPONSIBILITIES.keys())
            return {
                "success": False,
                "message": f"Unknown role '{role_name}', available roles: {available_roles}",
            }

        # Validate personality exists
        personality = personality_manager.get_personality(personality_id)
        if not personality:
            available_personalities = ", ".join(
                personality_manager.list_personalities().keys()
            )
            return {
                "success": False,
                "message": f"Unknown personality '{personality_id}', available personalities: {available_personalities}",
            }

        # Update role's personality temporarily (for this session)
        original_personality_id = ROLE_RESPONSIBILITIES[role_name]["personality_id"]
        ROLE_RESPONSIBILITIES[role_name]["personality_id"] = personality_id

        try:
            # Generate new role prompt with custom personality
            role_prompt = get_role_prompt(role_name)
            self.system_prompt = self._build_complete_prompt(role_prompt)
            self.current_role = role_name
            self.current_personality_id = personality_id
            self.reset_conversation()  # Reset conversation history when applying custom pairing

            role_desc = get_role_description(role_name)
            personality_name = personality.name

            return {
                "success": True,
                "message": f"Custom pairing applied: {role_desc} with {personality_name} personality",
                "current_role": role_desc,
                "current_personality": personality_name,
            }
        except Exception as e:
            # Restore original personality if there's an error
            ROLE_RESPONSIBILITIES[role_name]["personality_id"] = original_personality_id
            return {
                "success": False,
                "message": f"Failed to apply custom pairing: {str(e)}",
            }
