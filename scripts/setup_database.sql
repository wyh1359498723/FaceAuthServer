-- 创建数据库
CREATE DATABASE IF NOT EXISTS face_auth_db;

-- 创建用户并授权
CREATE USER IF NOT EXISTS 'face_auth_user'@'localhost' IDENTIFIED BY 'face_auth_password';
GRANT ALL PRIVILEGES ON face_auth_db.* TO 'face_auth_user'@'localhost';
FLUSH PRIVILEGES;

-- 使用数据库
USE face_auth_db;

-- 创建用户表
CREATE TABLE IF NOT EXISTS users (
  id INT AUTO_INCREMENT PRIMARY KEY,
  username VARCHAR(255) NOT NULL UNIQUE,
  face_data LONGTEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 创建认证日志表
CREATE TABLE IF NOT EXISTS auth_logs (
  id INT AUTO_INCREMENT PRIMARY KEY,
  user_id INT,
  success BOOLEAN,
  details TEXT,
  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (user_id) REFERENCES users(id)
); 