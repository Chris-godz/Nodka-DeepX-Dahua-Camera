#include "VideoSource.h"
#include "IMVApi.h"
#include <QDebug>
#include <QFileInfo>

// ============================================================================
// CameraVideoSource 实现
// ============================================================================

CameraVideoSource::CameraVideoSource(int deviceIndex, QObject* parent)
    : IVideoSource(parent)
    , m_deviceHandle(nullptr)
    , m_deviceIndex(deviceIndex)
    , m_isConnected(false)
    , m_isCapturing(false)
{
}

CameraVideoSource::~CameraVideoSource()
{
    close();
}

bool CameraVideoSource::open()
{
    qDebug() << "[CameraVideoSource] 尝试打开相机，设备索引:" << m_deviceIndex;
    
    // 枚举设备
    IMV_DeviceList deviceList;
    int ret = IMV_EnumDevices(&deviceList, interfaceTypeAll);
    if (ret != IMV_OK) {
        QString error = QString("枚举相机设备失败，错误码: %1").arg(ret);
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }
    
    if (m_deviceIndex < 0 || m_deviceIndex >= (int)deviceList.nDevNum) {
        QString error = QString("设备索引 %1 超出范围 (0-%2)").arg(m_deviceIndex).arg(deviceList.nDevNum - 1);
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }
    
    // 保存设备名称
    m_deviceName = QString::fromLocal8Bit(deviceList.pDevInfo[m_deviceIndex].serialNumber);
    
    // 创建设备句柄
    ret = IMV_CreateHandle((IMV_HANDLE*)&m_deviceHandle, modeByIndex, (void*)&m_deviceIndex);
    if (ret != IMV_OK) {
        QString error = QString("创建相机句柄失败，错误码: %1").arg(ret);
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }
    
    // 打开设备
    ret = IMV_Open((IMV_HANDLE)m_deviceHandle);
    if (ret != IMV_OK) {
        QString error = QString("打开相机失败，错误码: %1").arg(ret);
        qCritical() << error;
        IMV_DestroyHandle((IMV_HANDLE)m_deviceHandle);
        m_deviceHandle = nullptr;
        emit errorOccurred(error);
        return false;
    }
    
    // 开始采集
    ret = IMV_StartGrabbing((IMV_HANDLE)m_deviceHandle);
    if (ret != IMV_OK) {
        QString error = QString("启动相机采集失败，错误码: %1").arg(ret);
        qCritical() << error;
        IMV_Close((IMV_HANDLE)m_deviceHandle);
        IMV_DestroyHandle((IMV_HANDLE)m_deviceHandle);
        m_deviceHandle = nullptr;
        emit errorOccurred(error);
        return false;
    }
    
    m_isConnected = true;
    m_isCapturing = true;
    
    QString msg = QString("相机 %1 已连接并开始采集").arg(m_deviceName);
    qInfo() << msg;
    emit statusChanged(msg);
    
    return true;
}

void CameraVideoSource::close()
{
    if (!m_isConnected || !m_deviceHandle) {
        return;
    }
    
    qDebug() << "[CameraVideoSource] 关闭相机:" << m_deviceName;
    
    if (m_isCapturing) {
        IMV_StopGrabbing((IMV_HANDLE)m_deviceHandle);
        m_isCapturing = false;
    }
    
    IMV_Close((IMV_HANDLE)m_deviceHandle);
    IMV_DestroyHandle((IMV_HANDLE)m_deviceHandle);
    
    m_deviceHandle = nullptr;
    m_isConnected = false;
    
    emit statusChanged("相机已断开");
}

bool CameraVideoSource::grabFrame(cv::Mat& outFrame)
{
    if (!m_isConnected || !m_deviceHandle) {
        return false;
    }
    
    IMV_Frame frame;
    int ret = IMV_GetFrame((IMV_HANDLE)m_deviceHandle, &frame, 1000);
    if (ret != IMV_OK) {
        return false;
    }
    
    // 转换为 RGB 格式
    if (frame.frameInfo.pixelFormat == gvspPixelMono8) {
        // 单通道灰度图
        outFrame = cv::Mat(frame.frameInfo.height, frame.frameInfo.width, CV_8UC1, frame.pData).clone();
        cv::cvtColor(outFrame, outFrame, cv::COLOR_GRAY2BGR);
    }
    else if (frame.frameInfo.pixelFormat == gvspPixelBGR8) {
        // BGR 格式
        outFrame = cv::Mat(frame.frameInfo.height, frame.frameInfo.width, CV_8UC3, frame.pData).clone();
    }
    else if (frame.frameInfo.pixelFormat == gvspPixelRGB8) {
        // RGB 格式，需要转换为 BGR
        cv::Mat rgb(frame.frameInfo.height, frame.frameInfo.width, CV_8UC3, frame.pData);
        cv::cvtColor(rgb, outFrame, cv::COLOR_RGB2BGR);
    }
    else {
        qWarning() << "[CameraVideoSource] 不支持的像素格式:" << frame.frameInfo.pixelFormat;
        IMV_ReleaseFrame((IMV_HANDLE)m_deviceHandle, &frame);
        return false;
    }
    
    IMV_ReleaseFrame((IMV_HANDLE)m_deviceHandle, &frame);
    return true;
}

QString CameraVideoSource::getSourceName() const
{
    return QString("相机 %1").arg(m_deviceName);
}

// ============================================================================
// VideoFileSource 实现
// ============================================================================

VideoFileSource::VideoFileSource(const QString& filePath, QObject* parent)
    : IVideoSource(parent)
    , m_filePath(filePath)
    , m_loop(false)
{
}

VideoFileSource::~VideoFileSource()
{
    close();
}

bool VideoFileSource::open()
{
    qDebug() << "[VideoFileSource] 尝试打开视频文件:" << m_filePath;
    
    QFileInfo fileInfo(m_filePath);
    if (!fileInfo.exists()) {
        QString error = QString("视频文件不存在: %1").arg(m_filePath);
        qCritical() << error;
        emit errorOccurred(error);
        return false;
    }
    
    // 尝试使用不同的backend打开视频文件
    std::string filePath = m_filePath.toStdString();
    qDebug() << "[VideoFileSource] 文件大小:" << fileInfo.size() << "bytes";
    qDebug() << "[VideoFileSource] 文件路径(std::string):" << QString::fromStdString(filePath);
    
    // 先尝试默认backend
    m_capture.open(filePath);
    
    if (!m_capture.isOpened()) {
        qWarning() << "[VideoFileSource] 默认backend失败，尝试使用FFMPEG backend...";
        // 尝试明确指定FFMPEG backend
        m_capture.open(filePath, cv::CAP_FFMPEG);
    }
    
    if (!m_capture.isOpened()) {
        qWarning() << "[VideoFileSource] FFMPEG backend失败，尝试使用MSMF backend...";
        // 尝试Windows Media Foundation
        m_capture.open(filePath, cv::CAP_MSMF);
    }
    
    if (!m_capture.isOpened()) {
        QString error = QString("无法打开视频文件: %1 (已尝试所有backend)").arg(m_filePath);
        qCritical() << error;
        qCritical() << "[VideoFileSource] OpenCV版本:" << CV_VERSION;
        qCritical() << "[VideoFileSource] 请确保OpenCV编译时包含了FFMPEG或Media Foundation支持";
        emit errorOccurred(error);
        return false;
    }
    
    QString msg = QString("视频文件已打开: %1 (%2 帧, %.1f FPS)")
        .arg(fileInfo.fileName())
        .arg(getTotalFrames())
        .arg(getFPS());
    qInfo() << msg;
    emit statusChanged(msg);
    
    return true;
}

void VideoFileSource::close()
{
    if (m_capture.isOpened()) {
        qDebug() << "[VideoFileSource] 关闭视频文件:" << m_filePath;
        m_capture.release();
        emit statusChanged("视频文件已关闭");
    }
}

bool VideoFileSource::isOpened() const
{
    return m_capture.isOpened();
}

bool VideoFileSource::grabFrame(cv::Mat& outFrame)
{
    if (!m_capture.isOpened()) {
        return false;
    }
    
    bool success = m_capture.read(outFrame);
    
    // 如果读取失败且开启了循环播放
    if (!success && m_loop) {
        m_capture.set(cv::CAP_PROP_POS_FRAMES, 0);  // 重置到开头
        success = m_capture.read(outFrame);
        if (success) {
            qDebug() << "[VideoFileSource] 视频循环播放，从头开始";
        }
    }
    
    return success;
}

int VideoFileSource::getTotalFrames() const
{
    if (!m_capture.isOpened()) {
        return 0;
    }
    return static_cast<int>(m_capture.get(cv::CAP_PROP_FRAME_COUNT));
}

int VideoFileSource::getCurrentFramePos() const
{
    if (!m_capture.isOpened()) {
        return 0;
    }
    return static_cast<int>(m_capture.get(cv::CAP_PROP_POS_FRAMES));
}

double VideoFileSource::getFPS() const
{
    if (!m_capture.isOpened()) {
        return 0.0;
    }
    return m_capture.get(cv::CAP_PROP_FPS);
}

void VideoFileSource::setFramePos(int pos)
{
    if (m_capture.isOpened()) {
        m_capture.set(cv::CAP_PROP_POS_FRAMES, pos);
    }
}

// ============================================================================
// VideoSourceManager 实现
// ============================================================================

VideoSourceManager::VideoSourceManager(QObject* parent)
    : QObject(parent)
    , m_currentSource(nullptr)
{
}

VideoSourceManager::~VideoSourceManager()
{
    closeSource();
}

bool VideoSourceManager::setSource(VideoSourceType type, const QString& param)
{
    closeSource();
    
    if (type == VideoSourceType::Camera) {
        int deviceIndex = param.isEmpty() ? 0 : param.toInt();
        return openCamera(deviceIndex);
    }
    else if (type == VideoSourceType::VideoFile) {
        return openVideoFile(param);
    }
    
    return false;
}

VideoSourceType VideoSourceManager::getCurrentType() const
{
    if (!m_currentSource) {
        return VideoSourceType::Camera;  // 默认
    }
    return m_currentSource->getType();
}

bool VideoSourceManager::openCamera(int deviceIndex)
{
    closeSource();
    
    qDebug() << "[VideoSourceManager] 切换到相机源，设备索引:" << deviceIndex;
    
    auto* cameraSource = new CameraVideoSource(deviceIndex, this);
    
    // 连接信号
    connect(cameraSource, &IVideoSource::statusChanged, this, &VideoSourceManager::statusChanged);
    connect(cameraSource, &IVideoSource::errorOccurred, this, &VideoSourceManager::errorOccurred);
    
    if (!cameraSource->open()) {
        delete cameraSource;
        return false;
    }
    
    m_currentSource = cameraSource;
    emit sourceChanged(VideoSourceType::Camera, cameraSource->getSourceName());
    
    return true;
}

bool VideoSourceManager::openVideoFile(const QString& filePath)
{
    closeSource();
    
    qDebug() << "[VideoSourceManager] 切换到视频文件源:" << filePath;
    
    auto* fileSource = new VideoFileSource(filePath, this);
    
    // 连接信号
    connect(fileSource, &IVideoSource::statusChanged, this, &VideoSourceManager::statusChanged);
    connect(fileSource, &IVideoSource::errorOccurred, this, &VideoSourceManager::errorOccurred);
    
    if (!fileSource->open()) {
        delete fileSource;
        return false;
    }
    
    // 默认循环播放视频文件
    fileSource->setLoop(true);
    
    m_currentSource = fileSource;
    emit sourceChanged(VideoSourceType::VideoFile, fileSource->getSourceName());
    
    return true;
}

void VideoSourceManager::closeSource()
{
    if (m_currentSource) {
        qDebug() << "[VideoSourceManager] 关闭当前源:" << m_currentSource->getSourceName();
        m_currentSource->close();
        m_currentSource->deleteLater();
        m_currentSource = nullptr;
    }
}

bool VideoSourceManager::grabFrame(cv::Mat& outFrame)
{
    if (!m_currentSource) {
        return false;
    }
    return m_currentSource->grabFrame(outFrame);
}

bool VideoSourceManager::isOpened() const
{
    if (!m_currentSource) {
        return false;
    }
    return m_currentSource->isOpened();
}
