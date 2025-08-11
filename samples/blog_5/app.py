"""
Flask Web Application for ChatBot
Provides web interfaces for chat and configuration management
"""

import os
import json
from flask import Flask, render_template, request, jsonify, Response
from flask_cors import CORS
from chatbot import ChatBot
from personality import personality_manager

app = Flask(__name__)
app.secret_key = os.environ.get("SECRET_KEY", "your-secret-key-change-this")
CORS(app)  # Enable CORS for API endpoints

# Global chatbot instance
chatbot = None


def initialize_chatbot():
    """Initialize chatbot with error handling"""
    global chatbot
    try:
        chatbot = ChatBot()
        return True
    except Exception as e:
        print(f"Failed to initialize chatbot: {e}")
        return False


@app.before_request
def ensure_chatbot():
    """Ensure chatbot is initialized before each request"""
    global chatbot
    if chatbot is None:
        if not initialize_chatbot():
            print("Warning: Chatbot initialization failed. Some features may not work.")


@app.route("/")
def index():
    """Home page - redirect to chat"""
    return render_template("index.html")


@app.route("/chat")
def chat_page():
    """Chat interface page"""
    if chatbot is None:
        return render_template(
            "error.html", error="Chatbot not initialized. Please check configuration."
        )

    role_info = chatbot.get_role_info()
    return render_template(
        "chat.html",
        current_role=role_info.get(
            "current_role_description", role_info["current_role"]
        ),
        assistant_name=chatbot.config.assistant_name,
    )


@app.route("/config")
def config_page():
    """Configuration and role management page"""
    if chatbot is None:
        return render_template(
            "error.html", error="Chatbot not initialized. Please check configuration."
        )

    role_info = chatbot.get_role_info()
    roles_info = chatbot.list_available_roles()
    config_info = chatbot.get_config_info()
    history = chatbot.get_conversation_history()

    # Get personality information
    personalities = personality_manager.list_personalities()
    personalities_data = {}
    for pid, personality in personality_manager.personalities.items():
        personalities_data[pid] = {
            "name": personality.name,
            "system_prompt": personality.get_system_prompt(),
            "behavior_guide": personality.get_behavior_guide(),
            "response_style": personality.get_response_style(),
        }

    return render_template(
        "config.html",
        role_info=role_info,
        roles=roles_info["roles"],
        config=config_info,
        history=history,
        history_count=len(history),
        personalities=personalities,
        personalities_data=personalities_data,
    )


# API Endpoints


@app.route("/api/chat/stream", methods=["POST"])
def api_chat_stream():
    """API endpoint for streaming chat messages"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    data = request.get_json()
    if not data or "message" not in data:
        return jsonify({"error": "Message is required"}), 400

    message = data["message"].strip()
    if not message:
        return jsonify({"error": "Message cannot be empty"}), 400

    if len(message) > 2000:
        return jsonify(
            {"error": "Message too long, please limit to 2000 characters"}
        ), 400

    def generate():
        try:
            for chunk in chatbot.stream_chat(message):
                yield f"data: {json.dumps({'chunk': chunk, 'done': False})}\n\n"
            yield f"data: {json.dumps({'chunk': '', 'done': True})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e)})}\n\n"

    resp = Response(generate(), mimetype="text/event-stream")
    # Disable buffering and caching to ensure progressive delivery
    resp.headers["Cache-Control"] = "no-cache"
    resp.headers["X-Accel-Buffering"] = "no"
    resp.headers["Connection"] = "keep-alive"
    return resp


@app.route("/api/roles", methods=["GET"])
def api_get_roles():
    """API endpoint to get all available roles"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        roles_info = chatbot.list_available_roles()
        return jsonify(roles_info)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/roles/<role_name>", methods=["POST"])
def api_switch_role(role_name):
    """API endpoint to switch chatbot role"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        result = chatbot.change_role(role_name)
        if result["success"]:
            return jsonify(result)
        else:
            return jsonify(result), 400
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/history", methods=["GET"])
def api_get_history():
    """API endpoint to get conversation history"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        history = chatbot.get_conversation_history()
        return jsonify({"history": history, "count": len(history)})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/history", methods=["DELETE"])
def api_clear_history():
    """API endpoint to clear conversation history"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        chatbot.reset_conversation()
        return jsonify({"success": True, "message": "Conversation history cleared"})
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/config", methods=["GET"])
def api_get_config():
    """API endpoint to get configuration info"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        config_info = chatbot.get_config_info()
        return jsonify(config_info)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/role-info", methods=["GET"])
def api_get_role_info():
    """API endpoint to get current role information"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    try:
        role_info = chatbot.get_role_info()
        return jsonify(role_info)
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/personalities", methods=["GET"])
def api_get_personalities():
    """API endpoint to get all available personalities"""
    try:
        personalities = personality_manager.list_personalities()
        personalities_data = {}
        for pid, personality in personality_manager.personalities.items():
            personalities_data[pid] = {
                "name": personality.name,
                "system_prompt": personality.get_system_prompt(),
                "behavior_guide": personality.get_behavior_guide(),
                "response_style": personality.get_response_style(),
            }

        return jsonify(
            {"personalities": personalities, "personalities_data": personalities_data}
        )
    except Exception as e:
        return jsonify({"error": str(e)}), 500


@app.route("/api/custom-pairing", methods=["POST"])
def api_apply_custom_pairing():
    """API endpoint to apply custom role-personality pairing"""
    if chatbot is None:
        return jsonify({"error": "Chatbot not initialized"}), 500

    data = request.get_json()
    if not data or "role" not in data or "personality_id" not in data:
        return jsonify({"error": "Role and personality_id are required"}), 400

    role_name = data["role"]
    personality_id = data["personality_id"]

    try:
        result = chatbot.apply_custom_pairing(role_name, personality_id)
        if result["success"]:
            return jsonify(result)
        else:
            return jsonify(result), 400
    except Exception as e:
        return jsonify({"error": str(e)}), 500


# Error handlers


@app.errorhandler(404)
def not_found(error):
    """Handle 404 errors"""
    return render_template("error.html", error="Page not found"), 404


@app.errorhandler(500)
def internal_error(error):
    """Handle 500 errors"""
    return render_template("error.html", error="Internal server error"), 500


def main():
    """Main function for running the web application"""
    import argparse

    parser = argparse.ArgumentParser(description="ChatBot - Web Interface")
    parser.add_argument(
        "--port", type=int, default=3033, help="Port for web server (default: 3033)"
    )
    parser.add_argument(
        "--host", default="0.0.0.0", help="Host for web server (default: 0.0.0.0)"
    )
    parser.add_argument("--debug", action="store_true", help="Enable debug mode")

    args = parser.parse_args()

    # Initialize chatbot
    if initialize_chatbot():
        print("✅ Chatbot initialized successfully")
        print("🌐 Starting web server...")
        print(f"🔗 Chat interface: http://localhost:{args.port}/chat")
        print(f"⚙️ Configuration: http://localhost:{args.port}/config")
        print(f"🏠 Home page: http://localhost:{args.port}/")
    else:
        print("❌ Warning: Chatbot initialization failed")
        print("💡 Please check your .env file and API key configuration")
        print("🌐 Starting web server anyway...")

    # Run the app
    app.run(debug=args.debug, host=args.host, port=args.port, threaded=True)


if __name__ == "__main__":
    main()
