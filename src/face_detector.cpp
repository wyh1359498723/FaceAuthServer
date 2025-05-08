#include "face_detector.h"
#include <iostream>

FaceDetector::FaceDetector() : initialized_(false) {
}

FaceDetector::~FaceDetector() {
}

bool FaceDetector::initialize(const std::string& face_cascade_path) {
    // 加载人脸分类器
    if (!face_cascade_.load(face_cascade_path)) {
        std::cerr << "Error: 无法加载人脸分类器: " << face_cascade_path << std::endl;
        return false;
    }
    
    initialized_ = true;
    return true;
}

std::vector<cv::Rect> FaceDetector::detectFaces(const cv::Mat& image) {
    if (!initialized_) {
        std::cerr << "Error: 人脸检测器未初始化" << std::endl;
        return std::vector<cv::Rect>();
    }
    
    // 转换为灰度图像
    cv::Mat gray;
    if (image.channels() > 1) {
        cv::cvtColor(image, gray, cv::COLOR_BGR2GRAY);
    } else {
        gray = image.clone();
    }
    
    // 直方图均衡化以提高检测性能
    cv::equalizeHist(gray, gray);
    
    // 检测人脸
    std::vector<cv::Rect> faces;
    face_cascade_.detectMultiScale(gray, faces, 1.1, 3, 0, cv::Size(30, 30));
    
    return faces;
}

cv::Mat FaceDetector::extractFaceFeatures(const cv::Mat& face_image) {
    // 在实际应用中，这里可以提取更复杂的特征，例如使用深度学习模型
    // 这里我们简单地调整大小并归一化图像作为特征
    cv::Mat resized;
    cv::resize(face_image, resized, cv::Size(100, 100));
    
    cv::Mat features;
    if (resized.channels() > 1) {
        cv::cvtColor(resized, features, cv::COLOR_BGR2GRAY);
    } else {
        features = resized.clone();
    }
    
    // 规范化特征
    cv::Mat normalized;
    cv::normalize(features, normalized, 0, 255, cv::NORM_MINMAX);
    
    return normalized;
} 