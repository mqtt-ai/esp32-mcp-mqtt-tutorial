import os
from fastapi import FastAPI, HTTPException, BackgroundTasks
from pydantic import BaseModel
import logging
from openai import OpenAI

logger = logging.getLogger(__name__)


class VisionRequest(BaseModel):
    """
    Vision request model for incoming image and text data.

    示例请求体:
    {
        "role": "user",
        "content": [
            {"type": "text", "text": "这些是什么"},
            {
                "type": "image_url",
                "image_url": {
                    "url": "https://dashscope.oss-cn-beijing.aliyuncs.com/images/dog_and_girl.jpeg"
                }
            },
        ]
    }
    """

    role: str
    content: list[dict]


class VisionResponse(BaseModel):
    """Vision response model for API responses"""

    response: str


# Initialize FastAPI app
app = FastAPI(
    title="ESP32 AI Vision Assistant API",
    description="Web Server for ESP32 Vision assistant with AI integration",
    version="1.0.0",
)


@app.post("/explain_photo", response_model=VisionResponse)
async def process_audio(request: VisionRequest):
    try:
        return get_response(request)

    except Exception as e:
        logger.error(f"API processing error: {e}", exc_info=True)
        raise HTTPException(status_code=500, detail=f"Processing failed: {str(e)}")


def get_response(request: VisionRequest) -> VisionResponse:
    client = OpenAI(
        api_key=os.getenv("DASHSCOPE_API_KEY"),
        base_url="https://dashscope.aliyuncs.com/compatible-mode/v1",
    )
    completion = client.chat.completions.create(
        model="qwen-vl-plus",
        messages=[request],
    )
    # 提取第一个 choice 的 message.content 字段
    content = ""
    if completion.choices and hasattr(completion.choices[0], "message"):
        content = completion.choices[0].message.content
    return VisionResponse(response=content)


if __name__ == "__main__":
    import uvicorn

    uvicorn.run("vision_server:app", host="0.0.0.0", port=8001, reload=True)
