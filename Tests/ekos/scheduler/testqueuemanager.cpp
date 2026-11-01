/*
    SPDX-FileCopyrightText: 2026 KStars Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/
/*
 * Unit tests for the programmatic tasks of the QueueManager (park, unpark, delay) that the scheduler adds
 * on its own, in particular in a session in which no collection file or queue UI loaded the task templates.
 */
#include "ekos/scheduler/taskqueue/queue/queuemanager.h"
#include "ekos/scheduler/taskqueue/queue/queueitem.h"
#include "ekos/scheduler/taskqueue/task.h"
#include "ekos/scheduler/taskqueue/tasktemplate.h"
#include "ekos/scheduler/taskqueue/templatemanager.h"
#include "ekos/scheduler/taskqueue/actions/setaction.h"
#include "auxiliary/kspaths.h"
#include "../../testhelpers.h"

#include <QDir>
#include <QFileInfo>
#include <QTest>

class TestQueueManager : public QObject
{
        Q_OBJECT
    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();
        void init();
        void cleanup();

        // A fresh session that never loaded the templates still creates the scheduler's tasks
        void testAddTaskWithoutLoadedTemplates_data();
        void testAddTaskWithoutLoadedTemplates();
        // Templates that are already loaded are used as they are, not loaded again
        void testAddTaskKeepsLoadedTemplates();
};

void TestQueueManager::initTestCase()
{
    KTEST_BEGIN();

    // The system templates must come from the source tree, not from an (old or empty) installed copy
    const QString located = KSPaths::locate(QStandardPaths::AppDataLocation, "taskqueue/templates/system",
                                            QStandardPaths::LocateDirectory);
    const QString expected = QDir(KTest::kstarsDataDir()).filePath("taskqueue/templates/system");
    QVERIFY2(!located.isEmpty(), "System template directory not found");
    QVERIFY2(QFileInfo(located).canonicalFilePath() == QFileInfo(expected).canonicalFilePath(),
             qPrintable(QString("System templates found in '%1', expected '%2'").arg(located, expected)));
}

void TestQueueManager::cleanupTestCase()
{
    Ekos::TemplateManager::release();
    KTEST_END();
}

void TestQueueManager::init()
{
    // A new, never initialized template manager is what a session without a collection run has
    Ekos::TemplateManager::release();
}

void TestQueueManager::cleanup()
{
    Ekos::TemplateManager::release();
}

void TestQueueManager::testAddTaskWithoutLoadedTemplates_data()
{
    QTest::addColumn<QString>("templateId");
    QTest::addColumn<QString>("parameter");
    QTest::addColumn<int>("value");
    QTest::addColumn<int>("firstActionType");
    QTest::addColumn<QString>("element");

    QTest::newRow("unpark") << "mount_unpark" << "wait_timeout" << 60
                            << static_cast<int>(Ekos::TaskAction::SET) << "UNPARK";
    QTest::newRow("park") << "mount_park" << "wait_timeout" << 60
                          << static_cast<int>(Ekos::TaskAction::SET) << "PARK";
    QTest::newRow("delay") << "delay" << "delay_seconds" << 5
                           << static_cast<int>(Ekos::TaskAction::DELAY) << "";
}

void TestQueueManager::testAddTaskWithoutLoadedTemplates()
{
    QFETCH(QString, templateId);
    QFETCH(QString, parameter);
    QFETCH(int, value);
    QFETCH(int, firstActionType);
    QFETCH(QString, element);

    Ekos::TemplateManager *manager = Ekos::TemplateManager::Instance();
    QVERIFY2(manager->allTemplates().isEmpty(), "The template manager must start without loaded templates");

    Ekos::QueueManager queue;
    bool added = false;
    if (templateId == "mount_unpark")
        added = queue.addMountUnparkTask(value);
    else if (templateId == "mount_park")
        added = queue.addMountParkTask(value);
    else
        added = queue.addDelayTask(value);

    QVERIFY2(added, qPrintable(QString("No '%1' task created although the template exists").arg(templateId)));
    QCOMPARE(queue.count(), 1);

    Ekos::Task *task = queue.itemAt(0)->task();
    QVERIFY(task != nullptr);
    QCOMPARE(task->templateId(), templateId);
    // The executor assigns the device by its interface at run time
    QVERIFY(task->device().isEmpty());
    QCOMPARE(task->parameters().value(parameter).toInt(), value);
    QVERIFY(task->actionCount() > 0);

    Ekos::TaskAction *first = task->actions().first();
    QCOMPARE(static_cast<int>(first->type()), firstActionType);
    if (first->type() == Ekos::TaskAction::SET)
    {
        QCOMPARE(first->property(), QString("TELESCOPE_PARK"));
        auto setAction = qobject_cast<Ekos::SetAction *>(first);
        QVERIFY(setAction != nullptr);
        QCOMPARE(setAction->element(), element);
    }

    QVERIFY(manager->getTemplate(templateId) != nullptr);
}

void TestQueueManager::testAddTaskKeepsLoadedTemplates()
{
    Ekos::TemplateManager *manager = Ekos::TemplateManager::Instance();
    QVERIFY(manager->initialize());

    Ekos::TaskTemplate *unparkTemplate = manager->getTemplate("mount_unpark");
    QVERIFY(unparkTemplate != nullptr);
    const int templateCount = manager->allTemplates().size();

    Ekos::QueueManager queue;
    QVERIFY(queue.addMountUnparkTask());
    QVERIFY(queue.addMountParkTask());
    QCOMPARE(queue.count(), 2);

    // A reload would have deleted and recreated the templates
    QCOMPARE(manager->getTemplate("mount_unpark"), unparkTemplate);
    QCOMPARE(manager->allTemplates().size(), templateCount);
}

QTEST_GUILESS_MAIN(TestQueueManager)

#include "testqueuemanager.moc"
