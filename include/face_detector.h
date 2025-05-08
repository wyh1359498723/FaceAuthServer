#ifndef FACE_DETECTOR_H
#define FACE_DETECTOR_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class FaceDetector {
public:
    FaceDetector();
    ~FaceDetector();

    // 初始化检测器，加载模型
    bool initialize(const std::string& face_cascade_path);
    
    // 从图像中检测人脸
    std::vector<cv::Rect> detectFaces(const cv::Mat& image);
    
    // 从图像中提取人脸特征
    cv::Mat extractFaceFeatures(const cv::Mat& face_image);

private:
    cv::CascadeClassifier face_cascade_;
    bool initialized_;
};

#endif // FACE_DETECTOR_H 