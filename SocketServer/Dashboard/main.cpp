#include "DashboardWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Eraser Dashboard");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Eraser");
    app.setStyle("Fusion");
    
    DashboardWindow w;
    w.show();
    
    return app.exec();
}