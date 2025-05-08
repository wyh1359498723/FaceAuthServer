#include "auth_server.h"
#include "utils.h"
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <json/json.h>
#include <chrono>
#include <ctime>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>

AuthServer::AuthServer() : running_(false) {
    // 默认配置
    model_path_ = "models/haarcascade_frontalface_default.xml";
    db_host_ = "localhost";
    db_user_ = "root";
    db_password_ = "4819603p";
    db_name_ = "face_auth_db";
}

AuthServer::~AuthServer() {
    stop();
}

bool AuthServer::initialize(const std::string& config_file) {
    // 加载配置文件
    if (!loadConfig(config_file)) {
        std::cerr << "无法加载配置" << std::endl;
        return false;
    }

    // 创建必要的目录
    ensureDirectories();

    // 初始化人脸检测器
    if (!face_detector_.initialize(model_path_)) {
        std::cerr << "无法初始化人脸检测器" << std::endl;
        return false;
    }

    // 初始化人脸识别器
    if (!face_recognizer_.initialize()) {
        std::cerr << "无法初始化人脸识别器" << std::endl;
        return false;
    }

    // 连接数据库
    if (!db_manager_.connect(db_host_, db_user_, db_password_, db_name_)) {
        std::cerr << "无法连接到数据库" << std::endl;
        return false;
    }

    // 创建数据库表
    if (!db_manager_.createTables()) {
        std::cerr << "无法创建数据库表" << std::endl;
        return false;
    }

    std::cout << "认证服务器初始化成功" << std::endl;
    return true;
}

// 检查目录是否存在
bool dirExists(const std::string& path) {
    struct stat info;
    if (stat(path.c_str(), &info) != 0) {
        return false;
    }
    return (info.st_mode & S_IFDIR) != 0;
}

// 创建目录
bool createDirectory(const std::string& path) {
    return mkdir(path.c_str(), 0755) == 0;
}

// 检查文件是否存在
bool fileExists(const std::string& path) {
    struct stat info;
    return stat(path.c_str(), &info) == 0;
}

// 删除文件
bool removeFile(const std::string& path) {
    return unlink(path.c_str()) == 0;
}

void AuthServer::ensureDirectories() {
    std::vector<std::string> dirs = {
        "face_auth_data",
        "face_auth_data/faces",
        "face_auth_data/temp",
        "face_auth_data/logs"
    };

    for (const auto& dir : dirs) {
        if (!dirExists(dir)) {
            if (createDirectory(dir)) {
                std::cout << "创建目录: " << dir << std::endl;
            } else {
                throw std::runtime_error("无法创建目录: " + dir);
            }
        }
    }

    // 测试写入权限
    std::string test_file = "face_auth_data/.write_test";
    std::ofstream test(test_file);
    if (test.is_open()) {
        test << "test";
        test.close();
        if (removeFile(test_file)) {
            std::cout << "目录权限验证" << std::endl;
        } else {
            throw std::runtime_error("无法删除测试文件, 权限问题");
        }
    } else {
        throw std::runtime_error("无法写入目录");
    }
}

bool AuthServer::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = true;
    return true;
}

void AuthServer::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        running_ = false;
        // 断开数据库连接
        db_manager_.disconnect();
    }
}

bool AuthServer::loadConfig(const std::string& config_file) {
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "无法打开配置文件: " << config_file << std::endl;
            return false;
        }

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(file, root)) {
            std::cerr << "无法解析配置文件: " << reader.getFormattedErrorMessages() << std::endl;
            return false;
        }

        if (root.isMember("model_path")) {
            model_path_ = root["model_path"].asString();
        }

        if (root.isMember("database")) {
            const Json::Value& db = root["database"];
            if (db.isMember("host")) db_host_ = db["host"].asString();
            if (db.isMember("user")) db_user_ = db["user"].asString();
            if (db.isMember("password")) db_password_ = db["password"].asString();
            if (db.isMember("name")) db_name_ = db["name"].asString();
        }

        return true;
    } catch (const std::exception& e) {
        std::cerr << "加载配置错误: " << e.what() << std::endl;
        return false;
    }
}

Json::Value AuthServer::registerUser(const std::string& username, const std::string& password, const std::string& face_data) {
    Json::Value response;
    response["type"] = "register";

    // 验证用户名
    if (username.empty()) {
        response["success"] = false;
        response["message"] = "用户名不能为空";
        return response;
    }

    // 验证密码
    if (password.empty()) {
        response["success"] = false;
        response["message"] = "密码不能为空";
        return response;
    }

    // 验证人脸数据
    if (face_data.empty()) {
        response["success"] = false;
        response["message"] = "人脸数据不能为空";
        return response;
    }

    try {
        // 解码人脸数据
        cv::Mat face_image = decodeImage(face_data);
        if (face_image.empty()) {
            response["success"] = false;
            response["message"] = "无效人脸图像数据";
            return response;
        }

        // 检测人脸
        std::vector<cv::Rect> faces = face_detector_.detectFaces(face_image);
        if (faces.empty()) {
            response["success"] = false;
            response["message"] = "图像中未检测到人脸";
            return response;
        }

        // 保存用户信息到数据库
        if (!db_manager_.addUser(username, password, face_data)) {
            response["success"] = false;
            response["message"] = "无法将用户添加到数据库";
            return response;
        }

        // 在注册时训练人脸识别模型
        cv::Rect face = *std::max_element(faces.begin(), faces.end(), 
            [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });
        cv::Mat face_roi = face_image(face);
        
        // 获取用户ID
        UserInfo user = db_manager_.getUserByUsername(username);
        if (user.id > 0) {
            face_recognizer_.train(user.id, face_roi);
            std::cout << "已训练人脸识别模型，用户ID: " << user.id << std::endl;
        }

        response["success"] = true;
        response["message"] = "用户注册成功";
        return response;
    } catch (const std::exception& e) {
        response["success"] = false;
        response["message"] = std::string("注册错误: ") + e.what();
        return response;
    }
}

Json::Value AuthServer::authenticateUser(const std::string& username, const std::string& password, const std::string& face_data) {
    Json::Value response;
    response["type"] = "login";

    std::cout << "正在认证用户: " << username << std::endl;

    // 验证用户名和密码
    if (username.empty() || password.empty()) {
        response["success"] = false;
        response["message"] = "用户名或密码不能为空";
        return response;
    }

    // 验证人脸数据
    if (face_data.empty()) {
        response["success"] = false;
        response["message"] = "需要人脸数据进行认证";
        return response;
    }

    try {
        // 获取用户信息
        UserInfo user = db_manager_.getUserByUsername(username);
        std::cout << "查询到的用户ID: " << user.id << ", 用户名: " << user.username << std::endl;
        
        if (user.id == 0) {
            response["success"] = false;
            response["message"] = "用户未找到";
            return response;
        }
        
        std::cout << "用户的人脸文件路径: " << user.file_path << std::endl;

        // 验证密码 - 从数据库获取用户的实际密码
        std::string hashed_password = utils::sha256(password);
        std::string stored_password = db_manager_.getUserPassword(user.id);
        
        std::cout << "输入的密码哈希: " << hashed_password << std::endl;
        std::cout << "存储的密码哈希: " << stored_password << std::endl;
        
        if (hashed_password != stored_password) {
            response["success"] = false;
            response["message"] = "无效密码";
            db_manager_.logAuthentication(user.id, false, "密码验证失败");
            return response;
        }

        // 验证人脸数据
        cv::Mat login_face_image = decodeImage(face_data);
        if (login_face_image.empty()) {
            response["success"] = false;
            response["message"] = "无效人脸图像数据";
            db_manager_.logAuthentication(user.id, false, "无效人脸图像");
            return response;
        }
        
        std::cout << "成功解码登录人脸图像，尺寸: " 
                  << login_face_image.cols << "x" << login_face_image.rows << std::endl;

        // 检测人脸
        std::vector<cv::Rect> login_faces = face_detector_.detectFaces(login_face_image);
        std::cout << "登录图像中检测到 " << login_faces.size() << " 个人脸" << std::endl;
        
        if (login_faces.empty()) {
            response["success"] = false;
            response["message"] = "登录图像中未检测到人脸";
            db_manager_.logAuthentication(user.id, false, "未检测到人脸");
            return response;
        }

        // 获取用户的注册人脸数据
        cv::Mat registered_face_image;
        if (user.file_path != "LOGIN_IMAGE_NOT_SAVED") {
            // 尝试使用相对路径或绝对路径读取注册人脸
            std::string absolute_path = user.file_path;
            // 如果是相对路径，转换为绝对路径
            if (user.file_path.find("/") != 0) {
                // 相对于当前工作目录的路径
                char cwd[1024];
                if (getcwd(cwd, sizeof(cwd)) != NULL) {
                    absolute_path = std::string(cwd) + "/" + user.file_path;
                    std::cout << "转换为绝对路径: " << absolute_path << std::endl;
                }
            }
            
            // 从文件中读取注册人脸
            registered_face_image = cv::imread(absolute_path);
            std::cout << "尝试读取注册人脸图像从: " << absolute_path 
                      << (registered_face_image.empty() ? " [失败]" : " [成功]") << std::endl;
                      
            // 如果读取失败，尝试其他可能的路径
            if (registered_face_image.empty()) {
                std::string alt_path = "face_auth_data/faces/" + username + "_register.jpg";
                std::cout << "尝试备用路径: " << alt_path << std::endl;
                registered_face_image = cv::imread(alt_path);
                
                if (!registered_face_image.empty()) {
                    std::cout << "从备用路径成功读取图像" << std::endl;
                }
            }
        }
        
        if (registered_face_image.empty()) {
            response["success"] = false;
            response["message"] = "用户没有有效的注册人脸数据";
            db_manager_.logAuthentication(user.id, false, "没有注册人脸数据");
            return response;
        }
        
        std::cout << "成功读取注册人脸图像，尺寸: " 
                  << registered_face_image.cols << "x" << registered_face_image.rows << std::endl;

        std::vector<cv::Rect> registered_faces = face_detector_.detectFaces(registered_face_image);
        std::cout << "注册图像中检测到 " << registered_faces.size() << " 个人脸" << std::endl;
        
        if (registered_faces.empty()) {
            response["success"] = false;
            response["message"] = "注册图像中未检测到人脸";
            db_manager_.logAuthentication(user.id, false, "无效注册人脸数据");
            return response;
        }

        // 提取最大的人脸区域
        cv::Rect login_face = *std::max_element(login_faces.begin(), login_faces.end(), 
            [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });
        
        cv::Rect registered_face = *std::max_element(registered_faces.begin(), registered_faces.end(), 
            [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });

        // 提取人脸区域
        cv::Mat login_face_roi = login_face_image(login_face);
        cv::Mat registered_face_roi = registered_face_image(registered_face);

        // 预处理人脸图像
        cv::Mat login_processed = face_recognizer_.preprocessFace(login_face_roi);
        cv::Mat registered_processed = face_recognizer_.preprocessFace(registered_face_roi);
        
        // 先检查是否需要训练模型
        bool model_trained = false;
        try {
            // 尝试读取用户的训练数据
            std::pair<int, double> result = face_recognizer_.recognize(login_processed);
            model_trained = (result.first != -1);
        } catch (const std::exception& e) {
            std::cout << "识别模型可能未训练: " << e.what() << std::endl;
        }
        
        // 如果模型未训练，则为该用户训练模型
        if (!model_trained) {
            std::cout << "为用户 " << user.id << " 训练人脸识别模型" << std::endl;
            face_recognizer_.train(user.id, registered_processed);
        }
        
        // 比较人脸图像（使用LBPHFaceRecognizer）
        double confidence = face_recognizer_.compareFaces(registered_processed, login_processed);
        
        // 置信度阈值（OpenCV LBPH置信度越低越相似，与MSE相反）
        const double face_similarity_threshold = 70.0;
        bool face_verified = confidence < face_similarity_threshold;

        std::cout << "人脸相似度: " << confidence << ", 阈值: " << face_similarity_threshold << std::endl;
        std::cout << "人脸验证 " << (face_verified ? "通过" : "失败") << std::endl;

        // 记录人脸验证尝试
        db_manager_.storeFaceData(user.id, face_data, "login");
        
        if (!face_verified) {
            response["success"] = false;
            response["message"] = "人脸验证失败";
            response["face_verified"] = false;
            db_manager_.logAuthentication(user.id, false, "人脸验证失败, 置信度=" + std::to_string(confidence));
            return response;
        }

        // 更新最后登录时间
        db_manager_.updateLastLogin(user.id);

        // 认证成功
        response["success"] = true;
        response["message"] = "认证成功";
        response["face_verified"] = true;
        db_manager_.logAuthentication(user.id, true, "认证成功");
        
        return response;
    } catch (const std::exception& e) {
        std::cerr << "认证错误: " << e.what() << std::endl;
        response["success"] = false;
        response["message"] = std::string("认证错误: ") + e.what();
        return response;
    }
}

Json::Value AuthServer::updateUserFace(int user_id, const std::string& face_data) {
    Json::Value response;
    response["type"] = "update_face";

    if (user_id <= 0) {
        response["success"] = false;
        response["message"] = "无效用户ID";
        return response;
    }

    if (face_data.empty()) {
        response["success"] = false;
        response["message"] = "人脸数据不能为空";
        return response;
    }

    try {
        // 解码人脸数据
        cv::Mat face_image = decodeImage(face_data);
        if (face_image.empty()) {
            response["success"] = false;
            response["message"] = "无效人脸图像数据";
            return response;
        }

        // 检测人脸
        std::vector<cv::Rect> faces = face_detector_.detectFaces(face_image);
        if (faces.empty()) {
            response["success"] = false;
            response["message"] = "图像中未检测到人脸";
            return response;
        }

        // 更新数据库
        if (!db_manager_.updateUserFace(user_id, face_data)) {
            response["success"] = false;
            response["message"] = "无法更新人脸数据";
            return response;
        }

        // 提取最大的人脸区域并训练模型
        cv::Rect face = *std::max_element(faces.begin(), faces.end(), 
            [](const cv::Rect& a, const cv::Rect& b) { return a.area() < b.area(); });
        cv::Mat face_roi = face_image(face);
        
        // 训练人脸识别模型
        face_recognizer_.train(user_id, face_roi);
        std::cout << "更新人脸识别模型，用户ID: " << user_id << std::endl;

        response["success"] = true;
        response["message"] = "人脸数据更新成功";
        return response;
    } catch (const std::exception& e) {
        response["success"] = false;
        response["message"] = std::string("更新错误: ") + e.what();
        return response;
    }
}

cv::Mat AuthServer::decodeImage(const std::string& face_data) {
    try {
        // 首先保存图像到临时文件
        std::string temp_file = "face_auth_data/temp/temp_decode.jpg";
        std::ofstream file(temp_file, std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "无法创建临时文件用于图像解码" << std::endl;
            return cv::Mat();
        }
        
        // 将图像数据写入临时文件
        file.write(face_data.c_str(), face_data.size());
        file.close();
        
        // 使用OpenCV读取图像
        cv::Mat image = cv::imread(temp_file);
        
        // 检查图像是否成功读取
        if (image.empty()) {
            std::cerr << "无法从数据解码图像" << std::endl;
            return cv::Mat();
        }
        
        // 可选：删除临时文件
        std::remove(temp_file.c_str());
        
        return image;
    } catch (const std::exception& e) {
        std::cerr << "解码图像错误: " << e.what() << std::endl;
        return cv::Mat();
    }
}

std::string AuthServer::encodeImage(const cv::Mat& image) {
    // 将图像编码为Base64字符串
    return utils::matToBase64(image);
} 