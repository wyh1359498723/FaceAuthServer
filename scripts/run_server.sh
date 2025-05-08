#!/bin/bash

# 人脸认证服务器运行脚本
# 此脚本运行人脸认证服务器，使用配置文件

# 遇到错误时停止
set -e

# 获取脚本所在目录
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# 默认值
CONFIG_FILE="$PROJECT_ROOT/config.json"
PORT=8101

# 检查配置文件是否存在
if [ ! -f "$CONFIG_FILE" ]; then
    echo "错误: 配置文件未找到: $CONFIG_FILE"
    exit 1
fi

# 检查构建目录是否存在
BUILD_DIR="$PROJECT_ROOT/build"
if [ ! -d "$BUILD_DIR" ]; then
    echo "错误: 构建目录未找到: $BUILD_DIR"
    echo "请先使用: $SCRIPT_DIR/build.sh 构建项目"
    exit 1
fi

# 检查可执行文件是否存在
EXECUTABLE="$BUILD_DIR/bin/face_auth_server"
if [ ! -x "$EXECUTABLE" ]; then
    echo "错误: 可执行文件未找到或不可执行: $EXECUTABLE"
    echo "请先使用: $SCRIPT_DIR/build.sh 构建项目"
    exit 1
fi

# 运行服务器
echo "开始运行人脸认证服务器..."
echo "配置文件: $CONFIG_FILE"
echo "端口: $PORT"
echo 

# 使用stdbuf确保输出不缓冲
stdbuf -o0 "$EXECUTABLE" "$CONFIG_FILE" "$PORT" 

