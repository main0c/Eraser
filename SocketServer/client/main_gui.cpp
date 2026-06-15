#include <QApplication>
#include <QStyleFactory>
#include <QDir>
#include <QStandardPaths>
#include "ClientMainWindow.h"
#include <QDebug>

int main(int argc, char *argv[])
{
     qsrand(QTime::currentTime().msecsSinceStartOfDay());
    QApplication app(argc, argv);
    
    // 设置应用程序信息
    app.setApplicationName("Eraser Client");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Eraser");
    
    // 设置应用程序样式
    app.setStyle("Fusion");
    
    // 设置中文字体（如果需要）
    QFont font = app.font();
    font.setPointSize(9);
    app.setFont(font);
    
    // 创建并显示主窗口
    ClientMainWindow window;
    window.show();
    
    // 处理命令行参数（如果有）
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) {
            QString arg = QString(argv[i]);
            if (arg.startsWith("--server=")) {
                QString serverAddress = arg.mid(9); // 移除 "--server="
                // 这里可以设置默认服务器地址
                // window.setServerAddress(serverAddress);
            } else if (arg.startsWith("--id=")) {
                QString clientId = arg.mid(4); // 移除 "--id="
                // 这里可以设置客户端ID
                // window.setClientId(clientId);
            }
        }
    }
    
    qDebug() << "Eraser GUI Client 已启动";
    qDebug() << "版本: 1.0";
    qDebug() << "Qt版本:" << QT_VERSION_STR;
    qDebug() << "操作系统:" << QSysInfo::prettyProductName();
    qDebug() << "架构:" << QSysInfo::currentCpuArchitecture();
    
    return app.exec();
}
