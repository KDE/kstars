/*
    KStars UI tests for the GPG period of the guide module
    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_guide_gpgperiod.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "ekos/guide/guide.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "ekos/auxiliary/opticaltrainsettings.h"

namespace
{
// The period the user entered in the GPG settings with "Estimate period" off
constexpr double MANUAL_PERIOD = 712.0;
// A period learned earlier and stored in the optical train, e.g. with another mount
constexpr double LEARNED_PERIOD = 359.0;

// Stored guide settings of an optical train. The guide exposure only shows that the map was loaded.
// "gPGPeriod" is the learned period saved by Guide::saveGPGPeriod(); "kcfg_GPGPeriod" is the period field
// of the GPG settings page, which is stored only once the user has edited it with that train selected.
QVariantMap guideSettings(double exposure, bool estimate, bool hasLearnedPeriod, bool hasPeriodField)
{
    QVariantMap settings;
    settings["guideExposure"] = exposure;
    settings["kcfg_GPGEstimatePeriod"] = estimate;
    if (hasLearnedPeriod)
        settings["gPGPeriod"] = LEARNED_PERIOD;
    if (hasPeriodField)
        settings["kcfg_GPGPeriod"] = MANUAL_PERIOD;
    return settings;
}
}

TestEkosGuideGPGPeriod::TestEkosGuideGPGPeriod(QObject *parent) : QObject(parent)
{
    // Telescope Simulator, CCD Simulator and Guide Simulator, internal guider
    m_Helper = new TestEkosHelper("Internal");
}

bool TestEkosGuideGPGPeriod::storeGuideSettings(const QString &train, const QVariantMap &settings)
{
    const auto id = Ekos::OpticalTrainManager::Instance()->id(train);
    KVERIFY_SUB(Ekos::OpticalTrainManager::Instance()->exists(id));
    Ekos::OpticalTrainSettings::Instance()->setOpticalTrainID(id);
    Ekos::OpticalTrainSettings::Instance()->setOneSetting(Ekos::OpticalTrainSettings::Guide, settings);
    return true;
}

bool TestEkosGuideGPGPeriod::selectGuideTrain(const QString &train)
{
    KTRY_SET_COMBO_SUB(Ekos::Manager::Instance()->guideModule(), opticalTrainCombo, train);
    return true;
}

// Regression test: a GPG period learned with "Estimate period" on is stored in the optical train. Loading the
// train (switching trains, starting Ekos) restored it also with the estimation off, so a period entered by hand
// was replaced by the learned one, possibly learned with another mount. With the estimation on, the learned
// period of the train is still restored.
void TestEkosGuideGPGPeriod::testPeriodOnTrainLoad_data()
{
    QTest::addColumn<bool>("estimate");
    QTest::addColumn<bool>("hasLearnedPeriod");
    QTest::addColumn<bool>("hasPeriodField");
    QTest::addColumn<double>("expectedPeriod");

    // The train has a learned period, its period field was never edited
    QTest::newRow("EstimateOff_LearnedPeriod") << false << true << false << MANUAL_PERIOD;
    // The period was also entered by hand with this train selected
    QTest::newRow("EstimateOff_LearnedPeriodAndPeriodField") << false << true << true << MANUAL_PERIOD;
    QTest::newRow("EstimateOn_LearnedPeriod") << true << true << true << LEARNED_PERIOD;
    QTest::newRow("EstimateOn_NoLearnedPeriod") << true << false << false << MANUAL_PERIOD;
}

void TestEkosGuideGPGPeriod::testPeriodOnTrainLoad()
{
    QFETCH(bool, estimate);
    QFETCH(bool, hasLearnedPeriod);
    QFETCH(bool, hasPeriodField);
    QFETCH(double, expectedPeriod);

    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    const QString homeTrain = m_Helper->m_guidingTrain;
    const QString otherTrain = m_Helper->m_primaryTrain;
    QVERIFY(selectGuideTrain(homeTrain));

    QVERIFY(storeGuideSettings(otherTrain, guideSettings(2.5, estimate, hasLearnedPeriod, hasPeriodField)));
    QVERIFY(storeGuideSettings(homeTrain, guideSettings(1.5, estimate, false, false)));
    Options::setGPGEstimatePeriod(estimate);
    Options::setGPGPeriod(MANUAL_PERIOD);

    KTRY_GADGET(guide, QDoubleSpinBox, guideExposure);

    // Load the other train
    QVERIFY(selectGuideTrain(otherTrain));
    QCOMPARE(guideExposure->value(), 2.5);
    QCOMPARE(Options::gPGEstimatePeriod(), estimate);
    QCOMPARE(Options::gPGPeriod(), expectedPeriod);

    // And back: its settings have no learned period, so the period doesn't change
    QVERIFY(selectGuideTrain(homeTrain));
    QCOMPARE(guideExposure->value(), 1.5);
    QCOMPARE(Options::gPGPeriod(), expectedPeriod);
}

// Same with a new start of Ekos, which loads the stored settings of the guide train
void TestEkosGuideGPGPeriod::testPeriodOnEkosRestart()
{
    const QString train = m_Helper->m_guidingTrain;
    QVERIFY(selectGuideTrain(train));

    QVERIFY(m_Helper->shutdownEkosProfile());
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule() == nullptr, 10000);

    QVERIFY(storeGuideSettings(train, guideSettings(3.5, false, true, false)));
    Options::setGPGEstimatePeriod(false);
    Options::setGPGPeriod(MANUAL_PERIOD);
    Options::self()->save();

    QVERIFY(m_Helper->startEkosProfile());
    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    QVERIFY(guide != nullptr);
    KTRY_GADGET(guide, QDoubleSpinBox, guideExposure);
    // The stored settings of the train have been loaded
    QTRY_COMPARE_WITH_TIMEOUT(guide->opticalTrain(), train, 10000);
    QTRY_COMPARE_WITH_TIMEOUT(guideExposure->value(), 3.5, 10000);
    QCOMPARE(Options::gPGEstimatePeriod(), false);
    QCOMPARE(Options::gPGPeriod(), MANUAL_PERIOD);
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosGuideGPGPeriod::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    QStandardPaths::setTestModeEnabled(true);

    QVERIFY(m_Helper->startEkosProfile());
    Options::setVerboseLogging(false);
    Options::setLogToFile(false);
    // Primary train with the CCD Simulator, Secondary train with the Guide Simulator
    m_Helper->prepareOpticalTrains();
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::OpticalTrainManager::Instance()->getTrainNames().contains(m_Helper->m_guidingTrain),
                             10000);

    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule(), 1000);
    GUIManager::Instance()->close();
}

void TestEkosGuideGPGPeriod::cleanupTestCase()
{
    QVERIFY(m_Helper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

QTEST_KSTARS_MAIN(TestEkosGuideGPGPeriod)

#endif // HAVE_INDI
