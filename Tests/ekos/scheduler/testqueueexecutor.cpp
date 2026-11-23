/*
    SPDX-FileCopyrightText: 2026 KStars contributors

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/scheduler/taskqueue/queue/queueexecutor.h"
#include "ekos/scheduler/taskqueue/actions/scriptaction.h"
#include "ekos/scheduler/taskqueue/templatemanager.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

using namespace Ekos;

class TestQueueExecutor : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void cleanup();
        void testLateProgressAfterStop();
        void testStopCancelsAction_data();
        void testStopCancelsAction();
        void testRestartIgnoresPreviousAction();
        void testStopFromActionStarted();
        void testStopFromActionCompleted();
        void testNormalCompletion();
        void testScriptAbortDoesNotRetry();
        void testStopBeforeScriptLaunch();

    private:
        static QueueItem *addTask(QueueManager &manager, const QJsonArray &actions);
        static QJsonObject delay(int seconds = 600);
};

QueueItem *TestQueueExecutor::addTask(QueueManager &manager, const QJsonArray &actions)
{
    auto *task = new Task;
    const QJsonObject json
    {
        {"name", "Cancellation regression"},
        {"template_id", "queue_executor_regression"},
        {"actions", actions}
    };
    if (!task->loadFromJson(json))
    {
        delete task;
        return nullptr;
    }
    auto *item = new QueueItem(task);
    manager.addItem(item);
    return item;
}

QJsonObject TestQueueExecutor::delay(int seconds)
{
    return {{"type", "DELAY"}, {"duration", seconds}, {"unit", 0}};
}

void TestQueueExecutor::cleanup()
{
    TemplateManager::release();
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void TestQueueExecutor::testLateProgressAfterStop()
{
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {delay()});
    QVERIFY(item);
    auto *action = item->task()->actions().first();
    QSignalSpy progress(&executor, &QueueExecutor::progress);
    QVERIFY(executor.start());
    executor.stop();
    progress.clear();

    // The original executor still receives this callback and dereferences its cleared item.
    Q_EMIT action->progress("Buffered output delivered after Stop");
    QCoreApplication::processEvents();
    QCOMPARE(progress.count(), 0);
    QVERIFY(!executor.isRunning());
    QVERIFY(!executor.currentItem());
}

void TestQueueExecutor::testStopCancelsAction_data()
{
    QTest::addColumn<bool>("paused");
    QTest::newRow("running") << false;
    QTest::newRow("paused") << true;
}

void TestQueueExecutor::testStopCancelsAction()
{
    QFETCH(bool, paused);
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {delay(), delay()});
    auto *pending = addTask(manager, {delay()});
    QVERIFY(item);
    QVERIFY(pending);
    QSignalSpy stopped(&executor, &QueueExecutor::stopped);
    QSignalSpy started(&executor, &QueueExecutor::actionStarted);
    QVERIFY(executor.start());
    auto *action = executor.currentAction();
    QVERIFY(action);
    if (paused)
        executor.pause();
    executor.stop();
    executor.stop();

    QCOMPARE(action->status(), TaskAction::ABORTED);
    QCOMPARE(item->status(), QueueItem::ABORTED);
    QCOMPARE(pending->status(), QueueItem::PENDING);
    QCOMPARE(manager.state(), QueueManager::IDLE);
    QVERIFY(!manager.currentItem());
    QVERIFY(!executor.currentItem());
    QVERIFY(!executor.currentAction());
    QVERIFY(!executor.isPaused());
    QCOMPARE(started.count(), 1);
    QCOMPARE(stopped.count(), 1);
}

void TestQueueExecutor::testRestartIgnoresPreviousAction()
{
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *first = addTask(manager, {delay()});
    auto *second = addTask(manager, {delay()});
    QVERIFY(first);
    QVERIFY(second);
    auto *oldAction = first->task()->actions().first();
    QVERIFY(executor.start());
    executor.stop();
    QVERIFY(executor.start());
    QCOMPARE(executor.currentItem(), second);
    QSignalSpy progress(&executor, &QueueExecutor::progress);
    QSignalSpy completed(&executor, &QueueExecutor::actionCompleted);

    Q_EMIT oldAction->progress("Stale output from previous item");
    Q_EMIT oldAction->statusChanged(TaskAction::COMPLETED);
    QCoreApplication::processEvents();
    QCOMPARE(progress.count(), 0);
    QCOMPARE(completed.count(), 0);
    QCOMPARE(executor.currentItem(), second);
    QCOMPARE(second->status(), QueueItem::RUNNING);
}

void TestQueueExecutor::testStopFromActionStarted()
{
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {delay()});
    QVERIFY(item);
    connect(&executor, &QueueExecutor::actionStarted, &executor, &QueueExecutor::stop);
    QVERIFY(executor.start());
    QVERIFY(!executor.isRunning());
    QCOMPARE(item->status(), QueueItem::ABORTED);
    // Stop was delivered before TaskAction::start(); the timer must never be launched.
    QCOMPARE(item->task()->actions().first()->status(), TaskAction::PENDING);
}

void TestQueueExecutor::testStopFromActionCompleted()
{
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {delay(0), delay()});
    QVERIFY(item);
    QSignalSpy started(&executor, &QueueExecutor::actionStarted);
    connect(&executor, &QueueExecutor::actionCompleted, &executor, &QueueExecutor::stop);
    QVERIFY(executor.start());
    QTRY_VERIFY(!executor.isRunning());
    QCOMPARE(started.count(), 1);
    QCOMPARE(item->task()->actions().last()->status(), TaskAction::PENDING);
    QCOMPARE(manager.state(), QueueManager::IDLE);
}

void TestQueueExecutor::testNormalCompletion()
{
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {delay(0), delay(0)});
    QVERIFY(item);
    QSignalSpy completed(&executor, &QueueExecutor::completed);
    QVERIFY(executor.start());
    QTRY_COMPARE(completed.count(), 1);
    QCOMPARE(manager.state(), QueueManager::COMPLETED);
    QCOMPARE(item->status(), QueueItem::COMPLETED);
    QCOMPARE(item->progress(), 100);
}

void TestQueueExecutor::testScriptAbortDoesNotRetry()
{
    // cat blocks on its QProcess input pipe: no sleeps, shell, hardware or external network.
    const QString cat = QStandardPaths::findExecutable("cat");
    QVERIFY2(!cat.isEmpty(), "This regression requires the cat executable");
    ScriptAction action;
    action.setScriptPath(cat);
    action.setRetries(2);
    action.setTimeout(30);
    QSignalSpy started(&action, &TaskAction::started);
    QSignalSpy completed(&action, &TaskAction::completed);
    QSignalSpy failed(&action, &TaskAction::failed);
    QVERIFY(action.start());
    auto *process = action.findChild<QProcess *>();
    QVERIFY(process);
    QCOMPARE(process->state(), QProcess::Running);
    QVERIFY(action.abort());
    QTRY_COMPARE(process->state(), QProcess::NotRunning);
    QCoreApplication::processEvents();
    QCOMPARE(action.status(), TaskAction::ABORTED);
    QCOMPARE(action.currentRetry(), 0);
    QCOMPARE(started.count(), 1);
    QCOMPARE(completed.count(), 0);
    QCOMPARE(failed.count(), 0);
}

void TestQueueExecutor::testStopBeforeScriptLaunch()
{
    const QString cat = QStandardPaths::findExecutable("cat");
    QVERIFY2(!cat.isEmpty(), "This regression requires the cat executable");
    QueueManager manager;
    QueueExecutor executor(&manager);
    auto *item = addTask(manager, {QJsonObject{{"type", "SCRIPT"}, {"script_path", cat}}});
    QVERIFY(item);
    auto *action = item->task()->actions().first();
    auto *process = action->findChild<QProcess *>();
    QVERIFY(process);
    QSignalSpy launched(process, &QProcess::started);
    connect(&executor, &QueueExecutor::progress, &executor, &QueueExecutor::stop);
    QVERIFY(executor.start());
    QCoreApplication::processEvents();
    QVERIFY(!executor.isRunning());
    QCOMPARE(launched.count(), 0);
    QCOMPARE(action->status(), TaskAction::ABORTED);
}

int main(int argc, char **argv)
{
    QTemporaryDir home;
    if (!home.isValid())
        return 1;
    qputenv("HOME", home.path().toUtf8());
    qputenv("XDG_CONFIG_HOME", (home.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (home.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (home.path() + "/cache").toUtf8());
    QCoreApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);
    TestQueueExecutor test;
    return QTest::qExec(&test, argc, argv);
}

#include "testqueueexecutor.moc"
