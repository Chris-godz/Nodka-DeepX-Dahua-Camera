#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <opencv2/opencv.hpp>
#include <dxrt/dxrt_api.h>
#include <memory>
#include <vector>

#include "yolo/yolo.h"
#include "yolo/bbox.h"

class YoloDetector : public QObject
{
    Q_OBJECT

public:
    explicit YoloDetector(QObject* parent = nullptr);
    ~YoloDetector();

    // 初始化模型
    bool initializeModel(const QString& modelPath, int parameterIndex = 2);
    
    // 检查是否已初始化
    bool isInitialized() const { return m_initialized; }
    
    // 同步推理
    std::vector<BoundingBox> detectSync(const cv::Mat& image);
    
    // 异步推理
    bool detectAsync(const cv::Mat& image);
    
    // 获取配置信息
    YoloParam getConfig() const { return m_config; }
    int getImageWidth() const { return m_config.width; }
    int getImageHeight() const { return m_config.height; }
    int getNumClasses() const { return m_config.numClasses; }

signals:
    void detectionComplete(std::vector<BoundingBox> results);
    void errorOccurred(const QString& error);

private:
    // 预处理图像
    cv::Mat preprocessImage(const cv::Mat& image);
    
    // 后处理回调
    static int postProcessCallback(std::vector<std::shared_ptr<dxrt::Tensor>> outputs, void* arg);

private:
    bool m_initialized;
    QMutex m_mutex;
    
    // DXRT 推理引擎
    std::unique_ptr<dxrt::InferenceEngine> m_inferenceEngine;
    
    // YOLO 配置和处理器
    YoloParam m_config;
    std::unique_ptr<Yolo> m_yolo;
    
    // 缓冲区
    cv::Mat m_preprocessedImage;
    std::vector<uint8_t> m_outputBuffer;
    std::vector<BoundingBox> m_latestResults;
    
    // 保存输出张量（延长生命周期）
    dxrt::Tensors m_outputTensors;
};
