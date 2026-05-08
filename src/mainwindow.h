#pragma once

#include <QMainWindow>

class QTextEdit;
class QPushButton;
class QThread;
class ZmqServerWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startServer();
    void stopServer();
    void appendClient(const QString &clientId, const QString &hostName, const QString &ip);
    void appendDisk(const QString &clientId, const QString &diskSummary);
    void appendErase(const QString &clientId, const QString &eraseSummary);
    void appendError(const QString &msg);

private:
    QTextEdit *m_log;
    QPushButton *m_startBtn;
    QPushButton *m_stopBtn;
    QThread *m_workerThread;
    ZmqServerWorker *m_worker;
};
