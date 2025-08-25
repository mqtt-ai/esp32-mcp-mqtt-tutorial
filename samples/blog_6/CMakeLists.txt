# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.16)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# 将图片文件嵌入到固件中
target_embed_file(${COMPONENT_TARGET} "image.jpg" ALIAS "image_jpg")
project(esp32-demo)
