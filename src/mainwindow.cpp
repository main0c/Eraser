#include "mainwindow.h"

#include "zmqserverworker.h"

#include <QDateTime>
#include <QMetaObject>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      m_log(new QTextEdit(this)),
      m_startBtn(new QPushButton(QStringLiteral("启动服务"), this)),
      m_stopBtn(new QPushButton(QStringLiteral("停止服务"), this)),
      m_workerThread(new QThread(this)),
      m_worker(new ZmqServerWorker()) {
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);
    m_log->setReadOnly(true);
    m_stopBtn->setEnabled(false);

    layout->addWidget(m_startBtn);
    layout->addWidget(m_stopBtn);
    layout->addWidget(m_log);
    setCentralWidget(central);
    resize(900, 560);

    m_worker->moveToThread(m_workerThread);
    connect(m_startBtn, SIGNAL(clicked()), this, SLOT(startServer()));
    connect(m_stopBtn, SIGNAL(clicked()), this, SLOT(stopServer()));

    connect(m_worker, SIGNAL(clientConnected(QString,QString,QString)),
            this, SLOT(appendClient(QString,QString,QString)));
    connect(m_worker, SIGNAL(diskInfoReceived(QString,QString)),
            this, SLOT(appendDisk(QString,QString)));
    connect(m_worker, SIGNAL(eraseInfoReceived(QString,QString)),
            this, SLOT(appendErase(QString,QString)));
    connect(m_worker, SIGNAL(errorOccurred(QString)),
            this, SLOT(appendError(QString)));

    connect(m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);
}

MainWindow::~MainWindow() {
    stopServer();
}

void MainWindow::startServer() {
    m_workerThread->start();
    QMetaObject::invokeMethod(m_worker, "start", Qt::QueuedConnection,
                              Q_ARG(QString, "tcp://*:5555"));
    m_startBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    appendError(QStringLiteral("服务已启动: tcp://*:5555"));
}

void MainWindow::stopServer() {
    if (!m_workerThread->isRunning()) {
        return;
    }
    QMetaObject::invokeMethod(m_worker, "stop", Qt::QueuedConnection);
    m_workerThread->quit();
    m_workerThread->wait();
    m_startBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);
    appendError(QStringLiteral("服务已停止"));
}

void MainWindow::appendClient(const QString &clientId, const QString &hostName, const QString &ip) {
    m_log->append(QString("[%1] [连接] client=%2 host=%3 ip=%4")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                      .arg(clientId)
                      .arg(hostName)
                      .arg(ip));
}

void MainWindow::appendDisk(const QString &clientId, const QString &diskSummary) {
    m_log->append(QString("[%1] [磁盘] client=%2 %3")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                      .arg(clientId)
                      .arg(diskSummary));
}

void MainWindow::appendErase(const QString &clientId, const QString &eraseSummary) {
    m_log->append(QString("[%1] [擦除] client=%2 %3")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                      .arg(clientId)
                      .arg(eraseSummary));
}

void MainWindow::appendError(const QString &msg) {
    m_log->append(QString("[%1] [状态] %2")
                      .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"))
                      .arg(msg));
}
