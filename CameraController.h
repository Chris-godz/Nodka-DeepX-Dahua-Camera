#pragma once

#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtCore/QObject>
#include <opencv2/opencv.hpp>
#include "IMVApi.h"

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

    // ��ȡ�豸��Ϣ
    QStringList getDeviceList() const;
    DeviceInfo getDeviceInfo(int index) const;

signals:
    void imageUpdated();
    void statusChanged(const QString& message);

private:
    bool convertFrameToMat(IMV_Frame* frame);
    void logStatus(const QString& message);

private:
    IMV_HANDLE m_deviceHandle;
    bool m_isConnected;
    bool m_isCapturing;
    cv::Mat m_currentImage;
    bool m_hasNewImage;
    
    QList<DeviceInfo> m_deviceList;
};