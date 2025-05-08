#ifndef AUTH_SERVER_H
#define AUTH_SERVER_H

#include "face_detector.h"
#include "face_recognizer.h"
#include "db_manager.h"
#include <json/json.h>
#include <string>
#include <vector>
#include <map>
#include <mutex>

class AuthServer {
public:
    AuthServer();
    ~AuthServer();
    
    // 初始化服务器
    bool initialize(const std::string& config_file);
    
    // 启动服务器
    bool start();
    
    // 停止服务器
    void stop();
    
    // 注册新用户
    Json::Value registerUser(const std::string& username, const std::string& password, const std::string& face_data);
    
    // 认证用户
    Json::Value authenticateUser(const std::string& username, const std::string& password, const std::string& face_data);
    
    // 更新用户的人脸数据
    Json::Value updateUserFace(int user_id, const std::string& face_data);

private:
    // 加载配置文件
    bool loadConfig(const std::string& config_file);
    
    // 确保必要的目录存在
    void ensureDirectories();
    
    // 从Base64字符串解码图像
    cv::Mat decodeImage(const std::string& base64_image);
    
    // 将图像编码为Base64字符串
    std::string encodeImage(const cv::Mat& image);

    FaceDetector face_detector_;
    FaceRecognizer face_recognizer_;
    DBManager db_manager_;
    
    std::string model_path_;
    std::string db_host_;
    std::string db_user_;
    std::string db_password_;
    std::string db_name_;
    
    bool running_;
    std::mutex mutex_;
};

#endif // AUTH_SERVER_H 