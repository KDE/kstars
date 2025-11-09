/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "queueexecutor.h"
#include "../actions/taskaction.h"
#include "../templatemanager.h"
#include "ekos/manager.h"
#include "indi/indilistener.h"
#include <ekos_scheduler_debug.h>

namespace Ekos
{

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

    // Check if any task requires devices by looking at template interfaces
    bool requiresDevices = false;
    TemplateManager *templateMgr = TemplateManager::Instance();

    for (QueueItem *item : m_manager->items())
    {
        if (item && item->task())
        {
            // Check if task has explicit device OR template requires interfaces
            if (!item->device().isEmpty())
            {
                requiresDevices = true;
                break;
            }

            // Check template for interface requirements
            if (templateMgr)
            {
                TaskTemplate *tmpl = templateMgr->getTemplate(item->task()->templateId());
                if (tmpl && !tmpl->supportedInterfaces().isEmpty())
                {
                    requiresDevices = true;
                    break;
                }
            }
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

    emit newLog(i18n("Starting task queue with %1 items", m_manager->count()));

    m_manager->setState(QueueManager::RUNNING);
    emit started();

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
    emit paused();
}

void QueueExecutor::resume()
{
    if (!m_running || !m_paused)
        return;

    m_paused = false;
    m_manager->setState(QueueManager::RUNNING);
    emit resumed();

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
    m_currentItem = nullptr;
    m_currentAction = nullptr;
    m_currentActionIndex = -1;

    m_manager->setState(QueueManager::IDLE);
    emit stopped();
}

void QueueExecutor::abort()
{
    if (!m_running)
        return;

    m_abortRequested = true;

    // Stop current action if running
    if (m_currentAction && m_currentAction->status() == TaskAction::Status::RUNNING)
    {
        // Action will handle abort in its own way
        // We'll catch it in onActionStatusChanged
    }

    m_manager->setState(QueueManager::ABORTED);
    emit aborted();

    stop();
}

void QueueExecutor::executeNext()
{
    if (!m_running || m_paused || m_abortRequested)
        return;

    // Find next pending item
    QueueItem *nextItem = nullptr;
    for (QueueItem *item : m_manager->items())
    {
        if (item->status() == QueueItem::PENDING ||
                item->status() == QueueItem::SCHEDULED)
        {
            nextItem = item;
            break;
        }
    }

    if (!nextItem)
    {
        // Queue completed
        m_running = false;
        m_manager->setState(QueueManager::COMPLETED);
        emit completed();
        return;
    }

    executeItem(nextItem);
}

void QueueExecutor::executeItem(QueueItem *item)
{
    if (!item || !item->task())
        return;

    m_currentItem = item;
    m_currentActionIndex = 0;
    m_manager->setCurrentItem(item);

    // Connect to item signals
    connect(item, &QueueItem::statusChanged,
            this, &QueueExecutor::onItemStatusChanged);

    // Check if device needs to be assigned at runtime
    Task *task = item->task();
    if (task->device().isEmpty())
    {
        // Get template to find required device interface
        TemplateManager *templateMgr = TemplateManager::Instance();
        if (templateMgr)
        {
            TaskTemplate *tmpl = templateMgr->getTemplate(task->templateId());
            if (tmpl && !tmpl->supportedInterfaces().isEmpty())
            {
                // Combine all supported interfaces using BIT-OR
                uint32_t requiredInterfaces = 0;
                for (int interface : tmpl->supportedInterfaces())
                {
                    requiredInterfaces |= static_cast<uint32_t>(interface);
                }

                // Get ALL devices and check if they have all required interfaces
                auto allDevices = INDIListener::devices();
                QString assignedDevice;

                qCInfo(KSTARS_EKOS_SCHEDULER) << "Searching for device with required interfaces:" << requiredInterfaces;

                for (const auto &device : allDevices)
                {
                    if (device && device->isConnected())
                    {
                        uint32_t deviceInterfaces = device->getDriverInterface();
                        qCInfo(KSTARS_EKOS_SCHEDULER) << "  Checking device:" << device->getDeviceName()
                                                      << "interfaces:" << deviceInterfaces
                                                      << "(" << QString::number(deviceInterfaces, 2) << ")";

                        // Check if device has ALL required interfaces using BIT-AND
                        if ((deviceInterfaces & requiredInterfaces) == requiredInterfaces)
                        {
                            assignedDevice = device->getDeviceName();
                            qCInfo(KSTARS_EKOS_SCHEDULER) << "Found matching device:" << assignedDevice
                                                          << "with interfaces:" << deviceInterfaces
                                                          << "required:" << requiredInterfaces;
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
                    QString errorMsg = i18n("No connected device found with required interfaces");
                    qCWarning(KSTARS_EKOS_SCHEDULER) << errorMsg << QString::number(requiredInterfaces, 2) << "for task:" << task->name();

                    // Handle based on task's device mapping failure action
                    switch (task->deviceMappingFailureAction())
                    {
                        case TaskAction::SKIP_TO_NEXT_TASK:
                            qCInfo(KSTARS_EKOS_SCHEDULER) << "Skipping task due to missing device (failure_action: skip_to_next_task)";
                            item->setStatus(QueueItem::SKIPPED);
                            item->setErrorMessage(errorMsg);
                            // Don't emit itemFailed for skipped tasks - just move to next
                            m_currentItem = nullptr;
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
                            emit itemFailed(item, errorMsg);
                            m_currentItem = nullptr;
                            executeNext();
                            return;
                    }
                }
            }
        }
    }

    item->setStatus(QueueItem::RUNNING);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Starting task:" << item->task()->name()
                                  << "with" << item->task()->actions().size() << "actions";

    emit itemStarted(item);

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

void QueueExecutor::executeAction(TaskAction *action)
{
    if (!action || m_paused || m_abortRequested)
        return;

    m_currentAction = action;

    // Connect to action signals
    connect(action, &TaskAction::statusChanged,
            this, &QueueExecutor::onActionStatusChanged);
    connect(action, &TaskAction::progress,
            this, &QueueExecutor::onActionProgress);

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Executing action" << (m_currentActionIndex + 1)
                                  << "of" << m_currentItem->task()->actions().size()
                                  << "- Type:" << action->type();

    emit actionStarted(action);

    // Reset retry counter before first execution
    action->resetRetryCounter();

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

    TaskAction *action = qobject_cast<TaskAction *>(sender());
    if (!action || action != m_currentAction)
        return;

    switch (status)
    {
        case TaskAction::Status::COMPLETED:
            // Disconnect signals - action is done
            disconnect(action, nullptr, this, nullptr);

            qCInfo(KSTARS_EKOS_SCHEDULER) << "Action" << (m_currentActionIndex + 1) << "completed successfully";
            emit actionCompleted(action);
            updateProgress();

            // Move to next action
            m_currentActionIndex++;
            if (m_currentActionIndex < m_currentItem->task()->actions().size())
            {
                m_currentAction = m_currentItem->task()->actions().at(m_currentActionIndex);
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
            emit actionFailed(action, action->errorMessage());
            handleActionFailure(action);
            break;

        case TaskAction::Status::ABORTED:
            // Disconnect signals - action is done
            disconnect(action, nullptr, this, nullptr);

            qCWarning(KSTARS_EKOS_SCHEDULER) << "Action" << (m_currentActionIndex + 1) << "aborted";
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
    emit progress(m_currentItem->progress(), message);
}

void QueueExecutor::handleItemCompletion(QueueItem *item)
{
    if (!item)
        return;

    qCInfo(KSTARS_EKOS_SCHEDULER) << "Task completed:" << item->task()->name();

    // Set progress to 100% on completion
    item->setProgress(100);

    item->setStatus(QueueItem::COMPLETED);
    emit itemCompleted(item);

    m_currentItem = nullptr;
    m_currentAction = nullptr;
    m_currentActionIndex = -1;

    // Continue to next item
    executeNext();
}

void QueueExecutor::handleItemFailure(QueueItem *item)
{
    if (!item)
        return;

    qCWarning(KSTARS_EKOS_SCHEDULER) << "Task failed:" << item->task()->name()
                                     << "- Error:" << item->errorMessage();

    item->setStatus(QueueItem::FAILED);
    emit itemFailed(item, item->errorMessage());

    m_currentItem = nullptr;
    m_currentAction = nullptr;
    m_currentActionIndex = -1;

    // Stop queue execution on failure
    abort();
}

void QueueExecutor::handleActionFailure(TaskAction *action)
{
    if (!action || !m_currentItem)
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
            if (m_currentActionIndex < m_currentItem->task()->actions().size())
            {
                m_currentAction = m_currentItem->task()->actions().at(m_currentActionIndex);
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
            m_currentItem = nullptr;
            m_currentAction = nullptr;
            m_currentActionIndex = -1;
            executeNext();
            break;
    }
}

void QueueExecutor::updateProgress()
{
    if (!m_currentItem || !m_currentItem->task())
        return;

    Task *task = m_currentItem->task();
    if (task->actionCount() == 0)
        return;

    // Calculate progress based on completed actions
    int progress = (m_currentActionIndex * 100) / task->actionCount();
    m_currentItem->setProgress(progress);
}

} // namespace Ekos
