/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/*
 * Unit tests for task queue templates, in particular optional device capabilities
 * declared with "required_properties" (e.g. a dome shutter).
 */

#include "ekos/scheduler/taskqueue/tasktemplate.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QTest>

using Ekos::TaskTemplate;

class TestTaskTemplate : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testRequiredPropertiesParsing_data();
        void testRequiredPropertiesParsing();
        void testRequiredPropertiesRoundTrip();
        void testMissingRequiredProperties_data();
        void testMissingRequiredProperties();
        void testSystemDomeShutterTemplates_data();
        void testSystemDomeShutterTemplates();
        void testSystemDomeParkTemplatesHaveNoRequirements();
        void testObservatoryCollections();

    private:
        static QJsonObject minimalTemplate();
        static QJsonObject readJson(const QString &path);
        static QJsonObject findSystemTemplate(const QString &id);
        static QStringList collectionTemplateIds(const QString &file);
};

QJsonObject TestTaskTemplate::minimalTemplate()
{
    QJsonObject action;
    action["type"] = "SET";
    action["property"] = "DOME_SHUTTER";
    action["element"] = "SHUTTER_OPEN";
    action["value"] = true;

    QJsonObject json;
    json["id"] = "test_template";
    json["name"] = "Test Template";
    json["category"] = "Dome";
    json["supported_interfaces"] = QJsonArray{32};
    json["actions"] = QJsonArray{action};
    return json;
}

QJsonObject TestTaskTemplate::readJson(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QJsonObject();
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject TestTaskTemplate::findSystemTemplate(const QString &id)
{
    QDir dir(QString(TASKQUEUE_DATA_DIR) + "/templates/system");
    for (const QString &name : dir.entryList(QStringList() << "*.json", QDir::Files))
    {
        const QJsonArray templates = readJson(dir.filePath(name))["templates"].toArray();
        for (const QJsonValue &val : templates)
        {
            if (val.toObject()["id"].toString() == id)
                return val.toObject();
        }
    }
    return QJsonObject();
}

QStringList TestTaskTemplate::collectionTemplateIds(const QString &file)
{
    QStringList ids;
    const QJsonArray tasks = readJson(QString(TASKQUEUE_DATA_DIR) + "/collections/" + file)["tasks"].toArray();
    for (const QJsonValue &val : tasks)
        ids << val.toObject()["template_id"].toString();
    return ids;
}

void TestTaskTemplate::testRequiredPropertiesParsing_data()
{
    QTest::addColumn<QJsonValue>("value");
    QTest::addColumn<QStringList>("expected");

    QTest::newRow("absent") << QJsonValue(QJsonValue::Undefined) << QStringList();
    QTest::newRow("empty") << QJsonValue(QJsonArray()) << QStringList();
    QTest::newRow("single") << QJsonValue(QJsonArray{"DOME_SHUTTER"}) << QStringList{"DOME_SHUTTER"};
    QTest::newRow("multiple") << QJsonValue(QJsonArray{"DOME_SHUTTER", "DOME_SHUTTER_PARK_POLICY"})
                              << QStringList{"DOME_SHUTTER", "DOME_SHUTTER_PARK_POLICY"};
    QTest::newRow("blank entries ignored") << QJsonValue(QJsonArray{"", "DOME_SHUTTER", 5})
                                           << QStringList{"DOME_SHUTTER"};
}

void TestTaskTemplate::testRequiredPropertiesParsing()
{
    QFETCH(QJsonValue, value);
    QFETCH(QStringList, expected);

    QJsonObject json = minimalTemplate();
    if (!value.isUndefined())
        json["required_properties"] = value;

    TaskTemplate tmpl;
    QVERIFY(tmpl.loadFromJson(json));
    QCOMPARE(tmpl.requiredProperties(), expected);
}

void TestTaskTemplate::testRequiredPropertiesRoundTrip()
{
    // Without requirements the key is not written, keeping existing user templates unchanged
    TaskTemplate plain;
    QVERIFY(plain.loadFromJson(minimalTemplate()));
    QVERIFY(!plain.toJson().contains("required_properties"));

    QJsonObject json = minimalTemplate();
    json["required_properties"] = QJsonArray{"DOME_SHUTTER"};
    TaskTemplate original;
    QVERIFY(original.loadFromJson(json));

    const QJsonObject saved = original.toJson();
    QCOMPARE(saved["required_properties"].toArray(), QJsonArray{"DOME_SHUTTER"});

    TaskTemplate reloaded;
    QVERIFY(reloaded.loadFromJson(saved));
    QCOMPARE(reloaded.requiredProperties(), original.requiredProperties());
}

void TestTaskTemplate::testMissingRequiredProperties_data()
{
    QTest::addColumn<QStringList>("required");
    QTest::addColumn<QStringList>("deviceProperties");
    QTest::addColumn<QStringList>("expectedMissing");

    QTest::newRow("no requirements, bare device") << QStringList() << QStringList() << QStringList();
    QTest::newRow("dome with shutter") << QStringList{"DOME_SHUTTER"}
                                       << QStringList{"DOME_PARK", "DOME_SHUTTER"} << QStringList();
    QTest::newRow("roll-off roof without shutter") << QStringList{"DOME_SHUTTER"}
            << QStringList{"DOME_PARK"} << QStringList{"DOME_SHUTTER"};
    QTest::newRow("partially supported") << QStringList{"DOME_SHUTTER", "DOME_SHUTTER_PARK_POLICY"}
                                         << QStringList{"DOME_SHUTTER"} << QStringList{"DOME_SHUTTER_PARK_POLICY"};
}

void TestTaskTemplate::testMissingRequiredProperties()
{
    QFETCH(QStringList, required);
    QFETCH(QStringList, deviceProperties);
    QFETCH(QStringList, expectedMissing);

    TaskTemplate tmpl;
    tmpl.setRequiredProperties(required);

    const QSet<QString> available(deviceProperties.begin(), deviceProperties.end());
    const QStringList missing = tmpl.missingRequiredProperties([&available](const QString & name)
    {
        return available.contains(name);
    });
    QCOMPARE(missing, expectedMissing);
}

void TestTaskTemplate::testSystemDomeShutterTemplates_data()
{
    QTest::addColumn<QString>("id");
    QTest::addColumn<QString>("element");

    QTest::newRow("open") << "dome_shutter_open" << "SHUTTER_OPEN";
    QTest::newRow("close") << "dome_shutter_close" << "SHUTTER_CLOSE";
}

void TestTaskTemplate::testSystemDomeShutterTemplates()
{
    QFETCH(QString, id);
    QFETCH(QString, element);

    const QJsonObject json = findSystemTemplate(id);
    QVERIFY2(!json.isEmpty(), qPrintable(QString("System template %1 not found").arg(id)));

    TaskTemplate tmpl;
    QVERIFY(tmpl.loadFromJson(json));
    QCOMPARE(tmpl.supportedInterfaces(), QList<uint32_t> {32});
    QCOMPARE(tmpl.requiredProperties(), QStringList{"DOME_SHUTTER"});

    const QJsonArray actions = tmpl.actionDefinitions();
    QCOMPARE(actions.size(), 2);

    const QJsonObject set = actions[0].toObject();
    QCOMPARE(set["type"].toString(), QString("SET"));
    QCOMPARE(set["property"].toString(), QString("DOME_SHUTTER"));
    QCOMPARE(set["element"].toString(), element);
    QCOMPARE(set["value"].toBool(), true);

    const QJsonObject evaluate = actions[1].toObject();
    QCOMPARE(evaluate["type"].toString(), QString("EVALUATE"));
    QCOMPARE(evaluate["property"].toString(), QString("DOME_SHUTTER"));
    QCOMPARE(evaluate["target"].toString(), QString("OK"));

    // A shutter that exists but fails to move must abort the queue, not be skipped
    QCOMPARE(set["failure_action"].toInt(-1), 0);
    QCOMPARE(evaluate["failure_action"].toInt(-1), 0);
}

void TestTaskTemplate::testSystemDomeParkTemplatesHaveNoRequirements()
{
    // Domes without a shutter (e.g. roll-off roofs) must still be parked and unparked
    for (const QString &id : QStringList{"dome_park", "dome_unpark"})
    {
        TaskTemplate tmpl;
        QVERIFY2(tmpl.loadFromJson(findSystemTemplate(id)), qPrintable(id));
        QVERIFY2(tmpl.requiredProperties().isEmpty(), qPrintable(id));
    }
}

void TestTaskTemplate::testObservatoryCollections()
{
    const QStringList startup = collectionTemplateIds("observatory_startup.json");
    const QStringList shutdown = collectionTemplateIds("observatory_shutdown.json");

    for (const QString &id : startup + shutdown)
        QVERIFY2(!findSystemTemplate(id).isEmpty(), qPrintable(QString("Unknown template %1").arg(id)));

    // Open the shutter after the dome is unparked
    QVERIFY(startup.contains("dome_shutter_open"));
    QVERIFY(startup.indexOf("dome_unpark") < startup.indexOf("dome_shutter_open"));

    // Close the shutter before the dome is parked, as the Observatory weather actions do
    QVERIFY(shutdown.contains("dome_shutter_close"));
    QVERIFY(shutdown.indexOf("dome_shutter_close") < shutdown.indexOf("dome_park"));
}

QTEST_GUILESS_MAIN(TestTaskTemplate)

#include "testtasktemplate.moc"
