#ifndef UTILS_H
#define UTILS_H

#include <string>
#include <vector>
#include <opencv2/opencv.hpp>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace utils {
    // Base64编码
    std::string base64Encode(const unsigned char* data, size_t length);
    
    // Base64解码
    std::vector<unsigned char> base64Decode(const std::string& encoded_data);
    
    // 从Base64字符串解码图像
    cv::Mat base64ToMat(const std::string& base64_image);
    
    // 将图像编码为Base64字符串
    std::string matToBase64(const cv::Mat& image, const std::string& format = ".jpg");
    
    // SHA-256哈希
    std::string sha256(const std::string& data);
    
    // 生成随机字符串
    std::string generateRandomString(size_t length);
    
    // 获取当前时间戳
    std::string getCurrentTimestamp();
    
    // 解析JSON字符串
    std::string parseJson(const std::string& json_string, const std::string& key);
    
    // 创建目录（支持多级目录创建）
    bool createDirectories(const std::string& dirPath);
}

#endif // UTILS_H 