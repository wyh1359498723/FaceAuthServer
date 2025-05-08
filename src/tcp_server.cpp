#include "tcp_server.h"
#include "utils.h"
#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstring>
#include <vector>
#include <thread>
#include <sstream>
#include <json/json.h>
#include <fstream>  // 添加 fstream 头文件

// 协议相关常量
const size_t HEADER_SIZE = 8; // 消息头大小，用于存储消息长度
const size_t MAX_BUFFER_SIZE = 1024 * 1024 * 10; // 最大缓冲区大小（10MB）

TcpServer::TcpServer(AuthServer& auth_server, int port)
    : auth_server_(auth_server), port_(port), server_socket_(-1), running_(false) {
}

TcpServer::~TcpServer() {
    stop();
}

bool TcpServer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        std::cerr << "服务器已经在运行" << std::endl;
        return false;
    }
    
    // 创建套接字
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        std::cerr << "无法创建套接字: " << strerror(errno) << std::endl;
        return false;
    }
    
    // 设置地址重用选项
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "无法设置套接字选项: " << strerror(errno) << std::endl;
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // 绑定地址
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "无法绑定: " << strerror(errno) << std::endl;
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // 开始监听
    if (listen(server_socket_, 5) < 0) {
        std::cerr << "无法监听: " << strerror(errno) << std::endl;
        close(server_socket_);
        server_socket_ = -1;
        return false;
    }
    
    // 启动服务器线程
    running_ = true;
    server_thread_ = std::thread(&TcpServer::serverThread, this);
    
    std::cout << "TCP服务器在端口 " << port_ << " 启动" << std::endl;
    return true;
}

void TcpServer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    // 关闭服务器套接字
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
    
    // 等待服务器线程结束
    if (server_thread_.joinable()) {
        server_thread_.join();
    }
    
    // 等待所有客户端线程结束
    for (auto& thread : client_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    client_threads_.clear();
    
    std::cout << "TCP服务器已停止" << std::endl;
}

void TcpServer::serverThread() {
    while (running_) {
        // 设置超时
        struct timeval timeout;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(server_socket_, &read_fds);
        
        int activity = select(server_socket_ + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0 && errno != EINTR) {
            std::cerr << "Select错误: " << strerror(errno) << std::endl;
            break;
        }
        
        if (!running_) {
            break;
        }
        
        if (activity > 0 && FD_ISSET(server_socket_, &read_fds)) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket_, (struct sockaddr *)&client_addr, &client_len);
            if (client_socket < 0) {
                std::cerr << "接受失败: " << strerror(errno) << std::endl;
                continue;
            }
            
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "新连接来自 " << client_ip << ":" << ntohs(client_addr.sin_port) << std::endl;
            
            // 创建客户端线程
            client_threads_.push_back(std::thread(&TcpServer::clientHandler, this, client_socket));
        }
    }
}

void TcpServer::clientHandler(int client_socket) {
    // 设置接收超时
    struct timeval tv;
    tv.tv_sec = 60; // 60秒超时
    tv.tv_usec = 0;
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    
    try {
        // 确保必要的目录结构存在
        utils::createDirectories("face_auth_data/temp");
        
        // 处理来自客户端的请求
        Message message;
        if (receiveMessage(client_socket, message)) {
            processMessage(client_socket, message);
        }
    } catch (const std::exception& e) {
        std::cerr << "处理客户端错误: " << e.what() << std::endl;
        sendError(client_socket, std::string("服务器错误: ") + e.what());
    }
    
    // 关闭客户端连接
    close(client_socket);
    std::cout << "客户端连接已关闭" << std::endl;
}

bool TcpServer::receiveMessage(int client_socket, Message& message) {
    const int buffer_size = 8192;
    char buffer[buffer_size];
    std::vector<char> all_data;
    
    // 接收数据
    while (true) {
        int received = recv(client_socket, buffer, buffer_size, 0);
        
        if (received < 0) {
            // 接收出错
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                std::cerr << "套接字超时" << std::endl;
            } else {
                std::cerr << "接收错误: " << strerror(errno) << std::endl;
            }
            return false;
        }
        
        if (received == 0) {
            // 连接关闭
            std::cout << "客户端关闭连接" << std::endl;
            return false;
        }
        
        // 添加接收到的数据
        all_data.insert(all_data.end(), buffer, buffer + received);
        std::cout << "接收到 " << received << " 字节的数据，总计 " << all_data.size() << " 字节" << std::endl;
        
        // 检查是否接收完毕
        if (received < buffer_size) {
            std::cout << "接收到的数据块小于缓冲区大小，可能已接收完毕" << std::endl;
            // A more reliable way to check if we're done receiving
            // 可能已接收完毕，设置短暂超时
            struct timeval short_timeout;
            short_timeout.tv_sec = 0;
            short_timeout.tv_usec = 500000; // 0.5秒
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&short_timeout, sizeof(short_timeout));
            
            // 使用MSG_PEEK测试是否还有更多数据
            char peek_buffer[1];
            received = recv(client_socket, peek_buffer, 1, MSG_PEEK);
            
            // 恢复原超时设置
            struct timeval timeout;
            timeout.tv_sec = 60;
            timeout.tv_usec = 0;
            setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
            
            if (received <= 0) {
                // 没有更多数据，结束接收
                std::cout << "没有更多数据，接收完毕" << std::endl;
                break;
            } else {
                // 还有更多数据，继续接收
                std::cout << "还有更多数据，继续接收" << std::endl;
            }
        }
    }
    
    // 解析接收到的数据
    if (all_data.size() < 8) {
        std::cerr << "无效消息: 太小" << std::endl;
        return false;
    }
    
    // 检查数据包头
    std::string header(all_data.begin(), all_data.begin() + 4);
    std::cout << "解析的头部字符串: '" << header << "'" << std::endl;
    
    if (header != "FACE") {
        std::cerr << "无效头部: " << header << std::endl;
        return false;
    }
    
    // 获取JSON长度
    int json_length = 0;
    memcpy(&json_length, all_data.data() + 4, 4);
    json_length = ntohl(json_length);
    std::cout << "解析的JSON长度: " << json_length << std::endl;
    
    if (json_length <= 0 || all_data.size() < 8 + json_length) {
        std::cerr << "无效JSON长度或不完整数据: " << json_length << std::endl;
        return false;
    }
    
    // 解析JSON数据
    std::string json_str(all_data.begin() + 8, all_data.begin() + 8 + json_length);
    std::cout << "JSON数据: " << json_str << std::endl;
    
    Json::Value json_obj;
    Json::Reader reader;
    if (!reader.parse(json_str, json_obj)) {
        std::cerr << "无法解析JSON: " << reader.getFormattedErrorMessages() << std::endl;
        return false;
    }
    
    // 解析消息类型
    std::string type = json_obj["type"].asString();
    if (type == "login") {
        message.type = MessageType::AUTHENTICATE_USER;
    } else if (type == "register") {
        message.type = MessageType::REGISTER_USER;
    } else {
        std::cerr << "未知消息类型: " << type << std::endl;
        return false;
    }
    
    // 提取数据
    message.data["username"] = json_obj["username"].asString();
    message.data["password"] = json_obj["password"].asString();
    
    // 提取人脸数据
    int face_data_size = json_obj["face_data_size"].asInt();
    if (face_data_size > 0 && all_data.size() >= 8 + json_length + face_data_size) {
        std::string face_data(all_data.begin() + 8 + json_length, 
                              all_data.begin() + 8 + json_length + face_data_size);
        message.data["face_data"] = face_data;
        
        // 保存接收到的人脸数据用于调试
        std::string temp_dir = "face_auth_data/temp";
        
        // 确保目录存在
        if (utils::createDirectories(temp_dir)) {
            std::string debug_face_file = temp_dir + "/received_face.jpg";
            std::ofstream face_file(debug_face_file, std::ios::binary);
            if (face_file.is_open()) {
                face_file.write(face_data.c_str(), face_data.size());
                face_file.close();
                std::cout << "已保存人脸数据到 " << debug_face_file << std::endl;
            }
        }
    } else {
        std::cerr << "无效人脸数据大小或不完整数据" << std::endl;
        return false;
    }
    
    return true;
}

bool TcpServer::sendMessage(int client_socket, const Message& message) {
    // 构造JSON响应
    Json::Value json_response;
    
    // 设置响应类型
    if (message.type == MessageType::RESPONSE) {
        // 从data中获取可能的type字段
        auto it = message.data.find("type");
        if (it != message.data.end()) {
            json_response["type"] = it->second;
        } else {
            // 改为与请求类型相同的响应类型 (login/register)
            auto it_req_type = message.data.find("request_type");
            if (it_req_type != message.data.end()) {
                json_response["type"] = it_req_type->second;
            } else {
                // 默认使用客户端能识别的类型
                json_response["type"] = "login";  
            }
        }
    } else if (message.type == MessageType::ERROR) {
        json_response["type"] = "error";
    } else {
        std::cerr << "发送无效消息类型" << std::endl;
        return false;
    }
    
    // 添加数据
    for (const auto& pair : message.data) {
        if (pair.first != "type" && pair.first != "request_type") { // 避免重复添加type字段
            json_response[pair.first] = pair.second;
        }
    }
    
    // 打印发送的响应内容
    std::cout << "发送响应: " << json_response.toStyledString() << std::endl;
    
    // 序列化JSON
    Json::FastWriter writer;
    std::string json_str = writer.write(json_response);
    
    // 构造响应数据包
    std::vector<char> packet;
    
    // 添加包头 - 保持与Python版本一致，使用'RESP'作为头
    packet.insert(packet.end(), {'R', 'E', 'S', 'P'});
    
    // 添加JSON长度 (网络字节序)
    uint32_t json_length = static_cast<uint32_t>(json_str.length());
    uint32_t net_length = htonl(json_length);
    const char* length_bytes = reinterpret_cast<const char*>(&net_length);
    packet.insert(packet.end(), length_bytes, length_bytes + 4);
    
    // 添加JSON数据
    packet.insert(packet.end(), json_str.begin(), json_str.end());
    
    std::cout << "准备发送响应: " << json_response["type"].asString() 
              << ", 成功: " << json_response["success"]
              << ", 大小: " << packet.size() << " 字节" << std::endl;
    
    // 发送数据 
    const size_t chunk_size = 4096;
    size_t total_sent = 0;
    
    while (total_sent < packet.size()) {
        size_t remaining = packet.size() - total_sent;
        size_t to_send = (remaining < chunk_size) ? remaining : chunk_size;
        
        int sent = send(client_socket, packet.data() + total_sent, to_send, 0);
        if (sent <= 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // 等待一下再试
                usleep(1000);
                continue;
            }
            std::cerr << "发送错误: " << strerror(errno) << std::endl;
            return false;
        }
        
        total_sent += sent;
        std::cout << "已发送 " << sent << " 字节，总计 " << total_sent << "/" << packet.size() << std::endl;
    }
    
    std::cout << "响应已完全发送: " << total_sent << " 字节" << std::endl;
    // 等待一小段时间确保数据发送完成
    usleep(100000); // 0.1秒
    
    return true;
}

void TcpServer::processMessage(int client_socket, const Message& message) {
    switch (message.type) {
        case MessageType::REGISTER_USER:
            handleRegister(client_socket, message);
            break;
        case MessageType::AUTHENTICATE_USER:
            handleAuthenticate(client_socket, message);
            break;
        default:
            std::cerr << "未知消息类型" << std::endl;
            sendError(client_socket, "未知消息类型");
            break;
    }
}

void TcpServer::handleRegister(int client_socket, const Message& message) {
    // 获取请求参数
    auto it_username = message.data.find("username");
    auto it_password = message.data.find("password");
    auto it_face_data = message.data.find("face_data");
    
    if (it_username == message.data.end() || it_password == message.data.end() || it_face_data == message.data.end()) {
        sendError(client_socket, "缺少注册所需的参数");
        return;
    }
    
    // 调用认证服务器进行注册
    Json::Value result = auth_server_.registerUser(it_username->second, it_password->second, it_face_data->second);
    
    // 创建包含请求类型的响应数据
    std::map<std::string, std::string> additional_data;
    additional_data["request_type"] = "register";
    
    // 发送响应
    sendResponse(client_socket, result["success"].asBool(), result["message"].asString(), additional_data);
}

void TcpServer::handleAuthenticate(int client_socket, const Message& message) {
    // 获取请求参数
    auto it_username = message.data.find("username");
    auto it_password = message.data.find("password");
    auto it_face_data = message.data.find("face_data");
    
    if (it_username == message.data.end() || it_password == message.data.end() || 
        it_face_data == message.data.end()) {
        sendError(client_socket, "缺少认证所需的参数");
        return;
    }
    
    // 调用认证服务器进行认证
    Json::Value result = auth_server_.authenticateUser(
        it_username->second, it_password->second, it_face_data->second);
    
    // 发送响应
    bool success = result["success"].asBool();
    std::string message_text = result["message"].asString();
    
    std::map<std::string, std::string> additional_data;
    additional_data["request_type"] = "login";
    
    if (success && result.isMember("face_verified")) {
        additional_data["face_verified"] = result["face_verified"].asBool() ? "true" : "false";
    }
    
    sendResponse(client_socket, success, message_text, additional_data);
}

void TcpServer::handleUpdateFace(int client_socket, const Message& message) {
    // 获取请求参数
    auto it_user_id = message.data.find("user_id");
    auto it_face_data = message.data.find("face_data");
    
    if (it_user_id == message.data.end() || it_face_data == message.data.end()) {
        sendError(client_socket, "缺少更新人脸所需的参数");
        return;
    }
    
    // 解析用户ID
    int user_id = 0;
    try {
        user_id = std::stoi(it_user_id->second);
    } catch (...) {
        sendError(client_socket, "无效用户ID");
        return;
    }
    
    // 调用认证服务器更新人脸数据
    Json::Value result = auth_server_.updateUserFace(user_id, it_face_data->second);
    
    // 发送响应
    sendResponse(client_socket, result["success"].asBool(), result["message"].asString());
}

void TcpServer::sendResponse(int client_socket, bool success, const std::string& message_text, 
                           const std::map<std::string, std::string>& data) {
    Message response;
    response.type = MessageType::RESPONSE;
    response.data["success"] = success ? "true" : "false";
    response.data["message"] = message_text;
    
    // 添加额外数据
    for (const auto& pair : data) {
        response.data[pair.first] = pair.second;
    }
    
    sendMessage(client_socket, response);
}

void TcpServer::sendError(int client_socket, const std::string& error_message) {
    Message error;
    error.type = MessageType::ERROR;
    error.data["success"] = "false";
    error.data["message"] = error_message;
    
    sendMessage(client_socket, error);
} 