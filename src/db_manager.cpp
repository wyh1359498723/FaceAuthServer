#include "db_manager.h"
#include "utils.h"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <sys/stat.h>
#include <fstream>
#include <algorithm>

// 获取当前时间戳，格式为: YYYY-MM-DD HH:MM:SS
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    time_t now_time = std::chrono::system_clock::to_time_t(now);
    
    char buffer[20];
    struct tm* timeinfo = localtime(&now_time);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    return std::string(buffer);
}

// 构造函数
DBManager::DBManager() : mysql_(nullptr), connected_(false) {
    // 初始化MySQL库
    mysql_ = mysql_init(nullptr);
    if (!mysql_) {
        throw std::runtime_error("初始化MySQL失败");
    }
}

// 析构函数
DBManager::~DBManager() {
    disconnect();
    
    if (mysql_) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

// 连接数据库
bool DBManager::connect(const std::string& host, const std::string& user, 
                      const std::string& password, const std::string& database) {
    if (connected_) {
        disconnect();
    }
    
    // 尝试socket连接
    const char* socket_paths[] = {
        "/tmp/mysql.sock",
        "/var/lib/mysql/mysql.sock",
        "/var/run/mysqld/mysqld.sock"
    };
    bool connected = false;
    
    // 先尝试连接到指定数据库
    for (const char* socket_path : socket_paths) {
        struct stat buf;
        if (stat(socket_path, &buf) == 0) {
            std::cout << "尝试通过socket连接到MySQL: " << socket_path << std::endl;
            if (mysql_real_connect(mysql_, NULL, user.c_str(), password.c_str(), 
                                database.c_str(), 0, socket_path, 0)) {
                connected = true;
                std::cout << "通过socket " << socket_path << " 连接成功" << std::endl;
                break;
            }
            std::cerr << "通过socket " << socket_path << " 连接失败: " << mysql_error(mysql_) << std::endl;
        }
    }
    
    // 如果连接失败，尝试连接到服务器并创建数据库
    if (!connected) {
        for (const char* socket_path : socket_paths) {
            struct stat buf;
            if (stat(socket_path, &buf) == 0) {
                std::cout << "尝试通过socket连接到MySQL服务器: " << socket_path << std::endl;
                if (mysql_real_connect(mysql_, NULL, user.c_str(), password.c_str(), 
                                    NULL, 0, socket_path, 0)) {
                    connected = true;
                    std::cout << "通过socket " << socket_path << " 连接到服务器成功" << std::endl;
                    
                    // 创建数据库
                    std::string query = "CREATE DATABASE IF NOT EXISTS `" + database + "`";
                    if (mysql_query(mysql_, query.c_str())) {
                        std::cerr << "无法创建数据库: " << mysql_error(mysql_) << std::endl;
                        mysql_close(mysql_);
                        mysql_ = mysql_init(nullptr);
                        return false;
                    }
                    
                    // 使用新创建的数据库
                    if (mysql_select_db(mysql_, database.c_str())) {
                        std::cerr << "无法选择数据库: " << mysql_error(mysql_) << std::endl;
                        mysql_close(mysql_);
                        mysql_ = mysql_init(nullptr);
                        return false;
                    }
                    
                    break;
                }
                std::cerr << "通过socket " << socket_path << " 连接到服务器失败: " << mysql_error(mysql_) << std::endl;
            }
        }
    }
    
    // 如果仍然连接失败，尝试TCP/IP连接
    if (!connected) {
        std::cerr << "所有socket连接尝试失败，尝试TCP/IP连接..." << std::endl;
        if (!mysql_real_connect(mysql_, host.c_str(), user.c_str(), password.c_str(), 
                              database.c_str(), 0, nullptr, 0)) {
            std::cerr << "TCP/IP连接失败: " << mysql_error(mysql_) << std::endl;
            return false;
        }
    }
    
    connected_ = true;
    std::cout << "连接到数据库: " << database << std::endl;
    
    // 创建表
    if (!createTables()) {
        std::cerr << "无法创建表" << std::endl;
        disconnect();
        return false;
    }
    
    return true;
}

// 断开连接
void DBManager::disconnect() {
    connected_ = false;
}

// 创建用户表
bool DBManager::createTables() {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    // 创建用户表
    const char* create_users_table = 
        "CREATE TABLE IF NOT EXISTS `users` ("
        "  `id` INT AUTO_INCREMENT PRIMARY KEY,"
        "  `username` VARCHAR(50) UNIQUE NOT NULL,"
        "  `password` VARCHAR(100) NOT NULL,"
        "  `created_at` DATETIME NOT NULL,"
        "  `last_login` DATETIME"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    
    if (mysql_query(mysql_, create_users_table)) {
        std::cerr << "无法创建用户表: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    // 创建人脸数据表
    const char* create_face_images_table = 
        "CREATE TABLE IF NOT EXISTS `face_images` ("
        "  `id` INT AUTO_INCREMENT PRIMARY KEY,"
        "  `user_id` INT NOT NULL,"
        "  `file_path` VARCHAR(255) NOT NULL,"
        "  `type` ENUM('register', 'login') NOT NULL,"
        "  `created_at` DATETIME NOT NULL,"
        "  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    
    if (mysql_query(mysql_, create_face_images_table)) {
        std::cerr << "无法创建face_images表: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    // 创建认证日志表
    const char* create_auth_logs_table = 
        "CREATE TABLE IF NOT EXISTS `auth_logs` ("
        "  `id` INT AUTO_INCREMENT PRIMARY KEY,"
        "  `user_id` INT NOT NULL,"
        "  `success` BOOLEAN NOT NULL DEFAULT 0,"
        "  `details` TEXT,"
        "  `created_at` DATETIME NOT NULL,"
        "  FOREIGN KEY (`user_id`) REFERENCES `users`(`id`) ON DELETE CASCADE"
        ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4";
    
    if (mysql_query(mysql_, create_auth_logs_table)) {
        std::cerr << "无法创建auth_logs表: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    return true;
}

// 添加用户
bool DBManager::addUser(const std::string& username, const std::string& password, const std::string& face_data) {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    // 检查用户名是否已存在
    if (getUserByUsername(username).id != 0) {
        std::cerr << "用户名已存在: " << username << std::endl;
        return false;
    }
    
    // 使用SHA-256哈希用户密码
    std::string hashed_password = utils::sha256(password);
    std::string timestamp = getCurrentTimestamp();
    
    // 准备插入用户的SQL语句
    const char* query = "INSERT INTO users (username, password, created_at) VALUES (?, ?, ?)";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        std::cerr << "无法初始化语句: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "无法准备语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[3];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = username.length();
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)hashed_password.c_str();
    bind[1].buffer_length = hashed_password.length();
    
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)timestamp.c_str();
    bind[2].buffer_length = timestamp.length();
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "无法绑定参数: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "无法执行语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 获取新插入用户的ID
    int user_id = mysql_insert_id(mysql_);
    mysql_stmt_close(stmt);
    
    // 保存人脸数据
    if (!storeFaceData(user_id, face_data, "register")) {
        std::cerr << "无法存储用户的人脸数据: " << username << std::endl;
        
        // 删除刚刚创建的用户记录
        std::string delete_query = "DELETE FROM users WHERE id = " + std::to_string(user_id);
        mysql_query(mysql_, delete_query.c_str());
        
        return false;
    }
    
    return true;
}

// 存储人脸数据
bool DBManager::storeFaceData(int user_id, const std::string& face_data, const std::string& type) {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    std::string timestamp = getCurrentTimestamp();
    
    // 保存人脸图像到文件
    std::string face_filename;
    if (type == "register") {
        // 获取用户名
        UserInfo user = getUserById(user_id);
        if (user.id == 0) {
            std::cerr << "获取用户名失败，无法保存人脸数据" << std::endl;
            return false;
        }
        face_filename = user.username + "_register.jpg";
    } else {
        // 登录时使用时间戳
        face_filename = "login_" + std::to_string(user_id) + "_" + 
                       timestamp.substr(0, 10) + "_" + timestamp.substr(11, 8) + ".jpg";
        // 替换冒号为下划线，避免文件名问题
        std::replace(face_filename.begin(), face_filename.end(), ':', '_');
    }
    
    // 确保目录存在
    std::string faces_dir = "face_auth_data/faces";
    struct stat st;
    if (stat(faces_dir.c_str(), &st) != 0) {
        if (mkdir(faces_dir.c_str(), 0755) != 0) {
            std::cerr << "创建faces目录失败" << std::endl;
            return false;
        }
    }
    
    // 使用相对路径以保持一致性
    std::string file_path = faces_dir + "/" + face_filename;
    std::cout << "保存人脸数据到：" << file_path << std::endl;
    
    // 如果是登录，不需要实际保存文件，只记录路径
    if (type == "login") {
        file_path = "LOGIN_IMAGE_NOT_SAVED";
    } else {
        // 保存图像到文件
        std::ofstream outfile(file_path, std::ios::binary);
        if (!outfile) {
            std::cerr << "无法打开文件用于写入: " << file_path << std::endl;
            return false;
        }
        outfile.write(face_data.c_str(), face_data.size());
        outfile.close();
        
        std::cout << "人脸数据保存成功，大小：" << face_data.size() << " 字节" << std::endl;
    }
    
    // 准备插入人脸数据的SQL语句
    const char* query = "INSERT INTO face_images (user_id, file_path, type, created_at) VALUES (?, ?, ?, ?)";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        std::cerr << "无法初始化语句: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "无法准备语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&user_id;
    
    bind[1].buffer_type = MYSQL_TYPE_STRING;
    bind[1].buffer = (void*)file_path.c_str();
    bind[1].buffer_length = file_path.length();
    
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)type.c_str();
    bind[2].buffer_length = type.length();
    
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)timestamp.c_str();
    bind[3].buffer_length = timestamp.length();
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "无法绑定参数: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "无法执行语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    mysql_stmt_close(stmt);
    return true;
}

// 更新用户的人脸数据
bool DBManager::updateUserFace(int user_id, const std::string& face_data) {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    // 检查用户是否存在
    if (getUserById(user_id).id == 0) {
        std::cerr << "用户未找到: " << user_id << std::endl;
        return false;
    }
    
    // 存储新的人脸数据
    return storeFaceData(user_id, face_data, "register");
}

// 获取所有用户
std::vector<UserInfo> DBManager::getAllUsers() {
    std::vector<UserInfo> users;
    
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return users;
    }
    
    // 查询所有用户
    const char* query = 
        "SELECT u.id, u.username, u.created_at, f.file_path "
        "FROM users u "
        "LEFT JOIN face_images f ON u.id = f.user_id "
        "WHERE f.type = 'register' "
        "ORDER BY f.created_at DESC";
    
    if (mysql_query(mysql_, query)) {
        std::cerr << "查询错误: " << mysql_error(mysql_) << std::endl;
        return users;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        std::cerr << "结果错误: " << mysql_error(mysql_) << std::endl;
        return users;
    }
    
    // 处理查询结果
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        UserInfo user;
        user.id = std::stoi(row[0]);
        user.username = row[1];
        user.created_at = row[2];
        
        if (row[3]) {
            user.file_path = row[3];
        }
        
        users.push_back(user);
    }
    
    mysql_free_result(result);
    return users;
}

// 根据ID获取用户
UserInfo DBManager::getUserById(int user_id) {
    UserInfo user;
    user.id = 0; // 默认值表示未找到
    
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return user;
    }
    
    // 查询用户
    std::string query = 
        "SELECT u.id, u.username, u.created_at, f.file_path "
        "FROM users u "
        "LEFT JOIN (SELECT * FROM face_images WHERE type = 'register' "
        "           ORDER BY created_at DESC LIMIT 1) f ON u.id = f.user_id "
        "WHERE u.id = " + std::to_string(user_id);
    
    if (mysql_query(mysql_, query.c_str())) {
        std::cerr << "查询错误: " << mysql_error(mysql_) << std::endl;
        return user;
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        std::cerr << "结果错误: " << mysql_error(mysql_) << std::endl;
        return user;
    }
    
    // 处理查询结果
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        user.id = std::stoi(row[0]);
        user.username = row[1];
        user.created_at = row[2];
        
        if (row[3]) {
            user.file_path = row[3];
        }
    }
    
    mysql_free_result(result);
    return user;
}

// 根据用户名获取用户
UserInfo DBManager::getUserByUsername(const std::string& username) {
    UserInfo user;
    user.id = 0; // 默认值表示未找到
    
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return user;
    }
    
    // 准备查询语句
    const char* query = 
        "SELECT u.id, u.username, u.created_at, f.file_path "
        "FROM users u "
        "LEFT JOIN (SELECT * FROM face_images WHERE type = 'register' "
        "           ORDER BY created_at DESC LIMIT 1) f ON u.id = f.user_id "
        "WHERE u.username = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        std::cerr << "无法初始化语句: " << mysql_error(mysql_) << std::endl;
        return user;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "无法准备语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return user;
    }
    
    // 绑定参数
    MYSQL_BIND bind[1];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)username.c_str();
    bind[0].buffer_length = username.length();
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "无法绑定参数: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return user;
    }
    
    // 执行查询
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "无法执行语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return user;
    }
    
    // 准备结果集
    if (mysql_stmt_store_result(stmt)) {
        std::cerr << "无法存储结果: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return user;
    }
    
    // 如果有结果
    if (mysql_stmt_num_rows(stmt) > 0) {
        // 绑定结果列
        MYSQL_BIND result_bind[4];
        memset(result_bind, 0, sizeof(result_bind));
        
        int id;
        char username_buf[51];
        char created_at_buf[20];
        char face_data_buf[65536]; // 大缓冲区用于存储人脸数据
        unsigned long username_length, created_at_length, face_data_length;
        my_bool is_null[4];
        
        result_bind[0].buffer_type = MYSQL_TYPE_LONG;
        result_bind[0].buffer = (void*)&id;
        result_bind[0].is_null = &is_null[0];
        
        result_bind[1].buffer_type = MYSQL_TYPE_STRING;
        result_bind[1].buffer = username_buf;
        result_bind[1].buffer_length = sizeof(username_buf);
        result_bind[1].length = &username_length;
        result_bind[1].is_null = &is_null[1];
        
        result_bind[2].buffer_type = MYSQL_TYPE_STRING;
        result_bind[2].buffer = created_at_buf;
        result_bind[2].buffer_length = sizeof(created_at_buf);
        result_bind[2].length = &created_at_length;
        result_bind[2].is_null = &is_null[2];
        
        result_bind[3].buffer_type = MYSQL_TYPE_STRING;
        result_bind[3].buffer = face_data_buf;
        result_bind[3].buffer_length = sizeof(face_data_buf);
        result_bind[3].length = &face_data_length;
        result_bind[3].is_null = &is_null[3];
        
        if (mysql_stmt_bind_result(stmt, result_bind)) {
            std::cerr << "无法绑定结果: " << mysql_stmt_error(stmt) << std::endl;
            mysql_stmt_close(stmt);
            return user;
        }
        
        // 获取数据
        if (mysql_stmt_fetch(stmt) == 0) {
            user.id = id;
            user.username = std::string(username_buf, username_length);
            user.created_at = std::string(created_at_buf, created_at_length);
            
            if (!is_null[3]) {
                user.file_path = std::string(face_data_buf, face_data_length);
            }
        }
    }
    
    mysql_stmt_close(stmt);
    return user;
}

// 记录认证日志
bool DBManager::logAuthentication(int user_id, bool success, const std::string& details) {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    std::string timestamp = getCurrentTimestamp();
    
    // 准备插入日志的SQL语句
    const char* query = "INSERT INTO auth_logs (user_id, success, details, created_at) VALUES (?, ?, ?, ?)";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        std::cerr << "无法初始化语句: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "无法准备语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[4];
    memset(bind, 0, sizeof(bind));
    
    char success_val = success ? 1 : 0;
    
    bind[0].buffer_type = MYSQL_TYPE_LONG;
    bind[0].buffer = (void*)&user_id;
    
    bind[1].buffer_type = MYSQL_TYPE_TINY;
    bind[1].buffer = (void*)&success_val;
    
    bind[2].buffer_type = MYSQL_TYPE_STRING;
    bind[2].buffer = (void*)details.c_str();
    bind[2].buffer_length = details.length();
    
    bind[3].buffer_type = MYSQL_TYPE_STRING;
    bind[3].buffer = (void*)timestamp.c_str();
    bind[3].buffer_length = timestamp.length();
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "无法绑定参数: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 执行插入
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "无法执行语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    mysql_stmt_close(stmt);
    
    // 如果认证成功，更新用户最后登录时间
    if (success) {
        std::string update_query = "UPDATE users SET last_login = '" + timestamp + "' WHERE id = " + std::to_string(user_id);
        
        if (mysql_query(mysql_, update_query.c_str())) {
            std::cerr << "无法更新最后登录时间: " << mysql_error(mysql_) << std::endl;
            return false;
        }
    }
    
    return true;
}

bool DBManager::updateLastLogin(int user_id) {
    if (!connected_) {
        std::cerr << "未连接到数据库" << std::endl;
        return false;
    }
    
    std::string timestamp = getCurrentTimestamp();
    
    // 准备更新用户最后登录时间的SQL语句
    const char* query = "UPDATE users SET last_login = ? WHERE id = ?";
    
    MYSQL_STMT* stmt = mysql_stmt_init(mysql_);
    if (!stmt) {
        std::cerr << "无法初始化语句: " << mysql_error(mysql_) << std::endl;
        return false;
    }
    
    if (mysql_stmt_prepare(stmt, query, strlen(query))) {
        std::cerr << "无法准备语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 绑定参数
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));
    
    bind[0].buffer_type = MYSQL_TYPE_STRING;
    bind[0].buffer = (void*)timestamp.c_str();
    bind[0].buffer_length = timestamp.length();
    
    bind[1].buffer_type = MYSQL_TYPE_LONG;
    bind[1].buffer = (void*)&user_id;
    
    if (mysql_stmt_bind_param(stmt, bind)) {
        std::cerr << "无法绑定参数: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    // 执行更新
    if (mysql_stmt_execute(stmt)) {
        std::cerr << "无法执行语句: " << mysql_stmt_error(stmt) << std::endl;
        mysql_stmt_close(stmt);
        return false;
    }
    
    mysql_stmt_close(stmt);
    
    std::cout << "更新用户最后登录时间: " << user_id << std::endl;
    return true;
}

std::string DBManager::getUserPassword(int user_id) {
    if (!mysql_) {
        std::cerr << "数据库连接未建立." << std::endl;
        return "";
    }

    // 准备SQL查询
    std::string query = "SELECT password FROM users WHERE id = " + std::to_string(user_id);
    
    if (mysql_query(mysql_, query.c_str())) {
        std::cerr << "查询错误 in getUserPassword: " << mysql_error(mysql_) << std::endl;
        return "";
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (!result) {
        std::cerr << "结果错误 in getUserPassword: " << mysql_error(mysql_) << std::endl;
        return "";
    }
    
    std::string password = "";
    MYSQL_ROW row = mysql_fetch_row(result);
    if (row) {
        password = row[0] ? row[0] : "";
    } else {
        std::cerr << "未找到用户 ID: " << user_id << std::endl;
    }
    
    mysql_free_result(result);
    return password;
} 