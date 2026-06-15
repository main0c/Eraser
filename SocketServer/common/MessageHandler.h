#ifndef MESSAGEHANDLER_H
#define MESSAGEHANDLER_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QByteArray>
#include <QString>
#include "messages.pb.h"
#ifndef CORE_SERVER
#include <QTreeWidgetItem>
#endif
#include <QDateTime>
using namespace socket_eraser;

// 磁盘信息及其 UI 状态封装
struct ClientDiskInfo {
    DiskInfo info;                   // 磁盘协议数据
    bool isSelected = false;         // 是否被选中

    // 擦除任务相关状态
    QString erasureMethod;           // 当前擦除方式
    int progressPercent = 0;         // 进度百分比（0-10000）
    QString status;                  // 状态描述
    qint64 startTime = 0;            // 开始时间
    qint64 estimatedEndTime = 0;     // 预计结束时间
    double speedMbPerSec = 0.0;      // 当前速度
#ifndef CORE_SERVER
    // UI 缓存指针
    QTreeWidgetItem* treeItem = nullptr; // TreeWidget 中的 item 指针
#endif
    ClientDiskInfo() = default;
    explicit ClientDiskInfo(const DiskInfo& diskInfo)
        : info(diskInfo)
    {}
};

// 客户端信息包装（含磁盘列表与管理状态）
struct ClientInfoWrap {
    ClientInfo clientInfo;                // 客户端协议数据
    QList<ClientDiskInfo> diskList;       // 所有磁盘信息

    QDateTime connectTime;                // 连接时间
    QDateTime lastHeartbeat;              // 最后心跳
    QDateTime disconnectTime;             // 断开时间
    bool isOnline = false;                // 在线状态
    QString disconnectReason;             // 断开原因

    ClientInfoWrap() = default;
    explicit ClientInfoWrap(const ClientInfo& info)
        : clientInfo(info)
    {}
};

class MessageHandler : public QObject
{
    Q_OBJECT

public:
    explicit MessageHandler(QObject *parent = nullptr);
    
    // 消息创建方法
    Message createClientRegisterMessage(const ClientInfo& clientInfo);
    Message createDiskInfoMessage(const QList<DiskInfo>& diskList);
    Message createErasureTaskMessage(const ClientInfo& clientInfo,
                                                     const ErasureTask& taskInfo);
    Message createErasureProgressMessage(const ClientInfo& clientInfo,
                                                         const ErasureProgress& progressInfo);
    Message createErasureCompleteMessage(const ClientInfo& clientInfo,
                                                         const ErasureTask& erasureInfo);
    Message createHeartbeatMessage(const ClientInfo& clientInfo);
    Message createClientDisconnectMessage(const ClientInfo& clientInfo);

    
    // Demo数据生成
    ClientInfo generateDemoClientInfo(const QString& clientId);
    DiskInfo generateDemoDiskInfo(const QString& diskId);
    ErasureTask generateDemoErasureInfo(const QString& diskId, const QString& method);
    
signals:
    void messageProcessed(const QString& messageType, const QString& clientId);
    void errorOccurred(const QString& error);

private:
    void logMessage(const QString& message);
};

#endif // MESSAGEHANDLER_H
