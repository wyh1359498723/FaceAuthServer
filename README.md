# 人脸认证服务器 (C++)

基于C++实现的人脸认证系统，允许用户通过人脸进行注册和登录。系统使用OpenCV进行人脸检测和识别，MySQL进行数据存储。

## 功能特性

- 支持用户人脸数据注册
- 支持密码和人脸双重认证
- 使用MySQL数据库存储用户数据和认证日志
- 提供RESTful API供客户端应用程序调用

## 环境要求

- C++11或更高版本
- OpenCV 3.4或更高版本（需包含人脸识别contrib模块）
- MySQL客户端库
- JsonCpp用于JSON解析
- CMake 3.10或更高版本

## 编译说明

1. 安装依赖：

```bash
# Ubuntu/Debian
sudo apt-get install cmake libopencv-dev libopencv-contrib-dev libmysqlclient-dev libjsoncpp-dev

# CentOS/RHEL
sudo yum install cmake opencv-devel opencv-contrib-devel mysql-devel jsoncpp-devel
```

2. 克隆仓库：

```bash
git clone https://github.com/yourusername/face_auth_cpp.git
cd face_auth_cpp
```

3. 编译项目：

```bash
mkdir -p build
cd build
cmake ..
make
```

或使用提供的编译脚本：

```bash
./scripts/build.sh
```

## 配置说明

创建一个`config.json`文件，结构如下：

```json
{
  "model_path": "models/haarcascade_frontalface_default.xml",
  "database": {
    "host": "localhost",
    "user": "your_username",
    "password": "your_password",
    "name": "face_auth_db"
  },
  "face_recognition": {
    "similarity_threshold": 80.0
  }
}
```

## 运行服务器

启动服务器：

```bash
./build/face_auth_server config.json 8101
```

或使用提供的运行脚本：

```bash
./scripts/run_server.sh --config config.json --port 8101
```

## API协议

服务器使用自定义二进制协议：

### 请求格式
- 4字节：头部标识（"FACE"）
- 4字节：JSON长度（网络字节序）
- JSON数据
- 人脸图像数据（如适用）

### 响应格式
- 4字节：头部标识（"RESP"）
- 4字节：JSON长度（网络字节序）
- JSON数据

### 请求类型

1. **注册**

```json
{
  "type": "register",
  "username": "user1",
  "password": "password",
  "face_data_size": 12345
}
```

2. **认证**

```json
{
  "type": "login",
  "username": "user1",
  "password": "password",
  "face_data_size": 12345
}
```

### 响应示例

1. **注册成功**

```json
{
  "type": "register",
  "success": true,
  "message": "用户注册成功"
}
```

2. **认证成功**

```json
{
  "type": "login",
  "success": true,
  "message": "认证成功",
  "face_verified": true
}
```

## 目录结构

- `/include` - 头文件
- `/src` - 源文件
- `/scripts` - 构建和运行脚本
- `/models` - 人脸检测模型
- `/face_auth_data` - 存储人脸数据和日志（注意：使用前需创建）

## 实现细节

此服务器支持：

1. **人脸检测**：使用OpenCV的Haar级联分类器
2. **人脸识别**：使用OpenCV的LBPH人脸识别器
3. **MySQL数据库**：存储用户数据、人脸图像和认证日志
4. **并发连接**：使用多线程处理多个客户端连接

## 使用说明

1. 首次使用需要设置数据库，创建相应的表结构
2. 确保配置文件中的数据库连接信息正确
3. 运行服务器后，客户端可以通过API进行注册和认证操作

## 许可证

MIT许可证 