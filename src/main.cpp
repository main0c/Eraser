#include <QApplication>

#include "mainwindow.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    MainWindow w;
    w.setWindowTitle(QStringLiteral("Eraser Server (Qt5.6 + ZeroMQ + Protobuf)"));
    w.show();
    return app.exec();
}
