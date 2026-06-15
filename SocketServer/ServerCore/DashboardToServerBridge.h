#ifndef DASHBOARDTOSERVERBRIDGE_H
#define DASHBOARDTOSERVERBRIDGE_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QTimer>
#include <QMutex>
#include <QHash>
#include <zmq.h>
#include "messages.pb.h"
#include "ClientManager.h"
#include "ErasureTaskManager.h"

using namespace socket_eraser;

class DashboardToServerBridge : public QObject
{
    Q_OBJECT

public:
    explicit DashboardToServerBridge(QObject *parent = nullptr);
    ~DashboardToServerBridge();

    bool start(const QString& bindAddress = "tcp://*:5556");
    void stop();
    bool isRunning() const;

    void setClientManager(ClientManager* manager);
    void setTaskManager(ErasureTaskManager* manager);

    // 通知Dashboard的公共接口
    void notifyClientConnected(const QString& serverClientId, const ClientInfo& clientInfo);
    void notifyClientDisconnected(const QString& serverClientId, const QString& reason);
    void notifyDiskUpdated(const QString& serverClientId, const QList<ClientDiskInfo>& diskList);
    void notifyTaskCreated(const QByteArray& taskData);
    void notifyTaskProgressUpdate(const QByteArray& taskData);
    void notifyTaskStatusChanged(const QByteArray& taskData);
    void notifyTaskCompleted(const QByteArray& taskData);
    void notifyTaskProgress(const QString& clientId, const ErasureProgress& progress);

private slots:
    void processMessages();

private:
    void initializeZMQ();
    void cleanupZMQ();
    
    void handleQueryRequest(const Message& message);
    void handleQueryClients(const QueryRequest& query, const QString& requestId);
    void handleQueryClientDetail(const QueryRequest& query, const QString& requestId);
    void handleQueryDiskInfo(const QueryRequest& query, const QString& requestId);
    void handleQueryTasks(const QueryRequest& query, const QString& requestId);
    void handleQueryTaskDetail(const QueryRequest& query, const QString& requestId);
    void handleQueryStatistics(const QueryRequest& query, const QString& requestId);

    void sendResponse(const QString& dashboardId, const Message& response);
    void broadcastToAllDashboards(const Message& message);

private:
    void* m_context;
    void* m_socket;
    bool m_running;
    QString m_bindAddress;
    
    ClientManager* m_clientManager;
    ErasureTaskManager* m_taskManager;
    
    QMutex m_mutex;
    QTimer* m_messageTimer;
    
    // 跟踪已连接的Dashboard
    QHash<QString, bool> m_connectedDashboards;
};

#endif // DASHBOARDTOSERVERBRIDGE_H