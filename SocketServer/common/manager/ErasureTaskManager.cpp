#include "ErasureTaskManager.h"
#include "TaskWorkerThread.h"
#include <QUuid>
#include <QDebug>
#include "Utils.h"
ErasureTaskManager::ErasureTaskManager(QObject *parent)
    : QObject(parent)
    , m_stopRequested(false)
    , m_workerThread(nullptr)
{
    startWorker();
}

ErasureTaskManager::~ErasureTaskManager()
{
    stopWorker();
}

void ErasureTaskManager::setClientId(const QString& clientId)
{
    m_currentClientId = clientId;
}

QString ErasureTaskManager::getClientId() const
{
    return m_currentClientId;
}

void ErasureTaskManager::setTaskStorageCallback(
    std::function<void(const ErasureTask&)> saveCallback,
    std::function<ErasureTask(const QString&)> loadCallback,
    std::function<void(const QString&)> deleteCallback)
{
    m_saveCallback = saveCallback;
    m_loadCallback = loadCallback;
    m_deleteCallback = deleteCallback;
}
ErasureTask ErasureTaskManager::createTask(QString& taskId, const QString& clientId, const QString& diskId,
                                    const QString& diskModel, const QString& method, 
                                    int passCount, qint64 totalBytes, bool verificationEnabled)
{
    if (clientId.isEmpty()) {
        qWarning() << "客户端ID不能为空";
        return ErasureTask();
    }

    if (taskId.isEmpty()) {
        taskId = generateTaskId(clientId, diskId);
    }

    ErasureTask task;
    task.set_server_client_id(clientId.toStdString());
    task.set_task_id(taskId.toStdString());
    task.set_disk_id(diskId.toStdString());
    task.mutable_disk_info()->set_model(diskModel.toStdString());
    task.set_erasure_method(method.toStdString());
    task.set_pass_count(passCount);
    task.set_total_bytes(totalBytes);
    task.set_erased_bytes(0);
    task.set_progress_percent(0);
    task.set_status(TaskStatus::STARTED);
    task.set_verification_enabled(verificationEnabled);
    task.set_verification_passed(false);
    task.set_start_time(QDateTime::currentMSecsSinceEpoch());
    task.set_speed_mb_per_s(0.0);

    if (m_workerThread) {
        enqueueOperation([this, taskId, clientId, diskId, diskModel, method, passCount, totalBytes, verificationEnabled]() {
            createTaskImpl(taskId, clientId, diskId, diskModel, method, passCount, totalBytes, verificationEnabled);
        });
    } else {
        createTaskImpl(taskId, clientId, diskId, diskModel, method, passCount, totalBytes, verificationEnabled);
    }

    return task;
}

bool ErasureTaskManager::updateTaskProgress(const QString& taskId, const ErasureProgress& progress)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId, progress]() {
            updateTaskProgressImpl(taskId, progress);
        });
        return true;
    }
    return updateTaskProgressImpl(taskId, progress);
}

bool ErasureTaskManager::updateTaskInfo(const QString& taskId, const ErasureTask& taskInfo)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId, taskInfo]() {
            updateTaskInfoImpl(taskId, taskInfo);
        });
        return true;
    }
    return updateTaskInfoImpl(taskId, taskInfo);
}

bool ErasureTaskManager::updateTaskStatus(const QString& taskId, TaskStatus status,
                                          const QString& errorMessage)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId, status, errorMessage]() {
            updateTaskStatusImpl(taskId, status, errorMessage);
        });
        return true;
    }
    return updateTaskStatusImpl(taskId, status, errorMessage);
}

bool ErasureTaskManager::completeTask(const QString& taskId, bool success, bool verificationPassed)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId, success, verificationPassed]() {
            completeTaskImpl(taskId, success, verificationPassed);
        });
        return true;
    }
    return completeTaskImpl(taskId, success, verificationPassed);
}

bool ErasureTaskManager::pauseTask(const QString& taskId)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId]() {
            pauseTaskImpl(taskId);
        });
        return true;
    }
    return pauseTaskImpl(taskId);
}

bool ErasureTaskManager::resumeTask(const QString& taskId)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId]() {
            resumeTaskImpl(taskId);
        });
        return true;
    }
    return resumeTaskImpl(taskId);
}

bool ErasureTaskManager::stopTask(const QString& taskId, const QString& reason)
{
    if (m_workerThread) {
        enqueueOperation([this, taskId, reason]() {
            stopTaskImpl(taskId, reason);
        });
        return true;
    }
    return stopTaskImpl(taskId, reason);
}

ErasureTask ErasureTaskManager::getTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.value(taskId, ErasureTask());
}

QList<ErasureTask> ErasureTaskManager::getAllTasks()
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.values();
}

QList<ErasureTask> ErasureTaskManager::getTasksByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> result;
    for (const ErasureTask& task : m_tasks.values()) {
        if (QString::fromStdString(task.server_client_id()) == clientId) {
            result.append(task);
        }
    }
    return result;
}

QList<ErasureTask> ErasureTaskManager::getTasksByDisk(const QString& clientId, const QString& diskId)
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> result;
    for (const ErasureTask& task : m_tasks.values()) {
        if (QString::fromStdString(task.server_client_id()) == clientId &&
            QString::fromStdString(task.disk_id()) == diskId) {
            result.append(task);
        }
    }
    return result;
}

QList<ErasureTask> ErasureTaskManager::getActiveTasks()
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> activeTasks;
    for (const ErasureTask& task : m_tasks.values()) {
        if (task.status() == TaskStatus::STARTED ||
            task.status() == TaskStatus::IN_PROGRESS ||
            task.status() == TaskStatus::PAUSED) {
            activeTasks.append(task);
        }
    }
    return activeTasks;
}

QList<ErasureTask> ErasureTaskManager::getActiveTasksByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> activeTasks;
    for (const ErasureTask& task : m_tasks.values()) {
        if (QString::fromStdString(task.server_client_id()) == clientId &&
            (task.status() == TaskStatus::STARTED ||
             task.status() == TaskStatus::IN_PROGRESS ||
             task.status() == TaskStatus::PAUSED)) {
            activeTasks.append(task);
        }
    }
    return activeTasks;
}

QList<ErasureTask> ErasureTaskManager::getCompletedTasks()
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> completedTasks;
    for (const ErasureTask& task : m_tasks.values()) {
        if (task.status() == TaskStatus::COMPLETED ||
            task.status() == TaskStatus::FAILED ||
            task.status() == TaskStatus::STOPPED) {
            completedTasks.append(task);
        }
    }
    return completedTasks;
}

QList<ErasureTask> ErasureTaskManager::getCompletedTasksByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    QList<ErasureTask> completedTasks;
    for (const ErasureTask& task : m_tasks.values()) {
        if (QString::fromStdString(task.server_client_id()) == clientId &&
            (task.status() == TaskStatus::COMPLETED ||
             task.status() == TaskStatus::FAILED ||
             task.status() == TaskStatus::STOPPED)) {
            completedTasks.append(task);
        }
    }
    return completedTasks;
}

QStringList ErasureTaskManager::getAllClientIds()
{
    QMutexLocker locker(&m_mutex);
    return m_clientDisks.keys();
}

QStringList ErasureTaskManager::getDiskIdsByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    if (!m_clientDisks.contains(clientId)) {
        return QStringList();
    }
    return m_clientDisks[clientId].values();
}

bool ErasureTaskManager::hasTask(const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    return m_tasks.contains(taskId);
}

bool ErasureTaskManager::hasActiveTaskForDisk(const QString& clientId, const QString& diskId, const QString& taskId)
{
    QMutexLocker locker(&m_mutex);
    for (const ErasureTask& task : m_tasks.values()) {
        if (QString::fromStdString(task.server_client_id()) != clientId ||
            QString::fromStdString(task.disk_id()) != diskId) {
            continue;
        }

        if (task.status() != TaskStatus::STARTED &&
            task.status() != TaskStatus::IN_PROGRESS &&
            task.status() != TaskStatus::PAUSED) {
            continue;
        }

        if (!taskId.isEmpty() && QString::fromStdString(task.task_id()) == taskId) {
            continue;
        }

        return true;
    }
    return false;
}

void ErasureTaskManager::clearCompletedTasks()
{
    QMutexLocker locker(&m_mutex);
    QStringList toRemove;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (it->status() == TaskStatus::COMPLETED ||
            it->status() == TaskStatus::FAILED ||
            it->status() == TaskStatus::STOPPED) {
            toRemove.append(it.key());
            if (m_deleteCallback) {
                m_deleteCallback(it.key());
            }
        }
    }
    
    for (const QString& taskId : toRemove) {
        ErasureTask task = m_tasks[taskId];
        m_tasks.remove(taskId);
        
        QString clientId = QString::fromStdString(task.server_client_id());
        QString diskId = QString::fromStdString(task.disk_id());
        m_clientDisks[clientId].remove(diskId);
        if (m_clientDisks[clientId].isEmpty()) {
            m_clientDisks.remove(clientId);
            emit clientRemoved(clientId);
        }
        
        emit taskRemoved(taskId);
    }
}

void ErasureTaskManager::clearCompletedTasksByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    QStringList toRemove;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (QString::fromStdString(it->server_client_id()) == clientId &&
            (it->status() == TaskStatus::COMPLETED ||
             it->status() == TaskStatus::FAILED ||
             it->status() == TaskStatus::STOPPED)) {
            toRemove.append(it.key());
            if (m_deleteCallback) {
                m_deleteCallback(it.key());
            }
        }
    }
    
    for (const QString& taskId : toRemove) {
        ErasureTask task = m_tasks[taskId];
        m_tasks.remove(taskId);
        m_clientDisks[clientId].remove(QString::fromStdString(task.disk_id()));
        emit taskRemoved(taskId);
    }
    
    if (m_clientDisks[clientId].isEmpty()) {
        m_clientDisks.remove(clientId);
        emit clientRemoved(clientId);
    }
}

void ErasureTaskManager::clearAllTasks()
{
    QMutexLocker locker(&m_mutex);
    if (m_deleteCallback) {
        for (const QString& taskId : m_tasks.keys()) {
            m_deleteCallback(taskId);
        }
    }
    
    m_tasks.clear();
    m_clientDisks.clear();
}

void ErasureTaskManager::clearTasksByClient(const QString& clientId)
{
    QMutexLocker locker(&m_mutex);
    QStringList toRemove;
    for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
        if (QString::fromStdString(it->server_client_id()) == clientId) {
            toRemove.append(it.key());
            if (m_deleteCallback) {
                m_deleteCallback(it.key());
            }
        }
    }
    
    for (const QString& taskId : toRemove) {
        m_tasks.remove(taskId);
        emit taskRemoved(taskId);
    }
    
    m_clientDisks.remove(clientId);
    emit clientRemoved(clientId);
}

void ErasureTaskManager::startWorker()
{
    if (m_workerThread) {
        return;
    }

    m_stopRequested = false;
    m_workerThread = new TaskWorkerThread(this);
    m_workerThread->startWorker();
}

void ErasureTaskManager::stopWorker()
{
    if (!m_workerThread) {
        return;
    }

    m_stopRequested = true;
    m_workerThread->stopWorker();
    m_workerThread->deleteLater();
    m_workerThread = nullptr;
}

void ErasureTaskManager::enqueueOperation(const std::function<void()>& operation)
{
    if (!m_workerThread) {
        startWorker();
    }

    if (m_workerThread) {
        m_workerThread->enqueueOperation(operation);
    }
}

void ErasureTaskManager::createTaskImpl(const QString& taskId, const QString& clientId, const QString& diskId,
                                        const QString& diskModel, const QString& method, int passCount,
                                        qint64 totalBytes, bool verificationEnabled)
{
    if (clientId.isEmpty()) {
        qWarning() << "客户端ID不能为空";
        return;
    }

    if (hasActiveTaskForDisk(clientId, diskId, taskId)) {
        qWarning() << "磁盘已有活动任务:" << clientId << diskId;
        return;
    }

    QMutexLocker locker(&m_mutex);

    ErasureTask task;
    task.set_server_client_id(clientId.toStdString());
    task.set_task_id(taskId.toStdString());
    task.set_disk_id(diskId.toStdString());
    task.mutable_disk_info()->set_model(diskModel.toStdString());
    task.set_erasure_method(method.toStdString());
    task.set_pass_count(passCount);
    task.set_total_bytes(totalBytes);
    task.set_erased_bytes(0);
    task.set_progress_percent(0);
    task.set_status(TaskStatus::STARTED);
    task.set_verification_enabled(verificationEnabled);
    task.set_verification_passed(false);
    task.set_start_time(QDateTime::currentMSecsSinceEpoch());
    task.set_speed_mb_per_s(0.0);

    qint64 estimatedSeconds = totalBytes / (1024 * 1024 * 10);
    task.set_estimated_end_time(QDateTime::currentMSecsSinceEpoch() + estimatedSeconds * 1000);

    m_tasks[taskId] = task;
    m_clientDisks[clientId].insert(diskId);

    locker.unlock();
    saveTaskToStorage(task);

    QByteArray taskData = Utils::serializeTask(task);
    emit taskCreated(taskData);
    qDebug() << "创建擦除任务:" << taskId << "客户端:" << clientId << "磁盘:" << diskId;
}

bool ErasureTaskManager::updateTaskProgressImpl(const QString& taskId, const ErasureProgress& progress)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return false;
    }

    ErasureTask task = m_tasks[taskId];
    task.set_progress_percent(progress.progress_percent());
    task.set_erased_bytes(progress.erased_bytes());
    task.set_speed_mb_per_s(progress.speed_mb_per_s());
    m_tasks[taskId] = task;

    locker.unlock();
    updateTaskInStorage(task);

    QByteArray taskData = Utils::serializeProgress(progress);
    emit taskProgressUpdated(taskData);

    return true;
}

bool ErasureTaskManager::updateTaskInfoImpl(const QString& taskId, const ErasureTask& taskInfo)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return false;
    }

    ErasureTask task = m_tasks[taskId];
    task.set_status(taskInfo.status());
    task.set_progress_percent(taskInfo.progress_percent());
    task.set_erased_bytes(taskInfo.erased_bytes());
    task.set_total_bytes(taskInfo.total_bytes());
    task.set_speed_mb_per_s(taskInfo.speed_mb_per_s());
    task.set_error_message(taskInfo.error_message());
    task.set_verification_passed(taskInfo.verification_passed());

    if (taskInfo.start_time() > 0) {
        task.set_start_time(taskInfo.start_time());
    }
    if (taskInfo.estimated_end_time() > 0) {
        task.set_estimated_end_time(taskInfo.estimated_end_time());
    }

    m_tasks[taskId] = task;

    locker.unlock();
    updateTaskInStorage(task);

    QByteArray taskData = Utils::serializeTask(task);
    emit taskStatusChanged(taskData);

    return true;
}

bool ErasureTaskManager::updateTaskStatusImpl(const QString& taskId, TaskStatus status,
                                               const QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return false;
    }

    ErasureTask task = m_tasks[taskId];
    task.set_status(status);
    task.set_error_message(errorMessage.toStdString());
    m_tasks[taskId] = task;

    locker.unlock();
    updateTaskInStorage(task);

    QByteArray taskData = Utils::serializeTask(task);
    emit taskStatusChanged(taskData);

    return true;
}

bool ErasureTaskManager::completeTaskImpl(const QString& taskId, bool success, bool verificationPassed)
{
    QMutexLocker locker(&m_mutex);

    if (!m_tasks.contains(taskId)) {
        return false;
    }

    ErasureTask task = m_tasks[taskId];
    task.set_status(success ? TaskStatus::COMPLETED : TaskStatus::FAILED);
    if (success) {
        task.set_progress_percent(10000);
        task.set_erased_bytes(task.total_bytes());
    }
    task.set_verification_passed(verificationPassed);
    task.set_estimated_end_time(QDateTime::currentMSecsSinceEpoch());
    m_tasks[taskId] = task;

    locker.unlock();
    updateTaskInStorage(task);

    QByteArray taskData = Utils::serializeTask(task);
    emit taskCompleted(taskData);

    qDebug() << "任务完成:" << taskId << (success ? "成功" : "失败");

    return true;
}

bool ErasureTaskManager::pauseTaskImpl(const QString& taskId)
{
    return updateTaskStatusImpl(taskId, TaskStatus::PAUSED);
}

bool ErasureTaskManager::resumeTaskImpl(const QString& taskId)
{
    return updateTaskStatusImpl(taskId, TaskStatus::IN_PROGRESS);
}

bool ErasureTaskManager::stopTaskImpl(const QString& taskId, const QString& reason)
{
    return updateTaskStatusImpl(taskId, TaskStatus::STOPPED, reason);
}

QString ErasureTaskManager::generateTaskId(const QString& clientId, const QString& diskId)
{
    return QString("TASK_%1_%2_%3").arg(clientId).arg(diskId).arg(QDateTime::currentMSecsSinceEpoch());
}

void ErasureTaskManager::saveTaskToStorage(const ErasureTask& task)
{
    if (m_saveCallback) {
        m_saveCallback(task);
    }
}

void ErasureTaskManager::updateTaskInStorage(const ErasureTask& task)
{
    if (m_saveCallback) {
        m_saveCallback(task);
    }
}
