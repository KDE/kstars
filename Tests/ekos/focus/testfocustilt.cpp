/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "config-kstars.h"

#include "kstarsdata.h"
#include "Options.h"

#ifdef HAVE_QTGRAPHS
#include "ekos/focus/tiltcorrectionwidget.h"
#endif

#include "../../testhelpers.h"

#include <QDoubleSpinBox>
#include <QTest>

/**
 * The plate settings of the Tilt Correction Advisory belong to the Focus settings of an optical train: the widget only
 * reaches them through the store Focus gives it. How Focus keeps them is tested with the simulators in
 * Tests/kstars_ui/test_ekos_focus.cpp, testTiltSettingsPerTrain.
 */
class TestFocusTilt : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testWidgetStore();
};

void TestFocusTilt::initTestCase()
{
    KTEST_BEGIN();
    KStarsData *data = KStarsData::Create();
    QVERIFY(data != nullptr);
    QVERIFY(data->initialize());
}

void TestFocusTilt::cleanupTestCase()
{
    KTEST_END();
}

void TestFocusTilt::testWidgetStore()
{
#ifdef HAVE_QTGRAPHS
    Ekos::TiltCorrectionWidget widget;
    auto radiusSpin = [&widget](double value) -> QDoubleSpinBox *
    {
        for (auto spin : widget.findChildren<QDoubleSpinBox *>())
            if (qAbs(spin->value() - value) < 1e-6)
                return spin;
        return nullptr;
    };

    // Loaded from the store
    QVariantMap stored = {{"radius", 77.0}};
    QList<QVariantMap> saved;
    Ekos::TiltCorrectionWidget::TrainSettingsStore store;
    store.load = [&stored]()
    {
        return stored;
    };
    store.save = [&saved](const QVariantMap & settings)
    {
        saved << settings;
    };
    widget.setTrainSettingsStore(store);
    QDoubleSpinBox *spin = radiusSpin(77.0);
    QVERIFY(spin);

    // A change goes to the store, with all the plate settings, and to the global default for new trains
    spin->setValue(80.0);
    QVERIFY(!saved.isEmpty());
    QCOMPARE(saved.last().value("radius").toDouble(), 80.0);
    for (const auto &key :
            {"plateType", "threadIndex", "screwPitch", "plateAngle", "mode"
            })
        QVERIFY2(saved.last().contains(key), key);
    QCOMPARE(Options::tiltRadius(), 80.0);

    // Without a store only the global default changes
    saved.clear();
    widget.setTrainSettingsStore({});
    spin->setValue(81.0);
    QVERIFY(saved.isEmpty());
    QCOMPARE(Options::tiltRadius(), 81.0);
#else
    QSKIP("Built without Qt Graphs: no Aberration Inspector");
#endif
}

QTEST_MAIN(TestFocusTilt)

#include "testfocustilt.moc"
