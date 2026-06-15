#include "TaskWorkerThread.h"
#include <QThread>
#include <QDebug>

TaskWorkerThread::TaskWorkerThread(QObject *parent)
    : QObject(parent)
    , m_stopRequested(false)
    , m_running(false)
{
}

TaskWorkerThread::~TaskWorkerThread()
{
    stopWorker();
}

void TaskWorkerThread::startWorker()
{
    if (m_running) {
        return;
    }

    m_stopRequested = false;
    m_running = true;
    QThread* thread = new QThread(this);
    moveToThread(thread);
    connect(thread, &QThread::started, this, &TaskWorkerThread::run, Qt::DirectConnection);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void TaskWorkerThread::stopWorker()
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_stopRequested = true;
        m_queueCondition.wakeAll();
    }

    if (m_running) {
        QThread* thread = this->thread();
        if (thread) {
            thread->quit();
            thread->wait(2000);
        }
        m_running = false;
    }
}

void TaskWorkerThread::enqueueOperation(const std::function<void()>& operation)
{
    {
        QMutexLocker locker(&m_queueMutex);
        m_operationQueue.enqueue(operation);
        m_queueCondition.wakeAll();
    }
}

void TaskWorkerThread::run()
{
    while (!m_stopRequested) {
        std::function<void()> operation;
        {
            QMutexLocker locker(&m_queueMutex);
            while (m_operationQueue.isEmpty() && !m_stopRequested) {
                m_queueCondition.wait(&m_queueMutex);
            }
            if (m_stopRequested) {
                break;
            }
            operation = m_operationQueue.dequeue();
        }

        if (operation) {
            operation();
        }
    }

    m_running = false;
}
