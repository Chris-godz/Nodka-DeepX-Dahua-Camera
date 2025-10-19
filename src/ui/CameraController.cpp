#include "CameraController.h"
#include <QDebug>
#include <chrono>

CameraController::CameraController(QObject *parent) 
    : QObject(parent)
    , m_deviceHandle(nullptr)
    , m_isConnected(false)
    , m_isCapturing(false)
    , m_hasNewImage(false)
    , m_yoloDetector(nullptr)
    , m_yoloEnabled(false)
{
    m_yoloDetector = new YoloDetector(this);
}

CameraController::~CameraController()
{
    if (m_isConnected)
    {
        disconnectCamera();
    }
}
QStringList CameraController::refreshDevices()
{
    QStringList deviceNames;
    m_deviceList.clear(); // 发现设备
    IMV_DeviceList deviceInfoList;
    int ret = IMV_EnumDevices(&deviceInfoList, interfaceTypeAll);
    if (ret != IMV_OK)
    {
        logStatus(QString("枚举设备失败，错误码: %1").arg(ret));
        return deviceNames;
    }
    logStatus(QString("发现 %1 个设备").arg(deviceInfoList.nDevNum));
    for (unsigned int i = 0; i < deviceInfoList.nDevNum; i++)
    {
        IMV_DeviceInfo *pDevInfo = &deviceInfoList.pDevInfo[i];
        DeviceInfo info;
        info.index = i;
        info.name = QString::fromLocal8Bit(pDevInfo->modelName);
        info.serialNumber = QString::fromLocal8Bit(pDevInfo->serialNumber); // 如果是GigE相机，获取IP地址
        if (pDevInfo->nCameraType == typeGigeCamera)
        {
            info.ipAddress = QString::fromLocal8Bit(pDevInfo->DeviceSpecificInfo.gigeDeviceInfo.ipAddress);
        }
        m_deviceList.append(info);
        QString displayName = QString("%1 - %2").arg(info.name, info.serialNumber);
        if (!info.ipAddress.isEmpty())
        {
            displayName += QString(" (%1)").arg(info.ipAddress);
        }
        deviceNames.append(displayName);
    }
    return deviceNames;
}
bool CameraController::connectCamera(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= m_deviceList.size())
    {
        logStatus("无效的设备索引");
        return false;
    } // 创建设备句柄
    unsigned int cameraIndex = deviceIndex;
    int ret = IMV_CreateHandle(&m_deviceHandle, modeByIndex, (void *)&cameraIndex);
    if (ret != IMV_OK)
    {
        logStatus(QString("创建设备句柄失败，错误码: %1").arg(ret));
        return false;
    } // 打开相机
    ret = IMV_Open(m_deviceHandle);
    if (ret != IMV_OK)
    {
        logStatus(QString("打开相机失败，错误码: %1").arg(ret));
        IMV_DestroyHandle(m_deviceHandle);
        m_deviceHandle = nullptr;
        return false;
    } // 关闭触发模式
    ret = IMV_SetEnumFeatureSymbol(m_deviceHandle, "TriggerMode", "Off");
    if (ret != IMV_OK)
    {
        logStatus(QString("设置触发模式失败，错误码: %1").arg(ret)); // 不是致命错误，继续
    }
    m_isConnected = true;
    logStatus(QString("成功连接到设备: %1").arg(m_deviceList[deviceIndex].name));
    return true;
}
void CameraController::disconnectCamera()
{
    if (!m_isConnected)
    {
        return;
    } // 停止采集
    if (m_isCapturing)
    {
        stopCapture();
    } // 关闭相机
    if (m_deviceHandle)
    {
        IMV_Close(m_deviceHandle);
        IMV_DestroyHandle(m_deviceHandle);
        m_deviceHandle = nullptr;
    }
    m_isConnected = false;
    m_currentImage.release();
    m_hasNewImage = false;
    logStatus("相机已断开连接");
}
bool CameraController::startCapture()
{
    if (!m_isConnected)
    {
        logStatus("相机未连接");
        return false;
    }
    int ret = IMV_StartGrabbing(m_deviceHandle);
    if (ret != IMV_OK)
    {
        logStatus(QString("开始采集失败，错误码: %1").arg(ret));
        return false;
    }
    m_isCapturing = true;
    logStatus("开始连续采集");
    return true;
}
bool CameraController::stopCapture()
{
    if (!m_isConnected || !m_isCapturing)
    {
        return true;
    }
    int ret = IMV_StopGrabbing(m_deviceHandle);
    if (ret != IMV_OK)
    {
        logStatus(QString("停止采集失败，错误码: %1").arg(ret));
        return false;
    }
    m_isCapturing = false;
    logStatus("停止采集");
    return true;
}
bool CameraController::snapImage()
{
    if (!m_isConnected)
    {
        logStatus("相机未连接");
        return false;
    }
    
    // 如果没有在采集，需要临时启动采集
    bool wasCapturing = m_isCapturing;
    if (!m_isCapturing)
    {
        int ret = IMV_StartGrabbing(m_deviceHandle);
        if (ret != IMV_OK)
        {
            logStatus(QString("启动采集失败，错误码: %1").arg(ret));
            return false;
        }
    }
    
    // 获取一帧
    IMV_Frame frame;
    int ret = IMV_GetFrame(m_deviceHandle, &frame, 2000); // 增加超时时间到2秒
    
    // 如果之前没有在采集，现在停止
    if (!wasCapturing && m_isCapturing == false)
    {
        IMV_StopGrabbing(m_deviceHandle);
    }
    
    if (ret != IMV_OK)
    {
        logStatus(QString("获取帧失败，错误码: %1").arg(ret));
        return false;
    }
    
    bool success = convertFrameToMat(&frame);
    
    // 释放帧
    IMV_ReleaseFrame(m_deviceHandle, &frame);
    
    if (success)
    {
        m_hasNewImage = true;
        emit imageUpdated();
        logStatus("单帧采集成功");
    }
    
    return success;
}

bool CameraController::grabFrame()
{
    if (!m_isConnected || !m_isCapturing)
    {
        return false;
    }
    
    IMV_Frame frame;
    int ret = IMV_GetFrame(m_deviceHandle, &frame, 100); // 100ms超时
    
    if (ret != IMV_OK)
    {
        // 超时不记录错误，这在连续采集中是正常的
        if (ret != -119) // -119 是超时错误
        {
            logStatus(QString("获取帧失败，错误码: %1").arg(ret));
        }
        return false;
    }
    
    bool success = convertFrameToMat(&frame);
    
    // 释放帧
    IMV_ReleaseFrame(m_deviceHandle, &frame);
    
    if (success)
    {
        // 如果启用了 YOLO 检测，进行推理
        if (m_yoloEnabled && m_yoloDetector && m_yoloDetector->isInitialized())
        {
            processImageWithYolo(m_currentImage);
        }
        
        m_hasNewImage = true;
        emit imageUpdated();
    }
    
    return success;
}

bool CameraController::saveCurrentImage(const QString &filename)
{
    if (m_currentImage.empty())
    {
        logStatus("没有可保存的图像");
        return false;
    }
    try
    {
        cv::imwrite(filename.toLocal8Bit().data(), m_currentImage);
        logStatus(QString("图像已保存: %1").arg(filename));
        return true;
    }
    catch (const cv::Exception &e)
    {
        logStatus(QString("保存图像失败: %1").arg(e.what()));
        return false;
    }
}
QStringList CameraController::getDeviceList() const
{
    QStringList result;
    for (const auto &device : m_deviceList)
    {
        result.append(device.name);
    }
    return result;
}
DeviceInfo CameraController::getDeviceInfo(int index) const
{
    if (index >= 0 && index < m_deviceList.size())
    {
        return m_deviceList[index];
    }
    return DeviceInfo();
}
bool CameraController::convertFrameToMat(IMV_Frame *frame)
{
    if (!frame || !frame->pData)
    {
        logStatus("无效的帧数据");
        return false;
    }
    cv::Mat srcImage;
    unsigned char *pDstData = nullptr;
    try
    {
        if (frame->frameInfo.pixelFormat == gvspPixelMono8)
        { // 单色8位
            srcImage = cv::Mat(frame->frameInfo.height, frame->frameInfo.width, CV_8UC1, frame->pData);
        }
        else if (frame->frameInfo.pixelFormat == gvspPixelBGR8)
        { // BGR8位
            srcImage = cv::Mat(frame->frameInfo.height, frame->frameInfo.width, CV_8UC3, frame->pData);
        }
        else
        { // 其他格式转换为BGR8
            pDstData = new unsigned char[frame->frameInfo.width * frame->frameInfo.height * 3];
            IMV_PixelConvertParam stPixelConvertParam = {0};
            stPixelConvertParam.nWidth = frame->frameInfo.width;
            stPixelConvertParam.nHeight = frame->frameInfo.height;
            stPixelConvertParam.ePixelFormat = frame->frameInfo.pixelFormat;
            stPixelConvertParam.pSrcData = frame->pData;
            stPixelConvertParam.nSrcDataLen = frame->frameInfo.size;
            stPixelConvertParam.nPaddingX = frame->frameInfo.paddingX;
            stPixelConvertParam.nPaddingY = frame->frameInfo.paddingY;
            stPixelConvertParam.eBayerDemosaic = demosaicNearestNeighbor;
            stPixelConvertParam.eDstPixelFormat = gvspPixelBGR8;
            stPixelConvertParam.pDstBuf = pDstData;
            stPixelConvertParam.nDstBufSize = frame->frameInfo.width * frame->frameInfo.height * 3;
            int nRet = IMV_PixelConvert(m_deviceHandle, &stPixelConvertParam);
            if (nRet != IMV_OK)
            {
                logStatus(QString("像素格式转换失败，错误码: %1").arg(nRet));
                delete[] pDstData;
                return false;
            }
            srcImage = cv::Mat(frame->frameInfo.height, frame->frameInfo.width, CV_8UC3, pDstData);
        }
        if (srcImage.empty())
        {
            logStatus("创建Mat失败");
            if (pDstData)
                delete[] pDstData;
            return false;
        } // 复制图像数据
        m_currentImage = srcImage.clone();
        if (pDstData)
        {
            delete[] pDstData;
        }
        return true;
    }
    catch (const cv::Exception &e)
    {
        logStatus(QString("OpenCV异常: %1").arg(e.what()));
        if (pDstData)
            delete[] pDstData;
        return false;
    }
    catch (...)
    {
        logStatus("未知异常");
        if (pDstData)
            delete[] pDstData;
        return false;
    }
}

// YOLO 检测相关实现
bool CameraController::initializeYolo(const QString& modelPath, int parameterIndex)
{
    if (!m_yoloDetector) {
        logStatus("YOLO 检测器未创建");
        return false;
    }
    
    bool success = m_yoloDetector->initializeModel(modelPath, parameterIndex);
    if (success) {
        logStatus(QString("YOLO 模型加载成功: %1").arg(modelPath));
    } else {
        logStatus("YOLO 模型加载失败");
    }
    
    return success;
}

void CameraController::processImageWithYolo(cv::Mat& image)
{
    static int totalFrames = 0;
    static int processedFrames = 0;
    totalFrames++;
    
    if (!m_yoloEnabled) {
        if (totalFrames % 100 == 0) {  // 每100帧打印一次
            qDebug() << "[CAMERA] YOLO 未启用, 已跳过" << totalFrames << "帧";
        }
        return;
    }
    
    if (!m_yoloDetector || !m_yoloDetector->isInitialized()) {
        if (totalFrames % 100 == 0) {
            qWarning() << "[CAMERA] YOLO 检测器未初始化, 已跳过" << totalFrames << "帧";
        }
        return;
    }
    
    processedFrames++;
    qDebug() << "[CAMERA] ========== 处理第" << processedFrames << "帧 (总帧数:" << totalFrames << ") ==========";
    qDebug() << "[CAMERA] 图像尺寸:" << image.cols << "x" << image.rows;
    
    try {
        // 执行同步检测
        qDebug() << "[CAMERA] 调用 YOLO 检测...";
        auto startTime = std::chrono::high_resolution_clock::now();
        
        m_latestDetections = m_yoloDetector->detectSync(image);
        
        auto endTime = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
        
        qDebug() << "[CAMERA] YOLO 检测完成, 耗时:" << duration << "ms";
        qDebug() << "[CAMERA] 检测结果数量:" << m_latestDetections.size();
        
        // 发送检测结果信号
        emit detectionsUpdated(m_latestDetections);
        
        qDebug() << "[CAMERA] 第" << processedFrames << "帧处理完成 ✓";
    }
    catch (const std::exception& e) {
        QString errorMsg = QString("YOLO 检测失败: %1").arg(e.what());
        qCritical() << "[CAMERA ERROR]" << errorMsg;
        logStatus(errorMsg);
        m_latestDetections.clear();
    }
}
void CameraController::logStatus(const QString &message)
{
    qDebug() << message;
    emit statusChanged(message);
}