/********************************************************************************
** Form generated from reading UI file 'MainWindow.ui'
**
** Created by: Qt User Interface Compiler version 6.7.3
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralwidget;
    QHBoxLayout *horizontalLayout;
    QLabel *imageLabel;
    QVBoxLayout *controlLayout;
    QGroupBox *deviceGroupBox;
    QVBoxLayout *deviceLayout;
    QPushButton *refreshButton;
    QComboBox *deviceComboBox;
    QPushButton *connectButton;
    QPushButton *disconnectButton;
    QGroupBox *captureGroupBox;
    QVBoxLayout *captureLayout;
    QPushButton *startButton;
    QPushButton *stopButton;
    QPushButton *snapButton;
    QPushButton *saveButton;
    QGroupBox *statusGroupBox;
    QVBoxLayout *statusLayout;
    QTextEdit *statusTextEdit;
    QSpacerItem *verticalSpacer;
    QMenuBar *menubar;
    QStatusBar *statusbar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(1000, 700);
        centralwidget = new QWidget(MainWindow);
        centralwidget->setObjectName("centralwidget");
        horizontalLayout = new QHBoxLayout(centralwidget);
        horizontalLayout->setObjectName("horizontalLayout");
        imageLabel = new QLabel(centralwidget);
        imageLabel->setObjectName("imageLabel");
        imageLabel->setMinimumSize(QSize(640, 480));
        imageLabel->setStyleSheet(QString::fromUtf8("border: 1px solid gray;"));
        imageLabel->setAlignment(Qt::AlignCenter);
        imageLabel->setScaledContents(true);

        horizontalLayout->addWidget(imageLabel);

        controlLayout = new QVBoxLayout();
        controlLayout->setObjectName("controlLayout");
        deviceGroupBox = new QGroupBox(centralwidget);
        deviceGroupBox->setObjectName("deviceGroupBox");
        deviceLayout = new QVBoxLayout(deviceGroupBox);
        deviceLayout->setObjectName("deviceLayout");
        refreshButton = new QPushButton(deviceGroupBox);
        refreshButton->setObjectName("refreshButton");

        deviceLayout->addWidget(refreshButton);

        deviceComboBox = new QComboBox(deviceGroupBox);
        deviceComboBox->setObjectName("deviceComboBox");

        deviceLayout->addWidget(deviceComboBox);

        connectButton = new QPushButton(deviceGroupBox);
        connectButton->setObjectName("connectButton");

        deviceLayout->addWidget(connectButton);

        disconnectButton = new QPushButton(deviceGroupBox);
        disconnectButton->setObjectName("disconnectButton");
        disconnectButton->setEnabled(false);

        deviceLayout->addWidget(disconnectButton);


        controlLayout->addWidget(deviceGroupBox);

        captureGroupBox = new QGroupBox(centralwidget);
        captureGroupBox->setObjectName("captureGroupBox");
        captureGroupBox->setEnabled(false);
        captureLayout = new QVBoxLayout(captureGroupBox);
        captureLayout->setObjectName("captureLayout");
        startButton = new QPushButton(captureGroupBox);
        startButton->setObjectName("startButton");

        captureLayout->addWidget(startButton);

        stopButton = new QPushButton(captureGroupBox);
        stopButton->setObjectName("stopButton");
        stopButton->setEnabled(false);

        captureLayout->addWidget(stopButton);

        snapButton = new QPushButton(captureGroupBox);
        snapButton->setObjectName("snapButton");

        captureLayout->addWidget(snapButton);

        saveButton = new QPushButton(captureGroupBox);
        saveButton->setObjectName("saveButton");

        captureLayout->addWidget(saveButton);


        controlLayout->addWidget(captureGroupBox);

        statusGroupBox = new QGroupBox(centralwidget);
        statusGroupBox->setObjectName("statusGroupBox");
        statusLayout = new QVBoxLayout(statusGroupBox);
        statusLayout->setObjectName("statusLayout");
        statusTextEdit = new QTextEdit(statusGroupBox);
        statusTextEdit->setObjectName("statusTextEdit");
        statusTextEdit->setMaximumSize(QSize(16777215, 150));
        statusTextEdit->setReadOnly(true);

        statusLayout->addWidget(statusTextEdit);


        controlLayout->addWidget(statusGroupBox);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Policy::Minimum, QSizePolicy::Policy::Expanding);

        controlLayout->addItem(verticalSpacer);


        horizontalLayout->addLayout(controlLayout);

        MainWindow->setCentralWidget(centralwidget);
        menubar = new QMenuBar(MainWindow);
        menubar->setObjectName("menubar");
        menubar->setGeometry(QRect(0, 0, 1000, 22));
        MainWindow->setMenuBar(menubar);
        statusbar = new QStatusBar(MainWindow);
        statusbar->setObjectName("statusbar");
        MainWindow->setStatusBar(statusbar);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "\347\233\270\346\234\272\345\233\276\345\203\217\351\207\207\351\233\206", nullptr));
        imageLabel->setText(QCoreApplication::translate("MainWindow", "\347\233\270\346\234\272\345\233\276\345\203\217\346\230\276\347\244\272\345\214\272\345\237\237", nullptr));
        deviceGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\350\256\276\345\244\207\346\216\247\345\210\266", nullptr));
        refreshButton->setText(QCoreApplication::translate("MainWindow", "\345\210\267\346\226\260\350\256\276\345\244\207", nullptr));
        connectButton->setText(QCoreApplication::translate("MainWindow", "\350\277\236\346\216\245\347\233\270\346\234\272", nullptr));
        disconnectButton->setText(QCoreApplication::translate("MainWindow", "\346\226\255\345\274\200\350\277\236\346\216\245", nullptr));
        captureGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\345\233\276\345\203\217\351\207\207\351\233\206", nullptr));
        startButton->setText(QCoreApplication::translate("MainWindow", "\345\274\200\345\247\213\351\207\207\351\233\206", nullptr));
        stopButton->setText(QCoreApplication::translate("MainWindow", "\345\201\234\346\255\242\351\207\207\351\233\206", nullptr));
        snapButton->setText(QCoreApplication::translate("MainWindow", "\345\215\225\345\270\247\351\207\207\351\233\206", nullptr));
        saveButton->setText(QCoreApplication::translate("MainWindow", "\344\277\235\345\255\230\345\233\276\345\203\217", nullptr));
        statusGroupBox->setTitle(QCoreApplication::translate("MainWindow", "\347\212\266\346\200\201\344\277\241\346\201\257", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
