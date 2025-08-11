# LlamaIndex + AI Chat Web Application

A modern web-based chat application powered by LlamaIndex and AI models, providing an intuitive interface for AI conversations.

## Features

- 🌐 Modern web interface with real-time streaming responses
- 🤖 Support for multiple AI models
- 🎭 Multiple AI personality roles (assistant, entertainer, advisor, etc.)
- 📝 Conversation history management
- ⚙️ Interactive configuration page
- 🔒 Secure environment variable management
- 🛠️ Based on LlamaIndex framework

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
OPENAI_API_KEY=your_api_key_here
OPENAI_API_BASE=https://dashscope.aliyuncs.com/compatible-mode/v1
```

**Method 2: Set system environment variables**

```bash
export OPENAI_API_KEY=your_api_key_here
export OPENAI_API_BASE=https://dashscope.aliyuncs.com/compatible-mode/v1
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

The application provides RESTful API endpoints:

- `POST /api/chat/stream` - Streaming chat responses  
- `GET /api/roles` - Get available roles
- `POST /api/roles/<role_name>` - Switch to a role
- `GET /api/history` - Get conversation history
- `DELETE /api/history` - Clear conversation history
- `GET /api/config` - Get configuration info

### Quick API Test

```bash
# Send a chat message (streaming)
curl -N -X POST http://localhost:3033/api/chat/stream \
  -H "Content-Type: application/json" \
  -H "Accept: text/event-stream" \
  -d '{"message": "Hello!"}'

# Switch to entertainer role
curl -X POST http://localhost:3033/api/roles/entertainer
```

## Supported Models

The application supports various AI models depending on your configured service provider. You can switch models by setting the `MODEL_NAME` environment variable.

## Available Roles

The application includes several AI personality roles:

- **default** - Caring life assistant
- **travel** - Travel planning assistant  
- **english** - English professor assistant
- **entertainer** - Lively entertainment expert
- **nutrition** - Nutrition pairing expert
- **feynman** - Feynman learning method practice assistant

Switch roles through the web interface or API.

## Troubleshooting

### Common Issues

1. **ImportError: No module named 'llama_index'**
   - Solution: Run `pip install llama-index`

2. **API Key Error**
   - Check if the API key in the `.env` file is correct
   - Confirm that the API key is valid and has sufficient quota

3. **Network Connection Issues**
   - Check network connection
   - Confirm that `dashscope.aliyun.com` is accessible

4. **Port Already in Use**
   - Use `--port` argument: `uv run app.py --port 8080`
   - Check if another application is using port 3033

## Project Structure

```
blog_5/
├── app.py              # Flask web application (main entry point)
├── chatbot.py          # Core chatbot logic
├── config.py           # Configuration management
├── roles.py            # AI personality roles
├── templates/          # HTML templates
├── static/             # CSS, JavaScript assets
├── pyproject.toml      # Project configuration
└── .env               # Environment variables (create yourself)
```

## Development

The project uses a modular architecture:
- **Flask web framework** for the HTTP server
- **LlamaIndex** for LLM integration
- **Bootstrap** for responsive UI
- **Real-time streaming** for chat responses

## License

This project is for learning and research purposes only. Please ensure compliance with your AI model service provider's terms before use.
