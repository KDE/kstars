/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "queueexecutor.h"
#include "../actions/taskaction.h"
#include "../actions/evaluateaction.h"
#include "../templatemanager.h"
#include "ekos/manager.h"
#include "indi/indilistener.h"
#include <ekos_scheduler_debug.h>
#include <QStringList>

namespace Ekos
{

namespace
{

QString supportedInterfaceList(const QList<uint32_t> &interfaces)
{
    QStringList values;
    values.reserve(interfaces.size());

    for (uint32_t interface : interfaces)
        values << QString::number(interface);

    return values.join(", ");
}

bool matchesSupportedInterfaces(const QList<uint32_t> &supportedInterfaces, uint32_t deviceInterfaces)
{
    // supported_interfaces is a list of acceptable INDI driver interface flags.
    // A device only needs to expose one of them to satisfy the template.
    for (uint32_t supportedInterface : supportedInterfaces)
    {
        if (deviceInterfaces & supportedInterface)
            return true;
    }

    return false;
}

TaskTemplate *resolveTaskTemplate(Task *task)
{
    if (!task || task->templateId().isEmpty())
        return nullptr;

    TemplateManager *templateMgr = TemplateManager::Instance();
    if (!templateMgr)
        return nullptr;

    if (templateMgr->allTemplates().isEmpty() && !templateMgr->initialize())
        return nullptr;

    return templateMgr->getTemplate(task->templateId());
}

bool ensureSupportedInterfaces(Task *task, QList<uint32_t> &supportedInterfaces)
{
    if (!task)
        return false;

    supportedInterfaces = task->supportedInterfaces();
    if (!supportedInterfaces.isEmpty() || task->templateId().isEmpty())
        return true;

    // Older queue files do not persist supported_interfaces, so recover them
    // from the template when the template is still available.
    if (TaskTemplate *tmpl = resolveTaskTemplate(task))
    {
        supportedInterfaces = tmpl->supportedInterfaces();
        task->setSupportedInterfaces(supportedInterfaces);
        return true;
    }

    return false;
}

}

QueueExecutor::QueueExecutor(QueueManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
{
    Q_ASSERT(manager);
}

QueueExecutor::~QueueExecutor()
{
    stop();
}

Task *QueueExecutor::taskForItem(QueueItem *item, const QString &context)
{
    if (!item)
    {
        const QString message = i18n("Task queue executor cannot %1 because no queue item is active.", context);
        qCWarning(KSTARS_EKOS_SCHEDULER) << message;
        Q_EMIT newLog(message);
        return nullptr;
    }

    Task *task = item->task();
    if (!task)
    {
        const QString itemName = item->name().isEmpty() ? item->id() : item->name();
        const QString message = i18n("Task queue item '%1' has no task while trying to %2.", itemName, context);
        qCWarning(KSTARS_EKOS_SCHEDULER) << message;
        Q_EMIT newLog(message);
        return nullptr;
    }

    return task;
}

void QueueExecutor::clearCurrentExecution()
{
    if (m_currentAction)
        disconnect(m_currentAction, nullptr, this, nullptr);

    if (m_currentItem)
        disconnect(m_currentItem, nullptr, this, nullptr);

    if (m_manager)
        m_manager->setCurrentItem(nullptr);

    m_currentItem = nullptr;
    m_currentAction = nullptr;
    m_currentActionIndex = -1;
}

bool QueueExecutor::start()
{
    if (m_running)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Queue already running, cannot start";
        return false;
    }

    if (!m_manager || m_manager->count() == 0)
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Cannot start queue: no items";
        return false;
    }

    // Check if any task requires devices by looking at the task's recorded
    // interface requirements. Older queue files may need a template lookup.
    bool requiresDevices = false;

    for (QueueItem *item : m_manager->items())
    {
        Task *task = taskForItem(item, i18n("start a queue"));
        if (!task)
            return false;

        // Check if task has explicit device OR template requires interfaces
        if (!task->device().isEmpty())
        {
            requiresDevices = true;
            break;
        }

        QList<uint32_t> supportedInterfaces;
        if (!ensureSupportedInterfaces(task, supportedInterfaces))
        {
            const QString errorMsg = i18n("Cannot start queue: task template '%1' is unavailable for '%2'.")
                                     .arg(task->templateId(), task->name());
            qCWarning(KSTARS_EKOS_SCHEDULER) << errorMsg;
            emit newLog(errorMsg);
            return false;
        }

        if (!supportedInterfaces.isEmpty())
        {
            requiresDevices = true;
            break;
        }
    }

    // Only check Ekos status if tasks require devices
    if (requiresDevices)
    {
        Ekos::Manager *ekosManager = Ekos::Manager::Instance();
        if (!ekosManager)
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Cannot start queue: Ekos Manager not available";
            return false;
        }

        if (ekosManager->indiStatus() != Ekos::Success)
        {
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Cannot start queue: No Ekos profile running. Please start an equipment profile first.";
            return false;
        }
    }
    else
    {
        qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting queue with device-independent tasks only - Ekos not required";
    }

    m_running = true;
    m_paused = false;
    m_abortRequested = false;

    Q_EMIT newLog(i18n("Starting task queue with %1 items", m_manager->count()));

    m_manager->setState(QueueManager::RUNNING);
    Q_EMIT started();

    // Start executing from first pending item
    executeNext();

    return true;
}

void QueueExecutor::pause()
{
    if (!m_running || m_paused)
        return;

    m_paused = true;
    m_manager->setState(QueueManager::PAUSED);
    Q_EMIT paused();
}

void QueueExecutor::resume()
{
    if (!m_running || !m_paused)
        return;

    m_paused = false;
    m_manager->setState(QueueManager::RUNNING);
    Q_EMIT resumed();

    // Continue execution
    if (!m_currentItem)
    {
        executeNext();
    }
}

void QueueExecutor::stop()
{
    if (!m_running)
        return;

    m_running = false;
    m_paused = false;
    clearCurrentExecution();

    m_manager->setState(QueueManager::IDLE);
    Q_EMIT stopped();
}

void QueueExecutor::abort()
{
    if (!m_running)
        return;

    m_abortRequested = true;

    TaskAction *action = m_currentAction;
    if (action)
        disconnect(action, nullptr, this, nullptr);

    if (m_currentItem)
        disconnect(m_currentItem, nullptr, this, nullptr);

    if (action && action->status() == TaskAction::Status::RUNNING)
        action->abort();

    m_running = false;
    m_paused = false;
    clearCurrentExecution();

    m_manager->setState(QueueManager::ABORTED);
    Q_EMIT aborted();
    Q_EMIT stopped();
}

void QueueExecutor::executeNext()
{
    if (!m_running || m_paused || m_abortRequested)
        return;

    // Find next pending item
    QueueItem *nextItem = nullptr;
    for (QueueItem *item : m_manager->items())
    {
        if (!item)
        {
            const QString message = i18n("Task queue contains an invalid empty item. Skipping it.");
            qCWarning(KSTARS_EKOS_SCHEDULER) << message;
            Q_EMIT newLog(message);
            continue;
        }

        if (item->status() == QueueItem::PENDING ||
                item->status() == QueueItem::SCHEDULED)
        {
            if (!item->task())
            {
                const QString message = i18n("Task queue item '%1' has no task. Marking it failed.", item->id());
                qCWarning(KSTARS_EKOS_SCHEDULER) << message;
                Q_EMIT newLog(message);
                item->setErrorMessage(message);
                item->setStatus(QueueItem::FAILED);
                Q_EMIT itemFailed(item, message);
                continue;
            }

            nextItem = item;
            break;
        }
    }

    if (!nextItem)
    {
        // Queue completed
        m_running = false;
        m_manager->setState(QueueManager::COMPLETED);
        Q_EMIT completed();
        return;
    }

    executeItem(nextItem);
}

void QueueExecutor::executeItem(QueueItem *item)
{
    Task *task = taskForItem(item, i18n("execute an item"));
    if (!task)
        return;

    m_currentItem = item;
    m_currentActionIndex = 0;
    m_manager->setCurrentItem(item);

    // Connect to item signals
    connect(item, &QueueItem::statusChanged,
            this, &QueueExecutor::onItemStatusChanged);

    // Check if device needs to be assigned at runtime
    if (task->device().isEmpty())
    {
        QList<uint32_t> supportedInterfaces;
        if (!ensureSupportedInterfaces(task, supportedInterfaces))
        {
            const QString errorMsg = i18n("Task template '%1' is unavailable for runtime device assignment.")
                                     .arg(task->templateId());
            qCWarning(KSTARS_EKOS_SCHEDULER) << errorMsg << "for task:" << task->name();
            emit newLog(i18n("Task '%1' failed device assignment: %2", task->name(), errorMsg));
            item->setErrorMessage(errorMsg);
            item->setStatus(QueueItem::FAILED);
            emit itemFailed(item, errorMsg);
            clearCurrentExecution();
            executeNext();
            return;
        }

        if (!supportedInterfaces.isEmpty())
        {
            auto allDevices = INDIListener::devices();
            QString assignedDevice;

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Searching for device matching supported interfaces:"
                                          << supportedInterfaceList(supportedInterfaces)
                                          << "for task:" << task->name();

            for (const auto &device : allDevices)
            {
                if (device && device->isConnected())
                {
                    uint32_t deviceInterfaces = device->getDriverInterface();
                    qCInfo(KSTARS_EKOS_SCHEDULER) << "  Checking device:" << device->getDeviceName()
                                                  << "interfaces:" << deviceInterfaces
                                                  << "(" << QString::number(deviceInterfaces, 2) << ")";

                    if (matchesSupportedInterfaces(supportedInterfaces, deviceInterfaces))
                    {
                        assignedDevice = device->getDeviceName();
                        qCInfo(KSTARS_EKOS_SCHEDULER) << "Found matching device:" << assignedDevice
                                                      << "with interfaces:" << deviceInterfaces
                                                      << "matching supported interfaces:"
                                                      << supportedInterfaceList(supportedInterfaces);
                        break;
                    }
                }
            }

            if (!assignedDevice.isEmpty())
            {
                task->setDevice(assignedDevice);
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Dynamically assigned device:" << assignedDevice << "to task:" << task->name();
            }
            else
            {
                // No device found with required interfaces - check failure action
                QString errorMsg = i18n("No connected device found matching supported interfaces (%1)")
                                   .arg(supportedInterfaceList(supportedInterfaces));
                qCWarning(KSTARS_EKOS_SCHEDULER) << errorMsg << "for task:" << task->name();
                emit newLog(i18n("Task '%1' failed device assignment: %2", task->name(), errorMsg));

                handleMissingDevice(item, errorMsg);
                return;
            }
        }
    }

    item->setStatus(QueueItem::RUNNING);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting task:" << task->name()
                                  << "with" << task->actions().size() << "actions";

    Q_EMIT itemStarted(item);

    // Start executing actions
    if (task->actions().isEmpty())
    {
        qCWarning(KSTARS_EKOS_SCHEDULER) << "Task has no actions, marking as completed";
        // No actions, mark as completed
        handleItemCompletion(item);
        return;
    }

    // Execute first action
    m_currentAction = task->actions().first();
    executeAction(m_currentAction);
}

void QueueExecutor::handleMissingDevice(QueueItem *item, const QString &errorMsg)
{
    Task *task = taskForItem(item, i18n("handle a missing device"));
    if (!task)
        return;

    switch (task->deviceMappingFailureAction())
    {
        case TaskAction::SKIP_TO_NEXT_TASK:
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Skipping task due to missing device (failure_action: skip_to_next_task)";
            item->setStatus(QueueItem::SKIPPED);
            item->setErrorMessage(errorMsg);
            // Skipped tasks are not failures; continue with the next pending queue item.
            clearCurrentExecution();
            executeNext();
            return;

        case TaskAction::CONTINUE:
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Marking task as completed despite missing device (failure_action: continue)";
            item->setErrorMessage(errorMsg);
            handleItemCompletion(item);
            return;

        case TaskAction::ABORT_QUEUE:
        default:
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Aborting queue due to missing device (failure_action: abort_queue)";
            item->setErrorMessage(errorMsg);
            item->setStatus(QueueItem::FAILED);
            Q_EMIT itemFailed(item, errorMsg);
            clearCurrentExecution();
            executeNext();
            return;
    }
}

void QueueExecutor::executeAction(TaskAction *action)
{
    if (!action || m_paused || m_abortRequested)
        return;

    Task *task = taskForItem(m_currentItem, i18n("execute an action"));
    if (!task)
        return;

    m_currentAction = action;

    // Connect to action signals
    connect(action, &TaskAction::statusChanged,
            this, &QueueExecutor::onActionStatusChanged);
    connect(action, &TaskAction::progress,
            this, &QueueExecutor::onActionProgress);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing action" << (m_currentActionIndex + 1)
                                  << "of" << task->actions().size()
                                  << "- Type:" << action->type();

    Q_EMIT actionStarted(action);

    // Reset retry counter before first execution
    action->resetRetryCounter();

    // For EVALUATE actions, check if the previous SET action was skipped
    // If so, configure the EVALUATE to accept IDLE state since the property
    // state may not have been updated if the value was already correct
    if (action->type() == TaskAction::EVALUATE && m_currentActionIndex > 0)
    {
        TaskAction *prevAction = task->actions().at(m_currentActionIndex - 1);
        if (prevAction && prevAction->type() == TaskAction::SET && prevAction->wasSkipped())
        {
            auto *evaluateAction = qobject_cast<EvaluateAction *>(action);
            if (evaluateAction)
            {
                qCInfo(KSTARS_EKOS_SCHEDULER) << "Previous SET action was skipped, configuring EVALUATE to accept IDLE state";
                evaluateAction->setAcceptIdleOnSkippedPredecessor(true);
            }
        }
    }

    // Execute the action
    action->start();
}

void QueueExecutor::onItemStatusChanged(QueueItem::Status status)
{
    Q_UNUSED(status);
    // Item status is updated by action completion
    // We handle it in onActionStatusChanged
}

void QueueExecutor::onActionStatusChanged(TaskAction::Status status)
{
    if (!m_currentAction || !m_currentItem)
        return;

    Task *task = taskForItem(m_currentItem, i18n("handle an action status change"));
    if (!task)
        return;

    TaskAction *action = qobject_cast<TaskAction *>(sender());
    if (!action || action != m_currentAction)
        return;

    switch (status)
    {
        case TaskAction::Status::COMPLETED:
            // Disconnect signals - action is done
            disconnect(action, nullptr, this, nullptr);

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Action" << (m_currentActionIndex + 1) << "completed successfully";
            Q_EMIT actionCompleted(action);
            updateProgress();

            // Move to next action
            m_currentActionIndex++;
            if (m_currentActionIndex < task->actions().size())
            {
                m_currentAction = task->actions().at(m_currentActionIndex);
                executeAction(m_currentAction);
            }
            else
            {
                // All actions completed
                handleItemCompletion(m_currentItem);
            }
            break;

        case TaskAction::Status::FAILED:
            // Disconnect signals - action is done
            disconnect(action, nullptr, this, nullptr);

            qCWarning(KSTARS_EKOS_SCHEDULER) << "Action" << (m_currentActionIndex + 1) << "failed:" << action->errorMessage();
            Q_EMIT newLog(i18n("Task '%1', action %2 failed: %3",
                               task->name(), m_currentActionIndex + 1, action->errorMessage()));
            Q_EMIT actionFailed(action, action->errorMessage());
            handleActionFailure(action);
            break;

        case TaskAction::Status::ABORTED:
            // Disconnect signals - action is done
            disconnect(action, nullptr, this, nullptr);

            qCWarning(KSTARS_EKOS_SCHEDULER) << "Action" << (m_currentActionIndex + 1) << "aborted";
            emit newLog(i18n("Task '%1', action %2 aborted.",
                             task->name(), m_currentActionIndex + 1));
            // Action was aborted
            handleItemFailure(m_currentItem);
            break;

        default:
            // ACTION_IDLE, ACTION_RUNNING - intermediate states during retries, keep signals connected
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Action status changed to intermediate state - waiting for completion";
            break;
    }
}

void QueueExecutor::onActionProgress(const QString &message)
{
    updateProgress();
    Q_EMIT progress(m_currentItem->progress(), message);
}

void QueueExecutor::handleItemCompletion(QueueItem *item)
{
    Task *task = taskForItem(item, i18n("complete an item"));
    if (!task)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Task completed:" << task->name();

    // Set progress to 100% on completion
    item->setProgress(100);

    item->setStatus(QueueItem::COMPLETED);
    Q_EMIT itemCompleted(item);

    clearCurrentExecution();

    // Continue to next item
    executeNext();
}

void QueueExecutor::handleItemFailure(QueueItem *item)
{
    Task *task = taskForItem(item, i18n("fail an item"));
    if (!task)
        return;

    qCWarning(KSTARS_EKOS_SCHEDULER) << "Task failed:" << task->name()
                                     << "- Error:" << item->errorMessage();
    emit newLog(i18n("Task '%1' failed: %2", task->name(), item->errorMessage()));

    item->setStatus(QueueItem::FAILED);
    Q_EMIT itemFailed(item, item->errorMessage());

    clearCurrentExecution();

    // Stop queue execution on failure
    abort();
}

void QueueExecutor::handleActionFailure(TaskAction *action)
{
    if (!action || !m_currentItem)
        return;

    Task *task = taskForItem(m_currentItem, i18n("handle an action failure"));
    if (!task)
        return;

    // Check failure action policy
    switch (action->failureAction())
    {
        case TaskAction::ABORT_QUEUE:
            qCWarning(KSTARS_EKOS_SCHEDULER) << "Failure action: Aborting entire queue";
            // Abort entire queue
            m_currentItem->setErrorMessage(action->errorMessage());
            handleItemFailure(m_currentItem);
            break;

        case TaskAction::CONTINUE:
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Failure action: Continuing to next action";
            // Log error and continue to next action
            m_currentActionIndex++;
            if (m_currentActionIndex < task->actions().size())
            {
                m_currentAction = task->actions().at(m_currentActionIndex);
                executeAction(m_currentAction);
            }
            else
            {
                // No more actions, complete item despite error
                handleItemCompletion(m_currentItem);
            }
            break;

        case TaskAction::FailureAction::SKIP_TO_NEXT_TASK:
            qCInfo(KSTARS_EKOS_SCHEDULER) << "Failure action: Skipping to next task";
            // Skip remaining actions, move to next item
            m_currentItem->setStatus(QueueItem::SKIPPED);
            clearCurrentExecution();
            executeNext();
            break;
    }
}

void QueueExecutor::updateProgress()
{
    Task *task = taskForItem(m_currentItem, i18n("update progress"));
    if (!task)
        return;

    if (task->actionCount() == 0)
        return;

    // Calculate progress based on completed actions
    int progress = (m_currentActionIndex * 100) / task->actionCount();
    m_currentItem->setProgress(progress);
}

} // namespace Ekos
