#ifndef TASKWORKERTHREAD_H
#define TASKWORKERTHREAD_H

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <functional>

class TaskWorkerThread : public QObject
{
    Q_OBJECT
public:
    explicit TaskWorkerThread(QObject *parent = nullptr);
    ~TaskWorkerThread() override;

    void startWorker();
    void stopWorker();
    void enqueueOperation(const std::function<void()>& operation);

private slots:
    void run();

private:
    QMutex m_queueMutex;
    QQueue<std::function<void()>> m_operationQueue;
    QWaitCondition m_queueCondition;
    bool m_stopRequested;
    bool m_running;
};

#endif
