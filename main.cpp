#include <QtWidgets/QApplication>
#include <QStyleFactory>
#include <QDir>
#include "MainWindow.h"
int main(int argc, char *argv[])
{
    QApplication app(argc, argv); // 设置应用程序信息
    app.setApplicationName("简单相机控制器");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("KISS Camera App");           // 设置工作目录为可执行文件所在目录
    QDir::setCurrent(QApplication::applicationDirPath()); // 创建并显示主窗口
    MainWindow window;
    window.show();
    return app.exec();
}