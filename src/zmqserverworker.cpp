#include "zmqserverworker.h"

#include <QString>
#include <QStringList>
#include <QThread>
#include <exception>

#include <zmq.hpp>

#include "eraser.pb.h"

ZmqServerWorker::ZmqServerWorker(QObject *parent)
    : QObject(parent), m_running(false) {}

ZmqServerWorker::~ZmqServerWorker() {
    stop();
}

void ZmqServerWorker::start(const QString &bindAddress) {
    m_running.store(true);

    try {
        zmq::context_t context(1);
        zmq::socket_t pullSocket(context, ZMQ_PULL);
        pullSocket.bind(bindAddress.toStdString().c_str());

        while (m_running.load()) {
            zmq::message_t msg;
            const bool ok = pullSocket.recv(&msg, ZMQ_DONTWAIT);
            if (!ok) {
                QThread::msleep(50);
                continue;
            }

            eraser::ClientReport report;
            if (!report.ParseFromArray(msg.data(), static_cast<int>(msg.size()))) {
                emit errorOccurred(QStringLiteral("收到无法解析的 protobuf 数据包"));
                continue;
            }

            emit clientConnected(QString::fromStdString(report.client_id()),
                                 QString::fromStdString(report.host_name()),
                                 QString::fromStdString(report.ip()));

            QStringList diskLines;
            for (int i = 0; i < report.disks_size(); ++i) {
                const eraser::DiskInfo &d = report.disks(i);
                diskLines << QString("%1(fs:%2, serial:%3, used:%4/%5)")
                                 .arg(QString::fromStdString(d.disk_name()))
                                 .arg(QString::fromStdString(d.filesystem()))
                                 .arg(QString::fromStdString(d.serial()))
                                 .arg(static_cast<qulonglong>(d.used_bytes()))
                                 .arg(static_cast<qulonglong>(d.total_bytes()));
            }
            if (!diskLines.isEmpty()) {
                emit diskInfoReceived(QString::fromStdString(report.client_id()), diskLines.join("; "));
            }

            if (report.has_erase()) {
                const eraser::EraseInfo &e = report.erase();
                const QString eraseSummary =
                    QString("task:%1 algo:%2 progress:%3%% finished:%4 result:%5")
                        .arg(QString::fromStdString(e.task_id()))
                        .arg(QString::fromStdString(e.algorithm()))
                        .arg(e.progress())
                        .arg(e.finished() ? "true" : "false")
                        .arg(QString::fromStdString(e.result()));
                emit eraseInfoReceived(QString::fromStdString(report.client_id()), eraseSummary);
            }
        }
    } catch (const std::exception &e) {
        emit errorOccurred(QStringLiteral("ZMQ 异常: %1").arg(e.what()));
    }
}

void ZmqServerWorker::stop() {
    m_running.store(false);
}
