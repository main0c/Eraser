#ifndef UTILS_H
#define UTILS_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QString>
#include <QDateTime>
#include <QColor>
#include "messages.pb.h"
using namespace socket_eraser;
class Utils
{
public:
    Utils();

    static QString formatBytes(qint64 bytes);
    static QString formatDuration(const QDateTime& start, const QDateTime& end = QDateTime());
    static QString getHealthStatusText(double health);
    static QColor getHealthStatusColor(double health);
    static QString getStatusString(TaskStatus status);
    static QColor getTaskStatusColor(TaskStatus status);
    static QString getLocalIPString();
    
    static QByteArray serializeTask(const ErasureTask& task);
    static QByteArray serializeProgress(const ErasureProgress& progress);
    static ErasureProgress deserializeProgress(const QByteArray& progressData);  
    static ErasureTask deserializeTask(const QByteArray& taskData);
    static Message deserializeMessage(const QByteArray& msgData);
    static QByteArray serializeMessage(const Message& msg);
    static QByteArray serializeResponse(const ServerResponse& response);    
    static ServerResponse deserializeResponse(const QByteArray& response);

    static int generateRandomInt(int min, int max);
    static qint64 generateRandomInt64(qint64 min, qint64 max);

    static QString generateServerClientId();
};

#endif // UTILS_H
