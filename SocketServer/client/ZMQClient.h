#ifndef ZMQCLIENT_H
#define ZMQCLIENT_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QTimer>
#include <zmq.h>
#include "MessageHandler.h"
#include "messages.pb.h"

class ZMQClient : public QObject
{
    Q_OBJECT

public:
    explicit ZMQClient(const QString& clientId, const QString& serverAddress = "tcp://localhost:5555", QObject *parent = nullptr);
    ~ZMQClient();
    
    bool connectToServer();
    void disconnectFromServer();
    bool isConnected() const;
    void setServerAddress(QString serverAddress);
    bool sendClientRegister(const ClientInfo& clientInfo);
    bool sendDiskInfo(const ClientInfo& clientInfo, const QList<DiskInfo>& diskList);
    bool sendErasureTask(const ClientInfo& clientInfo, const ErasureTask& taskInfo);
    bool sendErasureProgress(const ClientInfo& clientInfo, const ErasureProgress& progressInfo);
    bool sendErasureComplete(const ClientInfo& clientInfo, const ErasureTask& erasureInfo);
    bool sendHeartbeat(const ClientInfo& clientInfo);
    bool sendClientDisconnect(const ClientInfo& clientInfo);
    
    void startHeartbeat(int intervalMs = 10000);
    void stopHeartbeat();

signals:
    void connected();
    void disconnected();
    bool resultOfClientRegister(const QString& server_clinent_id);

    void errorOccurred(const QString& error);
    void serverResponseReceived(const ServerResponse& response);
    void messageSent(const QString& messageType);
    void clientStarted();
    void clientStopped();

private slots:
    void sendHeartbeatMessage();

private:
    bool initializeZMQ();
    void cleanupZMQ();
    bool sendMessage(const Message& message);
    bool receiveResponse(ServerResponse& response);
    void logMessage(const QString& message);

private:
    QString m_clientId;
    QString m_server_clientId;

    int64_t generateRequestId();

    QString m_serverAddress;
    void* m_context;
    void* m_socket;
    bool m_connected;
    
    MessageHandler* m_messageHandler;
    QTimer* m_heartbeatTimer;
    int64_t m_currentRequestId;

    static const int SOCKET_TIMEOUT = 5000;
};

#endif // ZMQCLIENT_H
