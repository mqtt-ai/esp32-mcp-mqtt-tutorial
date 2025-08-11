"""
Configuration management module - simplified version
Centralized management of all chatbot configuration parameters
"""

import os
import logging
from typing import Optional
from dataclasses import dataclass
from dotenv import load_dotenv


@dataclass
class ChatConfig:
    """Chatbot configuration class"""

    # API-related configuration
    api_key: str
    api_base: Optional[str] = None
    model_name: str = "qwen-turbo"

    # Model parameters
    temperature: float = 0.7
    max_tokens: int = 2048

    # Conversation management
    max_history_length: int = 20

    # System configuration
    assistant_name: str = "AI Assistant"
    enable_debug: bool = False

    # Logging configuration
    enable_conversation_logging: bool = True
    log_level: str = "INFO"

    def __post_init__(self):
        """Post-initialization validation"""
        self._validate_config()

    def _validate_config(self):
        """Validate configuration parameters - simplified version"""
        if not self.api_key:
            raise ValueError("API key cannot be empty")

        if not self.api_key.startswith(("sk-", "tk-")):
            raise ValueError(
                "API key format is incorrect, should start with 'sk-' or 'tk-'"
            )

        # Basic range checks
        if not 0.0 <= self.temperature <= 2.0:
            raise ValueError(
                f"Temperature should be between 0.0-2.0, current value: {self.temperature}"
            )

        if not 1 <= self.max_tokens <= 32000:
            raise ValueError(
                f"Max tokens should be between 1-32000, current value: {self.max_tokens}"
            )

        if not self.assistant_name.strip():
            raise ValueError("Assistant name cannot be empty")

        # Validate log level
        valid_log_levels = ["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"]
        if self.log_level.upper() not in valid_log_levels:
            raise ValueError(f"Log level must be one of: {', '.join(valid_log_levels)}")

    @classmethod
    def from_env(cls, env_file: str = ".env") -> "ChatConfig":
        """Load configuration from environment variables"""
        # Load environment variables
        load_dotenv(env_file)

        return cls(
            api_key=cls._get_env_required("DASHSCOPE_API_KEY"),
            api_base=os.getenv("DASHSCOPE_API_BASE"),
            model_name=os.getenv("MODEL_NAME", "qwen-turbo"),
            temperature=cls._get_env_float("TEMPERATURE", 0.7),
            max_tokens=cls._get_env_int("MAX_TOKENS", 2048),
            max_history_length=cls._get_env_int("MAX_HISTORY_LENGTH", 20),
            assistant_name=os.getenv("ASSISTANT_NAME", "AI Assistant"),
            enable_debug=cls._get_env_bool("DEBUG", False),
            enable_conversation_logging=cls._get_env_bool(
                "ENABLE_CONVERSATION_LOGGING", True
            ),
            log_level=os.getenv("LOG_LEVEL", "INFO").upper(),
        )

    @staticmethod
    def _get_env_required(key: str) -> str:
        """Get required environment variable"""
        value = os.getenv(key)
        if not value:
            raise ValueError(f"Missing required environment variable: {key}")
        return value

    @staticmethod
    def _get_env_float(key: str, default: float) -> float:
        """Get float environment variable"""
        value = os.getenv(key)
        if not value:
            return default
        try:
            return float(value)
        except ValueError:
            print(
                f"⚠️ Environment variable {key} is not a valid number, using default: {default}"
            )
            return default

    @staticmethod
    def _get_env_int(key: str, default: int) -> int:
        """Get integer environment variable"""
        value = os.getenv(key)
        if not value:
            return default
        try:
            return int(value)
        except ValueError:
            print(
                f"⚠️ Environment variable {key} is not a valid integer, using default: {default}"
            )
            return default

    @staticmethod
    def _get_env_bool(key: str, default: bool) -> bool:
        """Get boolean environment variable"""
        value = os.getenv(key)
        if not value:
            return default
        return value.lower() in ("true", "1", "yes", "on")

    def to_dict(self) -> dict:
        """Convert to dictionary format"""
        return {
            "api_key": "***" + self.api_key[-4:]
            if len(self.api_key) > 4
            else "***",  # Hide API key
            "api_base": self.api_base,
            "model_name": self.model_name,
            "temperature": self.temperature,
            "max_tokens": self.max_tokens,
            "max_history_length": self.max_history_length,
            "assistant_name": self.assistant_name,
            "enable_debug": self.enable_debug,
            "enable_conversation_logging": self.enable_conversation_logging,
            "log_level": self.log_level,
        }

    def __str__(self) -> str:
        """String representation"""
        config_info = []
        config_info.append(f"Model: {self.model_name}")
        config_info.append(f"Temperature: {self.temperature}")
        config_info.append(f"Max Tokens: {self.max_tokens}")
        config_info.append(f"History Length: {self.max_history_length}")
        config_info.append(f"Assistant Name: {self.assistant_name}")
        if self.enable_debug:
            config_info.append("Debug Mode: Enabled")
        if self.enable_conversation_logging:
            config_info.append(f"Logging: {self.log_level}")

        return " | ".join(config_info)


def setup_logging(config: ChatConfig) -> logging.Logger:
    """Setup logging configuration based on config"""
    logger = logging.getLogger("chatbot")

    # Clear existing handlers
    logger.handlers.clear()

    # Set log level
    log_level = getattr(logging, config.log_level.upper())
    logger.setLevel(log_level)

    # Create console handler
    console_handler = logging.StreamHandler()
    console_handler.setLevel(log_level)

    # Create formatter
    formatter = logging.Formatter(
        fmt="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        datefmt="%Y-%m-%d %H:%M:%S",
    )
    console_handler.setFormatter(formatter)

    # Add handler to logger
    logger.addHandler(console_handler)

    return logger
