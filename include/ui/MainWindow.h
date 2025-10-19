#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QGraphicsView>
#include <QtWidgets/QGraphicsScene>
#include <QtWidgets/QGraphicsPixmapItem>
#include <QtGui/QPixmap>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QElapsedTimer>
#include "ui_MainWindow.h"
#include "CameraController.h"
#include "yolo/bbox.h"
#include <vector>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onRefreshDevices();
    void onConnectCamera();
    void onDisconnectCamera();
    void onStartCapture();
    void onStopCapture();
    void onSnapImage();
    void onSaveImage();
    void onSwitchVideoSource();
    void updateImage();
    // 改为无参数，从CameraController主动获取数据
    void onDetectionsUpdated();
    
    // YOLO 控制槽函数
    void onLoadYoloModel();
    void onToggleYoloDetection();

private:
    void setupUI();
    void updateStatus(const QString& message);
    void enableCaptureControls(bool enabled);
    void enableConnectionControls(bool enabled);
    void updateFPS();
    
    // GraphicsView 相关
    void loadImageToGraphicsView(const QString& imagePath, int targetWidth = 300, int targetHeight = 500);
    void drawDetectionsOnImage(cv::Mat& image, const std::vector<BoundingBox>& detections);
    QImage cvMatToQImage(const cv::Mat& mat);

private:
    Ui::MainWindow ui;
    CameraController* m_cameraController;
    QTimer* m_updateTimer;
    QPixmap m_currentImage;
    
    // GraphicsView 场景和元素
    QGraphicsScene* m_graphicsScene;
    QGraphicsPixmapItem* m_graphicsPixmapItem;
    
    // FPS计算
    QElapsedTimer m_fpsTimer;
    int m_frameCount;
    double m_currentFPS;
    
    // YOLO 检测结果
    std::vector<BoundingBox> m_latestDetections;
    
    // YOLO 状态
    bool m_yoloModelLoaded;
    QString m_currentModelPath;
    int m_currentModelIndex;
    
    // YOLO UI 控件
    QPushButton* m_loadModelButton;
    QPushButton* m_toggleYoloButton;
    QLabel* m_yoloStatusLabel;
};