#include "MainWindow.h"
#include "VideoSource.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QTextStream>
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_cameraController(new CameraController(this)), m_updateTimer(new QTimer(this)), m_graphicsScene(nullptr), m_graphicsPixmapItem(nullptr), m_frameCount(0), m_currentFPS(0.0), m_yoloModelLoaded(false), m_currentModelIndex(2)
{
    qDebug() << "########## MainWindow 构造函数第一行 ##########";
    
    // 立即设置窗口标题作为测试
    setWindowTitle("【测试】MainWindow 构造函数被调用");
    
    ui.setupUi(this);
    qDebug() << "########## UI 设置完成 ##########";
    
    setWindowTitle("【测试】UI 设置完成");
    setupUI();
    qDebug() << "setupUI 完成";
    
    // 初始化 GraphicsView 场景
    m_graphicsScene = new QGraphicsScene(this);
    ui.graphicsView->setScene(m_graphicsScene);
    ui.graphicsView->setRenderHint(QPainter::Antialiasing);
    ui.graphicsView->setRenderHint(QPainter::SmoothPixmapTransform);
    ui.graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui.graphicsView->setStyleSheet("border: 1px solid gray;");
    
    // === 调试信息：准备加载 Nodka-Deepx.png ===
    qDebug() << "========== 构造函数：准备加载图片 ==========";
    qDebug() << "应用程序目录:" << QApplication::applicationDirPath();
    qDebug() << "当前工作目录:" << QDir::currentPath();
    
    // 优先使用 Qt 资源系统（内嵌在程序中），然后尝试外部文件
    QStringList possiblePaths = {
        ":/Nodka-Deepx.png",  // Qt 资源系统（推荐）
        "Nodka-Deepx.png",  // 当前目录（应该在 x64/Debug/）
        QApplication::applicationDirPath() + "/Nodka-Deepx.png",  // 应用程序目录
        QApplication::applicationDirPath() + "/../../Nodka-Deepx.png"  // 项目根目录
    };
    
    QString foundPath;
    for (const QString& path : possiblePaths)
    {
        bool exists = (path.startsWith(":/")) ? QFile::exists(path) : QFileInfo(path).exists();
        qDebug() << "检查路径:" << path << "是否存在:" << exists;
        if (exists)
        {
            foundPath = path;
            qDebug() << "*** 找到图片文件:" << path;
            break;
        }
    }
    
    if (foundPath.isEmpty())
    {
        qDebug() << "!!! 错误：在所有可能的路径中都找不到 Nodka-Deepx.png";
        updateStatus("错误：找不到 Nodka-Deepx.png 图片文件");
    }
    else
    {
        qDebug() << "使用路径:" << foundPath;
        loadImageToGraphicsView(foundPath);
    }
    
    // 初始化FPS计时器
    m_fpsTimer.start();
    
    // 连接信号和槽
    connect(ui.refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(ui.connectButton, &QPushButton::clicked, this, &MainWindow::onConnectCamera);
    connect(ui.disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectCamera);
    connect(ui.switchSourceButton, &QPushButton::clicked, this, &MainWindow::onSwitchVideoSource);
    connect(ui.startButton, &QPushButton::clicked, this, &MainWindow::onStartCapture);
    connect(ui.stopButton, &QPushButton::clicked, this, &MainWindow::onStopCapture);
    connect(ui.snapButton, &QPushButton::clicked, this, &MainWindow::onSnapImage);
    connect(ui.saveButton, &QPushButton::clicked, this, &MainWindow::onSaveImage);
    // 连接相机控制器信号
    connect(m_cameraController, &CameraController::statusChanged, this, &MainWindow::updateStatus);
    
    // ========== 轮询模式：不再连接 detectionsUpdated 信号 ==========
    // connect(m_cameraController, &CameraController::detectionsUpdated, this, &MainWindow::onDetectionsUpdated);
    // 现在在 updateImage() 定时器中主动轮询检测结果
    
    // 设置更新定时器
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateImage);
    m_updateTimer->setInterval(50); // 20 FPS
    // 初始刷新设备列表
    onRefreshDevices();
    
    updateStatus("程序启动完成，请点击'切换视频源'选择相机或视频文件");
}
MainWindow::~MainWindow()
{
    if (m_cameraController->isConnected())
    {
        m_cameraController->disconnectCamera();
    }
    
    // 清理Graphics Scene
    if (m_graphicsScene)
    {
        m_graphicsScene->clear();
        delete m_graphicsScene;
    }
}
void MainWindow::setupUI()
{
    setWindowTitle("大華相机控制器 - FPS: 0.0");
    
    // 创建 YOLO 控制按钮（假设 UI 中有一个容器或我们添加到状态栏）
    m_loadModelButton = new QPushButton("加载 YOLO 模型", this);
    m_toggleYoloButton = new QPushButton("启用检测", this);
    m_yoloStatusLabel = new QLabel("YOLO: 未加载", this);
    
    // 初始状态
    m_toggleYoloButton->setEnabled(false);
    m_toggleYoloButton->setCheckable(true);
    
    // 将按钮添加到状态栏
    statusBar()->addPermanentWidget(m_yoloStatusLabel);
    statusBar()->addPermanentWidget(m_loadModelButton);
    statusBar()->addPermanentWidget(m_toggleYoloButton);
    
    // 连接信号
    connect(m_loadModelButton, &QPushButton::clicked, this, &MainWindow::onLoadYoloModel);
    connect(m_toggleYoloButton, &QPushButton::clicked, this, &MainWindow::onToggleYoloDetection);
    
    // 设置初始状态
    enableCaptureControls(false);
    enableConnectionControls(true);
}
void MainWindow::onRefreshDevices()
{
    updateStatus("正在刷新设备列表...");
    QStringList devices = m_cameraController->refreshDevices();
    ui.deviceComboBox->clear();
    ui.deviceComboBox->addItems(devices);
    if (devices.isEmpty())
    {
        updateStatus("未发现任何相机设备");
    }
    else
    {
        updateStatus(QString("发现 %1 个设备").arg(devices.size()));
    }
}
void MainWindow::onConnectCamera()
{
    int index = ui.deviceComboBox->currentIndex();
    if (index < 0)
    {
        QMessageBox::warning(this, "警告", "请先选择一个设备");
        return;
    }
    updateStatus("正在连接相机...");
    if (m_cameraController->connectCamera(index))
    {
        enableConnectionControls(false);
        enableCaptureControls(true);
        ui.disconnectButton->setEnabled(true);
        updateStatus("相机连接成功");
    }
    else
    {
        QMessageBox::critical(this, "错误", "连接相机失败");
        updateStatus("相机连接失败");
    }
}
void MainWindow::onDisconnectCamera()
{
    if (m_updateTimer->isActive())
    {
        m_updateTimer->stop();
    }
    
    // 检查当前视频源类型
    VideoSourceType sourceType = m_cameraController->getCurrentSourceType();
    
    m_cameraController->disconnectCamera();
    
    if (sourceType == VideoSourceType::Camera)
    {
        // 相机模式：完全断开，禁用采集控制
        enableConnectionControls(true);
        enableCaptureControls(false);
        ui.disconnectButton->setEnabled(false);
        ui.stopButton->setEnabled(false);
        
        // 清空相机图像显示（imageLabel）
        ui.imageLabel->clear();
        ui.imageLabel->setText("相机图像显示区域");
        m_currentImage = QPixmap();  // 清空当前图像
        
        // 重置FPS显示
        setWindowTitle("大華相机控制器 - FPS: 0.0");
        
        updateStatus("相机已断开连接");
    }
    else
    {
        // 视频文件模式：保持采集控制启用，允许重新播放
        enableConnectionControls(false);
        enableCaptureControls(true);
        ui.disconnectButton->setEnabled(true);
        ui.startButton->setEnabled(true);
        ui.stopButton->setEnabled(false);
        
        updateStatus("视频已停止，可以点击'开始采集'重新播放");
    }
}

void MainWindow::onSwitchVideoSource()
{
    // 停止当前采集
    if (m_updateTimer->isActive())
    {
        m_updateTimer->stop();
    }
    if (m_cameraController->isConnected())
    {
        m_cameraController->disconnectCamera();
    }
    
    // 弹出对话框让用户选择
    QStringList items;
    items << "工业相机" << "视频文件 (MP4)";
    bool ok;
    QString item = QInputDialog::getItem(this, "切换视频源", "选择视频源类型:", items, 0, false, &ok);
    
    if (!ok || item.isEmpty())
    {
        updateStatus("取消切换");
        return;
    }
    
    if (item == "工业相机")
    {
        // 切换到相机模式
        enableConnectionControls(true);
        enableCaptureControls(false);
        ui.disconnectButton->setEnabled(false);
        onRefreshDevices();
        updateStatus("已切换到相机模式，请选择相机并连接");
    }
    else if (item == "视频文件 (MP4)")
    {
        // 选择视频文件
        QString videoPath = QFileDialog::getOpenFileName(
            this,
            "选择视频文件",
            "C:/Users/Administrator/Desktop/CameraQtApp_VS2022/assets/video",
            "视频文件 (*.mp4 *.avi *.mkv *.mov);;所有文件 (*.*)"
        );
        
        if (!videoPath.isEmpty())
        {
            qDebug() << "[MainWindow] 准备打开视频文件:" << videoPath;
            if (m_cameraController->openVideoFile(videoPath))
            {
                qDebug() << "[MainWindow] 视频文件打开成功";
                updateStatus(QString("视频文件已打开: %1").arg(videoPath));
                enableConnectionControls(false);
                enableCaptureControls(true);
                ui.disconnectButton->setEnabled(true);
                
                qDebug() << "[MainWindow] 准备调用 onStartCapture() 自动播放...";
                // 自动开始播放
                onStartCapture();
                qDebug() << "[MainWindow] onStartCapture() 调用完成";
            }
            else
            {
                qDebug() << "[MainWindow] 打开视频文件失败！";
                QMessageBox::critical(this, "错误", "打开视频文件失败");
                updateStatus("打开视频文件失败");
            }
        }
        else
        {
            qDebug() << "[MainWindow] 用户取消选择视频文件";
        }
    }
}

void MainWindow::onStartCapture()
{
    qDebug() << "[MainWindow::onStartCapture] ========== 函数开始 ==========";
    
    // 检查当前视频源类型
    VideoSourceType sourceType = m_cameraController->getCurrentSourceType();
    qDebug() << "[MainWindow::onStartCapture] 当前视频源类型:" << (sourceType == VideoSourceType::Camera ? "相机" : "视频文件");
    
    bool success = false;
    if (sourceType == VideoSourceType::Camera)
    {
        qDebug() << "[MainWindow::onStartCapture] 相机模式：调用 startCapture()";
        // 相机模式：调用相机API启动采集
        success = m_cameraController->startCapture();
        qDebug() << "[MainWindow::onStartCapture] 相机 startCapture() 返回:" << success;
    }
    else if (sourceType == VideoSourceType::VideoFile)
    {
        qDebug() << "[MainWindow::onStartCapture] 视频文件模式：无需调用 startCapture()";
        // 视频文件模式：直接启动定时器播放
        success = true;  // 视频文件已经打开，无需调用startCapture
    }
    
    qDebug() << "[MainWindow::onStartCapture] success =" << success;
    
    if (success)
    {
        qDebug() << "[MainWindow::onStartCapture] 成功，准备启动定时器";
        ui.startButton->setEnabled(false);
        ui.stopButton->setEnabled(true);
        
        // 重置FPS计数器
        m_frameCount = 0;
        m_currentFPS = 0.0;
        m_fpsTimer.restart();
        
        // 设置更新定时器间隔为 33ms (约30 FPS)，0 表示尽快执行
        m_updateTimer->setInterval(0);  // 0 = 尽快执行，不阻塞事件循环
        qDebug() << "[MainWindow::onStartCapture] 定时器间隔设置为:" << m_updateTimer->interval() << "ms";
        m_updateTimer->start();
        qDebug() << "[MainWindow::onStartCapture] 定时器已启动，isActive =" << m_updateTimer->isActive();
        
        if (sourceType == VideoSourceType::Camera)
        {
            updateStatus("开始连续采集");
        }
        else
        {
            updateStatus("开始播放视频");
            qDebug() << "[MainWindow::onStartCapture] 状态更新为：开始播放视频";
        }
    }
    else
    {
        qDebug() << "[MainWindow::onStartCapture] 失败！";
        QMessageBox::critical(this, "错误", "启动采集失败");
        updateStatus("启动采集失败");
    }
    
    qDebug() << "[MainWindow::onStartCapture] ========== 函数结束 ==========";
}
void MainWindow::onStopCapture()
{
    m_updateTimer->stop();
    
    // 检查当前视频源类型
    VideoSourceType sourceType = m_cameraController->getCurrentSourceType();
    
    bool success = false;
    if (sourceType == VideoSourceType::Camera)
    {
        // 相机模式：调用相机API停止采集
        success = m_cameraController->stopCapture();
    }
    else if (sourceType == VideoSourceType::VideoFile)
    {
        // 视频文件模式：只需停止定时器即可
        success = true;
    }
    
    if (success)
    {
        ui.startButton->setEnabled(true);
        ui.stopButton->setEnabled(false);

        // 停止后重置窗口标题
        setWindowTitle("大華相机控制器 - FPS: 0.0");
        
        if (sourceType == VideoSourceType::Camera)
        {
            updateStatus("停止采集");
        }
        else
        {
            updateStatus("停止播放");
        }
    }
    else
    {
        QMessageBox::critical(this, "错误", "停止采集失败");
        updateStatus("停止采集失败");
    }
}
void MainWindow::onSnapImage()
{
    updateStatus("正在单帧采集...");
    if (m_cameraController->snapImage())
    { // 手动触发一次图像更新
        updateImage();
        updateStatus("单帧采集成功");
    }
    else
    {
        QMessageBox::critical(this, "错误", "单帧采集失败");
        updateStatus("单帧采集失败");
    }
}
void MainWindow::onSaveImage()
{
    if (m_currentImage.isNull())
    {
        QMessageBox::warning(this, "警告", "没有可保存的图像");
        return;
    }
    QString fileName = QFileDialog::getSaveFileName(this, "保存图像", QString("camera_image_%1.bmp").arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss")), "BMP Files (*.bmp);;JPEG Files (*.jpg);;PNG Files (*.png)");
    if (!fileName.isEmpty())
    {
        if (m_cameraController->saveCurrentImage(fileName))
        {
            updateStatus(QString("图像已保存到: %1").arg(fileName));
        }
        else
        {
            QMessageBox::critical(this, "错误", "保存图像失败");
            updateStatus("保存图像失败");
        }
    }
}
void MainWindow::updateImage()
{
    static int frameCounter = 0;
    frameCounter++;
    
    if (frameCounter == 1 || frameCounter % 30 == 0) {
        qDebug() << "[MainWindow::updateImage] 定时器触发，帧计数:" << frameCounter;
    }
    
    // ========== 轮询模式：主动从 YoloDetector 获取最新检测结果 ==========
    // 直接从 YoloDetector 获取（绕过 CameraController）
    auto latestDetections = m_cameraController->getYoloDetector()->getLatestResults();
    if (!latestDetections.empty()) {
        m_latestDetections = latestDetections;
        
        if (frameCounter % 10 == 0) {  // 每10帧打印一次
            qDebug() << "[MainWindow::updateImage] 轮询到检测结果:" << m_latestDetections.size() << "个目标";
        }
    }
    // =================================================
    
    // 在连续采集模式下，主动获取一帧
    m_cameraController->grabFrame();
    
    if (m_cameraController->hasNewImage())
    {
        if (frameCounter == 1 || frameCounter % 30 == 0) {
            qDebug() << "[MainWindow::updateImage] 有新图像";
        }
        
        cv::Mat image = m_cameraController->getCurrentImage();
        m_cameraController->clearNewImageFlag();
        if (!image.empty())
        {
            if (frameCounter == 1 || frameCounter % 30 == 0) {
                qDebug() << "[MainWindow::updateImage] 图像有效，尺寸:" << image.cols << "x" << image.rows;
            }
            
            // 如果有检测结果，在图像上绘制检测框
            if (!m_latestDetections.empty())
            {
                drawDetectionsOnImage(image, m_latestDetections);
            }
            
            // 转换OpenCV Mat到QPixmap
            cv::Mat rgbImage;
            if (image.channels() == 3)
            {
                cv::cvtColor(image, rgbImage, cv::COLOR_BGR2RGB);
            }
            else
            {
                cv::cvtColor(image, rgbImage, cv::COLOR_GRAY2RGB);
            }
            QImage qimg(rgbImage.data, rgbImage.cols, rgbImage.rows, rgbImage.step, QImage::Format_RGB888);
            m_currentImage = QPixmap::fromImage(qimg);
            
            // 在 imageLabel 中显示相机实时图像（左侧）
            ui.imageLabel->setPixmap(m_currentImage);
            
            // 更新FPS
            updateFPS();
        }
    }
}
void MainWindow::updateStatus(const QString &message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logMessage = QString("[%1] %2").arg(timestamp, message);
    ui.statusTextEdit->append(logMessage); // 自动滚动到底部
    QTextCursor cursor = ui.statusTextEdit->textCursor();
    cursor.movePosition(QTextCursor::End);
    ui.statusTextEdit->setTextCursor(cursor); // 更新状态栏
    ui.statusbar->showMessage(message, 3000);
}
void MainWindow::enableCaptureControls(bool enabled) { ui.captureGroupBox->setEnabled(enabled); }
void MainWindow::enableConnectionControls(bool enabled)
{
    ui.refreshButton->setEnabled(enabled);
    ui.deviceComboBox->setEnabled(enabled);
    ui.connectButton->setEnabled(enabled);
}

void MainWindow::updateFPS()
{
    m_frameCount++;
    
    // 每秒更新一次FPS显示
    qint64 elapsed = m_fpsTimer.elapsed();
    if (elapsed >= 1000) // 1秒
    {
        m_currentFPS = (m_frameCount * 1000.0) / elapsed;
        
        // 更新窗口标题显示FPS
        setWindowTitle(QString("大華相机控制器 - FPS: %1").arg(m_currentFPS, 0, 'f', 1));
        
        // 重置计数器
        m_frameCount = 0;
        m_fpsTimer.restart();
    }
}

void MainWindow::loadImageToGraphicsView(const QString& imagePath, int targetWidth, int targetHeight)
{
    // === 调试信息：开始加载图片 ===
    qDebug() << "========== loadImageToGraphicsView 开始 ==========";
    qDebug() << "传入的图片路径:" << imagePath;
    qDebug() << "目标宽度:" << targetWidth << "目标高度:" << targetHeight;
    qDebug() << "应用程序目录:" << QApplication::applicationDirPath();
    qDebug() << "当前工作目录:" << QDir::currentPath();
    
    // 检查文件是否存在
    QFileInfo fileInfo(imagePath);
    qDebug() << "文件是否存在:" << fileInfo.exists();
    qDebug() << "文件绝对路径:" << fileInfo.absoluteFilePath();
    qDebug() << "文件大小:" << fileInfo.size() << "bytes";
    qDebug() << "是否为文件:" << fileInfo.isFile();
    qDebug() << "是否可读:" << fileInfo.isReadable();
    
    // 加载图片
    QPixmap pixmap(imagePath);
    
    qDebug() << "QPixmap 是否为空:" << pixmap.isNull();
    qDebug() << "QPixmap 原始宽度:" << pixmap.width();
    qDebug() << "QPixmap 原始高度:" << pixmap.height();
    
    if (pixmap.isNull())
    {
        QString errorMsg = QString("无法加载图片: %1\n文件存在: %2\n绝对路径: %3")
            .arg(imagePath)
            .arg(fileInfo.exists() ? "是" : "否")
            .arg(fileInfo.absoluteFilePath());
        
        qDebug() << "!!! 错误:" << errorMsg;
        updateStatus(errorMsg);
        QMessageBox::warning(this, "警告", errorMsg);
        return;
    }
    
    qDebug() << "图片加载成功！原始宽度:" << pixmap.width() << "原始高度:" << pixmap.height();
    
    // 如果指定了目标尺寸，进行缩放
    if (targetWidth > 0 || targetHeight > 0)
    {
        int finalWidth = targetWidth > 0 ? targetWidth : pixmap.width();
        int finalHeight = targetHeight > 0 ? targetHeight : pixmap.height();
        
        // 如果只指定了一个维度，按比例计算另一个维度
        if (targetWidth > 0 && targetHeight <= 0)
        {
            finalHeight = static_cast<int>(pixmap.height() * (static_cast<double>(targetWidth) / pixmap.width()));
        }
        else if (targetHeight > 0 && targetWidth <= 0)
        {
            finalWidth = static_cast<int>(pixmap.width() * (static_cast<double>(targetHeight) / pixmap.height()));
        }
        
        pixmap = pixmap.scaled(finalWidth, finalHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        qDebug() << "图片已缩放到:" << pixmap.width() << "x" << pixmap.height();
    }
    
    // 清空现有场景
    m_graphicsScene->clear();
    m_graphicsPixmapItem = nullptr;
    
    // 添加图片到场景
    m_graphicsPixmapItem = m_graphicsScene->addPixmap(pixmap);
    qDebug() << "图片已添加到场景，最终尺寸:" << pixmap.width() << "x" << pixmap.height();
    
    // 设置场景矩形为图片大小
    m_graphicsScene->setSceneRect(pixmap.rect());
    qDebug() << "场景矩形已设置:" << pixmap.rect();
    
    // 重置变换，以固定尺寸显示图片（1:1）
    ui.graphicsView->resetTransform();
    ui.graphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    qDebug() << "图片以固定尺寸（1:1）显示";
    
    QString sizeInfo = (targetWidth > 0 || targetHeight > 0) 
        ? QString("已缩放到 %1x%2").arg(pixmap.width()).arg(pixmap.height())
        : QString("原始尺寸 %1x%2").arg(pixmap.width()).arg(pixmap.height());
    
    updateStatus(QString("已加载图片: %1 (%2)").arg(imagePath).arg(sizeInfo));
}

// ==================== YOLO 控制函数 ====================

void MainWindow::onLoadYoloModel()
{
    qDebug() << "[UI] ========== 开始加载 YOLO 模型 ==========";
    
    // 使用老版本 YOLOv7 模型（传统 anchor-based YOLO）
    QString modelPath = "./assets/models/YoloV7.dxnn";
    int paramIndex = 4;  // yolov7_640 (索引 4)
    
    qDebug() << "[UI] 使用固定模型文件:" << modelPath;
    qDebug() << "[UI] 使用固定参数索引:" << paramIndex << "(yolov7_640)";
    
    updateStatus("正在加载 YOLO 模型...");
    m_yoloStatusLabel->setText("YOLO: 加载中...");
    QApplication::processEvents(); // 更新 UI
    
    qDebug() << "[UI] 调用 CameraController::initializeYolo...";
    
    try {
        if (m_cameraController->initializeYolo(modelPath, paramIndex)) {
            qDebug() << "[UI] YOLO 模型加载成功";
            m_yoloModelLoaded = true;
            m_currentModelPath = modelPath;
            m_currentModelIndex = paramIndex;
            
            QFileInfo fileInfo(modelPath);
            QString statusText = QString("YOLO: %1 已加载").arg(fileInfo.fileName());
            m_yoloStatusLabel->setText(statusText);
            m_toggleYoloButton->setEnabled(true);
            
            updateStatus(QString("YOLO 模型加载成功: %1").arg(fileInfo.fileName()));
            
            QMessageBox::information(this, "成功", "YOLO 模型加载成功！\n您现在可以启用检测。");
        } else {
            qWarning() << "[UI] YOLO 模型加载失败";
            m_yoloStatusLabel->setText("YOLO: 加载失败");
            updateStatus("YOLO 模型加载失败");
            QMessageBox::critical(this, "错误", "加载 YOLO 模型失败，请检查模型文件和参数配置。\n\n查看控制台日志获取详细信息。");
        }
    }
    catch (const std::exception& e) {
        qCritical() << "[UI EXCEPTION] YOLO 加载异常:" << e.what();
        m_yoloStatusLabel->setText("YOLO: 异常");
        updateStatus("YOLO 模型加载异常");
        QMessageBox::critical(this, "异常", QString("加载 YOLO 模型时发生异常:\n%1").arg(e.what()));
    }
    catch (...) {
        qCritical() << "[UI EXCEPTION] YOLO 加载未知异常";
        m_yoloStatusLabel->setText("YOLO: 异常");
        updateStatus("YOLO 模型加载异常");
        QMessageBox::critical(this, "异常", "加载 YOLO 模型时发生未知异常！");
    }
    
    qDebug() << "[UI] ========== YOLO 模型加载流程结束 ==========";
}

void MainWindow::onToggleYoloDetection()
{
    if (!m_yoloModelLoaded) {
        QMessageBox::warning(this, "警告", "请先加载 YOLO 模型！");
        m_toggleYoloButton->setChecked(false);
        return;
    }
    
    bool enable = m_toggleYoloButton->isChecked();
    m_cameraController->enableYoloDetection(enable);
    
    if (enable) {
        m_toggleYoloButton->setText("禁用检测");
        m_yoloStatusLabel->setText("YOLO: 检测中...");
        m_yoloStatusLabel->setStyleSheet("color: green; font-weight: bold;");
        updateStatus("YOLO 检测已启用");
    } else {
        m_toggleYoloButton->setText("启用检测");
        QFileInfo fileInfo(m_currentModelPath);
        m_yoloStatusLabel->setText(QString("YOLO: %1 已加载").arg(fileInfo.fileName()));
        m_yoloStatusLabel->setStyleSheet("");
        updateStatus("YOLO 检测已禁用");
        
        // 清空检测结果
        m_latestDetections.clear();
    }
}
