#include "utils.h"
#include <json/json.h>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <errno.h>
#include <string.h>

namespace utils {

// Base64编码表
static const std::string base64_chars = 
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// 判断字符是否为Base64字符
static inline bool is_base64(unsigned char c) {
    return (isalnum(c) || (c == '+') || (c == '/'));
}

std::string base64Encode(const unsigned char* data, size_t length) {
    std::string ret;
    int i = 0;
    int j = 0;
    unsigned char char_array_3[3];
    unsigned char char_array_4[4];

    while (length--) {
        char_array_3[i++] = *(data++);
        if (i == 3) {
            char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
            char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
            char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
            char_array_4[3] = char_array_3[2] & 0x3f;

            for(i = 0; i < 4; i++)
                ret += base64_chars[char_array_4[i]];
            i = 0;
        }
    }

    if (i) {
        for(j = i; j < 3; j++)
            char_array_3[j] = '\0';

        char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
        char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
        char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
        char_array_4[3] = char_array_3[2] & 0x3f;

        for (j = 0; j < i + 1; j++)
            ret += base64_chars[char_array_4[j]];

        while((i++ < 3))
            ret += '=';
    }

    return ret;
}

std::vector<unsigned char> base64Decode(const std::string& encoded_data) {
    size_t in_len = encoded_data.size();
    int i = 0;
    int j = 0;
    int in_ = 0;
    unsigned char char_array_4[4], char_array_3[3];
    std::vector<unsigned char> ret;

    while (in_len-- && (encoded_data[in_] != '=') && is_base64(encoded_data[in_])) {
        char_array_4[i++] = encoded_data[in_]; in_++;
        if (i == 4) {
            for (i = 0; i < 4; i++)
                char_array_4[i] = base64_chars.find(char_array_4[i]);

            char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
            char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
            char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

            for (i = 0; i < 3; i++)
                ret.push_back(char_array_3[i]);
            i = 0;
        }
    }

    if (i) {
        for (j = i; j <4; j++)
            char_array_4[j] = 0;

        for (j = 0; j <4; j++)
            char_array_4[j] = base64_chars.find(char_array_4[j]);

        char_array_3[0] = (char_array_4[0] << 2) + ((char_array_4[1] & 0x30) >> 4);
        char_array_3[1] = ((char_array_4[1] & 0xf) << 4) + ((char_array_4[2] & 0x3c) >> 2);
        char_array_3[2] = ((char_array_4[2] & 0x3) << 6) + char_array_4[3];

        for (j = 0; j < i - 1; j++)
            ret.push_back(char_array_3[j]);
    }

    return ret;
}

cv::Mat base64ToMat(const std::string& base64_image) {
    // 删除可能存在的格式前缀 (data:image/jpeg;base64,)
    std::string base64_data = base64_image;
    size_t pos = base64_data.find(',');
    if (pos != std::string::npos) {
        base64_data = base64_data.substr(pos + 1);
    }

    // 解码Base64
    std::vector<unsigned char> decoded_data = base64Decode(base64_data);
    if (decoded_data.empty()) {
        return cv::Mat();
    }

    // 将解码后的数据转换为OpenCV Mat
    return cv::imdecode(decoded_data, cv::IMREAD_COLOR);
}

std::string matToBase64(const cv::Mat& image, const std::string& format) {
    std::vector<unsigned char> buffer;
    cv::imencode(format, image, buffer);
    return base64Encode(buffer.data(), buffer.size());
}

std::string sha256(const std::string& data) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data.c_str(), data.size());
    SHA256_Final(hash, &sha256);
    
    std::stringstream ss;
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    
    return ss.str();
}

std::string generateRandomString(size_t length) {
    static const char charset[] = 
        "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);

    std::string result;
    result.reserve(length);
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    
    return result;
}

std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    time_t now_c = std::chrono::system_clock::to_time_t(now);
    
    char buffer[20];
    struct tm* timeinfo = localtime(&now_c);
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    
    return std::string(buffer);
}

std::string parseJson(const std::string& json_string, const std::string& key) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_string, root)) {
        return "";
    }
    
    if (root.isMember(key)) {
        return root[key].asString();
    }
    
    return "";
}

bool createDirectories(const std::string& dirPath) {
    // 分解路径
    std::string path = dirPath;
    
    // 替换Windows风格的路径分隔符
    for (char& c : path) {
        if (c == '\\') c = '/';
    }
    
    // 确保路径以 / 结尾
    if (!path.empty() && path.back() != '/') {
        path += '/';
    }
    
    // 创建每一级目录
    size_t pos = 0;
    bool result = true;
    
    while ((pos = path.find('/', pos)) != std::string::npos) {
        std::string subdir = path.substr(0, pos);
        if (!subdir.empty()) {
            // 尝试创建目录
            int ret = mkdir(subdir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
            if (ret != 0 && errno != EEXIST) {
                std::cerr << "创建目录失败 " << subdir 
                          << ": " << strerror(errno) << std::endl;
                result = false;
                break;
            }
        }
        pos++;
    }
    
    // 验证目录权限
    if (result) {
        std::string testFile = dirPath + "/.write_test";
        std::ofstream test(testFile.c_str());
        if (test.is_open()) {
            test << "test";
            test.close();
            // 删除测试文件
            std::remove(testFile.c_str());
            std::cout << "目录权限验证" << std::endl;
        } else {
            std::cerr << "无法写入目录: " << dirPath << std::endl;
            result = false;
        }
    }
    
    return result;
}

} // namespace utils 