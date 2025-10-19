#pragma once

#include <QObject>
#include <QString>
#include <QMutex>
#include <QThread>
#include <opencv2/opencv.hpp>
#include <dxrt/dxrt_api.h>
#include <memory>
#include <vector>
#include <queue>
#include <condition_variable>

#include "yolo/yolo.h"
#include "yolo/bbox.h"

// 前向声明
class YoloDetector;

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
    
    // 同步推理（单线程版本 - 已弃用）
    std::vector<BoundingBox> detectSync(const cv::Mat& image);
    
    // 异步推理（多线程版本 - 推荐使用）
    bool detectAsync(const cv::Mat& image);
    
    // 获取最新检测结果（线程安全）
    std::vector<BoundingBox> getLatestResults();
    
    // 获取配置信息
    YoloParam getConfig() const { return m_config; }
    int getImageWidth() const { return m_config.width; }
    int getImageHeight() const { return m_config.height; }
    int getNumClasses() const { return m_config.numClasses; }

signals:
    // 改为无参数信号，避免Qt信号系统传递大型数据导致QRingBuffer溢出
    // 接收方应调用 getLatestResults() 获取数据
    void detectionComplete();
    void errorOccurred(const QString& error);

private:
    // 预处理图像
    cv::Mat preprocessImage(const cv::Mat& image);
    
    // 后处理（在单独线程中执行）
    std::vector<BoundingBox> postProcess(const uint8_t* outputData,
                                        const std::vector<std::vector<int64_t>>& outputShapes,
                                        dxrt::DataType dataType,
                                        int originalWidth,
                                        int originalHeight);
    
    // 坐标缩放（从模型输入尺寸映射回原始图像）
    void scaleCoordinates(std::vector<BoundingBox>& boxes,
                         int srcWidth, int srcHeight,
                         int dstWidth, int dstHeight);
    
    // 后处理回调（静态函数，供 DXRT 调用）
    static int postProcessCallback(std::vector<std::shared_ptr<dxrt::Tensor>> outputs, void* arg);
    
    // 后处理实现（从 buffer 进行）
    void postProcessFromBuffer(void* outputData, int outputLength);

private:
    bool m_initialized;
    QMutex m_mutex;
    QMutex m_resultMutex;  // 保护检测结果的独立锁
    
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
    
    // 保存原始图像尺寸（用于坐标缩放）
    int m_currentOriginalWidth;
    int m_currentOriginalHeight;
};
