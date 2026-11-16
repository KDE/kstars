/*
    KStars UI tests for the Guide failure diagnosis

    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_guide_diagnosis.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "indi/indimount.h"
#include "ekos/guide/guide.h"
#include "ekos/auxiliary/diagnosismanager.h"
#include "ekos/auxiliary/opticaltrainmanager.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cmath>

namespace
{
// Kocab, as in the other UI tests
const double TARGET_RA = 14.845;
const double TARGET_DEC = 74.106;
// The default W/E guide rate of the Telescope Simulator
const double DEFAULT_GUIDE_RATE = 0.5;
}

TestEkosGuideDiagnosis::TestEkosGuideDiagnosis(QObject *parent) : QObject(parent)
{
    // Telescope Simulator, CCD Simulator and Guide Simulator, internal guider
    m_Helper = new TestEkosHelper("Internal");
}

bool TestEkosGuideDiagnosis::setRaGuideRate(double rate)
{
    auto mount = Ekos::OpticalTrainManager::Instance()->getMount(m_Helper->m_guidingTrain);
    KVERIFY_SUB(mount != nullptr);
    auto guideRate = mount->getNumber("GUIDE_RATE");
    KVERIFY_SUB(guideRate.isValid());
    auto we = guideRate.findWidgetByName("GUIDE_RATE_WE");
    KVERIFY_SUB(we != nullptr);
    we->setValue(rate);
    mount->sendNewProperty(guideRate);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(std::abs(mount->getNumber("GUIDE_RATE").findWidgetByName("GUIDE_RATE_WE")->getValue() - rate)
                                 < 1e-6, 5000);
    return true;
}

bool TestEkosGuideDiagnosis::startCalibration(int pulse)
{
    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    KWRAP_SUB(KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(guide, 1000));

    // Calibrate from scratch, quickly: 2 steps per axis give up after 14 iterations
    Options::setReuseGuideCalibration(false);
    Options::setSerializedCalibration("");
    Options::setAutoModeIterations(2);
    Options::setCalibrationPulseDuration(pulse);
    Options::setTwoAxisEnabled(true);
    guide->clearCalibration();

    KTRY_SET_COMBO_SUB(guide, opticalTrainCombo, m_Helper->m_guidingTrain);
    KTRY_SET_CHECKBOX_SUB(guide, guideAutoStar, true);
    KTRY_SET_COMBO_SUB(guide, guideSquareSize, "32");
    KTRY_SET_COMBO_SUB(guide, guideBinning, "1x1");
    KTRY_SET_DOUBLESPINBOX_SUB(guide, guideExposure, 1.0);

    KTRY_GADGET_SUB(guide, QPushButton, guideB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(guideB->isEnabled(), 10000);
    KTRY_CLICK_SUB(guide, guideB);
    KTRY_VERIFY_WITH_TIMEOUT_SUB(m_GuideStates.contains(Ekos::GUIDE_CALIBRATING), 60000);
    return true;
}

QStringList TestEkosGuideDiagnosis::lastReportItems() const
{
    QStringList ids;
    if (!m_Reports.isEmpty())
        for (const auto &item : m_Reports.last()["items"].toArray())
            ids << item.toObject()["id"].toString();
    return ids;
}

// A mount whose RA axis does not move: calibration fails with RA drift too short, and the diagnosis points at the
// RA axis of the mount (balance, clutch, power, cables) before any setting. Raising the calibration pulse from the
// diagnosis changes the option and calibrates again.
void TestEkosGuideDiagnosis::testRaDriftTooShort()
{
    QVERIFY(setRaGuideRate(0));
    QVERIFY(startCalibration());

    QTRY_VERIFY_WITH_TIMEOUT(!m_Reports.isEmpty(), 240000);
    const QJsonObject report = m_Reports.last();
    QCOMPARE(report["module"].toString(), QString("guide"));
    QCOMPARE(report["reasonKey"].toString(), QString("calibration_ra_drift_too_short"));
    QCOMPARE(report["train"].toString(), m_Helper->m_guidingTrain);
    const QStringList items = lastReportItems();
    QVERIFY2(!items.isEmpty() && items.first() == "check_ra_axis", qPrintable(QJsonDocument(report).toJson()));
    QVERIFY2(items.contains("guide_pulse_raise"), qPrintable(items.join(", ")));
    // The star did not move: the drift and the pulse are reported
    bool hasDrift = false;
    for (const auto &evidence : report["evidence"].toArray())
        hasDrift = hasDrift || evidence.toObject()["key"].toString() == "drift";
    QVERIFY2(hasDrift, qPrintable(QJsonDocument(report).toJson()));
    // The log gives the reason of the guider first, then its diagnosis
    int reasonLine = -1, diagnosisLine = -1;
    for (int i = 0; i < m_GuideLog.size(); i++)
    {
        if (reasonLine < 0 && m_GuideLog[i].contains("RA drift is too short"))
            reasonLine = i;
        if (diagnosisLine < 0 && m_GuideLog[i].contains("Guide failed:"))
            diagnosisLine = i;
    }
    QVERIFY2(reasonLine >= 0 && diagnosisLine >= 0 && reasonLine < diagnosisLine, qPrintable(m_GuideLog.join("\n")));

    // Raise the pulse: the option doubles and calibration starts again by itself
    m_GuideStates.clear();
    auto diagnosis = Ekos::Manager::Instance()->diagnosisManager();
    const QJsonObject result = diagnosis->apply(report["id"].toString(), "guide_pulse_raise", true);
    QVERIFY2(result["state"].toString() != "refused", qPrintable(QJsonDocument(result).toJson()));
    QCOMPARE(Options::calibrationPulseDuration(), 1000U);
    QTRY_VERIFY_WITH_TIMEOUT(m_GuideStates.contains(Ekos::GUIDE_CALIBRATING), 60000);

    // Stopping that calibration is not a failure: no new report
    const int reports = m_Reports.size();
    Ekos::Manager::Instance()->guideModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(m_GuideStates.contains(Ekos::GUIDE_ABORTED) || m_GuideStates.contains(Ekos::GUIDE_IDLE), 30000);
    QTest::qWait(2000);
    QCOMPARE(m_Reports.size(), reports);
}

// Stopping a calibration that works is not a failure and gets no diagnosis
void TestEkosGuideDiagnosis::testStopGetsNoDiagnosis()
{
    QVERIFY(setRaGuideRate(DEFAULT_GUIDE_RATE));
    QVERIFY(startCalibration());
    // Let it pulse a few times
    QTest::qWait(5000);
    Ekos::Manager::Instance()->guideModule()->abort();
    QTRY_VERIFY_WITH_TIMEOUT(m_GuideStates.contains(Ekos::GUIDE_ABORTED) || m_GuideStates.contains(Ekos::GUIDE_IDLE), 30000);
    QTest::qWait(2000);
    QVERIFY2(m_Reports.isEmpty(), qPrintable(QJsonDocument(m_Reports.value(0)).toJson()));
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosGuideDiagnosis::initTestCase()
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
    // Focal lengths and apertures of both scopes, which the guide calibration needs
    m_Helper->prepareMountModule();
    GUIManager::Instance()->close();

    // Track a field with stars
    QVERIFY(m_Helper->slewTo(TARGET_RA, TARGET_DEC, true));

    auto diagnosis = Ekos::Manager::Instance()->diagnosisManager();
    connect(diagnosis, &Ekos::DiagnosisManager::newDiagnosis, this, [this](const QString & module, const QJsonObject & report)
    {
        if (module == "guide")
            m_Reports << report;
    });
    connect(diagnosis, &Ekos::DiagnosisManager::diagnosisUpdated, this, [this](const QJsonObject & update)
    {
        m_Updates << update;
    });
    connect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newStatus, this, [this](Ekos::GuideState state)
    {
        m_GuideStates << state;
    });
    connect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newLog, this, [this](const QString & text)
    {
        m_GuideLog << text;
    });
}

void TestEkosGuideDiagnosis::cleanupTestCase()
{
    disconnect(Ekos::Manager::Instance()->diagnosisManager(), nullptr, this, nullptr);
    disconnect(Ekos::Manager::Instance()->guideModule(), nullptr, this, nullptr);
    QVERIFY(m_Helper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosGuideDiagnosis::init()
{
    m_Reports.clear();
    m_Updates.clear();
    m_GuideStates.clear();
    m_GuideLog.clear();
    Ekos::Manager::Instance()->diagnosisManager()->clear();
}

void TestEkosGuideDiagnosis::cleanup()
{
    Ekos::Manager::Instance()->guideModule()->abort();
    setRaGuideRate(DEFAULT_GUIDE_RATE);
}

QTEST_KSTARS_MAIN(TestEkosGuideDiagnosis)

#endif // HAVE_INDI
