/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "queuemanager.h"
#include "../actions/taskaction.h"

#include <QObject>
#include <QTimer>

namespace Ekos
{

/**
 * @class QueueExecutor
 * @brief Executes queue items sequentially
 *
 * QueueExecutor is responsible for:
 * - Sequential task execution
 * - Action-level execution control
 * - Retry handling
 * - FailureAction enforcement
 * - Real-time progress monitoring
 */
class QueueExecutor : public QObject
{
        Q_OBJECT

    public:
        explicit QueueExecutor(QueueManager *manager, QObject *parent = nullptr);
        ~QueueExecutor() override;

        // Execution control
        bool start();
        void pause();
        void resume();
        void stop();
        void abort();

        // Current execution state
        bool isRunning() const
        {
            return m_running;
        }
        bool isPaused() const
        {
            return m_paused;
        }

        QueueItem *currentItem() const
        {
            return m_currentItem;
        }
        TaskAction *currentAction() const
        {
            return m_currentAction;
        }

    signals:
        void started();
        void paused();
        void resumed();
        void stopped();
        void aborted();
        void completed();

        void itemStarted(QueueItem *item);
        void itemCompleted(QueueItem *item);
        void itemFailed(QueueItem *item, const QString &error);

        void actionStarted(TaskAction *action);
        void actionCompleted(TaskAction *action);
        void actionFailed(TaskAction *action, const QString &error);

        void progress(int percent, const QString &message);
        void newLog(const QString &message);

    private slots:
        void executeNext();
        void onItemStatusChanged(QueueItem::Status status);
        void onActionStatusChanged(TaskAction::Status status);
        void onActionProgress(const QString &message);

    private:
        void executeItem(QueueItem *item);
        void executeAction(TaskAction *action);
        void handleItemCompletion(QueueItem *item);
        void handleItemFailure(QueueItem *item);
        void handleActionFailure(TaskAction *action);
        void updateProgress();

        QueueManager *m_manager = nullptr;
        QueueItem *m_currentItem = nullptr;
        TaskAction *m_currentAction = nullptr;
        int m_currentActionIndex = -1;

        bool m_running = false;
        bool m_paused = false;
        bool m_abortRequested = false;
};

} // namespace Ekos
