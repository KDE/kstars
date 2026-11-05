/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Tests for the diagnosis value types: item ordering, JSON and log output.
 */

#include "ekos/auxiliary/diagnosis.h"

#include <QJsonArray>
#include <QTest>
#include <QTimeZone>

using namespace Ekos::Diagnosis;

class TestDiagnosis : public QObject
{
        Q_OBJECT

    private slots:
        void testOrder();
        void testFindItem();
        void testItemJson();
        void testReportJson();
        void testLogLines();

    private:
        static Item item(const QString &id, Tier tier, bool blocker = true);
};

Item TestDiagnosis::item(const QString &id, Tier tier, bool blocker)
{
    Item item;
    item.id = id;
    item.tier = tier;
    item.blocker = blocker;
    item.title = id;
    return item;
}

void TestDiagnosis::testOrder()
{
    QList<Item> items;
    items << item("tuning", Tier::Tuning)
          << item("hw_warning", Tier::Hardware, false)
          << item("hw_blocker_1", Tier::Hardware)
          << item("data", Tier::Data)
          << item("config_warning", Tier::Configuration, false)
          << item("hw_blocker_2", Tier::Hardware)
          << item("config", Tier::Configuration);

    order(items);

    QStringList ids;
    for (const auto &oneItem : items)
        ids << oneItem.id;

    // Tier first, blockers before warnings, and equal items keep their order
    QCOMPARE(ids, QStringList({"hw_blocker_1", "hw_blocker_2", "hw_warning", "config", "config_warning", "data", "tuning"}));
}

void TestDiagnosis::testFindItem()
{
    Report report;
    report.items << item("a", Tier::Hardware) << item("b", Tier::Data);

    QVERIFY(findItem(report, "b") != nullptr);
    QCOMPARE(findItem(report, "b")->id, QString("b"));
    QVERIFY(findItem(report, "c") == nullptr);
}

void TestDiagnosis::testItemJson()
{
    Item plain = item("align_bin_max", Tier::Data);
    plain.kind = Kind::Auto;
    plain.why = "why";
    plain.cost = ChangesSettings;

    QJsonObject json = toJson(plain);
    QCOMPARE(json["id"].toString(), QString("align_bin_max"));
    QCOMPARE(json["kind"].toString(), QString("auto"));
    QVERIFY(json["tier"].isDouble());
    QCOMPARE(json["tier"].toInt(), 2);
    QVERIFY(json["blocker"].isBool());
    QCOMPARE(json["blocker"].toBool(), true);
    QVERIFY(json["autoRetry"].isBool());
    QCOMPARE(json["title"].toString(), QString("align_bin_max"));
    QCOMPARE(json["why"].toString(), QString("why"));

    const QJsonObject cost = json["cost"].toObject();
    QCOMPARE(cost["changesSettings"].toBool(), true);
    QCOMPARE(cost["movesHardware"].toBool(), false);
    QCOMPARE(cost["needsInternet"].toBool(), false);
    QCOMPARE(cost["stopsActivity"].toBool(), false);
    QVERIFY(!cost.contains("downloadBytes"));

    // Optional fields are left out when empty
    QVERIFY(!json.contains("steps"));
    QVERIFY(!json.contains("verify"));
    QVERIFY(!json.contains("openTarget"));
    QVERIFY(!json.contains("linkModule"));

    Item full = item("index_install_07", Tier::Data);
    full.kind = Kind::Manual;
    full.cost = NeedsInternet | MovesHardware;
    full.downloadBytes = 151000000;
    full.steps << "one" << "two";
    full.verify = Verify{"index_scan", "series_installed", "==", 1};
    full.openTarget = "optical_train";
    full.linkModule = "guide";

    json = toJson(full);
    QCOMPARE(json["kind"].toString(), QString("manual"));
    QCOMPARE(json["cost"].toObject()["downloadBytes"].toDouble(), 151000000.0);
    QCOMPARE(json["cost"].toObject()["movesHardware"].toBool(), true);
    QCOMPARE(json["steps"].toArray().size(), 2);
    QCOMPARE(json["verify"].toObject()["action"].toString(), QString("index_scan"));
    QCOMPARE(json["verify"].toObject()["metric"].toString(), QString("series_installed"));
    QCOMPARE(json["verify"].toObject()["op"].toString(), QString("=="));
    QCOMPARE(json["verify"].toObject()["threshold"].toDouble(), 1.0);
    QCOMPARE(json["openTarget"].toString(), QString("optical_train"));
    QCOMPARE(json["linkModule"].toString(), QString("guide"));

    QCOMPARE(kindKey(Kind::Open), QString("open"));
    QCOMPARE(kindKey(Kind::Link), QString("link"));
    QCOMPARE(kindKey(Kind::Info), QString("info"));
}

void TestDiagnosis::testReportJson()
{
    Report report;
    report.id = "id-1";
    report.module = "align";
    report.train = "Primary";
    report.state = 2;
    report.status = "Failed";
    report.reason = 23;
    report.reasonKey = "mount_parked";
    report.title = "The mount is parked";
    report.summary = "Summary";
    report.created = QDateTime(QDate(2026, 11, 4), QTime(21, 0, 0), QTimeZone::utc());
    report.attempt = 2;
    report.tried << "align_exposure_5s";
    report.ruledOut << "Checked line";
    report.notChecked << "Not checked line";

    Evidence evidence;
    evidence.key = "fov";
    evidence.label = "Field of view";
    evidence.value = "24.1 x 16.0";
    evidence.unit = "arcmin";
    evidence.level = Evidence::Bad;
    report.evidence << evidence;
    report.items << item("mount_unpark", Tier::Hardware);

    const QJsonObject json = toJson(report);
    for (const auto &key : QStringList({"id", "module", "train", "status", "reasonKey", "title", "summary", "created"}))
        QVERIFY2(json[key].isString(), qPrintable(key));
    for (const auto &key : QStringList({"state", "reason", "attempt"}))
        QVERIFY2(json[key].isDouble(), qPrintable(key));
    for (const auto &key : QStringList({"evidence", "ruledOut", "notChecked", "tried", "items"}))
        QVERIFY2(json[key].isArray(), qPrintable(key));

    QCOMPARE(json["reason"].toInt(), 23);
    QCOMPARE(json["attempt"].toInt(), 2);
    QCOMPARE(json["created"].toString(), QString("2026-11-04T21:00:00Z"));
    QCOMPARE(json["tried"].toArray().first().toString(), QString("align_exposure_5s"));

    const QJsonObject evidenceJson = json["evidence"].toArray().first().toObject();
    QCOMPARE(evidenceJson["key"].toString(), QString("fov"));
    QCOMPARE(evidenceJson["level"].toString(), QString("bad"));
    QCOMPARE(evidenceJson["unit"].toString(), QString("arcmin"));
    QCOMPARE(evidenceJson["value"].toString(), QString("24.1 x 16.0"));

    QCOMPARE(json["items"].toArray().first().toObject()["id"].toString(), QString("mount_unpark"));
}

void TestDiagnosis::testLogLines()
{
    Report report;
    report.title = "The mount is parked";
    report.summary = "Align was asked to sync, but the mount is parked.";
    report.ruledOut << "Camera, optics and pixel size are set.";

    Item unpark = item("mount_unpark", Tier::Hardware);
    unpark.title = "Unpark the mount";
    unpark.cost = MovesHardware;
    Item index = item("index_install_07", Tier::Data);
    index.title = "Install index files 4207";
    index.cost = NeedsInternet;
    index.downloadBytes = 151000000;
    report.items << unpark << index;

    const QStringList lines = toLogLines(report, "Align");
    QCOMPARE(lines.size(), 5);
    QCOMPARE(lines[0], QString("Align failed: The mount is parked. Align was asked to sync, but the mount is parked."));
    QCOMPARE(lines[1], QString("Checked: Camera, optics and pixel size are set."));
    QCOMPARE(lines[2], QString("Suggested:"));
    QCOMPARE(lines[3], QString(" 1. Unpark the mount (moves equipment)"));
    QCOMPARE(lines[4], QString(" 2. Install index files 4207 (download about 151 MB)"));

    // Not checked lines, no summary, no items
    Report minimal;
    minimal.title = "Align failed";
    minimal.notChecked << "Mount clock: no time was received from your device.";
    const QStringList minimalLines = toLogLines(minimal, "Align");
    QCOMPARE(minimalLines.size(), 2);
    QCOMPARE(minimalLines[0], QString("Align failed: Align failed"));
    QVERIFY(minimalLines[1].startsWith("Not checked: Mount clock"));
}

QTEST_GUILESS_MAIN(TestDiagnosis)

#include "testdiagnosis.moc"
