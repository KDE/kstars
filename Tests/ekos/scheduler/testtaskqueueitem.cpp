/*
    SPDX-FileCopyrightText: 2026 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * This file contains unit tests for scheduler task queue item ownership and loading.
 */

#include "ekos/scheduler/taskqueue/queue/queueitem.h"
#include "ekos/scheduler/taskqueue/task.h"

#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtTest/QTest>
#else
#include <QTest>
#endif

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>

class TestTaskQueueItem : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void setTaskIgnoresSelfAssignment();
        void taskPointerClearsWhenTaskIsDestroyed();
        void loadFromJsonRejectsMissingTask();
        void failedLoadFromJsonKeepsExistingItem();
        void tasklessJsonRemainsRejected();

    private:
        Ekos::Task *createTask(const QString &name, const QString &device = QString()) const;
        QJsonObject tasklessItemJson(const QString &id) const;
        QJsonObject invalidTaskItemJson(const QString &id) const;
};

#include "testtaskqueueitem.moc"

Ekos::Task *TestTaskQueueItem::createTask(const QString &name, const QString &device) const
{
    auto *task = new Ekos::Task;
    task->setName(name);
    task->setDevice(device);
    return task;
}

QJsonObject TestTaskQueueItem::tasklessItemJson(const QString &id) const
{
    QJsonObject json;
    json["id"] = id;
    json["status"] = static_cast<int>(Ekos::QueueItem::PENDING);
    json["created_time"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    json["progress"] = 0;
    return json;
}

QJsonObject TestTaskQueueItem::invalidTaskItemJson(const QString &id) const
{
    QJsonObject task;
    task["name"] = "Invalid task";
    task["template_id"] = "invalid";
    task["device"] = "";
    task["category"] = "Test";
    task["status"] = static_cast<int>(Ekos::Task::PENDING);
    task["parameters"] = QJsonObject();
    task["actions"] = QJsonArray();

    QJsonObject item = tasklessItemJson(id);
    item["task"] = task;
    return item;
}

void TestTaskQueueItem::setTaskIgnoresSelfAssignment()
{
    Ekos::QueueItem item(createTask("Original task", "Camera"));
    Ekos::Task *task = item.task();

    item.setTask(task);

    QVERIFY(item.task() == task);
    QCOMPARE(item.name(), QString("Original task"));
    QCOMPARE(item.device(), QString("Camera"));
}

void TestTaskQueueItem::taskPointerClearsWhenTaskIsDestroyed()
{
    auto *task = createTask("Externally destroyed task", "Mount");
    Ekos::QueueItem item(task);

    QVERIFY(item.task() == task);
    delete task;

    QVERIFY(item.task() == nullptr);
    QCOMPARE(item.name(), QString());
    QCOMPARE(item.device(), QString());
    QVERIFY(!item.toJson().contains("task"));
}

void TestTaskQueueItem::loadFromJsonRejectsMissingTask()
{
    Ekos::QueueItem item;

    QVERIFY(!item.loadFromJson(tasklessItemJson("missing-task")));
    QVERIFY(item.task() == nullptr);
}

void TestTaskQueueItem::failedLoadFromJsonKeepsExistingItem()
{
    Ekos::QueueItem item(createTask("Original task", "Dome"));
    Ekos::Task *task = item.task();
    const QDateTime scheduledTime = QDateTime::currentDateTimeUtc().addSecs(300);

    item.setId("original-id");
    item.setScheduledTime(scheduledTime);
    item.setStatus(Ekos::QueueItem::RUNNING);
    item.setProgress(42);
    item.setErrorMessage("keep this error");

    QVERIFY(!item.loadFromJson(invalidTaskItemJson("replacement-id")));

    QCOMPARE(item.id(), QString("original-id"));
    QVERIFY(item.task() == task);
    QCOMPARE(item.name(), QString("Original task"));
    QCOMPARE(item.device(), QString("Dome"));
    QCOMPARE(item.status(), Ekos::QueueItem::RUNNING);
    QCOMPARE(item.progress(), 42);
    QCOMPARE(item.errorMessage(), QString("keep this error"));
    QCOMPARE(item.scheduledTime(), scheduledTime);
}

void TestTaskQueueItem::tasklessJsonRemainsRejected()
{
    Ekos::QueueItem source;
    QJsonObject json = source.toJson();

    QVERIFY(!json.contains("task"));

    Ekos::QueueItem target;
    QVERIFY(!target.loadFromJson(json));
    QVERIFY(target.task() == nullptr);
}

QTEST_GUILESS_MAIN(TestTaskQueueItem)
