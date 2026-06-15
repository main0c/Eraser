#include <QDebug>
#include <QUuid>
#include <QDateTime>
#include <QTime> // Qt 5.6 使用 QTime::currentTime().msecsSinceStartOfDay() 或 qsrand/qrand
#include "Utils.h"
#include "MessageHandler.h"

// 辅助函数：生成随机数 (兼容 Qt 5.6，因为 QRandomGenerator 是 Qt 5.10+ 引入的)
static int generateRandomInt(int min, int max) {
    if (min > max) qSwap(min, max);
    return min + (qrand() % (max - min + 1));
}

static qint64 generateRandomInt64(qint64 min, qint64 max) {
    if (min > max) qSwap(min, max);
    double range = static_cast<double>(max - min);
    double randomVal = static_cast<double>(qrand()) / RAND_MAX;
    return min + static_cast<qint64>(randomVal * range);
}

// 构造函数
MessageHandler::MessageHandler(QObject *parent)
    : QObject(parent)
{
    // 在 Qt 5.6 中，通常需要在应用启动时调用 qsrand，这里确保种子已设置（如果外部未设置）
    // 注意：多次调用 qsrand 可能会降低随机性，通常只在 main 中调用一次。
    // static bool seeded = false;
    // if (!seeded) {
    //     qsrand(QTime::currentTime().msecsSinceStartOfDay());
    //     seeded = true;
    // }
}


// 创建客户端注册消息
Message MessageHandler::createClientRegisterMessage(const ClientInfo& clientInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_CLIENT_REGISTER);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    logMessage(QString("创建客户端注册消息: %1").arg(QString::fromStdString(clientInfo.client_id())));
    return message;
}

// 创建磁盘信息消息
Message MessageHandler::createDiskInfoMessage(const QList<DiskInfo>& diskList)
{
    Message message;
    message.set_type(MESSAGE_TYPE_DISK_INFO);
    
    for (const auto& disk : diskList) {
        DiskInfo* diskInfo = message.add_disk_list();
        diskInfo->CopyFrom(disk);
    }

    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    logMessage(QString("创建磁盘信息消息: 磁盘数量: %1").arg(diskList.size()));
    return message;
}

// 创建擦除任务消息（用于任务创建、暂停、停止等状态变更）
Message MessageHandler::createErasureTaskMessage(const ClientInfo& clientInfo,
                                                                 const ErasureTask& taskInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_ERASURE_START);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.mutable_task_info()->CopyFrom(taskInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    logMessage(QString("创建擦除任务消息: %1, 磁盘: %2, 状态: %3")
               .arg(QString::fromStdString(clientInfo.client_id()))
               .arg(QString::fromStdString(taskInfo.disk_id()))
               .arg(Utils::getStatusString(taskInfo.status())));
    return message;
}

// 创建擦除进度消息（用于实时进度更新）
Message MessageHandler::createErasureProgressMessage(const ClientInfo& clientInfo,
                                                                    const ErasureProgress& progressInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_ERASURE_PROGRESS);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.mutable_progress_info()->CopyFrom(progressInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());
    logMessage(QString("创建擦除进度消息: %1, 磁盘: %2, 进度: %3%")
               .arg(QString::fromStdString(clientInfo.client_id()))
               .arg(QString::fromStdString(progressInfo.disk_id()))
               .arg(progressInfo.progress_percent() / 100.0));
    return message;
}

// 创建擦除完成消息
Message MessageHandler::createErasureCompleteMessage(const ClientInfo& clientInfo,
                                                                    const ErasureTask& erasureInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_ERASURE_COMPLETE);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.mutable_task_info()->CopyFrom(erasureInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    logMessage(QString("创建擦除完成消息: %1, 磁盘: %2, 状态: %3")
               .arg(QString::fromStdString(clientInfo.client_id()))
               .arg(QString::fromStdString(erasureInfo.disk_id()))
               .arg(Utils::getStatusString(erasureInfo.status())));
    return message;
}

// 创建心跳消息
Message MessageHandler::createHeartbeatMessage(const ClientInfo& clientInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_HEARTBEAT);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    logMessage(QString("创建心跳消息: %1").arg(QString::fromStdString(clientInfo.client_id())));
    return message;
}

// 创建客户端断开连接消息
Message MessageHandler::createClientDisconnectMessage(const ClientInfo& clientInfo)
{
    Message message;
    message.set_type(MESSAGE_TYPE_CLIENT_DISCONNECT);
    message.mutable_client_info()->CopyFrom(clientInfo);
    message.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    logMessage(QString("创建客户端断开消息: %1").arg(QString::fromStdString(clientInfo.client_id())));
    return message;
}


// 生成演示用 ClientInfo
ClientInfo MessageHandler::generateDemoClientInfo(const QString& clientId)
{
    ClientInfo clientInfo;
    std::string cid = clientId.toStdString();
    clientInfo.set_client_id(cid);
    std::string hostname = QString("DEMO-PC-%1").arg(clientId.left(8)).toStdString();
    clientInfo.set_hostname(hostname);
    std::string ip_address = QString("192.168.2.%1").arg(generateRandomInt(100, 255)).toStdString();
    clientInfo.set_ip_address(ip_address);
    clientInfo.set_os_type("Windows");
    clientInfo.set_os_version("Windows 10 Pro");
    clientInfo.set_timestamp(QDateTime::currentMSecsSinceEpoch());

    return clientInfo;
}

// 生成演示用 DiskInfo
DiskInfo MessageHandler::generateDemoDiskInfo(const QString& diskId)
{
    DiskInfo diskInfo;
    diskInfo.set_disk_id(diskId.toStdString());

    QStringList models = {"Samsung SSD 970 EVO", "Western Digital WD10EZEX", "Seagate Barracuda", "Crucial MX500"};
    QString model = models[generateRandomInt(0, models.size() - 1)];
    diskInfo.set_model(model.toStdString());

    diskInfo.set_serial_number(QString("SN%1%2").arg(diskId).arg(generateRandomInt(1000, 9999)).toStdString());

    QStringList interfaces = {"SATA", "NVMe", "USB", "SAS"};
    diskInfo.set_interface_type(interfaces[generateRandomInt(0, interfaces.size() - 1)].toStdString());

    // 生成随机容量 (100GB - 4TB)
    qint64 totalSize = static_cast<qint64>(generateRandomInt(100, 4096)) * 1024LL * 1024 * 1024;
    diskInfo.set_total_size(totalSize);

    // 生成随机使用量 (30% - 80%)
    double usageRatio = 0.3 + (generateRandomInt(0, 50) / 100.0);
    qint64 usedSize = static_cast<qint64>(totalSize * usageRatio);
    diskInfo.set_used_size(usedSize);
    diskInfo.set_free_size(totalSize - usedSize);

    QStringList fileSystems = {"NTFS", "exFAT", "ext4", "APFS"};
    diskInfo.set_file_system(fileSystems[generateRandomInt(0, fileSystems.size() - 1)].toStdString());

    diskInfo.set_is_system_disk(generateRandomInt(0, 9) < 3); // 30%概率是系统盘
    diskInfo.set_is_removable(generateRandomInt(0, 9) < 2); // 20%概率是可移动磁盘

    QStringList mountPoints = {"C:\\", "D:\\", "E:\\", "/mnt/data", "/media/usb"};
    diskInfo.set_mount_point(mountPoints[generateRandomInt(0, mountPoints.size() - 1)].toStdString());

    // 生成健康状态 (60-100)
    diskInfo.set_health_status(60.0 + generateRandomInt(0, 40));

    return diskInfo;
}

// 生成演示用 ErasureTask
ErasureTask MessageHandler::generateDemoErasureInfo(const QString& diskId, const QString& method)
{
    ErasureTask erasureInfo;
    erasureInfo.set_disk_id(diskId.toStdString());

    QStringList methods = {"Quick", "DoD 5220.22-M", "Gutmann", "Zero Fill"};
    if (method.isEmpty()) {
        erasureInfo.set_erasure_method(methods[generateRandomInt(0, methods.size() - 1)].toStdString());
    } else {
        erasureInfo.set_erasure_method(method.toStdString());
    }

    // 根据方法设置遍数
    QString erasureMethod = QString::fromStdString(erasureInfo.erasure_method());
    if (erasureMethod == "Quick") {
        erasureInfo.set_pass_count(1);
    } else if (erasureMethod == "DoD 5220.22-M") {
        erasureInfo.set_pass_count(3);
    } else if (erasureMethod == "Gutmann") {
        erasureInfo.set_pass_count(35);
    } else {
        erasureInfo.set_pass_count(1);
    }

    // 生成随机字节数 (100GB - 2TB)
    qint64 totalBytes = static_cast<qint64>(generateRandomInt(100, 2048)) * 1024LL * 1024 * 1024;
    erasureInfo.set_total_bytes(totalBytes);

    // 生成随机进度 (0-10000)
    int32_t progress = generateRandomInt(0, 10000);
    erasureInfo.set_progress_percent(progress);
    erasureInfo.set_erased_bytes(static_cast<qint64>(totalBytes * progress / 10000.0));

    erasureInfo.set_start_time(QDateTime::currentMSecsSinceEpoch() - static_cast<qint64>(generateRandomInt(0, 3600000))); // 1小时内开始

    // 预计完成时间
     if (progress < 10000) {
         qint64 remainingBytes = totalBytes - erasureInfo.erased_bytes();
         qint64 estimatedSeconds = remainingBytes / (1024 * 1024 * 10); // 假设速度为10MB/s
         erasureInfo.set_estimated_end_time(QDateTime::currentMSecsSinceEpoch() + estimatedSeconds * 1000);
     } else {
         erasureInfo.set_estimated_end_time(QDateTime::currentMSecsSinceEpoch());
     }

    // 设置状态
    if (progress >= 10000) {
        erasureInfo.set_status(TaskStatus::COMPLETED);
    } else if (progress > 0) {
        erasureInfo.set_status(TaskStatus::IN_PROGRESS);
    } else {
        erasureInfo.set_status(TaskStatus::STARTED);
    }

    erasureInfo.set_verification_enabled(true);
    erasureInfo.set_verification_passed(progress >= 10000);

    return erasureInfo;
}

// 日志辅助
void MessageHandler::logMessage(const QString& message)
{
    qDebug() << "[MessageHandler]" << message;
    emit messageProcessed("INFO", "MessageHandler");
}
