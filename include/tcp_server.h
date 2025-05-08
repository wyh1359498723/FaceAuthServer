#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include "auth_server.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>
#include <queue>
#include <condition_variable>
#include <atomic>

// 定义通信协议的消息类型
enum class MessageType {
    REGISTER_USER,      // 注册用户
    AUTHENTICATE_USER,  // 认证用户
    UPDATE_USER_FACE,   // 更新用户人脸
    RESPONSE,           // 响应消息
    ERROR               // 错误消息
};

// 通信消息结构
struct Message {
    MessageType type;
    std::map<std::string, std::string> data;
};

class TcpServer {
public:
    TcpServer(AuthServer& auth_server, int port = 8080);
    ~TcpServer();
    
    // 启动TCP服务器
    bool start();
    
    // 停止TCP服务器
    void stop();

private:
    // 服务器主线程
    void serverThread();
    
    // 客户端处理线程
    void clientHandler(int client_socket);
    
    // 接收消息
    bool receiveMessage(int client_socket, Message& message);
    
    // 发送消息
    bool sendMessage(int client_socket, const Message& message);
    
    // 处理客户端消息
    void processMessage(int client_socket, const Message& message);
    
    // 处理注册请求
    void handleRegister(int client_socket, const Message& message);
    
    // 处理认证请求
    void handleAuthenticate(int client_socket, const Message& message);
    
    // 处理更新人脸请求
    void handleUpdateFace(int client_socket, const Message& message);
    
    // 发送响应
    void sendResponse(int client_socket, bool success, const std::string& message, 
                     const std::map<std::string, std::string>& data = {});
    
    // 发送错误
    void sendError(int client_socket, const std::string& error_message);

    AuthServer& auth_server_;
    int port_;
    int server_socket_;
    std::atomic<bool> running_;
    std::mutex mutex_;
    std::thread server_thread_;
    std::vector<std::thread> client_threads_;
};

#endif // TCP_SERVER_H 