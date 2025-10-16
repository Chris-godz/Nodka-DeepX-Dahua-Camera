#include "MainWindow.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QDateTime>
MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), m_cameraController(new CameraController(this)), m_updateTimer(new QTimer(this)), m_frameCount(0), m_currentFPS(0.0)
{
    ui.setupUi(this);
    setupUI();
    
    // 初始化FPS计时器
    m_fpsTimer.start();
    
    // 连接信号和槽
    connect(ui.refreshButton, &QPushButton::clicked, this, &MainWindow::onRefreshDevices);
    connect(ui.connectButton, &QPushButton::clicked, this, &MainWindow::onConnectCamera);
    connect(ui.disconnectButton, &QPushButton::clicked, this, &MainWindow::onDisconnectCamera);
    connect(ui.startButton, &QPushButton::clicked, this, &MainWindow::onStartCapture);
    connect(ui.stopButton, &QPushButton::clicked, this, &MainWindow::onStopCapture);
    connect(ui.snapButton, &QPushButton::clicked, this, &MainWindow::onSnapImage);
    connect(ui.saveButton, &QPushButton::clicked, this, &MainWindow::onSaveImage);
    // 连接相机控制器信号
    connect(m_cameraController, &CameraController::statusChanged, this, &MainWindow::updateStatus); // 设置更新定时器
    connect(m_updateTimer, &QTimer::timeout, this, &MainWindow::updateImage);
    m_updateTimer->setInterval(50); // 20 FPS
    // 初始刷新设备列表
    onRefreshDevices();
    updateStatus("程序启动完成");
}
MainWindow::~MainWindow()
{
    if (m_cameraController->isConnected())
    {
        m_cameraController->disconnectCamera();
    }
}
void MainWindow::setupUI()
{
    setWindowTitle("简单相机控制器 - FPS: 0.0");
    
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
    m_cameraController->disconnectCamera();
    enableConnectionControls(true);
    enableCaptureControls(false);
    ui.disconnectButton->setEnabled(false);
    ui.stopButton->setEnabled(false); // 清空图像显示
    ui.imageLabel->clear();
    ui.imageLabel->setText("相机图像显示区域");
    
    // 重置FPS显示
    setWindowTitle("简单相机控制器 - FPS: 0.0");
    
    updateStatus("相机已断开连接");
}
void MainWindow::onStartCapture()
{
    if (m_cameraController->startCapture())
    {
        ui.startButton->setEnabled(false);
        ui.stopButton->setEnabled(true);
        
        // 重置FPS计数器
        m_frameCount = 0;
        m_currentFPS = 0.0;
        m_fpsTimer.restart();
        
        m_updateTimer->start();
        updateStatus("开始连续采集");
    }
    else
    {
        QMessageBox::critical(this, "错误", "启动采集失败");
        updateStatus("启动采集失败");
    }
}
void MainWindow::onStopCapture()
{
    m_updateTimer->stop();
    
    if (m_cameraController->stopCapture())
    {
        ui.startButton->setEnabled(true);
        ui.stopButton->setEnabled(false);
        
        // 停止后重置窗口标题
        setWindowTitle("简单相机控制器 - FPS: 0.0");
        
        updateStatus("停止采集");
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
    // 在连续采集模式下，主动获取一帧
    m_cameraController->grabFrame();
    
    if (m_cameraController->hasNewImage())
    {
        cv::Mat image = m_cameraController->getCurrentImage();
        m_cameraController->clearNewImageFlag();
        if (!image.empty())
        {
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
            m_currentImage = QPixmap::fromImage(qimg); // 缩放图像以适应显示区域
            QPixmap scaledPixmap = m_currentImage.scaled(ui.imageLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            ui.imageLabel->setPixmap(scaledPixmap);
            
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
        setWindowTitle(QString("简单相机控制器 - FPS: %1").arg(m_currentFPS, 0, 'f', 1));
        
        // 重置计数器
        m_frameCount = 0;
        m_fpsTimer.restart();
    }
}