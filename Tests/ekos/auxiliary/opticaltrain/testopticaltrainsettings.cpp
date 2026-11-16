/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/auxiliary/opticaltrainsettings.h"

#include <QJsonObject>
#include <QTest>

using Ekos::OpticalTrainSettings;

/**
 * Module metadata in the settings of an optical train: what a module keeps per train besides its settings. A module
 * never sees it among its settings, and saving its settings never loses it.
 */
class TestOpticalTrainSettings : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testWithoutMetadata();
        void testWithMetadata();
};

namespace
{
const QVariantMap METADATA = {{"tilt", QVariantMap{{"radius", 64.0}}}, {"reference", QVariantList{1.8, 1.9}}};

QVariantMap stored()
{
    return {{"focusTicks", 200}, {"focusExposure", 2.0}, {OpticalTrainSettings::METADATA_KEY, METADATA}};
}
}

void TestOpticalTrainSettings::testWithoutMetadata()
{
    // The module gets its settings only
    const QVariantMap settings = OpticalTrainSettings::withoutMetadata(stored()).toMap();
    QCOMPARE(settings, QVariantMap({{"focusTicks", 200}, {"focusExposure", 2.0}}));

    // Also as read back from the database
    const QVariant json = QJsonObject::fromVariantMap(stored());
    QVERIFY(!OpticalTrainSettings::withoutMetadata(json).toMap().contains(OpticalTrainSettings::METADATA_KEY));

    // Metadata alone is no settings: the module loads its defaults
    const QVariantMap onlyMetadata = {{OpticalTrainSettings::METADATA_KEY, METADATA}};
    QVERIFY(!OpticalTrainSettings::withoutMetadata(onlyMetadata).isValid());

    // Anything else is left as it is
    const QVariantMap plain = {{"focusTicks", 200}};
    QCOMPARE(OpticalTrainSettings::withoutMetadata(plain).toMap(), plain);
    QCOMPARE(OpticalTrainSettings::withoutMetadata(QVariant(5)).toInt(), 5);
    QVERIFY(!OpticalTrainSettings::withoutMetadata(QVariant()).isValid());
}

void TestOpticalTrainSettings::testWithMetadata()
{
    // A module saving its settings keeps the stored metadata
    const QVariantMap saved = {{"focusTicks", 400}};
    QVariantMap merged = OpticalTrainSettings::withMetadata(saved, stored()).toMap();
    QCOMPARE(merged.value("focusTicks").toInt(), 400);
    QCOMPARE(merged.value(OpticalTrainSettings::METADATA_KEY).toMap(), METADATA);
    QVERIFY(!merged.contains("focusExposure"));

    // Also over settings read back from the database
    merged = OpticalTrainSettings::withMetadata(saved, QVariant(QJsonObject::fromVariantMap(stored()))).toMap();
    QCOMPARE(merged.value(OpticalTrainSettings::METADATA_KEY).toMap().value("tilt").toMap().value("radius").toDouble(), 64.0);

    // Settings that carry their own metadata keep theirs
    const QVariantMap own = {{"focusTicks", 400}, {OpticalTrainSettings::METADATA_KEY, QVariantMap{{"tilt", 1}}}};
    QCOMPARE(OpticalTrainSettings::withMetadata(own, stored()).toMap(), own);

    // Nothing stored: as saved
    QCOMPARE(OpticalTrainSettings::withMetadata(saved, QVariantMap{{"focusTicks", 200}}).toMap(), saved);
    QCOMPARE(OpticalTrainSettings::withMetadata(saved, QVariant()).toMap(), saved);

    // Cleared settings still keep the metadata; a value that is no map cannot carry it
    QCOMPARE(OpticalTrainSettings::withMetadata(QVariant(), stored()).toMap(),
    QVariantMap({{OpticalTrainSettings::METADATA_KEY, METADATA}}));
    QCOMPARE(OpticalTrainSettings::withMetadata(QVariant(7), stored()).toInt(), 7);
}

QTEST_GUILESS_MAIN(TestOpticalTrainSettings)

#include "testopticaltrainsettings.moc"
