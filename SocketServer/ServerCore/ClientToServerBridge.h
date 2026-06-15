#ifndef CLIENTTOSERVERBRIDGE_H
#define CLIENTTOSERVERBRIDGE_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <zmq.h>
#include "messages.pb.h"
#include "ClientManager.h"
#include "ErasureTaskManager.h"

using namespace socket_eraser;

class ClientToServerBridge : public QObject
{
    Q_OBJECT

public:
    explicit ClientToServerBridge(QObject *parent = nullptr);
    ~ClientToServerBridge();

    bool start(const QString& bindAddress = "tcp://*:5555");
    void stop();
    bool isRunning() const;

    void setClientManager(ClientManager* manager);
    void setTaskManager(ErasureTaskManager* manager);

signals:
    void bridgeStarted();
    void bridgeStopped();
    void bridgeError(const QString& error);

private slots:
    void processMessages();
    void checkClientHeartbeats();
    void runEventLoop();
    void onWorkerFinished();

private:
    void initializeZMQ();
    void cleanupZMQ();
    bool readAndHandleMessage();
    
    bool handleMessage(const Message& message);
    bool handleClientRegister(const Message& message);
    void handleDiskInfo(const Message& message);
    void handleErasureStart(const Message &message);
    void handleErasureProgress(const Message& message);
    void handleErasureComplete(const Message& message);
    void handleHeartbeat(const Message& message);
    void handleClientDisconnect(const Message& message);

    void sendResponse(const QString& clientId, const ServerResponse& response);

private:
    void* m_context;
    void* m_socket;
    bool m_running;
    QString m_bindAddress;
    
    ClientManager* m_clientManager;
    ErasureTaskManager* m_taskManager;
    
    QMutex m_mutex;

    QThread* m_workerThread;
    bool m_stopRequested;

    QTimer* m_messageTimer;
    QTimer* m_heartbeatTimer;

    static const int HEARTBEAT_INTERVAL = 30000;
    static const int CLIENT_TIMEOUT = 60000;
};

#endif // CLIENTTOSERVERBRIDGE_H