#ifndef ERASURETASKMANAGER_H
#define ERASURETASKMANAGER_H
#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include <QObject>
#include <QList>
#include <QHash>
#include <QSet>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QThread>
#include <functional>
#include "messages.pb.h"
using namespace socket_eraser;

class TaskWorkerThread;

class ErasureTaskManager : public QObject
{
    Q_OBJECT

public:
    explicit ErasureTaskManager(QObject *parent = nullptr);
    ~ErasureTaskManager() override;
    
    void setClientId(const QString& clientId);
    QString getClientId() const;
    
    void setTaskStorageCallback(std::function<void(const ErasureTask&)> saveCallback,
                                std::function<ErasureTask(const QString&)> loadCallback,
                                std::function<void(const QString&)> deleteCallback);
    
    ErasureTask createTask(QString& taskId, const QString& clientId, const QString& diskId, const QString& diskModel,
                   const QString& method, int passCount, qint64 totalBytes,
                   bool verificationEnabled);
    
    bool updateTaskProgress(const QString& taskId, const ErasureProgress& progress);
    bool updateTaskInfo(const QString& taskId, const ErasureTask& taskInfo);
    bool updateTaskStatus(const QString& taskId, TaskStatus status,
                         const QString& errorMessage = "");
    bool completeTask(const QString& taskId, bool success, bool verificationPassed);
    bool pauseTask(const QString& taskId);
    bool resumeTask(const QString& taskId);
    bool stopTask(const QString& taskId, const QString& reason = "用户手动停止");
    
    ErasureTask getTask(const QString& taskId);
    QList<ErasureTask> getAllTasks();
    QList<ErasureTask> getTasksByClient(const QString& clientId);
    QList<ErasureTask> getTasksByDisk(const QString& clientId, const QString& diskId);
    QList<ErasureTask> getActiveTasks();
    QList<ErasureTask> getActiveTasksByClient(const QString& clientId);
    QList<ErasureTask> getCompletedTasks();
    QList<ErasureTask> getCompletedTasksByClient(const QString& clientId);
    
    QStringList getAllClientIds();
    QStringList getDiskIdsByClient(const QString& clientId);
    
    bool hasTask(const QString& taskId);
    bool hasActiveTaskForDisk(const QString& clientId, const QString& diskId, const QString& taskId="");
    
    void clearCompletedTasks();
    void clearCompletedTasksByClient(const QString& clientId);
    void clearAllTasks();
    void clearTasksByClient(const QString& clientId);

signals:
    void taskCreated(const QByteArray& taskData);
    void taskProgressUpdated(const QByteArray& taskData);//ErasureProgress
    void taskStatusChanged(const QByteArray& taskData);
    void taskCompleted(const QByteArray& taskData);
    void taskRemoved(const QString& taskId);
    void clientAdded(const QString& clientId);
    void clientRemoved(const QString& clientId);

private:
    QString generateTaskId(const QString& clientId, const QString& diskId);
    void startWorker();
    void stopWorker();
    void enqueueOperation(const std::function<void()>& operation);
    void createTaskImpl(const QString& taskId, const QString& clientId, const QString& diskId,
                        const QString& diskModel, const QString& method, int passCount,
                        qint64 totalBytes, bool verificationEnabled);
    bool updateTaskProgressImpl(const QString& taskId, const ErasureProgress& progress);
    bool updateTaskInfoImpl(const QString& taskId, const ErasureTask& taskInfo);
    bool updateTaskStatusImpl(const QString& taskId, TaskStatus status,
                              const QString& errorMessage = "");
    bool completeTaskImpl(const QString& taskId, bool success, bool verificationPassed);
    bool pauseTaskImpl(const QString& taskId);
    bool resumeTaskImpl(const QString& taskId);
    bool stopTaskImpl(const QString& taskId, const QString& reason = "用户手动停止");
    void saveTaskToStorage(const ErasureTask& task);
    void updateTaskInStorage(const ErasureTask& task);

    QString m_currentClientId;
    QHash<QString, ErasureTask> m_tasks;
    QHash<QString, QSet<QString>> m_clientDisks;
    QMutex m_mutex;
    QMutex m_queueMutex;
    QQueue<std::function<void()>> m_operationQueue;
    QWaitCondition m_queueCondition;
    bool m_stopRequested;
    TaskWorkerThread* m_workerThread;
    
    std::function<void(const ErasureTask&)> m_saveCallback;
    std::function<ErasureTask(const QString&)> m_loadCallback;
    std::function<void(const QString&)> m_deleteCallback;
};

#endif // ERASURETASKMANAGER_H
