#ifndef CLIENTMANAGER_H
#define CLIENTMANAGER_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QHash>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include "messages.pb.h"
#include "MessageHandler.h"

using namespace socket_eraser;



class ClientManager : public QObject
{
    Q_OBJECT

public:
    explicit ClientManager(QObject *parent = nullptr);
    
    // 客户端生命周期管理
    QString registerClient(const ClientInfo& clientInfo);  // 返回 server_client_id（含数据库保存）
    void unregisterClient(const QString& serverClientId, const QString& reason = "未知原因");
    
    // 客户端信息查询
    bool hasClient(const QString& serverClientId) ;
    ClientInfoWrap getClientData(const QString& serverClientId) ;
    QList<ClientInfoWrap> getAllClients() ;

    QStringList getAllClientIds() ;
    QStringList getOnlineClientIds() ;
    QStringList getOfflineClientIds() ;
    int getClientCount() ;
    int getOnlineClientCount() ;
    int getOfflineClientCount() ;
    
    // 客户端状态管理
    void updateHeartbeat(const QString& serverClientId);
    void markClientOffline(const QString& serverClientId, const QString& reason = "未知原因");
    void markClientOnline(const QString& serverClientId);
    
    // 磁盘信息管理
    void updateClientDisks(ClientInfo clientInfo, const std::vector<DiskInfo>& diskList);  // 含数据库保存
    QList<ClientDiskInfo> getClientDisks(const QString& serverClientId) ;
    
    // 心跳超时检查（返回超时的客户端ID列表）
    QStringList checkHeartbeatTimeout(int timeoutMs = 60000) ;
    
    // 数据清理
    void cleanupOldRecords(int daysToKeep = 7);
    void clearAll();  // 清空所有客户端数据
    
signals:
    void clientRegistered(const QString& serverClientId, const QByteArray& clientData); //ClientInfo& clientInfo
    void clientUnregistered(const QString& serverClientId, const QString& reason);
    void clientHeartbeatUpdated(const QString& serverClientId);
    void clientDisksUpdated(const QString& serverClientId, int diskCount);
    void clientStatusChanged(const QString& serverClientId, bool isOnline, const QString& reason);

private:
    QHash<QString, ClientInfoWrap> m_clients;

    QMutex m_mutex;
};

#endif // CLIENTMANAGER_H
