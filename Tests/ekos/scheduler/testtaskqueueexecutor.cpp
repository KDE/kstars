/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for scheduler task queue execution hardening.
 */

#include "ekos/scheduler/taskqueue/queue/queueexecutor.h"
#include "ekos/scheduler/taskqueue/queue/queueitem.h"
#include "ekos/scheduler/taskqueue/queue/queuemanager.h"
#include "ekos/scheduler/taskqueue/task.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QSignalSpy>
#include <QtTest/QTest>
#else
#include <QSignalSpy>
#include <QTest>
#endif

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

class TestTaskQueueExecutor : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void queueItemRejectsJsonWithoutTask();
        void queueManagerSkipsJsonItemsWithoutTask();
        void executorRejectsTasklessItemsBeforeStart();

    private:
        QJsonObject tasklessItemJson(const QString &id) const;
        QJsonObject validDelayItemJson(const QString &id) const;
};

#include "testtaskqueueexecutor.moc"

QJsonObject TestTaskQueueExecutor::tasklessItemJson(const QString &id) const
{
    QJsonObject json;
    json["id"] = id;
    json["status"] = static_cast<int>(Ekos::QueueItem::PENDING);
    json["created_time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    json["progress"] = 0;
    return json;
}

QJsonObject TestTaskQueueExecutor::validDelayItemJson(const QString &id) const
{
    QJsonObject action;
    action["type"] = "DELAY";
    action["duration"] = 0;
    action["unit"] = 0;
    action["timeout"] = 1;
    action["retries"] = 0;
    action["failure_action"] = static_cast<int>(Ekos::TaskAction::ABORT_QUEUE);

    QJsonArray actions;
    actions.append(action);

    QJsonObject task;
    task["name"] = "Test delay";
    task["template_id"] = "delay";
    task["device"] = "";
    task["category"] = "Test";
    task["status"] = static_cast<int>(Ekos::Task::PENDING);
    task["parameters"] = QJsonObject();
    task["actions"] = actions;

    QJsonObject item = tasklessItemJson(id);
    item["task"] = task;
    return item;
}

void TestTaskQueueExecutor::queueItemRejectsJsonWithoutTask()
{
    Ekos::QueueItem item;

    QVERIFY(!item.loadFromJson(tasklessItemJson("missing-task")));
    QVERIFY(item.task() == nullptr);
}

void TestTaskQueueExecutor::queueManagerSkipsJsonItemsWithoutTask()
{
    QJsonArray items;
    items.append(tasklessItemJson("missing-task"));
    items.append(validDelayItemJson("valid-delay"));

    QJsonObject queue;
    queue["state"] = static_cast<int>(Ekos::QueueManager::IDLE);
    queue["items"] = items;

    Ekos::QueueManager manager;
    QVERIFY(manager.fromJson(queue));
    QCOMPARE(manager.count(), 1);
    QVERIFY(manager.itemAt(0) != nullptr);
    QCOMPARE(manager.itemAt(0)->id(), QString("valid-delay"));
    QVERIFY(manager.itemAt(0)->task() != nullptr);
    QVERIFY(manager.currentItem() == nullptr);
}

void TestTaskQueueExecutor::executorRejectsTasklessItemsBeforeStart()
{
    Ekos::QueueManager manager;
    auto *item = new Ekos::QueueItem(&manager);
    manager.addItem(item);

    Ekos::QueueExecutor executor(&manager);
    QSignalSpy logSpy(&executor, &Ekos::QueueExecutor::newLog);

    QVERIFY(!executor.start());
    QVERIFY(!executor.isRunning());
    QCOMPARE(manager.state(), Ekos::QueueManager::IDLE);
    QCOMPARE(item->status(), Ekos::QueueItem::PENDING);
    QVERIFY(!logSpy.isEmpty());
}

QTEST_GUILESS_MAIN(TestTaskQueueExecutor)
