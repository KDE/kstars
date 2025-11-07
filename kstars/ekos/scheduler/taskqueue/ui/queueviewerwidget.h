/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "../queue/queuemanager.h"
#include "../queue/queueexecutor.h"

#include <QWidget>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QStatusBar>

namespace Ekos
{

class TemplateLibraryWidget;
class DeviceSelectionDialog;
class ParameterCustomizationDialog;
class TaskTemplate;

/**
 * @class QueueViewerWidget
 * @brief Widget for viewing and controlling the task queue
 *
 * Displays queue items with their actions, status, and progress.
 * Provides controls for starting, pausing, and managing the queue.
 */
class QueueViewerWidget : public QWidget
{
        Q_OBJECT

    public:
        explicit QueueViewerWidget(QWidget *parent = nullptr);
        ~QueueViewerWidget() override;

        /** @brief Set the queue manager to display */
        void setQueueManager(QueueManager *manager);

        /** @brief Set the queue executor */
        void setQueueExecutor(QueueExecutor *executor);

    public slots:
        /** @brief Refresh the queue display */
        void refreshQueue();

    private slots:
        // Template and task management
        void onAddFromTemplate();
        void onTemplateSelected(TaskTemplate *tmpl);

        // Queue controls
        void onStartQueue();
        void onPauseQueue();
        void onStopQueue();
        void onClearQueue();

        // Queue persistence
        void onSaveQueue();
        void onLoadQueue();

        // Collections
        void onLoadCollection();

        // Item management
        void onRemoveItem();
        void onMoveUp();
        void onMoveDown();

        // Queue manager signals
        void onItemAdded(QueueItem *item, int index);
        void onItemRemoved(QueueItem *item, int index);
        void onItemMoved(int fromIndex, int toIndex);
        void onQueueCleared();
        void onStateChanged(QueueManager::QueueState state);

        // Executor signals
        void onExecutorStarted();
        void onExecutorPaused();
        void onExecutorStopped();
        void onExecutorCompleted();
        void onItemStarted(QueueItem *item);
        void onItemCompleted(QueueItem *item);
        void onItemFailed(QueueItem *item, const QString &error);
        void onProgress(int percent, const QString &message);

        // UI updates
        void onSelectionChanged();

    private:
        void setupUI();
        void setupConnections();
        void updateControls();
        void updateItemRow(int row, QueueItem *item);
        QString statusToString(QueueItem::Status status) const;
        QIcon statusToIcon(QueueItem::Status status) const;

        // Collection loading helpers
        void loadCollectionFile(const QString &filePath);
        QStringList findDevicesByInterface(uint32_t interfaceMask);

        // UI Components
        QTableWidget *m_queueTable = nullptr;
        QPushButton *m_startButton = nullptr;
        QPushButton *m_pauseButton = nullptr;
        QPushButton *m_stopButton = nullptr;
        QPushButton *m_clearButton = nullptr;
        QPushButton *m_removeButton = nullptr;
        QPushButton *m_moveUpButton = nullptr;
        QPushButton *m_moveDownButton = nullptr;
        QPushButton *m_addTemplateButton = nullptr;
        QPushButton *m_saveQueueButton = nullptr;
        QPushButton *m_loadQueueButton = nullptr;
        QPushButton *m_collectionsButton = nullptr;

        // Status bar components
        QStatusBar *m_statusBar = nullptr;
        QLabel *m_statusLabel = nullptr;
        QProgressBar *m_progressBar = nullptr;
        QLabel *m_statsLabel = nullptr;

        // Backend
        QueueManager *m_manager = nullptr;
        TemplateLibraryWidget *m_templateLibrary = nullptr;
        QueueExecutor *m_executor = nullptr;

        // Last used queue file path for save/load operations
        QString m_lastQueueFilePath;
};

} // namespace Ekos
