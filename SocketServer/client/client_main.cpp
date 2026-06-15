#include <QCoreApplication>
#include <QTimer>
#include <QDebug>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QThread>

#include "ZMQClient.h"
#include "../common/MessageHandler.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    
    app.setApplicationName("Socket Eraser Demo Client");
    app.setApplicationVersion("1.0");
    app.setOrganizationName("Socket Eraser");
    
    QCommandLineParser parser;
    parser.setApplicationDescription("Socket Eraser Demo Client");
    parser.addHelpOption();
    parser.addVersionOption();
    
    QCommandLineOption clientIdOption(QStringList() << "i" << "id",
                                   "客户端ID", "clientId");
    parser.addOption(clientIdOption);
    
    QCommandLineOption serverOption(QStringList() << "s" << "server",
                                 "服务器地址", "server", "tcp://localhost:5555");
    parser.addOption(serverOption);
    
    QCommandLineOption countOption(QStringList() << "c" << "count",
                               "创建客户端数量", "count", "1");
    parser.addOption(countOption);
    
    QCommandLineOption delayOption(QStringList() << "d" << "delay",
                               "启动延迟（秒）", "delay", "0");
    parser.addOption(delayOption);
    
    parser.process(app);
    
    QString clientId = parser.value(clientIdOption);
    QString serverAddress = parser.value(serverOption);
    int clientCount = parser.value(countOption).toInt();
    int startDelay = parser.value(delayOption).toInt();
    
    qDebug() << "=== Socket Eraser Demo Client ===";
    qDebug() << "服务器地址:" << serverAddress;
    qDebug() << "客户端数量:" << clientCount;
    qDebug() << "启动延迟:" << startDelay << "秒";
    
    if (clientCount > 1 && clientId.isEmpty()) {
        qDebug() << "注意: 批量模式下将自动生成客户端ID";
    }
    
    QList<ZMQClient*> clients;
    QList<MessageHandler*> handlers;
    
    if (startDelay > 0) {
        qDebug() << QString("等待 %1 秒后启动客户端...").arg(startDelay);
        QThread::sleep(startDelay);
    }
    
    for (int i = 1; i <= clientCount; ++i) {
        QString actualClientId;
        if (clientCount == 1 && !clientId.isEmpty()) {
            actualClientId = clientId;
        } else {
            actualClientId = QString("DEMO_CLIENT_%1").arg(i, 3, 10, QChar('0'));
        }
        
        ZMQClient* client = new ZMQClient(actualClientId, serverAddress);
        MessageHandler* handler = new MessageHandler(client);
        handlers.append(handler);
        
        QObject::connect(client, &ZMQClient::connected, [actualClientId]() {
            qDebug() << "客户端已连接:" << actualClientId;
        });
        
        QObject::connect(client, &ZMQClient::disconnected, [actualClientId]() {
            qDebug() << "客户端已断开:" << actualClientId;
        });
        
        QObject::connect(client, &ZMQClient::errorOccurred, [](const QString& error) {
            qDebug() << "客户端错误:" << error;
        });
        
        QObject::connect(client, &ZMQClient::messageSent, [actualClientId](const QString& type) {
            qDebug() << QString("[%1] 消息已发送: %2").arg(actualClientId, type);
        });
        
        clients.append(client);
        
        if (clientCount > 1) {
            QTimer::singleShot(i * 1000, [client, handler, actualClientId]() {
                if (client->connectToServer()) {
                    socket_eraser::ClientInfo clientInfo = handler->generateDemoClientInfo(actualClientId);
                    client->sendClientRegister(clientInfo);
                    client->startHeartbeat(10000);
                    
                    QTimer::singleShot(2000, [client, handler, actualClientId]() {
                        QList<socket_eraser::DiskInfo> disks;
                        for (int j = 1; j <= 3; ++j) {
                            QString diskId = QString("DISK_%1_%2").arg(actualClientId).arg(j);
                            disks.append(handler->generateDemoDiskInfo(diskId));
                        }
                        
                        socket_eraser::ClientInfo clientInfo = handler->generateDemoClientInfo(actualClientId);
                        client->sendDiskInfo(clientInfo, disks);
                    });
                }
            });
        } else {
            if (client->connectToServer()) {
                socket_eraser::ClientInfo clientInfo = handler->generateDemoClientInfo(actualClientId);
                client->sendClientRegister(clientInfo);
                client->startHeartbeat(10000);
                
                QTimer::singleShot(2000, [client, handler, actualClientId]() {
                    QList<socket_eraser::DiskInfo> disks;
                    for (int j = 1; j <= 3; ++j) {
                        QString diskId = QString("DISK_%1_%2").arg(actualClientId).arg(j);
                        disks.append(handler->generateDemoDiskInfo(diskId));
                    }
                    
                    socket_eraser::ClientInfo clientInfo = handler->generateDemoClientInfo(actualClientId);
                    client->sendDiskInfo(clientInfo, disks);
                });
            }
        }
    }
    
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        qDebug() << "正在停止所有客户端...";
        for (int i = 0; i < clients.size(); ++i) {
            clients[i]->disconnectFromServer();
            clients[i]->deleteLater();
        }
    });
    
    return app.exec();
}
