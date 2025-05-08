#ifndef DB_MANAGER_H
#define DB_MANAGER_H

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <map>

struct UserInfo {
    int id;
    std::string username;
    std::string file_path;
    std::string created_at;
};

class DBManager {
public:
    DBManager();
    ~DBManager();
    
    // 连接数据库
    bool connect(const std::string& host, const std::string& user, 
                 const std::string& password, const std::string& database);
    
    // 断开连接
    void disconnect();
    
    // 创建用户表
    bool createTables();
    
    // 添加用户
    bool addUser(const std::string& username, const std::string& password, const std::string& face_data);
    
    // 存储人脸数据
    bool storeFaceData(int user_id, const std::string& face_data, const std::string& type);
    
    // 更新用户的人脸数据
    bool updateUserFace(int user_id, const std::string& face_data);
    
    // 获取所有用户
    std::vector<UserInfo> getAllUsers();
    
    // 根据ID获取用户
    UserInfo getUserById(int user_id);
    
    // 根据用户名获取用户
    UserInfo getUserByUsername(const std::string& username);
    
    // 获取用户密码
    std::string getUserPassword(int user_id);
    
    // 记录认证日志
    bool logAuthentication(int user_id, bool success, const std::string& details);
    
    // 更新用户最后登录时间
    bool updateLastLogin(int user_id);

private:
    MYSQL* mysql_;
    bool connected_;
};

#endif // DB_MANAGER_H 