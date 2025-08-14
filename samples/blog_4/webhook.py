"""
ESP32 AI Voice Assistant Webhook Server

This server receives audio data from ESP32 via EMQX data integration,
processes it with AI models, and sends back generated audio responses.

Architecture:
ESP32 -> EMQX -> Webhook -> AI Model -> EMQX -> ESP32
"""

import base64
import logging
import os
import uuid
from typing import Optional

import httpx
import lameenc
import numpy as np
import uvicorn
from fastapi import FastAPI, HTTPException, BackgroundTasks
from openai import AsyncOpenAI
from pydantic import BaseModel


# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)

# Initialize FastAPI app
app = FastAPI(
    title="ESP32 AI Voice Assistant API",
    description="Webhook server for ESP32 voice assistant with AI integration",
    version="1.0.0"
)

# EMQX HTTP API Configuration - Using public broker
EMQX_HTTP_API_URL = "http://127.0.0.1:18083"
EMQX_USERNAME = os.getenv("EMQX_USERNAME", "admin")  # Use environment variables
EMQX_PASSWORD = os.getenv("EMQX_PASSWORD", "public")

# MQTT Configuration
MQTT_TOPIC = "emqx/esp32/playaudio"
MQTT_CLIENT_ID = f"ai-server-{uuid.uuid4()}"

# AsyncOpenAI Client Configuration
openai_client = AsyncOpenAI(
    api_key=os.getenv("OPENAI_API_KEY", "sk-673580f7138e4193964b734a259582"),
    # Use environment variable
    base_url="https://dashscope.aliyuncs.com/compatible-mode/v1"
)

# AI Model Prompt
OPENAI_PROMPT = """
In this conversation, you will act as a simple emotional assistant. 
Based on the voice I provide, generate a concise response.
Please note that the response should not exceed 20 characters and the content 
should be an emotional analysis or response to the voice content.
"""


class AudioRequest(BaseModel):
    """Audio request model for incoming audio data"""
    audio: str  # Base64 encoded audio data


class AudioResponse(BaseModel):
    """Audio response model for API responses"""
    success: bool
    message: str


class TestMqttPublishRequest(BaseModel):
    """Test MQTT publish request model"""
    audio_path: str


@app.get("/ping")
async def ping():
    """
    Health check endpoint

    Returns:
        dict: Simple pong response
    """
    return {"message": "pong", "status": "healthy"}


@app.post("/process_audio", response_model=AudioResponse)
async def process_audio(request: AudioRequest, background_tasks: BackgroundTasks):
    """
    Main API endpoint for processing audio from ESP32

    This endpoint receives audio data from EMQX data integration,
    processes it with AI, and sends back generated audio.

    Args:
        request: AudioRequest containing base64 encoded audio
        background_tasks: FastAPI background tasks manager

    Returns:
        AudioResponse: Processing status response
    """
    try:
        # Generate unique task ID for tracking
        task_id = str(uuid.uuid4())

        # Add background task for audio processing
        background_tasks.add_task(
            process_audio_task,
            request.audio,
            task_id
        )

        logger.info(f"Audio processing request received, Task ID: {task_id}")
        return AudioResponse(
            success=True,
            message="Audio processing task started"
        )

    except Exception as e:
        logger.error(f"API processing error: {e}", exc_info=True)
        raise HTTPException(
            status_code=500,
            detail=f"Processing failed: {str(e)}"
        )


@app.post("/test_mqtt_publish", response_model=AudioResponse)
async def test_mqtt_publish(request: TestMqttPublishRequest):
    """
    Test endpoint for MQTT publishing

    Args:
        request: TestMqttPublishRequest containing audio file path

    Returns:
        AudioResponse: Publishing status response
    """
    try:
        # Read audio file and encode to base64
        with open(request.audio_path, 'rb') as f:
            audio_data = f.read()

        base64_audio = base64.b64encode(audio_data).decode()

        # Publish to MQTT
        result = await publish_to_mqtt(base64_audio)

        if result:
            return AudioResponse(success=True, message="MQTT publish successful")
        else:
            raise HTTPException(status_code=500, detail="MQTT publish failed")

    except FileNotFoundError:
        raise HTTPException(status_code=404, detail="Audio file not found")
    except Exception as e:
        logger.error(f"Test MQTT publish error: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail=f"Test failed: {str(e)}")


async def call_qwen_ai_generate_audio(base64_audio: str) -> Optional[bytes]:
    """
    Call Alibaba Qwen AI to generate audio response

    Args:
        base64_audio: Base64 encoded input audio

    Returns:
        bytes: Generated audio data or None if failed
    """
    try:
        completion = await openai_client.chat.completions.create(
            model="qwen-omni-turbo",
            messages=[
                {
                    "role": "user",
                    "content": [
                        {
                            "type": "input_audio",
                            "input_audio": {
                                "data": f"data:;base64,{base64_audio}",
                                "format": "wav",
                            },
                        },
                        {
                            "type": "text",
                            "text": OPENAI_PROMPT,
                        },
                    ],
                },
            ],
            modalities=["text", "audio"],
            audio={"voice": "Cherry", "format": "wav"},
            stream=True,
            stream_options={"include_usage": True},
        )

        text_response = ""
        audio_response = ""

        # Process streaming response
        async for chunk in completion:
            if not chunk.choices:
                continue

            # Collect audio data
            if hasattr(chunk.choices[0].delta, "audio") and chunk.choices[0].delta.audio:
                audio_response += chunk.choices[0].delta.audio["data"]

            # Collect text response
            elif hasattr(chunk.choices[0].delta, "content") and chunk.choices[0].delta.content:
                text_response += chunk.choices[0].delta.content

        logger.info(f"Qwen AI Response: {text_response}")

        if audio_response:
            decoded_audio = base64.b64decode(audio_response)
            return decoded_audio
        else:
            logger.warning("No audio response received from AI")
            return None

    except Exception as e:
        logger.error(f"Qwen AI API error: {e}", exc_info=True)
        return None


def convert_audio_to_mp3(decoded_audio: bytes, output_file: str) -> Optional[str]:
    """
    Convert audio data to MP3 format

    Args:
        decoded_audio: Raw audio data
        output_file: Output MP3 file path

    Returns:
        str: Base64 encoded MP3 data or None if failed
    """
    try:
        # Convert audio to numpy array
        audio_np = np.frombuffer(decoded_audio, dtype=np.int16)

        # Initialize MP3 encoder
        encoder = lameenc.Encoder()
        encoder.set_bit_rate(32)
        encoder.set_in_sample_rate(24000)
        encoder.set_channels(1)
        encoder.set_quality(7)

        # Encode to MP3
        mp3_data = encoder.encode(audio_np.tobytes())
        mp3_data += encoder.flush()

        # Save to file
        with open(output_file, 'wb') as f:
            f.write(mp3_data)

        # Return base64 encoded MP3 data
        base64_audio = base64.b64encode(mp3_data).decode()
        return base64_audio

    except Exception as e:
        logger.error(f"Audio conversion error: {e}", exc_info=True)
        return None


async def publish_to_mqtt(audio_data: str) -> bool:
    """
    Publish audio data to MQTT topic via EMQX HTTP API

    Args:
        audio_data: Base64 encoded audio data

    Returns:
        bool: True if successful, False otherwise
    """
    try:
        publish_url = f"{EMQX_HTTP_API_URL}/api/v5/publish"
        headers = {
            "Content-Type": "application/json",
        }
        payload = {
            "topic": MQTT_TOPIC,
            "payload": audio_data,
            "payload_encoding": "base64",
        }

        # Use HTTP basic authentication
        http_auth = httpx.BasicAuth(EMQX_USERNAME, EMQX_PASSWORD)

        async with httpx.AsyncClient(auth=http_auth, timeout=30.0) as client:
            response = await client.post(publish_url, json=payload, headers=headers)

            if response.status_code in [200, 202]:
                logger.info(f"Audio published to MQTT topic: {MQTT_TOPIC}")
                return True
            else:
                logger.error(f"MQTT publish failed: {response.status_code} - {response.text}")
                return False

    except httpx.TimeoutException:
        logger.error("MQTT publish timeout")
        return False
    except Exception as e:
        logger.error(f"MQTT publish error: {e}", exc_info=True)
        return False


async def process_audio_task(base64_audio: str, task_id: str) -> None:
    """
    Background task for processing audio and publishing to MQTT

    Args:
        base64_audio: Base64 encoded input audio
        task_id: Unique task identifier for tracking
    """
    try:
        logger.info(f"Starting audio processing task: {task_id}")

        # Generate AI audio response
        decoded_audio = await call_qwen_ai_generate_audio(base64_audio)
        if not decoded_audio:
            logger.error(f"Task {task_id}: Audio generation failed")
            return

        # Convert to MP3 format
        output_file = f"audio_response_{task_id}.mp3"
        base64_mp3_audio = convert_audio_to_mp3(decoded_audio, output_file)

        if base64_mp3_audio:
            # Publish to MQTT
            success = await publish_to_mqtt(base64_mp3_audio)
            if success:
                logger.info(f"Task {task_id}: Audio processing completed successfully")
            else:
                logger.error(f"Task {task_id}: MQTT publishing failed")
        else:
            logger.error(f"Task {task_id}: Audio conversion failed")

        # Clean up temporary file
        try:
            if os.path.exists(output_file):
                os.remove(output_file)
                logger.debug(f"Temporary file {output_file} cleaned up")
        except Exception as cleanup_error:
            logger.warning(f"Failed to clean up {output_file}: {cleanup_error}")

    except Exception as e:
        logger.error(f"Audio processing task error: {e}", exc_info=True)
        raise e


@app.on_event("startup")
async def startup_event():
    """FastAPI startup event handler"""
    logger.info("ESP32 AI Voice Assistant Webhook Server starting up...")
    logger.info(f"EMQX Broker: {EMQX_HTTP_API_URL}")
    logger.info(f"MQTT Topic: {MQTT_TOPIC}")


@app.on_event("shutdown")
async def shutdown_event():
    """FastAPI shutdown event handler"""
    logger.info("ESP32 AI Voice Assistant Webhook Server shutting down...")


if __name__ == "__main__":
    # Run the server
    uvicorn.run(
        "webhook:app",
        host="0.0.0.0",
        port=int(os.getenv("PORT", 5005)),
        reload=bool(os.getenv("RELOAD", True)),
        log_level=os.getenv("LOG_LEVEL", "info").lower()
    )
