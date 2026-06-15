#include "ClientManager.h"
#include "Utils.h"
#include "../comLib/SqlDatabase/checkresultdb.h"
#include <QDebug>

ClientManager::ClientManager(QObject *parent)
    : QObject(parent)
{
}

// ==================== 客户端生命周期管理 ====================

QString ClientManager::registerClient(const ClientInfo& clientInfo)
{
    // 1. 生成唯一的 server_client_id
    QString serverClientId = Utils::generateServerClientId();
    
    // 2. 构建客户端包装对象
    ClientInfoWrap clientWrap;
    clientWrap.clientInfo = clientInfo;
    clientWrap.clientInfo.set_server_client_id(serverClientId.toStdString());
    clientWrap.connectTime = QDateTime::currentDateTime();
    clientWrap.lastHeartbeat = QDateTime::currentDateTime();
    clientWrap.isOnline = true;
    clientWrap.disconnectReason.clear();
    
    // 3. 保存到缓存
    {
        QMutexLocker locker(&m_mutex);
        m_clients[serverClientId] = clientWrap;
    }
    
    qDebug() << "客户端注册成功:" << serverClientId 
             << "主机名:" << QString::fromStdString(clientInfo.hostname());
    
    // 4. 保存到数据库（带异常处理）
    try {
        if (CheckResultDB::GetInstance().InsertClientInfo(clientInfo)) {
            qDebug() << "客户端信息已保存到数据库:" << serverClientId;
        } else {
            qWarning() << "保存客户端信息到数据库失败:" << serverClientId;
            // 注意：即使数据库保存失败，仍然继续注册流程，保证可用性
        }
    } catch (const std::exception& e) {
        qWarning() << "数据库操作异常:" << e.what() << "，客户端:" << serverClientId;
    }
    
    // 5. 发送信号
    emit clientRegistered(serverClientId, clientWrap.clientInfo);
    
    return serverClientId;
}

void ClientManager::unregisterClient(const QString& serverClientId, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_clients.contains(serverClientId)) {
        ClientInfoWrap& clientData = m_clients[serverClientId];
        clientData.isOnline = false;
        clientData.disconnectTime = QDateTime::currentDateTime();
        clientData.disconnectReason = reason;
        
        qDebug() << "客户端注销:" << serverClientId << "原因:" << reason;
        
        // 发送信号（在锁外）
        locker.unlock();
        emit clientUnregistered(serverClientId, reason);
        emit clientStatusChanged(serverClientId, false, reason);
    } else {
        qWarning() << "尝试注销不存在的客户端:" << serverClientId;
    }
}

// ==================== 客户端信息查询 ====================

bool ClientManager::hasClient(const QString& serverClientId)
{
    QMutexLocker locker(&m_mutex);
    return m_clients.contains(serverClientId);
}

ClientInfoWrap ClientManager::getClientData(const QString& serverClientId)
{
    QMutexLocker locker(&m_mutex);
    return m_clients.value(serverClientId, ClientInfoWrap());
}
QList<ClientInfoWrap>  ClientManager::getAllClients()
{
    QMutexLocker locker(&m_mutex);
    return m_clients.values();
}
QStringList ClientManager::getAllClientIds()
{
    QMutexLocker locker(&m_mutex);
    return m_clients.keys();
}

QStringList ClientManager::getOnlineClientIds()
{
    QMutexLocker locker(&m_mutex);
    QStringList onlineIds;
    for (const auto& clientData : m_clients.values()) {
        if (clientData.isOnline) {
            onlineIds.append(QString::fromStdString(clientData.clientInfo.server_client_id()));
        }
    }
    return onlineIds;
}

QStringList ClientManager::getOfflineClientIds()
{
    QMutexLocker locker(&m_mutex);
    QStringList offlineIds;
    for (const auto& clientData : m_clients.values()) {
        if (!clientData.isOnline) {
            offlineIds.append(QString::fromStdString(clientData.clientInfo.server_client_id()));
        }
    }
    return offlineIds;
}

int ClientManager::getClientCount()
{
    QMutexLocker locker(&m_mutex);
    return m_clients.size();
}

int ClientManager::getOnlineClientCount()
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& clientData : m_clients.values()) {
        if (clientData.isOnline) {
            count++;
        }
    }
    return count;
}

int ClientManager::getOfflineClientCount()
{
    QMutexLocker locker(&m_mutex);
    int count = 0;
    for (const auto& clientData : m_clients.values()) {
        if (!clientData.isOnline) {
            count++;
        }
    }
    return count;
}

// ==================== 客户端状态管理 ====================

void ClientManager::updateHeartbeat(const QString& serverClientId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_clients.contains(serverClientId)) {
        m_clients[serverClientId].lastHeartbeat = QDateTime::currentDateTime();
        
        // 如果之前是离线状态，标记为在线
        if (!m_clients[serverClientId].isOnline) {
            m_clients[serverClientId].isOnline = true;
            m_clients[serverClientId].disconnectTime = QDateTime();
            m_clients[serverClientId].disconnectReason.clear();
            
            locker.unlock();
            emit clientStatusChanged(serverClientId, true, "重新连接");
        }
        
        locker.unlock();
        emit clientHeartbeatUpdated(serverClientId);
    } else {
        qWarning() << "收到未知客户端的心跳:" << serverClientId;
    }
}

void ClientManager::markClientOffline(const QString& serverClientId, const QString& reason)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_clients.contains(serverClientId)) {
        m_clients[serverClientId].isOnline = false;
        m_clients[serverClientId].disconnectTime = QDateTime::currentDateTime();
        m_clients[serverClientId].disconnectReason = reason;
        
        locker.unlock();
        emit clientStatusChanged(serverClientId, false, reason);
        qDebug() << "客户端标记为离线:" << serverClientId << "原因:" << reason;
    }
}

void ClientManager::markClientOnline(const QString& serverClientId)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_clients.contains(serverClientId)) {
        m_clients[serverClientId].isOnline = true;
        m_clients[serverClientId].lastHeartbeat = QDateTime::currentDateTime();
        m_clients[serverClientId].disconnectTime = QDateTime();
        m_clients[serverClientId].disconnectReason.clear();
        
        locker.unlock();
        emit clientStatusChanged(serverClientId, true, "上线");
    }
}
// ==================== 磁盘信息管理 ====================

void ClientManager::updateClientDisks(ClientInfo clientInfo, const std::vector<DiskInfo>& diskList)
{
    QString serverClientId = QString::fromStdString(clientInfo.server_client_id());

    // 转换磁盘信息列表
    QList<ClientDiskInfo> newDisks;
    for (const DiskInfo& disk : diskList) {
        ClientDiskInfo clientDisk;
		clientDisk.info = disk;
        newDisks.append(clientDisk); // 占位，实际应填充数据
    }

    {
        QMutexLocker locker(&m_mutex);

        if (m_clients.contains(serverClientId)) {

            m_clients[serverClientId].diskList = newDisks;
        } else {
            qWarning() << "尝试更新未知客户端的磁盘信息:" << serverClientId;
        }
    } // 锁在此处释放

    // 2. 保存到数据库
    try {
        if (CheckResultDB::GetInstance().BatchInsertDiskInfos(diskList)) {
            qDebug() << "成功保存" << diskList.size() << "条磁盘信息到数据库";
        } else {
            qWarning() << "保存磁盘信息到数据库失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "数据库操作异常:" << e.what();
    }

    // 3. 发送信号
    emit clientDisksUpdated(serverClientId, static_cast<int>(diskList.size()));
    qDebug() << "已更新客户端" << serverClientId << "的磁盘信息，数量:" << diskList.size();
}
QList<ClientDiskInfo> ClientManager::getClientDisks(const QString& serverClientId)
{
    QMutexLocker locker(&m_mutex);
    if (m_clients.contains(serverClientId)) {
        return m_clients[serverClientId].diskList;
    }
    return QList<ClientDiskInfo>();
}

// ==================== 心跳超时检查 ====================

QStringList ClientManager::checkHeartbeatTimeout(int timeoutMs)
{
    QMutexLocker locker(&m_mutex);
    QStringList timeoutClients;
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        qint64 lastHeartbeatMs = it.value().lastHeartbeat.toMSecsSinceEpoch();
        if (currentTime - lastHeartbeatMs > timeoutMs) {
            timeoutClients.append(it.key());
        }
    }
    
    return timeoutClients;
}

// ==================== 数据清理 ====================

void ClientManager::cleanupOldRecords(int daysToKeep)
{
    QMutexLocker locker(&m_mutex);
    
    QDateTime cutoffDate = QDateTime::currentDateTime().addDays(-daysToKeep);
    QStringList toRemove;
    
    for (auto it = m_clients.begin(); it != m_clients.end(); ++it) {
        const ClientInfoWrap& clientData = it.value();
        if (!clientData.isOnline && clientData.disconnectTime.isValid() 
            && clientData.disconnectTime < cutoffDate) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& clientId : toRemove) {
        m_clients.remove(clientId);
        locker.unlock();
        emit clientUnregistered(clientId, "记录过期清理");
        qDebug() << "清理过期客户端记录:" << clientId;
        locker.relock();
    }
    
    if (!toRemove.isEmpty()) {
        qDebug() << "已清理" << toRemove.size() << "条过期客户端记录";
    }
}

void ClientManager::clearAll()
{
    QMutexLocker locker(&m_mutex);
    QStringList clientIds = m_clients.keys();
    m_clients.clear();
    
    locker.unlock();
    
    // 通知所有客户端已移除
    for (const QString& clientId : clientIds) {
        emit clientUnregistered(clientId, "服务器关闭");
    }
    
    qDebug() << "已清空所有客户端数据，数量:" << clientIds.size();
}
