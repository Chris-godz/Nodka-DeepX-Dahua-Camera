#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>
#include <opencv2/opencv.hpp>

// 视频源类型枚举
enum class VideoSourceType
{
    Camera,      // 大华工业相机
    VideoFile    // MP4/AVI 等视频文件
};

// 视频源抽象基类
class IVideoSource : public QObject
{
    Q_OBJECT

public:
    explicit IVideoSource(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~IVideoSource() = default;

    // 纯虚函数接口
    virtual bool open() = 0;
    virtual void close() = 0;
    virtual bool isOpened() const = 0;
    virtual bool grabFrame(cv::Mat& outFrame) = 0;
    virtual QString getSourceName() const = 0;
    virtual VideoSourceType getType() const = 0;

signals:
    void statusChanged(const QString& message);
    void errorOccurred(const QString& error);
};

// 大华相机视频源
class CameraVideoSource : public IVideoSource
{
    Q_OBJECT

public:
    explicit CameraVideoSource(int deviceIndex, QObject* parent = nullptr);
    ~CameraVideoSource() override;

    bool open() override;
    void close() override;
    bool isOpened() const override { return m_isConnected; }
    bool grabFrame(cv::Mat& outFrame) override;
    QString getSourceName() const override;
    VideoSourceType getType() const override { return VideoSourceType::Camera; }

private:
    void* m_deviceHandle;  // IMV_HANDLE
    int m_deviceIndex;
    bool m_isConnected;
    bool m_isCapturing;
    QString m_deviceName;
};

// 视频文件源
class VideoFileSource : public IVideoSource
{
    Q_OBJECT

public:
    explicit VideoFileSource(const QString& filePath, QObject* parent = nullptr);
    ~VideoFileSource() override;

    bool open() override;
    void close() override;
    bool isOpened() const override;
    bool grabFrame(cv::Mat& outFrame) override;
    QString getSourceName() const override { return m_filePath; }
    VideoSourceType getType() const override { return VideoSourceType::VideoFile; }

    // 视频文件特有功能
    int getTotalFrames() const;
    int getCurrentFramePos() const;
    double getFPS() const;
    void setFramePos(int pos);
    void setLoop(bool loop) { m_loop = loop; }

private:
    QString m_filePath;
    cv::VideoCapture m_capture;
    bool m_loop;  // 是否循环播放
};

// 视频源管理器
class VideoSourceManager : public QObject
{
    Q_OBJECT

public:
    explicit VideoSourceManager(QObject* parent = nullptr);
    ~VideoSourceManager();

    // 设置视频源
    bool setSource(VideoSourceType type, const QString& param = QString());
    
    // 获取当前源
    IVideoSource* getCurrentSource() const { return m_currentSource; }
    VideoSourceType getCurrentType() const;
    
    // 快捷方法
    bool openCamera(int deviceIndex);
    bool openVideoFile(const QString& filePath);
    void closeSource();
    
    bool grabFrame(cv::Mat& outFrame);
    bool isOpened() const;

signals:
    void sourceChanged(VideoSourceType type, const QString& name);
    void statusChanged(const QString& message);
    void errorOccurred(const QString& error);

private:
    IVideoSource* m_currentSource;
};
