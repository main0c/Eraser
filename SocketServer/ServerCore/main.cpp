#include <QCoreApplication>
#include <QTextStream>
#include <QDebug>
#include "ServerCoreApplication.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("ServerCore");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Eraser");

    qDebug() << "========================================";
    qDebug() << "ServerCore v1.0 Starting...";
    qDebug() << "========================================";

    ServerCoreApplication serverCore;
    
    if (!serverCore.initialize()) {
        qCritical() << "Failed to initialize ServerCore";
        return -1;
    }

    serverCore.start();

    qDebug() << "ServerCore is running. Press Ctrl+C to stop.";
    qDebug() << "========================================";

    return app.exec();
}