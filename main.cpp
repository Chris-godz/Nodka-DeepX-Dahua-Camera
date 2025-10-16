#include <QtWidgets/QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include "MainWindow.h"

// 自定义消息处理器，将 qDebug 输出重定向到文件和控制台
void customMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    static QFile logFile(QApplication::applicationDirPath() + "/debug.log");
    static bool isOpen = logFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append);
    
    QString txt;
    switch (type) {
    case QtDebugMsg:
        txt = QString("[DEBUG] %1").arg(msg);
        break;
    case QtInfoMsg:
        txt = QString("[INFO] %1").arg(msg);
        break;
    case QtWarningMsg:
        txt = QString("[WARNING] %1").arg(msg);
        break;
    case QtCriticalMsg:
        txt = QString("[CRITICAL] %1").arg(msg);
        break;
    case QtFatalMsg:
        txt = QString("[FATAL] %1").arg(msg);
        break;
    }
    
    QString logMsg = QString("%1 %2\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz")).arg(txt);
    
    if (isOpen) {
        QTextStream stream(&logFile);
        stream << logMsg;
        logFile.flush();
    }
    
    // 同时输出到标准错误
    fprintf(stderr, "%s", logMsg.toLocal8Bit().constData());
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    
    // 安装自定义消息处理器
    qInstallMessageHandler(customMessageHandler);
    
    qDebug() << "========== 应用程序启动 ==========";
    qDebug() << "启动时间:" << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    
    // 设置应用程序信息
    app.setApplicationName("简单相机控制器");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("KISS Camera App");           // 设置工作目录为可执行文件所在目录
    
    qDebug() << "应用程序目录:" << QApplication::applicationDirPath();
    qDebug() << "当前工作目录:" << QDir::currentPath();
    
    QDir::setCurrent(QApplication::applicationDirPath());
    qDebug() << "设置工作目录后:" << QDir::currentPath();
    
    qDebug() << "@@@@@@@@@@@@@@@ 准备创建 MainWindow @@@@@@@@@@@@@@@";
    // 创建并显示主窗口
    MainWindow window;
    qDebug() << "@@@@@@@@@@@@@@@ MainWindow 创建完成 @@@@@@@@@@@@@@@";
    window.show();
    qDebug() << "@@@@@@@@@@@@@@@ MainWindow.show() 完成 @@@@@@@@@@@@@@@";
    return app.exec();
}