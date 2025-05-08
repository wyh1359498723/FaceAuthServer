#include "auth_server.h"
#include "tcp_server.h"
#include <iostream>
#include <string>
#include <csignal>
#include <thread>
#include <chrono>

// 全局变量
AuthServer* g_auth_server = nullptr;
TcpServer* g_tcp_server = nullptr;

// 信号处理函数
void signal_handler(int signal) {
    std::cout << "收到信号 " << signal << std::endl;
    
    if (g_tcp_server) {
        std::cout << "停止TCP服务器..." << std::endl;
        g_tcp_server->stop();
    }
    
    if (g_auth_server) {
        std::cout << "停止认证服务器..." << std::endl;
        g_auth_server->stop();
    }
    
    exit(signal);
}

void print_usage(const char* program_name) {
    std::cout << "用法: " << program_name << " <配置文件路径> [端口号]" << std::endl;
    std::cout << "示例: " << program_name << " config.json 8080" << std::endl;
}

int main(int argc, char* argv[]) {
    // 检查命令行参数
    if (argc < 2) {
        std::cerr << "错误: 缺少配置文件路径" << std::endl;
        print_usage(argv[0]);
        return 1;
    }
    
    std::string config_file = argv[1];
    int port = 8080; // 默认端口
    
    // 如果提供了端口参数
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
            if (port <= 0 || port > 65535) {
                std::cerr << "错误: 无效的端口号. 必须在1到65535之间." << std::endl;
                return 1;
            }
        } catch (const std::exception& e) {
            std::cerr << "错误: 无效的端口号: " << e.what() << std::endl;
            return 1;
        }
    }
    
    // 注册信号处理函数
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    // 创建并初始化认证服务器
    g_auth_server = new AuthServer();
    
    if (!g_auth_server->initialize(config_file)) {
        std::cerr << "错误: 无法初始化认证服务器" << std::endl;
        delete g_auth_server;
        return 1;
    }
    
    // 启动认证服务器
    if (!g_auth_server->start()) {
        std::cerr << "错误: 无法启动认证服务器" << std::endl;
        delete g_auth_server;
        return 1;
    }
    
    // 创建并启动TCP服务器
    g_tcp_server = new TcpServer(*g_auth_server, port);
    
    if (!g_tcp_server->start()) {
        std::cerr << "错误: 无法启动TCP服务器" << std::endl;
        g_auth_server->stop();
        delete g_tcp_server;
        delete g_auth_server;
        return 1;
    }
    
    std::cout << "人脸认证服务器正在运行在端口 " << port << std::endl;
    std::cout << "按Ctrl+C停止。" << std::endl;
    
    // 主线程保持运行状态
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 不会执行到这里，但以防万一
    g_tcp_server->stop();
    g_auth_server->stop();
    
    delete g_tcp_server;
    delete g_auth_server;
    
    return 0;
} 