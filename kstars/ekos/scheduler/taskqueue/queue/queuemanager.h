/*
    SPDX-FileCopyrightText: 2025 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "queueitem.h"

#include <QObject>
#include <QString>
#include <QList>

namespace Ekos
{

/**
 * @class QueueManager
 * @brief Manages the task queue collection
 *
 * QueueManager is responsible for:
 * - Adding/removing/reordering queue items
 * - Queue state management
 * - Queue persistence (save/load)
 * - Item filtering and selection
 */
class QueueManager : public QObject
{
        Q_OBJECT

    public:
        enum QueueState
        {
            IDLE,         ///< Queue not running
            RUNNING,      ///< Queue executing
            PAUSED,       ///< Queue paused
            COMPLETED,    ///< All items completed
            ABORTED       ///< Queue aborted due to error
        };
        Q_ENUM(QueueState)

        explicit QueueManager(QObject *parent = nullptr);
        ~QueueManager() override;

        // Queue management
        void addItem(QueueItem *item);
        void insertItem(int index, QueueItem *item);
        bool removeItem(int index);
        bool removeItem(QueueItem *item);
        void clear();

        // Item reordering
        bool moveItem(int fromIndex, int toIndex);
        bool moveUp(int index);
        bool moveDown(int index);

        // Queue access
        const QList<QueueItem *> &items() const
        {
            return m_items;
        }

        int count() const
        {
            return m_items.size();
        }

        QueueItem *itemAt(int index) const;
        int indexOf(QueueItem *item) const;

        // State management
        QueueState state() const
        {
            return m_state;
        }
        void setState(QueueState state);

        // Current execution
        QueueItem *currentItem() const
        {
            return m_currentItem;
        }
        void setCurrentItem(QueueItem *item);
        int currentIndex() const;

        // Persistence
        bool saveQueue(const QString &filePath);
        bool loadQueue(const QString &filePath);
        QJsonObject toJson() const;
        bool fromJson(const QJsonObject &json);

        // Statistics
        int completedCount() const;
        int failedCount() const;
        int pendingCount() const;

        // Programmatic task creation for mount parking between jobs
        /**
         * @brief Creates and adds a mount park task programmatically
         * @param timeout Maximum time to wait for parking (seconds)
         * @return true if task was created and added successfully
         */
        bool addMountParkTask(int timeout = 60);

        /**
         * @brief Creates and adds a mount unpark task programmatically
         * @param timeout Maximum time to wait for unparking (seconds)
         * @return true if task was created and added successfully
         */
        bool addMountUnparkTask(int timeout = 60);

    signals:
        void itemAdded(QueueItem *item, int index);
        void itemRemoved(QueueItem *item, int index);
        void itemMoved(int fromIndex, int toIndex);
        void queueCleared();
        void stateChanged(QueueState newState);
        void currentItemChanged(QueueItem *item);

    private:
        QList<QueueItem *> m_items;
        QueueItem *m_currentItem = nullptr;
        QueueState m_state = IDLE;

        // Helper method to create tasks from templates
        Task *createTaskFromTemplate(const QString &templateId, const QMap<QString, QVariant> &parameters);

        // Helper method to load collection files (with "tasks" array)
        bool loadCollectionFromJson(const QJsonObject &json);
};

} // namespace Ekos
