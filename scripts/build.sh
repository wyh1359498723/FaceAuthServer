#!/bin/bash

# Face Authentication Server build script
# This script builds the face authentication server

# Stop on errors
set -e

# Get the directory containing the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build"
mkdir -p "$BUILD_DIR"

# Create models directory
MODELS_DIR="$PROJECT_ROOT/models"
mkdir -p "$MODELS_DIR"

# Download face detection model if not exists
FACE_CASCADE="$MODELS_DIR/haarcascade_frontalface_default.xml"
if [ ! -f "$FACE_CASCADE" ]; then
    echo "下载人脸检测模型..."
    wget -O "$FACE_CASCADE" https://github.com/opencv/opencv/raw/master/data/haarcascades/haarcascade_frontalface_default.xml
fi

# Configure and build
cd "$BUILD_DIR"
echo "配置构建..."
cmake -DCMAKE_BUILD_TYPE=Release "$PROJECT_ROOT"

echo "构建..."
cmake --build . -- -j$(nproc)

echo "构建完成!"
echo "你可以使用: $PROJECT_ROOT/scripts/run_server.sh 运行服务器" 