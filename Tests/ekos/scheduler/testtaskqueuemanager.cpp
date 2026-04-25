/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for scheduler task queue manager state handling.
 */

#include "ekos/scheduler/taskqueue/actions/taskaction.h"
#include "ekos/scheduler/taskqueue/queue/queuemanager.h"
#include "ekos/scheduler/taskqueue/task.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryFile>

class TestTaskQueueManager : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void removingCurrentItemClearsCurrentIndex();
        void pausedExecutionBlocksQueueMutation();
        void fromJsonRejectsWhileExecutionLocked_data();
        void fromJsonRejectsWhileExecutionLocked();
        void loadCollectionRejectsWhileExecutionLocked_data();
        void loadCollectionRejectsWhileExecutionLocked();
        void setCurrentItemRejectsForeignItems();
        void invalidLoadedStateFallsBackToIdle();
        void statisticsReflectQueueItemStatus();

    private:
        Ekos::Task *createTask(const QString &name, const QString &device = QString()) const;
        Ekos::QueueItem *createItem(const QString &name, const QString &device = QString()) const;
        QJsonObject validDelayItemJson(const QString &id) const;
};

#include "testtaskqueuemanager.moc"

Ekos::Task *TestTaskQueueManager::createTask(const QString &name, const QString &device) const
{
    auto *task = new Ekos::Task;
    task->setName(name);
    task->setDevice(device);
    return task;
}

Ekos::QueueItem *TestTaskQueueManager::createItem(const QString &name, const QString &device) const
{
    return new Ekos::QueueItem(createTask(name, device));
}

QJsonObject TestTaskQueueManager::validDelayItemJson(const QString &id) const
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

    QJsonObject item;
    item["id"] = id;
    item["status"] = static_cast<int>(Ekos::QueueItem::PENDING);
    item["created_time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    item["progress"] = 0;
    item["task"] = task;
    return item;
}

void TestTaskQueueManager::removingCurrentItemClearsCurrentIndex()
{
    Ekos::QueueManager manager;
    auto *first = createItem("First task");
    auto *second = createItem("Second task");

    manager.addItem(first);
    manager.addItem(second);
    manager.setCurrentItem(first);

    QVERIFY(manager.removeItem(0));
    QCOMPARE(manager.count(), 1);
    QVERIFY(manager.currentItem() == nullptr);
    QCOMPARE(manager.currentIndex(), -1);
    QCOMPARE(manager.itemAt(0), second);
    QVERIFY(!manager.toJson().contains("current_index"));
}

void TestTaskQueueManager::pausedExecutionBlocksQueueMutation()
{
    Ekos::QueueManager manager;
    auto *first = createItem("First task");
    auto *second = createItem("Second task");

    manager.addItem(first);
    manager.addItem(second);
    manager.setCurrentItem(first);
    first->setStatus(Ekos::QueueItem::COMPLETED);
    manager.setState(Ekos::QueueManager::PAUSED);

    manager.clear();
    QCOMPARE(manager.count(), 2);
    QVERIFY(manager.currentItem() == first);

    QVERIFY(!manager.removeItem(0));
    QCOMPARE(manager.count(), 2);
    QVERIFY(manager.currentItem() == first);

    QVERIFY(!manager.moveItem(0, 1));
    QCOMPARE(manager.itemAt(0), first);
    QCOMPARE(manager.itemAt(1), second);

    manager.resetAllItems();
    QCOMPARE(first->status(), Ekos::QueueItem::COMPLETED);
    QVERIFY(manager.currentItem() == first);
}

void TestTaskQueueManager::fromJsonRejectsWhileExecutionLocked_data()
{
    QTest::addColumn<int>("state");

    QTest::newRow("running") << static_cast<int>(Ekos::QueueManager::RUNNING);
    QTest::newRow("paused") << static_cast<int>(Ekos::QueueManager::PAUSED);
}

void TestTaskQueueManager::fromJsonRejectsWhileExecutionLocked()
{
    QFETCH(int, state);

    Ekos::QueueManager manager;
    auto *existing = createItem("Existing task");
    manager.addItem(existing);
    manager.setCurrentItem(existing);
    manager.setState(static_cast<Ekos::QueueManager::QueueState>(state));

    QJsonObject queue;
    QJsonArray items;
    items.append(validDelayItemJson("replacement"));
    queue["state"] = static_cast<int>(Ekos::QueueManager::IDLE);
    queue["current_index"] = 0;
    queue["items"] = items;

    QVERIFY(!manager.fromJson(queue));
    QCOMPARE(manager.count(), 1);
    QCOMPARE(manager.itemAt(0), existing);
    QVERIFY(manager.currentItem() == existing);
    QCOMPARE(manager.currentIndex(), 0);
    QCOMPARE(manager.state(), static_cast<Ekos::QueueManager::QueueState>(state));
}

void TestTaskQueueManager::loadCollectionRejectsWhileExecutionLocked_data()
{
    QTest::addColumn<int>("state");

    QTest::newRow("running") << static_cast<int>(Ekos::QueueManager::RUNNING);
    QTest::newRow("paused") << static_cast<int>(Ekos::QueueManager::PAUSED);
}

void TestTaskQueueManager::loadCollectionRejectsWhileExecutionLocked()
{
    QFETCH(int, state);

    Ekos::QueueManager manager;
    auto *existing = createItem("Existing task");
    manager.addItem(existing);
    manager.setCurrentItem(existing);
    manager.setState(static_cast<Ekos::QueueManager::QueueState>(state));

    QTemporaryFile file;
    QVERIFY(file.open());

    QJsonObject collection;
    collection["tasks"] = QJsonArray();
    file.write(QJsonDocument(collection).toJson(QJsonDocument::Compact));
    file.close();

    QVERIFY(!manager.loadQueue(file.fileName()));
    QCOMPARE(manager.count(), 1);
    QCOMPARE(manager.itemAt(0), existing);
    QVERIFY(manager.currentItem() == existing);
    QCOMPARE(manager.currentIndex(), 0);
    QCOMPARE(manager.state(), static_cast<Ekos::QueueManager::QueueState>(state));
}

void TestTaskQueueManager::setCurrentItemRejectsForeignItems()
{
    Ekos::QueueManager manager;
    auto *queued = createItem("Queued task");
    auto *foreign = createItem("Foreign task");

    manager.addItem(queued);
    manager.setCurrentItem(queued);
    manager.setCurrentItem(foreign);

    QVERIFY(manager.currentItem() == queued);
    QCOMPARE(manager.currentIndex(), 0);

    delete foreign;
}

void TestTaskQueueManager::invalidLoadedStateFallsBackToIdle()
{
    Ekos::QueueManager manager;
    QJsonObject queue;
    QJsonArray items;
    items.append(validDelayItemJson("valid-delay"));
    queue["state"] = 99;
    queue["current_index"] = 0;
    queue["items"] = items;

    QVERIFY(manager.fromJson(queue));
    QCOMPARE(manager.state(), Ekos::QueueManager::IDLE);
    QCOMPARE(manager.count(), 1);
    QVERIFY(manager.currentItem() == manager.itemAt(0));
    QCOMPARE(manager.currentIndex(), 0);
}

void TestTaskQueueManager::statisticsReflectQueueItemStatus()
{
    Ekos::QueueManager manager;
    auto *completed = createItem("Completed task");
    auto *failed = createItem("Failed task");
    auto *pending = createItem("Pending task");
    auto *scheduled = createItem("Scheduled task");

    manager.addItem(completed);
    manager.addItem(failed);
    manager.addItem(pending);
    manager.addItem(scheduled);

    completed->setStatus(Ekos::QueueItem::COMPLETED);
    failed->setStatus(Ekos::QueueItem::FAILED);
    scheduled->setStatus(Ekos::QueueItem::SCHEDULED);

    QCOMPARE(manager.completedCount(), 1);
    QCOMPARE(manager.failedCount(), 1);
    QCOMPARE(manager.pendingCount(), 2);
}

QTEST_GUILESS_MAIN(TestTaskQueueManager)
