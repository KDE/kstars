/*
    KStars UI tests for a guide camera that changes its size at runtime

    SPDX-FileCopyrightText: 2026 Andreas R. <temp@ruthner.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_guide_resolution.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "Options.h"
#include "indi/guimanager.h"
#include "indi/indicamera.h"
#include "indi/indicamerachip.h"
#include "ekos/guide/guide.h"
#include "ekos/guide/internalguide/gmath.h"
#include "ekos/auxiliary/opticaltrainmanager.h"
#include "fitsviewer/fitsdata.h"

namespace
{
// Kocab, as in the other UI tests
const double TARGET_RA = 14.845;
const double TARGET_DEC = 74.106;
// The resolutions of the CCD_RESOLUTION switch of the CCD Simulator
const QString SMALL = "1280 x 1024";
const QString LARGE = "6000 x 4000";
const QSize SMALL_SIZE(1280, 1024);
const QSize LARGE_SIZE(6000, 4000);
}

TestEkosGuideResolution::TestEkosGuideResolution(QObject *parent) : QObject(parent)
{
    // Telescope Simulator, CCD Simulator and Guide Simulator, internal guider
    m_Helper = new TestEkosHelper("Internal");
}

ISD::CameraChip *TestEkosGuideResolution::guideChip() const
{
    return m_Camera ? m_Camera->getChip(ISD::CameraChip::PRIMARY_CCD) : nullptr;
}

bool TestEkosGuideResolution::setResolution(const QString &resolution, const QSize &size)
{
    KVERIFY_SUB(m_Camera != nullptr);

    // libindi updates the property in its own thread before the update reaches the modules on the main thread. Wait
    // for the update itself: this slot is connected after the Guide module's, so Guide has seen it when this runs.
    bool delivered = false;
    auto connection = connect(m_Camera, &ISD::Camera::propertyUpdated, this, [this, size, &delivered](INDI::Property prop)
    {
        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
        if (prop.isNameMatch("CCD_FRAME") &&
                guideChip()->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH) &&
                maxW == size.width() && maxH == size.height())
            delivered = true;
    });

    auto sw = m_Camera->getSwitch("CCD_RESOLUTION");
    KVERIFY_SUB(sw.isValid());
    auto element = sw.findWidgetByName(resolution.toLatin1().constData());
    KVERIFY2_SUB(element != nullptr, qPrintable(resolution));
    if (element->getState() != ISS_ON)
    {
        sw.reset();
        element->setState(ISS_ON);
        m_Camera->sendNewProperty(sw);
        // The driver reports the new chip size with the frame limits
        delivered = QTest::qWaitFor([&delivered]()
        {
            return delivered;
        }, 10000);
    }
    else
    {
        int minX, maxX, minY, maxY, minW, maxW, minH, maxH;
        delivered = guideChip()->getFrameMinMax(&minX, &maxX, &minY, &maxY, &minW, &maxW, &minH, &maxH) &&
                    maxW == size.width() && maxH == size.height();
    }
    disconnect(connection);
    KVERIFY2_SUB(delivered, qPrintable(QString("no CCD_FRAME update for %1").arg(resolution)));
    return true;
}

bool TestEkosGuideResolution::captureAndCheck(const QSize &expected)
{
    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    m_Images.clear();
    // capture() refuses while the previous capture is still being processed. Not inside a KTRY macro: those evaluate
    // the expression more than once, and each call that succeeds starts a capture.
    bool started = false;
    for (int i = 0; i < 100 && !started; i++)
    {
        started = guide->capture();
        if (!started)
            QTest::qWait(100);
    }
    KVERIFY2_SUB(started, "Guide refused to capture");
    KTRY_VERIFY_WITH_TIMEOUT_SUB(!m_Images.isEmpty(), 60000);
    int x, y, w, h, binX, binY;
    KVERIFY_SUB(guideChip()->getFrame(&x, &y, &w, &h));
    KVERIFY_SUB(guideChip()->getBinning(&binX, &binY));
    KVERIFY2_SUB(m_Images.first() == expected,
                 qPrintable(QString("guide frame %1 x %2, expected %3 x %4; CCD_FRAME (%5,%6) %7 x %8, binning %9x%10")
                            .arg(m_Images.first().width()).arg(m_Images.first().height())
                            .arg(expected.width()).arg(expected.height()).arg(x).arg(y).arg(w).arg(h).arg(binX).arg(binY)));

    // The driver took the frame Guide asked for
    auto frame = m_Camera->getNumber("CCD_FRAME");
    KVERIFY_SUB(frame.isValid());
    KVERIFY2_SUB(frame.getState() != IPS_ALERT, "the driver refused the guide frame");
    KVERIFY2_SUB(x == 0 && y == 0 && w == expected.width() && h == expected.height(),
                 qPrintable(QString("CCD_FRAME (%1,%2) %3 x %4").arg(x).arg(y).arg(w).arg(h)));
    return true;
}

void TestEkosGuideResolution::testResolutionChange_data()
{
    QTest::addColumn<QString>("from");
    QTest::addColumn<QSize>("fromSize");
    QTest::addColumn<QString>("to");
    QTest::addColumn<QSize>("toSize");

    QTest::newRow("larger") << SMALL << SMALL_SIZE << LARGE << LARGE_SIZE;
    QTest::newRow("smaller") << LARGE << LARGE_SIZE << SMALL << SMALL_SIZE;
    QTest::newRow("unchanged") << SMALL << SMALL_SIZE << SMALL << SMALL_SIZE;
}

// After a resolution switch in the driver the next guide frame has the new size, in both directions, and the guide log
// says so. Without a switch nothing changes.
void TestEkosGuideResolution::testResolutionChange()
{
    QFETCH(QString, from);
    QFETCH(QSize, fromSize);
    QFETCH(QString, to);
    QFETCH(QSize, toSize);

    QVERIFY(setResolution(from, fromSize));
    QVERIFY(captureAndCheck(fromSize));

    QStringList log;
    auto connection = connect(Ekos::Manager::Instance()->guideModule(), &Ekos::Guide::newLog, this, [&log](const QString & text)
    {
        log << text;
    });
    QVERIFY(setResolution(to, toSize));
    const bool captured = captureAndCheck(toSize);
    disconnect(connection);
    QVERIFY(captured);

    const bool reported = log.filter("Guide camera size changed").size() > 0;
    QVERIFY2(reported == (from != to), qPrintable(log.join("\n")));
}

/* *********************************************************************************
 *
 * Test infrastructure
 *
 * ********************************************************************************* */

void TestEkosGuideResolution::initTestCase()
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
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::OpticalTrainManager::Instance()->getTrainNames().contains(m_Helper->m_primaryTrain),
                             10000);
    m_Helper->prepareMountModule();
    GUIManager::Instance()->close();

    QVERIFY(m_Helper->slewTo(TARGET_RA, TARGET_DEC, true));

    Ekos::Guide *guide = Ekos::Manager::Instance()->guideModule();
    KTRY_SWITCH_TO_MODULE_WITH_TIMEOUT(guide, 1000);
    // Only the CCD Simulator has the resolution switch (the Guide Simulator doesn't): guide with the primary train,
    // on its main chip (the switch doesn't change the guide head)
    Options::setUseGuideHead(false);
    KTRY_SET_COMBO(guide, opticalTrainCombo, m_Helper->m_primaryTrain);
    QTRY_COMPARE_WITH_TIMEOUT(guide->camera(), m_Helper->m_CCDDevice, 10000);
    m_Camera = Ekos::OpticalTrainManager::Instance()->getCamera(m_Helper->m_primaryTrain);
    QVERIFY(m_Camera != nullptr);
    QVERIFY(m_Camera->getSwitch("CCD_RESOLUTION").isValid());

    // SEP MultiStar as in the report, full frame (Subframe only switches the local star search there)
    Options::setGuideAlgorithm(SEP_MULTISTAR);
    KTRY_SET_CHECKBOX(guide, guideSubframe, false);
    KTRY_SET_COMBO(guide, guideBinning, "1x1");
    KTRY_SET_DOUBLESPINBOX(guide, guideExposure, 0.5);
    Options::setGuideDarkFrame(false);

    connect(m_Camera, &ISD::Camera::newImage, this, [this](const QSharedPointer<FITSData> &data)
    {
        if (data)
        {
            m_Images << QSize(data->width(), data->height());
        }
    });
}

void TestEkosGuideResolution::cleanupTestCase()
{
    if (m_Camera)
        disconnect(m_Camera, nullptr, this, nullptr);
    QVERIFY(m_Helper->shutdownEkosProfile());
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosGuideResolution::init()
{
}

void TestEkosGuideResolution::cleanup()
{
    Ekos::Manager::Instance()->guideModule()->abort();
}

QTEST_KSTARS_MAIN(TestEkosGuideResolution)

#endif // HAVE_INDI
