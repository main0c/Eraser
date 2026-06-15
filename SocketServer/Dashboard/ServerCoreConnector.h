#ifndef SERVERCORECONNECTOR_H
#define SERVERCORECONNECTOR_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QTimer>
#include <QUuid>
#include <zmq.h>
#include "messages.pb.h"
#include "MessageHandler.h"
#include <QProcess>
using namespace socket_eraser;

class ServerCoreConnector : public QObject
{
    Q_OBJECT

public:
    explicit ServerCoreConnector(QObject *parent = nullptr);
    ~ServerCoreConnector();

    bool connectToServer(const QString& serverAddress = "tcp://localhost:5556");
    void disconnectFromServer();
    bool isConnected() const;

    // 查询接口
    void queryAllClients();
    void queryClientDetail(const QString& clientId);
    void queryDiskInfo(const QString& clientId);
    void queryAllTasks();
    void queryTasksByClient(const QString& clientId);
    void queryTaskDetail(const QString& taskId);
    void queryStatistics();

    // 发送擦除命令到ServerCore（由ServerCore转发给客户端）
    void sendErasureCommand(const QString& clientId, const ErasureTask& task);

    void stopServerCore();
    bool isServerCoreRunning() const;
    bool isProcessRunningByName(const QString &processName) const;
    bool startServerCore(const QString &serverPath, const QStringList &arguments);
signals:
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    // 查询响应信号
    void clientsReceived(const QList<ClientInfo>& clients, int onlineCount, int offlineCount);
    void clientDetailReceived(const ClientInfo& client);
    void diskInfoReceived(const QString& clientId, const QList<DiskInfo>& disks);
    void tasksReceived(const QList<ErasureTask>& tasks);
    void taskDetailReceived(const ErasureTask& task);
    void statisticsReceived(int totalClients, int onlineClients, int offlineClients,
                           int totalTasks, int activeTasks, int completedTasks, int failedTasks);

    // 通知信号（ServerCore主动推送）
    void clientConnected(const QString& serverClientId, const ClientInfo& clientInfo);
    void clientDisconnected(const QString& serverClientId, const QString& reason);
    void diskUpdated(const QString& serverClientId, const QList<DiskInfo>& disks);
    void taskCreated(const ErasureTask& task);
    void taskProgressUpdated(const ErasureProgress& progress);
    void taskStatusChanged(const ErasureTask& task);
    void taskCompleted(const ErasureTask& task);

private slots:
    void processMessages();
    void checkConnection();

private:
    void initializeZMQ();
    void cleanupZMQ();
    
    void sendQuery(const QueryRequest::QueryType& queryType, const QString& filterClientId = "",
                   const QString& filterDiskId = "", const QString& filterTaskId = "");
    void handleNotification(const Message& message);
    void handleQueryResponse(const Message& message);

private:
    void* m_context;
    void* m_socket;
    bool m_connected;
    QString m_serverAddress;

    QTimer* m_messageTimer;
    QTimer* m_connectionTimer;
    QProcess *m_serverProcess;

    MessageHandler m_messageHandler;
    
    static const int RECONNECT_INTERVAL = 5000;
};

#endif // SERVERCORECONNECTOR_H