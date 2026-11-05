/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the diagnosis runner with a fake module and a fake action table:
 * - unknown items and stale reports are refused,
 * - one change per retry, the tried list carries over to the next report,
 * - hardware items need a confirmation and never retry by themselves,
 * - revert, manual done with verify, dismiss, timeouts.
 */

#include "ekos/auxiliary/diagnosismanager.h"
#include "Options.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

using namespace Ekos;
using namespace Ekos::Diagnosis;

class TestDiagnosisManager : public QObject
{
        Q_OBJECT

    private slots:
        void initTestCase();
        void init();
        void cleanup();

        void testSubmitPublishesAndLogs();
        void testRefusesUnknownItems();
        void testAutoItemRetriesOnce();
        void testTriedCarriesOver();
        void testHardwareNeedsConfirm();
        void testBusyAndAsyncFinish();
        void testRunningTimeout();
        void testFinishDuringApply();
        void testFailedAction();
        void testRevert();
        void testManualDoneVerified();
        void testManualDoneVerifyFailed();
        void testFixedOnSuccess();
        void testDismiss();
        void testUnregister();
        void testClientUtc();
        void testRefresh();

    private:
        Report makeReport(const QString &train = "Primary") const;
        QStringList states(const QSignalSpy &spy) const;

        std::unique_ptr<DiagnosisManager> m_Manager;
        QStringList m_Log;
        QStringList m_Retries;
        bool m_RetryResult { true };
        int m_Reverts { 0 };
        std::optional<Report> m_Refreshed;
};

void TestDiagnosisManager::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

void TestDiagnosisManager::init()
{
    Options::setDiagnosisDismissedItems(QStringList());
    m_Log.clear();
    m_Retries.clear();
    m_RetryResult = true;
    m_Reverts = 0;
    m_Refreshed.reset();

    m_Manager.reset(new DiagnosisManager());

    DiagnosisManager::ModuleHooks hooks;
    hooks.name = "Fake";
    hooks.retry = [this](const QString & train)
    {
        m_Retries << train;
        return m_RetryResult;
    };
    hooks.log = [this](const QString & text)
    {
        m_Log << text;
    };
    hooks.refresh = [this](const Report &) -> std::optional<Report>
    {
        return m_Refreshed;
    };
    m_Manager->registerModule("fake", hooks);

    // A setting item that applies synchronously and can be reverted
    m_Manager->registerAction("fake", "fake_setting", [this](const Item &, const Report &)
    {
        DiagnosisManager::ApplyResult result;
        result.outcome = DiagnosisManager::Outcome::Done;
        result.revert = [this]()
        {
            m_Reverts++;
        };
        return result;
    });

    // An item that finishes later, e.g. a download
    m_Manager->registerAction("fake", "fake_async_", [](const Item &, const Report &)
    {
        DiagnosisManager::ApplyResult result;
        result.outcome = DiagnosisManager::Outcome::Running;
        result.timeoutMs = 100;
        return result;
    }, true);

    // An item that moves hardware
    m_Manager->registerAction("fake", "fake_hardware", [](const Item &, const Report &)
    {
        DiagnosisManager::ApplyResult result;
        result.outcome = DiagnosisManager::Outcome::Done;
        return result;
    });

    // An item that cannot be applied
    m_Manager->registerAction("fake", "fake_broken", [](const Item &, const Report &)
    {
        DiagnosisManager::ApplyResult result;
        result.outcome = DiagnosisManager::Outcome::Failed;
        result.message = "broken";
        return result;
    });
}

void TestDiagnosisManager::cleanup()
{
    m_Manager.reset();
    Options::setDiagnosisDismissedItems(QStringList());
}

Report TestDiagnosisManager::makeReport(const QString &train) const
{
    Report report;
    report.module = "fake";
    report.train = train;
    report.reasonKey = "fake_failure";
    report.title = "Fake failure";

    Item setting;
    setting.id = "fake_setting";
    setting.kind = Kind::Auto;
    setting.title = "Change a setting";
    setting.cost = ChangesSettings;

    Item async;
    async.id = "fake_async_07";
    async.kind = Kind::Auto;
    async.title = "Download";

    Item hardware;
    hardware.id = "fake_hardware";
    hardware.kind = Kind::Auto;
    hardware.title = "Move";
    hardware.cost = MovesHardware;

    Item broken;
    broken.id = "fake_broken";
    broken.kind = Kind::Auto;
    broken.title = "Broken";

    Item manual;
    manual.id = "fake_manual";
    manual.kind = Kind::Manual;
    manual.title = "Do it by hand";

    Item info;
    info.id = "fake_info";
    info.kind = Kind::Info;
    info.title = "Information";

    Item noAction;
    noAction.id = "fake_no_action";
    noAction.kind = Kind::Auto;
    noAction.title = "No action";

    report.items << setting << async << hardware << broken << manual << info << noAction;
    return report;
}

QStringList TestDiagnosisManager::states(const QSignalSpy &spy) const
{
    QStringList list;
    for (const auto &arguments : spy)
        list << arguments.first().toJsonObject()["state"].toString();
    return list;
}

void TestDiagnosisManager::testSubmitPublishesAndLogs()
{
    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::newDiagnosis);
    m_Manager->submit(makeReport());

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(0).toString(), QString("fake"));
    const QJsonObject json = spy.first().at(1).toJsonObject();
    QVERIFY(!json["id"].toString().isEmpty());
    QVERIFY(!json["created"].toString().isEmpty());
    QCOMPARE(json["attempt"].toInt(), 1);

    QVERIFY(!m_Log.isEmpty());
    QCOMPARE(m_Log.first(), QString("Fake failed: Fake failure"));
    QVERIFY(m_Log.contains("Suggested:"));

    QCOMPARE(m_Manager->reports().size(), 1);
    QCOMPARE(m_Manager->reports("fake", "Primary").size(), 1);
    QCOMPARE(m_Manager->reports("other").size(), 0);
    QCOMPARE(m_Manager->reportsJson().size(), 1);

    // A second failure of the same module and train replaces the cached report
    m_Manager->submit(makeReport());
    QCOMPARE(m_Manager->reports().size(), 1);
    // Another train has its own report
    m_Manager->submit(makeReport("Secondary"));
    QCOMPARE(m_Manager->reports().size(), 2);
}

void TestDiagnosisManager::testRefusesUnknownItems()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    auto reasonOf = [](const QJsonObject & update)
    {
        return update["state"].toString() + ":" + update["reason"].toString();
    };

    QCOMPARE(reasonOf(m_Manager->apply("stale-id", "fake_setting")), QString("refused:unknown_item"));
    QCOMPARE(reasonOf(m_Manager->apply(id, "not_in_report")), QString("refused:unknown_item"));
    QCOMPARE(reasonOf(m_Manager->apply(id, "fake_no_action")), QString("refused:unknown_item"));
    QCOMPARE(reasonOf(m_Manager->apply(id, "fake_manual")), QString("refused:not_applicable"));
    QCOMPARE(reasonOf(m_Manager->apply(id, "fake_info")), QString("refused:not_applicable"));
    QCOMPARE(reasonOf(m_Manager->mark(id, "fake_manual", "explode")), QString("refused:unknown_action"));
    QCOMPARE(reasonOf(m_Manager->mark(id, "fake_info", "done")), QString("refused:not_applicable"));

    QVERIFY(m_Retries.isEmpty());
    QVERIFY(m_Manager->tried("fake", "Primary").isEmpty());
}

void TestDiagnosisManager::testAutoItemRetriesOnce()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    const QJsonObject update = m_Manager->apply(id, "fake_setting");

    QCOMPARE(update["state"].toString(), QString("done"));
    QCOMPARE(states(spy), QStringList({"done", "waiting"}));
    QCOMPARE(m_Retries, QStringList({"Primary"}));
    QCOMPARE(m_Manager->tried("fake", "Primary"), QStringList({"fake_setting"}));
    QVERIFY(!m_Manager->isBusy());

    // A retry that cannot start is reported
    m_RetryResult = false;
    spy.clear();
    m_Manager->apply(id, "fake_setting");
    QCOMPARE(states(spy), QStringList({"done", "waiting", "failed"}));
}

void TestDiagnosisManager::testTriedCarriesOver()
{
    m_Manager->submit(makeReport());
    m_Manager->apply(m_Manager->reports().first().id, "fake_setting");

    // The retry failed again: the next report carries the tried items
    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::newDiagnosis);
    m_Manager->submit(makeReport());
    QCOMPARE(spy.count(), 1);
    const QJsonObject json = spy.first().at(1).toJsonObject();
    QCOMPARE(json["attempt"].toInt(), 2);
    QCOMPARE(json["tried"].toArray().size(), 1);
    QCOMPARE(json["tried"].toArray().first().toString(), QString("fake_setting"));

    // Another train is a separate chain
    QVERIFY(m_Manager->tried("fake", "Secondary").isEmpty());
}

void TestDiagnosisManager::testHardwareNeedsConfirm()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    const QJsonObject refused = m_Manager->apply(id, "fake_hardware", false);
    QCOMPARE(refused["state"].toString(), QString("refused"));
    QCOMPARE(refused["reason"].toString(), QString("needs_confirm"));

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    QCOMPARE(m_Manager->apply(id, "fake_hardware", true)["state"].toString(), QString("done"));
    // Applied, but the manager never moves on to a retry by itself
    QCOMPARE(states(spy), QStringList({"done"}));
    QVERIFY(m_Retries.isEmpty());
    QCOMPARE(m_Manager->tried("fake", "Primary"), QStringList({"fake_hardware"}));
}

void TestDiagnosisManager::testBusyAndAsyncFinish()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QCOMPARE(m_Manager->apply(id, "fake_async_07")["state"].toString(), QString("running"));
    QVERIFY(m_Manager->isBusy());

    const QJsonObject busy = m_Manager->apply(id, "fake_setting");
    QCOMPARE(busy["reason"].toString(), QString("busy"));
    QCOMPARE(m_Manager->mark(id, "fake_manual", "done")["reason"].toString(), QString("busy"));

    // A finish for another item is ignored
    m_Manager->finishItem(id, "fake_setting", true);
    QVERIFY(m_Manager->isBusy());

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    m_Manager->finishItem(id, "fake_async_07", true);
    QVERIFY(!m_Manager->isBusy());
    QCOMPARE(states(spy), QStringList({"done", "waiting"}));
    QCOMPARE(m_Retries, QStringList({"Primary"}));
    QCOMPARE(m_Manager->tried("fake", "Primary"), QStringList({"fake_async_07"}));
}

void TestDiagnosisManager::testRunningTimeout()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    m_Manager->apply(id, "fake_async_07");
    QTRY_VERIFY_WITH_TIMEOUT(states(spy).contains("failed"), 5000);
    QVERIFY(!m_Manager->isBusy());
    QVERIFY(m_Retries.isEmpty());
    QVERIFY(m_Manager->tried("fake", "Primary").isEmpty());
}

void TestDiagnosisManager::testFinishDuringApply()
{
    // An action whose completion arrives before apply() returns
    DiagnosisManager *manager = m_Manager.get();
    m_Manager->registerAction("fake", "fake_async_", [manager](const Item & item, const Report & report)
    {
        manager->finishItem(report.id, item.id, true);
        DiagnosisManager::ApplyResult result;
        result.outcome = DiagnosisManager::Outcome::Running;
        return result;
    }, true);

    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    m_Manager->apply(id, "fake_async_07");
    QVERIFY(!m_Manager->isBusy());
    QVERIFY(!states(spy).contains("running"));
    QCOMPARE(states(spy), QStringList({"done", "waiting"}));
}

void TestDiagnosisManager::testFailedAction()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    const QJsonObject update = m_Manager->apply(id, "fake_broken");
    QCOMPARE(update["state"].toString(), QString("failed"));
    QCOMPARE(update["message"].toString(), QString("broken"));
    QVERIFY(!m_Manager->isBusy());
    QVERIFY(m_Retries.isEmpty());
    QVERIFY(m_Manager->tried("fake", "Primary").isEmpty());
}

void TestDiagnosisManager::testRevert()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QCOMPARE(m_Manager->mark(id, "fake_setting", "revert")["reason"].toString(), QString("unknown_item"));

    m_Manager->apply(id, "fake_setting");
    QCOMPARE(m_Reverts, 0);
    QCOMPARE(m_Manager->mark(id, "fake_setting", "revert")["state"].toString(), QString("reverted"));
    QCOMPARE(m_Reverts, 1);

    // Only once
    QCOMPARE(m_Manager->mark(id, "fake_setting", "revert")["state"].toString(), QString("refused"));
    QCOMPARE(m_Reverts, 1);

    // Revert still works after the module succeeded and the report is gone
    m_Manager->submit(makeReport());
    const QString secondId = m_Manager->reports().first().id;
    m_Manager->apply(secondId, "fake_setting");
    m_Manager->moduleSucceeded("fake", "Primary");
    QVERIFY(m_Manager->reports().isEmpty());
    QCOMPARE(m_Manager->mark(secondId, "fake_setting", "revert")["state"].toString(), QString("reverted"));
    QCOMPARE(m_Reverts, 2);
}

void TestDiagnosisManager::testManualDoneVerified()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    QCOMPARE(m_Manager->mark(id, "fake_manual", "done")["state"].toString(), QString("done"));
    QCOMPARE(m_Retries, QStringList({"Primary"}));
    QCOMPARE(m_Manager->tried("fake", "Primary"), QStringList({"fake_manual"}));

    m_Manager->moduleSucceeded("fake", "Primary");
    QCOMPARE(states(spy), QStringList({"done", "waiting", "verified"}));
    QCOMPARE(spy.last().first().toJsonObject()["itemId"].toString(), QString("fake_manual"));
    QVERIFY(m_Manager->tried("fake", "Primary").isEmpty());
}

void TestDiagnosisManager::testManualDoneVerifyFailed()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;
    m_Manager->mark(id, "fake_manual", "done");

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    m_Manager->submit(makeReport());
    QCOMPARE(states(spy), QStringList({"verify_failed"}));
    QCOMPARE(spy.first().first().toJsonObject()["diagnosisId"].toString(), id);
}

void TestDiagnosisManager::testFixedOnSuccess()
{
    // Success without a fix chain changes nothing visible
    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::diagnosisUpdated);
    m_Manager->submit(makeReport());
    m_Manager->moduleSucceeded("fake", "Primary");
    QVERIFY(spy.isEmpty());
    QVERIFY(m_Manager->reports().isEmpty());

    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;
    m_Manager->apply(id, "fake_setting");
    spy.clear();

    // Success of another train does not close this chain
    m_Manager->moduleSucceeded("fake", "Secondary");
    QVERIFY(spy.isEmpty());

    m_Manager->moduleSucceeded("fake", "Primary");
    QCOMPARE(states(spy), QStringList({"fixed"}));
    QCOMPARE(spy.first().first().toJsonObject()["itemId"].toString(), QString("fake_setting"));
    QCOMPARE(spy.first().first().toJsonObject()["diagnosisId"].toString(), id);
    QVERIFY(m_Manager->reports().isEmpty());
    QVERIFY(m_Manager->tried("fake", "Primary").isEmpty());

    // The next failure starts a new chain
    m_Manager->submit(makeReport());
    QCOMPARE(m_Manager->reports().first().attempt, 1);
}

void TestDiagnosisManager::testDismiss()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;

    QCOMPARE(m_Manager->mark(id, "fake_info", "dismiss")["state"].toString(), QString("dismissed"));
    QCOMPARE(m_Manager->dismissed("fake"), QStringList({"fake_info"}));
    QVERIFY(findItem(m_Manager->reports().first(), "fake_info") == nullptr);
    QVERIFY(Options::diagnosisDismissedItems().isEmpty());
    QVERIFY(m_Manager->dismissed("other").isEmpty());

    QCOMPARE(m_Manager->mark(id, "fake_manual", "dismiss_forever")["state"].toString(), QString("dismissed"));
    QCOMPARE(Options::diagnosisDismissedItems(), QStringList({"fake:fake_manual"}));
    QCOMPARE(m_Manager->dismissed("fake"), QStringList({"fake_info", "fake_manual"}));

    // A dismissed item can no longer be applied
    QCOMPARE(m_Manager->apply(id, "fake_info")["reason"].toString(), QString("unknown_item"));

    // Dismissed for good survives a new manager, dismissed for the session does not
    m_Manager.reset(new DiagnosisManager());
    QCOMPARE(m_Manager->dismissed("fake"), QStringList({"fake_manual"}));
}

void TestDiagnosisManager::testUnregister()
{
    m_Manager->submit(makeReport());
    const QString id = m_Manager->reports().first().id;
    QCOMPARE(m_Manager->apply(id, "fake_async_07")["state"].toString(), QString("running"));

    m_Manager->unregisterModule("fake");
    QVERIFY(!m_Manager->isBusy());
    QVERIFY(m_Manager->reports().isEmpty());

    // Actions are gone too
    m_Manager->submit(makeReport());
    QCOMPARE(m_Manager->apply(m_Manager->reports().first().id, "fake_setting")["reason"].toString(), QString("unknown_item"));
}

void TestDiagnosisManager::testClientUtc()
{
    QVERIFY(!m_Manager->clientUtc());

    const QDateTime client = QDateTime::currentDateTimeUtc().addSecs(300);
    m_Manager->setClientUtc(client);
    QVERIFY(m_Manager->clientUtc());
    QVERIFY(std::abs(m_Manager->clientUtc()->secsTo(client)) <= 2);

    // Invalid times are ignored
    m_Manager->setClientUtc(QDateTime());
    QVERIFY(std::abs(m_Manager->clientUtc()->secsTo(client)) <= 2);
}

void TestDiagnosisManager::testRefresh()
{
    m_Manager->submit(makeReport());
    const Report original = m_Manager->reports().first();

    QSignalSpy spy(m_Manager.get(), &DiagnosisManager::newDiagnosis);

    // Nothing to rebuild
    m_Manager->refresh("fake");
    QVERIFY(spy.isEmpty());

    // Same content: not published again
    m_Refreshed = original;
    m_Manager->refresh("fake");
    QVERIFY(spy.isEmpty());

    // Changed content keeps the id, so items stay valid for apply()
    Report changed = makeReport();
    changed.title = "Changed";
    m_Refreshed = changed;
    m_Manager->refresh("fake");
    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.first().at(1).toJsonObject()["id"].toString(), original.id);
    QCOMPARE(m_Manager->reports().first().title, QString("Changed"));

    // Without publishing
    changed.title = "Changed again";
    m_Refreshed = changed;
    spy.clear();
    m_Manager->refresh(QString(), false);
    QVERIFY(spy.isEmpty());
    QCOMPARE(m_Manager->reports().first().title, QString("Changed again"));
}

QTEST_GUILESS_MAIN(TestDiagnosisManager)

#include "testdiagnosismanager.moc"
