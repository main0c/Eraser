#ifndef SERVERCOREAPPLICATION_H
#define SERVERCOREAPPLICATION_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QCoreApplication>
#include <QTimer>
#include "ClientManager.h"
#include "ErasureTaskManager.h"
#include "ClientToServerBridge.h"
#include "DashboardToServerBridge.h"

class ServerCoreApplication : public QObject
{
    Q_OBJECT

public:
    explicit ServerCoreApplication(QObject *parent = nullptr);
    ~ServerCoreApplication();

    bool initialize();
    void start();
    void stop();

private slots:
    void onClientConnected(const QString& serverClientId, const ClientInfo& clientInfo);
    void onClientDisconnected(const QString& serverClientId, const QString& reason);
    void onDiskInfoReceived(const QString& serverClientId, int diskCount);
    void onErasureProgressUpdated(const QString& clientId, const ErasureProgress& progress);
    void onErasureCompleted(const QString& clientId, const QString& diskId, bool success);
    void onTaskCreated(const QByteArray& taskData);
    void onTaskProgressUpdated(const QByteArray& taskData);
    void onTaskStatusChanged(const QByteArray& taskData);
    void onTaskCompleted(const QByteArray& taskData);

private:
    ClientManager* m_clientManager;
    ErasureTaskManager* m_taskManager;
    ClientToServerBridge* m_clientBridge;
    DashboardToServerBridge* m_dashboardBridge;
    
    QTimer* m_cleanupTimer;
    bool m_initialized;
};

#endif // SERVERCOREAPPLICATION_H