#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QObject>
#include <QtCore/QAtomicInt>
#include <opencv2/opencv.hpp>
#include "IMVApi.h"
#include "YoloDetector.h"
#include "VideoSource.h"

struct DeviceInfo
{
    QString name;
    QString serialNumber;
    QString ipAddress;
    int index;
};

class CameraController : public QObject
{
    Q_OBJECT

public:
    CameraController(QObject* parent = nullptr);
    ~CameraController();

    // �豸����
    QStringList refreshDevices();
    bool connectCamera(int deviceIndex);
    void disconnectCamera();
    bool isConnected() const { return m_isConnected; }

    // 图像采集
    bool startCapture();
    bool stopCapture();
    bool snapImage();
    bool grabFrame();  // 在连续采集模式下获取一帧
    bool saveCurrentImage(const QString& filename);

    // 获取当前图像
    cv::Mat getCurrentImage() const { return m_currentImage; }
    bool hasNewImage() const { return m_hasNewImage; }
    void clearNewImageFlag() { m_hasNewImage = false; }

    // YOLO 检测相关
    bool initializeYolo(const QString& modelPath, int parameterIndex = 2);
    void enableYoloDetection(bool enable) { m_yoloEnabled = enable; }
    bool isYoloEnabled() const { return m_yoloEnabled; }
    std::vector<BoundingBox> getLatestDetections() const { return m_latestDetections; }
    
    // 轮询模式：直接访问 YoloDetector
    YoloDetector* getYoloDetector() const { return m_yoloDetector; }
    
    // 视频源管理
    bool openVideoFile(const QString& filePath);
    VideoSourceType getCurrentSourceType() const;

    // ��ȡ�豸��Ϣ
    QStringList getDeviceList() const;
    DeviceInfo getDeviceInfo(int index) const;

signals:
    void imageUpdated();
    void statusChanged(const QString& message);
    // 改为无参数信号，避免Qt信号系统传递大型数据导致QRingBuffer溢出
    // 接收方应调用 getLatestDetections() 获取数据
    void detectionsUpdated();

private:
    bool convertFrameToMat(IMV_Frame* frame);
    void logStatus(const QString& message);
    void processImageWithYolo(cv::Mat& image);

private:
    IMV_HANDLE m_deviceHandle;
    bool m_isConnected;
    bool m_isCapturing;
    cv::Mat m_currentImage;
    bool m_hasNewImage;
    
    QList<DeviceInfo> m_deviceList;
    
    // 视频源管理器
    VideoSourceManager* m_videoSourceManager;
    
    // YOLO 检测器
    YoloDetector* m_yoloDetector;
    bool m_yoloEnabled;
    std::vector<BoundingBox> m_latestDetections;
    
    // 信号限流标志 - 防止Qt事件队列溢出
    QAtomicInt m_pendingDetectionSignals;
};