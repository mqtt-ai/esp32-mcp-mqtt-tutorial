# LlamaIndex + AI Chat Web Application

A modern web-based chat application powered by LlamaIndex and DashScope (Qwen) models, providing an intuitive interface for AI conversations with personality-driven responses optimized for Chinese language and TTS output.

## Features

- 🌐 Modern web interface with real-time streaming responses via Server-Sent Events
- 🤖 Support for DashScope (Qwen) models with Chinese language optimization
- 🎭 Six AI personality roles with five distinct communication styles
- 🔄 Dynamic role-personality pairing system
- 📝 Conversation history management with configurable limits
- ⚙️ Interactive configuration and role management pages
- 🔒 Secure environment variable management with validation
- 🛠️ Based on LlamaIndex framework with Flask web server
- 🎵 TTS (Text-to-Speech) output optimization with formatting constraints

## Install Dependencies

### Using uv (Recommended)
```bash
# Install uv if not already installed
curl -LsSf https://astral.sh/uv/install.sh | sh

# Sync all dependencies (including web components)
uv sync
```

### Using pip (Alternative)
```bash
# Install project dependencies
pip install -e .

# Or install dependency packages directly  
pip install llama-index llama-index-llms-dashscope python-dotenv dashscope flask flask-cors

# Install development dependencies (optional)
pip install ruff
```

## Configuration

### 1. Get API Key

First, you need to get an API key for your AI model service:

1. Visit your AI model service provider (e.g., Alibaba Cloud DashScope Platform)
2. Register/Login to your account
3. Create an API Key

### 2. Set Environment Variables

**Method 1: Create .env file**

Create a `.env` file in the project root directory:

```
DASHSCOPE_API_KEY=your_api_key_here
DASHSCOPE_API_BASE=https://dashscope.aliyuncs.com/compatible-mode/v1
MODEL_NAME=qwen-turbo
TEMPERATURE=0.7
MAX_TOKENS=2048
MAX_HISTORY_LENGTH=20
ASSISTANT_NAME=AI Assistant
DEBUG=false
```

**Method 2: Set system environment variables**

```bash
export DASHSCOPE_API_KEY=your_api_key_here
export DASHSCOPE_API_BASE=https://dashscope.aliyuncs.com/compatible-mode/v1
export MODEL_NAME=qwen-turbo
export TEMPERATURE=0.7
export MAX_TOKENS=2048
```

## Usage

### Start Web Application

```bash
# Start with default settings
uv run app.py

# Custom port
uv run app.py --port 8080

# Enable debug mode
uv run app.py --debug

# Custom host and port
uv run app.py --host localhost --port 9000
```

### Web Interface Pages

- **Home** (`/`): Welcome page with feature overview
- **Chat** (`/chat`): Interactive chat interface with streaming responses  
- **Configuration** (`/config`): Role management, history, and settings

### API Endpoints

The application provides comprehensive RESTful API endpoints:

- `POST /api/chat/stream` - Streaming chat responses via Server-Sent Events
- `GET /api/roles` - Get all available roles with details
- `POST /api/roles/<role_name>` - Switch to a specific role
- `GET /api/history` - Get conversation history
- `DELETE /api/history` - Clear conversation history
- `GET /api/config` - Get configuration information
- `GET /api/role-info` - Get current role information
- `GET /api/personalities` - Get all available personalities
- `POST /api/custom-pairing` - Apply custom role-personality pairing

### Quick API Test

```bash
# Send a chat message (streaming)
curl -N -X POST http://localhost:3033/api/chat/stream \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{"message": "Hello!"}'

# Switch to entertainer role
curl -X POST http://localhost:3033/api/roles/entertainer

# Apply custom role-personality pairing
curl -X POST http://localhost:3033/api/custom-pairing \
  -H "Content-Type: application/json" \
  -d '{"role": "travel", "personality_id": "humorous_friend"}'

# Get all personalities
curl -X GET http://localhost:3033/api/personalities

# Clear conversation history
curl -X DELETE http://localhost:3033/api/history
```

## Supported Models

The application supports DashScope (Qwen) models optimized for Chinese language interactions. You can switch models by setting the `MODEL_NAME` environment variable:

- `qwen-turbo` - Fast and efficient (default)
- `qwen-plus` - Enhanced capabilities  
- `qwen-max` - Maximum performance

## Available Roles

The application includes six predefined AI roles:

- **default** (贴心生活助手) - Caring life assistant with warm personality
- **travel** (旅游规划助手) - Travel planning expert with enthusiastic explorer personality
- **english** (英文学习教练) - English learning coach with patient mentor personality
- **entertainer** (幽默的朋友) - Humorous entertainment expert with fun personality
- **nutrition** (家庭营养师) - Family nutrition expert with caring personality
- **feynman** (知识巩固教练) - Feynman learning method coach with Socratic teacher personality

## Available Personalities

The system includes five distinct personality types:

- **warm_caring** (温柔助理型) - Gentle, caring, supportive communication style
- **enthusiastic_explorer** (热情探索型) - Excited, adventurous, energetic communication
- **patient_mentor** (耐心导师型) - Patient, encouraging, educational guidance
- **humorous_friend** (幽默伙伴型) - Fun, playful, humor-filled interactions
- **socratic_teacher** (苏格拉底式导师型) - Question-based, thoughtful, reflective teaching

Roles and personalities can be mixed and matched via the web interface or `/api/custom-pairing` endpoint.

## Troubleshooting

### Common Issues

1. **ImportError: No module named 'llama_index'**
   - Solution: Run `pip install llama-index`

2. **API Key Error**  
   - Check if the `DASHSCOPE_API_KEY` in the `.env` file is correct
   - Ensure API key starts with 'sk-' or 'tk-' prefix
   - Confirm that the API key is valid and has sufficient quota

3. **Network Connection Issues**
   - Check network connection
   - Confirm that `dashscope.aliyuncs.com` is accessible
   - Verify firewall settings allow outbound HTTPS connections

4. **Port Already in Use**
   - Use `--port` argument: `uv run app.py --port 8080`
   - Check if another application is using port 3033

## Project Structure

```
blog_5/
├── app.py              # Flask web application (main entry point)  
├── chatbot.py          # Core chatbot logic with LlamaIndex integration
├── config.py           # Configuration management with validation
├── roles.py            # AI personality roles and TTS constraints
├── personality.py      # Personality system with communication styles
├── templates/          # HTML templates for web interface
│   ├── base.html       # Base template with Bootstrap
│   ├── index.html      # Welcome/home page
│   ├── chat.html       # Interactive chat interface
│   ├── config.html     # Role and history management
│   └── error.html      # Error pages
├── static/             # CSS, JavaScript assets
│   ├── css/style.css   # Custom styling
│   └── js/main.js      # Frontend chat logic
├── pyproject.toml      # Project configuration and dependencies
├── uv.lock            # Dependency lock file
└── .env               # Environment variables (create yourself)
```

## Development

The project uses a modular architecture:
- **Flask web framework** for the HTTP server with CORS support
- **LlamaIndex** for LLM integration with DashScope
- **Bootstrap 5** for modern responsive UI
- **Server-Sent Events (SSE)** for real-time streaming chat responses
- **Modular role-personality system** for dynamic AI behavior
- **TTS optimization** with Chinese language support
- **Environment-based configuration** with comprehensive validation

### Code Quality

The project includes development tools for maintaining code quality:

```bash
# Check code formatting and style
uv run ruff check

# Auto-format code  
uv run ruff format
```

## License

This project is for learning and research purposes only. Please ensure compliance with your AI model service provider's terms before use.
