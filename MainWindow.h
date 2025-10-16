#pragma once

#include <QtWidgets/QMainWindow>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QGroupBox>
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

private:
    Ui::MainWindow ui;
    CameraController* m_cameraController;
    QTimer* m_updateTimer;
    QPixmap m_currentImage;
    
    // FPS计算
    QElapsedTimer m_fpsTimer;
    int m_frameCount;
    double m_currentFPS;
};