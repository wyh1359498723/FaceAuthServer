#ifndef FACE_RECOGNIZER_H
#define FACE_RECOGNIZER_H

#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <string>
#include <vector>
#include <map>

class FaceRecognizer {
public:
    FaceRecognizer();
    ~FaceRecognizer();
    
    // 初始化识别器
    bool initialize();
    
    // 比较两个人脸的相似度
    double compareFaces(const cv::Mat& face1, const cv::Mat& face2);
    
    // 预处理人脸图像
    cv::Mat preprocessFace(const cv::Mat& face);
    
    // 训练识别器
    bool train(int user_id, const cv::Mat& face);
    
    // 识别人脸
    std::pair<int, double> recognize(const cv::Mat& face);
    
    // 保存模型
    bool saveModel(const std::string& filename = "face_model.yml");
    
    // 加载模型
    bool loadModel(const std::string& filename = "face_model.yml");

private:
    bool initialized_;
    cv::Ptr<cv::face::LBPHFaceRecognizer> lbph_model_;
    std::map<int, std::vector<cv::Mat>> training_faces_; // 用户ID -> 训练人脸
};

#endif // FACE_RECOGNIZER_H 