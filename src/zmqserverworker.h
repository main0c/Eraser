#pragma once

#include <QObject>
#include <QAtomicInteger>

class ZmqServerWorker : public QObject {
    Q_OBJECT
public:
    explicit ZmqServerWorker(QObject *parent = nullptr);
    ~ZmqServerWorker();

public slots:
    void start(const QString &bindAddress);
    void stop();

signals:
    void clientConnected(const QString &clientId, const QString &hostName, const QString &ip);
    void diskInfoReceived(const QString &clientId, const QString &diskSummary);
    void eraseInfoReceived(const QString &clientId, const QString &eraseSummary);
    void errorOccurred(const QString &message);

private:
    QAtomicInteger<bool> m_running;
};
