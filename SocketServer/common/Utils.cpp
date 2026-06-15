#include "Utils.h"
#include <QHostAddress>
#include <QNetworkInterface>
#include <QtGlobal>
#include <QUuid>
#include <cstdlib>
#include <QCryptographicHash>
Utils::Utils()
{

}
// 辅助函数：生成随机数 (兼容 Qt 5.6，没有 QRandomGenerator)
int Utils::generateRandomInt(int min, int max) {
    if (min > max) qSwap(min, max);
    return min + (qrand() % (max - min + 1));
}

qint64 Utils::generateRandomInt64(qint64 min, qint64 max) {
    if (min > max) qSwap(min, max);
    double range = static_cast<double>(max - min);
    double randomVal = static_cast<double>(qrand()) / static_cast<double>(RAND_MAX);
    return min + static_cast<qint64>(randomVal * range);
}

// 服务端生成唯一ID（时间戳+随机数+uuid），兼容 qt5.6 不使用 QRandomGenerator
QString Utils::generateServerClientId() 
{
    qint64 ms = QDateTime::currentMSecsSinceEpoch();
    // 取32位伪随机数 (用qrand()两次拼接，避免只有低16bit)
    quint32 randPart = (static_cast<quint32>(qrand()) << 16) | static_cast<quint32>(qrand() & 0xFFFF);
    QString uuidPart = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString serverId = QString("%1%2%3").arg(ms).arg(randPart, 8, 16, QLatin1Char('0')).arg(uuidPart);
    // 对生成的 ID 进行 MD5 哈希，并取前 16 位作为最终的唯一标识
    QByteArray hash = QCryptographicHash::hash(serverId.toUtf8(), QCryptographicHash::Md5);
    QString finalId = hash.toHex().left(16);
    return finalId;
}

QString Utils::formatBytes(qint64 bytes)
{
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    const qint64 TB = GB * 1024;

    if (bytes >= TB) {
        return QString("%1 TB").arg(static_cast<double>(bytes) / TB, 0, 'f', 2);
    } else if (bytes >= GB) {
        return QString("%1 GB").arg(static_cast<double>(bytes) / GB, 0, 'f', 2);
    } else if (bytes >= MB) {
        return QString("%1 MB").arg(static_cast<double>(bytes) / MB, 0, 'f', 2);
    } else if (bytes >= KB) {
        return QString("%1 KB").arg(static_cast<double>(bytes) / KB, 0, 'f', 2);
    } else {
        return QString("%1 B").arg(bytes);
    }
}


QString Utils::formatDuration(const QDateTime& start, const QDateTime& end)
{
    if (!start.isValid()) return "-";

    qint64 seconds = start.secsTo(end.isValid() ? end : QDateTime::currentDateTime());
    if (seconds < 0) seconds = 0;

    int days = seconds / 86400;
    int hours = (seconds % 86400) / 3600;
    int minutes = (seconds % 3600) / 60;
    int secs = seconds % 60;

    if (days > 0) {
        return QString("%1天 %2:%3:%4").arg(days).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else if (hours > 0) {
        return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(secs, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2").arg(minutes).arg(secs, 2, 10, QChar('0'));
    }
}
QString Utils::getHealthStatusText(double health)
{
    if (health >= 90) return "优秀";
    if (health >= 70) return "良好";
    if (health >= 50) return "一般";
    if (health >= 30) return "较差";
    return "危险";
}

QColor Utils::getHealthStatusColor(double health)
{
    if (health >= 90) return Qt::darkGreen;
    if (health >= 70) return Qt::green;
    if (health >= 50) return Qt::yellow;
    if (health >= 30) return Qt::darkYellow;
    return Qt::red;
}
QString Utils::getStatusString(TaskStatus status)
{
    switch (status) {
    case TaskStatus::UNKNOWN:
        return ("未知");
    case TaskStatus::PENDING:
        return ("等待中");
    case TaskStatus::STARTED:
        return ("已开始");
    case TaskStatus::IN_PROGRESS:
        return ("进行中");
    case TaskStatus::COMPLETED:
        return ("已完成");
    case TaskStatus::FAILED:
        return ("失败");
    case TaskStatus::PAUSED:
        return ("已暂停");
    case TaskStatus::STOPPED:
        return ("已停止");
    default:
        return ("未知");
    }
}

QColor Utils::getTaskStatusColor(TaskStatus status)
{
    switch (status) {
    case TaskStatus::UNKNOWN:
        return Qt::black;
    case TaskStatus::PENDING:
        return Qt::gray;
    case TaskStatus::STARTED:
    case TaskStatus::PAUSED:
        return Qt::darkYellow;
    case TaskStatus::IN_PROGRESS:
        return Qt::blue;
    case TaskStatus::COMPLETED:
        return Qt::green;
    case TaskStatus::FAILED:
    case TaskStatus::STOPPED:
        return Qt::red;
    default:
        return Qt::black;
    }
}
QString Utils::getLocalIPString() {

    // 获取本机 IPv4 地址
    QString ipAddress = "127.0.0.1";
    QList<QHostAddress> hostAddresses = QNetworkInterface::allAddresses();
    for (int i = 0; i < hostAddresses.size(); ++i) {
        const QHostAddress &address = hostAddresses[i];
        if (address.protocol() == QAbstractSocket::IPv4Protocol &&
                address != QHostAddress::LocalHost) {
            ipAddress = address.toString();
            break;
        }
    }
    return ipAddress;
}

QByteArray Utils::serializeTask(const ErasureTask& task)
{
    QByteArray result;
    try {
        std::string serialized;
        if (task.SerializeToString(&serialized)) {
            result = QByteArray::fromStdString(serialized);
        } else {
            qWarning() << "任务对象序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "任务对象序列化异常:" << e.what();
    }
    return result;
}

ErasureTask Utils::deserializeTask(const QByteArray& taskData)
{
    ErasureTask task;
    if (taskData.isEmpty()) {
        return task;
    }
    
    try {
        std::string data = taskData.toStdString();
        if (!task.ParseFromString(data)) {
            qWarning() << "任务对象反序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "任务对象反序列化异常:" << e.what();
    }
    return task;
}

ErasureProgress Utils::deserializeProgress(const QByteArray& progressData)
{
    ErasureProgress progress;
    if (progressData.isEmpty()) {
        return progress;
    }
    
    try {
        std::string data = progressData.toStdString();
        if (!progress.ParseFromString(data)) {
            qWarning() << "进度对象反序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "进度对象反序列化异常:" << e.what();
    }
    return progress;
}

QByteArray Utils::serializeProgress(const ErasureProgress& progress)
{
    QByteArray result;
    try {
        std::string serialized;
        if (progress.SerializeToString(&serialized)) {
            result = QByteArray::fromStdString(serialized);
        } else {
            qWarning() << "进度对象序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "进度对象序列化异常:" << e.what();
    }
    return result;
}

Message Utils::deserializeMessage(const QByteArray& msgData)
{
    Message msg;
    if (msgData.isEmpty()) {
        return msg;
    }
    
    try {
        std::string data = msgData.toStdString();
        if (!msg.ParseFromString(data)) {
            qWarning() << "消息对象反序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "消息对象反序列化异常:" << e.what();
    }
    return msg;
}

QByteArray Utils::serializeMessage(const Message& msg)
{
    QByteArray result;
    try {
        std::string serialized;
        if (msg.SerializeToString(&serialized)) {
            result = QByteArray::fromStdString(serialized);
        } else {
            qWarning() << "消息对象序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "消息对象序列化异常:" << e.what();
    }
    return result;
}

// 序列化服务器响应
QByteArray Utils::serializeResponse(const ServerResponse& response)
{
    QByteArray result;
    try {
        std::string serialized;
        if (response.SerializeToString(&serialized)) {
            result = QByteArray::fromStdString(serialized);
        } else {
            qWarning() << "服务器响应对象序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "服务器响应对象序列化异常:" << e.what();
    }
    return result;
}

// 反序列化服务器响应
ServerResponse Utils::deserializeResponse(const QByteArray& data)
{
    ServerResponse response;
    if (data.isEmpty()) {
        return response;
    }
    
    try {
        std::string strData = data.toStdString();
        if (!response.ParseFromString(strData)) {
            qWarning() << "服务器响应对象反序列化失败";
        }
    } catch (const std::exception& e) {
        qWarning() << "服务器响应对象反序列化异常:" << e.what();
    }
    return response;
}
