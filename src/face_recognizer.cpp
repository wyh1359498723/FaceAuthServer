#include "face_recognizer.h"
#include <iostream>
#include <fstream>
#include <sys/stat.h>
#include <sys/types.h>

FaceRecognizer::FaceRecognizer() : initialized_(false) {
}

FaceRecognizer::~FaceRecognizer() {
}

bool FaceRecognizer::initialize() {
    try {
        // 创建LBPH人脸识别器
        // 参数解释：
        // - radius：LBP特征的半径，更大的半径可以捕获更多的纹理信息
        // - neighbors：圆周上采样点的数量，增加采样点可以提高纹理表示能力
        // - grid_x, grid_y：将图像分割成的网格，增加网格数可以提高空间分辨率
        // - threshold：识别阈值，设置较低使识别更严格
        lbph_model_ = cv::face::LBPHFaceRecognizer::create(
            2,      // radius - 默认1，增加到2可以捕获更多纹理信息
            8,      // neighbors - 默认8，保持不变
            8,      // grid_x - 默认8，保持不变
            8,      // grid_y - 默认8，保持不变
            70.0    // threshold - 默认80，调低以提高识别的严格性
        );
        
        // 尝试加载已有模型
        if (!loadModel()) {
            // 如果不存在已有模型，则创建新模型
            std::cout << "未找到现有模型，创建新的LBPH模型" << std::endl;
        } else {
            std::cout << "成功加载现有LBPH模型" << std::endl;
        }
        
        initialized_ = true;
        std::cout << "LBPH人脸识别器初始化成功，使用增强参数配置" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "初始化人脸识别器失败: " << e.what() << std::endl;
        return false;
    }
}

double FaceRecognizer::compareFaces(const cv::Mat& face1, const cv::Mat& face2) {
    if (!initialized_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return 9999.0; // 返回一个很大的值表示不相似
    }
    
    try {
        // 预处理两张人脸图像
        cv::Mat processed_face1 = preprocessFace(face1);
        cv::Mat processed_face2 = preprocessFace(face2);
        
        if (processed_face1.empty() || processed_face2.empty()) {
            std::cerr << "错误: 无法预处理人脸用于比较" << std::endl;
            return 9999.0;
        }
        
        // 方法1：使用现有模型进行比较
        // 将face1临时添加到训练数据中，并标记为一个特殊ID
        const int temp_id = -999; // 使用不太可能与实际用户ID冲突的值
        
        // 备份当前训练数据和模型
        std::map<int, std::vector<cv::Mat>> backup_training_faces = training_faces_;
        
        // 临时添加人脸到训练集
        training_faces_[temp_id].push_back(processed_face1);
        
        // 准备训练数据
        std::vector<cv::Mat> faces;
        std::vector<int> labels;
        
        for (const auto& pair : training_faces_) {
            int id = pair.first;
            for (const auto& f : pair.second) {
                faces.push_back(f);
                labels.push_back(id);
            }
        }
        
        // 在当前LBPH模型上临时训练
        cv::Ptr<cv::face::LBPHFaceRecognizer> temp_model = cv::face::LBPHFaceRecognizer::create();
        temp_model->train(faces, labels);
        
        // 预测第二张脸
        int label = -1;
        double confidence = 0.0;
        temp_model->predict(processed_face2, label, confidence);
        
        // 恢复原始训练数据
        training_faces_ = backup_training_faces;
        
        // 返回置信度作为相似度度量（值越低越相似）
        return confidence;
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 比较人脸失败: " << e.what() << std::endl;
        return 9999.0;
    }
}

cv::Mat FaceRecognizer::preprocessFace(const cv::Mat& face) {
    if (!initialized_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return cv::Mat();
    }
    
    try {
        if (face.empty()) {
            std::cerr << "错误: 输入人脸图像为空" << std::endl;
            return cv::Mat();
        }
        
        // 将图像大小调整为固定尺寸
        cv::Mat resized;
        cv::resize(face, resized, cv::Size(100, 100), 0, 0, cv::INTER_CUBIC);
        
        // 转换为灰度图像
        cv::Mat gray;
        if (resized.channels() > 1) {
            cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);
        } else {
            gray = resized.clone();
        }
        
        // 高斯模糊以减少噪声
        cv::Mat blurred;
        cv::GaussianBlur(gray, blurred, cv::Size(3, 3), 0);
        
        // 直方图均衡化以增强对比度
        cv::Mat equalized;
        cv::equalizeHist(blurred, equalized);
        
        // 应用对比度限制自适应直方图均衡化(CLAHE)
        cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE();
        clahe->setClipLimit(4.0);
        cv::Mat clahe_img;
        clahe->apply(equalized, clahe_img);
        
        return clahe_img;
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 预处理人脸失败: " << e.what() << std::endl;
        return cv::Mat();
    }
}

bool FaceRecognizer::train(int user_id, const cv::Mat& face) {
    if (!initialized_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return false;
    }
    
    try {
        cv::Mat processed_face = preprocessFace(face);
        if (processed_face.empty()) {
            std::cerr << "错误: 无法预处理人脸用于训练" << std::endl;
            return false;
        }
        
        // 添加到训练集
        training_faces_[user_id].push_back(processed_face);
        
        // 准备训练数据
        std::vector<cv::Mat> faces;
        std::vector<int> labels;
        
        for (const auto& pair : training_faces_) {
            int id = pair.first;
            for (const auto& f : pair.second) {
                faces.push_back(f);
                labels.push_back(id);
            }
        }
        
        // 训练模型
        lbph_model_->train(faces, labels);
        
        // 保存模型
        saveModel();
        
        std::cout << "模型训练成功，当前有 " << faces.size() << " 张训练图像" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 训练模型失败: " << e.what() << std::endl;
        return false;
    }
}

std::pair<int, double> FaceRecognizer::recognize(const cv::Mat& face) {
    if (!initialized_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return {-1, 9999.0};
    }
    
    try {
        cv::Mat processed_face = preprocessFace(face);
        if (processed_face.empty()) {
            std::cerr << "错误: 无法预处理人脸用于识别" << std::endl;
            return {-1, 9999.0};
        }
        
        // 如果模型未训练，返回错误
        if (training_faces_.empty()) {
            std::cerr << "错误: 没有训练数据可用" << std::endl;
            return {-1, 9999.0};
        }
        
        // 预测
        int label = -1;
        double confidence = 0.0;
        lbph_model_->predict(processed_face, label, confidence);
        
        return {label, confidence};
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 识别人脸失败: " << e.what() << std::endl;
        return {-1, 9999.0};
    }
}

bool FaceRecognizer::saveModel(const std::string& filename) {
    if (!initialized_ || !lbph_model_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return false;
    }
    
    try {
        // 确保目录存在
        std::string dir = "face_auth_data/models";
        struct stat st;
        if (stat(dir.c_str(), &st) != 0) {
            // 目录不存在，创建它
            if (mkdir(dir.c_str(), 0755) != 0) {
                std::cerr << "错误: 无法创建目录: " << dir << std::endl;
                return false;
            }
        }
        
        std::string model_path = dir + "/" + filename;
        lbph_model_->write(model_path);
        
        // 保存训练数据
        std::string training_data_path = dir + "/training_data.dat";
        std::ofstream ofs(training_data_path, std::ios::binary);
        if (!ofs.is_open()) {
            std::cerr << "错误: 无法打开文件用于写入: " << training_data_path << std::endl;
            return false;
        }
        
        // 写入训练人脸数量
        size_t user_count = training_faces_.size();
        ofs.write(reinterpret_cast<char*>(&user_count), sizeof(user_count));
        
        for (const auto& pair : training_faces_) {
            int user_id = pair.first;
            ofs.write(reinterpret_cast<char*>(&user_id), sizeof(user_id));
            
            size_t face_count = pair.second.size();
            ofs.write(reinterpret_cast<char*>(&face_count), sizeof(face_count));
            
            for (const auto& face : pair.second) {
                size_t rows = face.rows;
                size_t cols = face.cols;
                int type = face.type();
                
                ofs.write(reinterpret_cast<char*>(&rows), sizeof(rows));
                ofs.write(reinterpret_cast<char*>(&cols), sizeof(cols));
                ofs.write(reinterpret_cast<char*>(&type), sizeof(type));
                
                if (face.isContinuous()) {
                    ofs.write(reinterpret_cast<const char*>(face.data), face.total() * face.elemSize());
                } else {
                    for (int r = 0; r < rows; ++r) {
                        ofs.write(reinterpret_cast<const char*>(face.ptr(r)), cols * face.elemSize());
                    }
                }
            }
        }
        
        ofs.close();
        std::cout << "保存模型到: " << model_path << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 保存模型失败: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "错误: 保存训练数据失败: " << e.what() << std::endl;
        return false;
    }
}

bool FaceRecognizer::loadModel(const std::string& filename) {
    if (!lbph_model_) {
        std::cerr << "错误: 人脸识别器未初始化" << std::endl;
        return false;
    }
    
    try {
        std::string dir = "face_auth_data/models";
        std::string model_path = dir + "/" + filename;
        std::string training_data_path = dir + "/training_data.dat";
        
        // 检查模型文件是否存在
        struct stat model_st;
        if (stat(model_path.c_str(), &model_st) != 0) {
            std::cout << "模型文件不存在: " << model_path << std::endl;
            return false;
        }
        
        // 检查训练数据文件是否存在
        struct stat training_st;
        if (stat(training_data_path.c_str(), &training_st) != 0) {
            std::cout << "训练数据文件不存在: " << training_data_path << std::endl;
            return false;
        }
        
        // 加载模型
        lbph_model_->read(model_path);
        
        // 加载训练数据
        std::ifstream ifs(training_data_path, std::ios::binary);
        if (!ifs.is_open()) {
            std::cerr << "错误: 无法打开文件用于读取: " << training_data_path << std::endl;
            return false;
        }
        
        training_faces_.clear();
        
        // 读取用户数量
        size_t user_count = 0;
        ifs.read(reinterpret_cast<char*>(&user_count), sizeof(user_count));
        
        for (size_t i = 0; i < user_count; ++i) {
            int user_id = 0;
            ifs.read(reinterpret_cast<char*>(&user_id), sizeof(user_id));
            
            size_t face_count = 0;
            ifs.read(reinterpret_cast<char*>(&face_count), sizeof(face_count));
            
            std::vector<cv::Mat> faces;
            for (size_t j = 0; j < face_count; ++j) {
                size_t rows = 0, cols = 0;
                int type = 0;
                
                ifs.read(reinterpret_cast<char*>(&rows), sizeof(rows));
                ifs.read(reinterpret_cast<char*>(&cols), sizeof(cols));
                ifs.read(reinterpret_cast<char*>(&type), sizeof(type));
                
                cv::Mat face(rows, cols, type);
                ifs.read(reinterpret_cast<char*>(face.data), face.total() * face.elemSize());
                
                faces.push_back(face);
            }
            
            training_faces_[user_id] = faces;
        }
        
        ifs.close();
        
        std::cout << "加载模型成功，共加载 " << training_faces_.size() << " 个用户的人脸数据" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "错误: 加载模型失败: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "错误: 加载训练数据失败: " << e.what() << std::endl;
        return false;
    }
} 