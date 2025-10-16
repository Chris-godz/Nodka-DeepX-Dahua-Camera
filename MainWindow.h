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
    void updateImage();

private:
    void setupUI();
    void updateStatus(const QString& message);
    void enableCaptureControls(bool enabled);
    void enableConnectionControls(bool enabled);
    void updateFPS();
    
    // GraphicsView 相关
    void loadImageToGraphicsView(const QString& imagePath, int targetWidth = 300, int targetHeight = 500);

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
};