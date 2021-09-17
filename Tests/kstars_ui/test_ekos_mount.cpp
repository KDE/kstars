/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>
    SPDX-FileCopyrightText: Fabrizio Pollastri <mxgbot@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* FP2020830
 * For now, tests covers only my changes to Mount Control.
 */

#include "test_ekos_mount.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "ekos/mount/mount.h"

TestEkosMount::TestEkosMount(QObject *parent) : QObject(parent)
{
}

void TestEkosMount::initTestCase()
{
    if (!qgetenv("CI").isEmpty())
        QSKIP("Skipping mount control test until QML/GL mixed window issue is resolved under EGLFS.");

    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    ekos = Ekos::Manager::Instance();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();

    // Wait for Mount to come up, switch to Mount tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->mountModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->mountModule(), 1000);

    // Open Mount Control window
    KTRY_MOUNT_GADGET(QPushButton, mountToolBoxB);
    KTRY_MOUNT_CLICK(mountToolBoxB);

    // Get Mount Control window object
    QWindow *mountControl = nullptr;
    for (QWindow *window : qApp->topLevelWindows())
    {
        if (window->title() != "Mount Control")
            mountControl = nullptr;
        else
        {
            mountControl = window;
            break;
        }
    }
    QVERIFY(mountControl != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(mountControl->isVisible(), 1000);

    // get access to Mount Control widgets

    raLabel = mountControl->findChild<QObject*>("targetRALabelObject");
    QVERIFY(raLabel != nullptr);
    deLabel = mountControl->findChild<QObject*>("targetDELabelObject");
    QVERIFY(deLabel != nullptr);

    coordRaDe = mountControl->findChild<QObject*>("equatorialCheckObject");
    QVERIFY(coordRaDe != nullptr);
    coordAzAl = mountControl->findChild<QObject*>("horizontalCheckObject");
    QVERIFY(coordAzAl != nullptr);
    coordHaDe = mountControl->findChild<QObject*>("haEquatorialCheckObject");
    QVERIFY(coordHaDe != nullptr);

    raText = mountControl->findChild<QObject*>("targetRATextObject");
    QVERIFY(raText != nullptr);
    deText = mountControl->findChild<QObject*>("targetDETextObject");
    QVERIFY(deText != nullptr);

    raValue = mountControl->findChild<QObject*>("raValueObject");
    QVERIFY(raValue != nullptr);
    deValue = mountControl->findChild<QObject*>("deValueObject");
    QVERIFY(deValue != nullptr);
    azValue = mountControl->findChild<QObject*>("azValueObject");
    QVERIFY(azValue != nullptr);
    altValue = mountControl->findChild<QObject*>("altValueObject");
    QVERIFY(altValue != nullptr);
    haValue = mountControl->findChild<QObject*>("haValueObject");
    QVERIFY(haValue != nullptr);
    zaValue = mountControl->findChild<QObject*>("zaValueObject");
    QVERIFY(zaValue != nullptr);
    gotoButton = mountControl->findChild<QObject*>("gotoButtonObject");
    QVERIFY(gotoButton != nullptr);
    syncButton = mountControl->findChild<QObject*>("syncButtonObject");
    QVERIFY(syncButton != nullptr);

}

void TestEkosMount::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosMount::init()
{
}

void TestEkosMount::cleanup()
{
}

void TestEkosMount::testMountCtrlCoordLabels()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    // Test proper setting of input coord label widget according to
    // type radio button selection

    // check initial setting RA/DE
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("RA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    return;
    // check transition RA/DE -> AZ/ALT
    coordAzAl->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("AZ:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("AL:"), 1000);

    // check transition AZ/ALT -> HA/DE
    coordHaDe->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("HA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition HA/DE -> RA/DE
    coordRaDe->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("RA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition RA/DE -> HA/DE
    coordHaDe->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("HA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition HA/DE -> AZ/AL
    coordAzAl->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("AZ:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("AL:"), 1000);

    // check transition AZ/AL -> RA/DE
    coordRaDe->setProperty("checked", true);
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("RA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);
}

void TestEkosMount::testMountCtrlCoordConversion()
{
    //  Test coord calculator with cyclic transform chain driven by
    //  cyclic changes of selection of coord type radiobutton

    dms RA, Dec;
    srand(1);
    int i;

    // montecarlo test with cyclic coords trasforms among all coord types
    for (i = 0; i < 10; i++)
    {
        // random coordinates: everywhere on the celestial sphere.
        RA.setD(rand() * 23.999999999 / RAND_MAX);
        Dec.setD(rand() * 180.0 / RAND_MAX - 90.);

        // random UTC range 2000-1-1 -> 2020-12-31
        KStarsDateTime JD(rand() * 7670.0 / RAND_MAX + 2451544.0);
        if (KStars::Instance() != nullptr) \
            if (KStars::Instance()->data() != nullptr) \
                KStars::Instance()->data()->clock()->setUTC(JD);

        // set coord input text boxes to random coord
        coordRaDe->setProperty("checked", true);
        raText->setProperty("text", QVariant(RA.toHMSString()));
        deText->setProperty("text", QVariant(Dec.toDMSString()));

        // forward chain RA/Dec -> Az/Alt -> HA/Dec -> RA/Dec

        // RA/Dec -> Az/Alt
        QVariant RAText = raText->property("text");
        QVariant DEText = raText->property("text");
        coordAzAl->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // Az/Alt -> HA/Dec
        RAText = raText->property("text");
        DEText = raText->property("text");
        coordHaDe->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // HA/Dec -> RA/Dec
        coordRaDe->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raText->property("text").toString(),
                                      false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deText->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 1000);

        // backward chain RA/Dec -> HA/Dec -> Az/Alt -> RA/Dec

        // RA/Dec -> HA/Dec
        RAText = raText->property("text");
        DEText = raText->property("text");
        coordHaDe->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // HA/Dec -> Az/Alt
        RAText = raText->property("text");
        DEText = raText->property("text");
        coordAzAl->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // Az/Alt -> RA/Dec
        RAText = raText->property("text");
        DEText = raText->property("text");
        coordRaDe->setProperty("checked", true);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raText->property("text").toString(),
                                      false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deText->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 1000);
    }
}

void TestEkosMount::testMountCtrlGoto()
{
    int i;

    // montecarlo test of GOTO with RA/Dec coordinate
    srand(1);
    coordRaDe->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // random UTC range 2000-1-1 -> 2020-12-31
        KStarsDateTime JD(rand() * 7670.0 / RAND_MAX + 2451544.0);
        if (KStars::Instance() != nullptr) \
            if (KStars::Instance()->data() != nullptr) \
                KStars::Instance()->data()->clock()->setUTC(JD);

        // convert Az/Alt -> RA/Dec
        SkyPoint sp;
        sp.setAz(Az);
        sp.setAlt(Alt);
        sp.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        dms RA  = sp.ra();
        dms Dec = sp.dec();

        // set coord input text boxes to random coord
        QTest::qWait(20);
        QVariant RAText = raText->property("text");
        QVariant DEText = raText->property("text");
        raText->setProperty("text", QVariant(RA.toHMSString()));
        deText->setProperty("text", QVariant(Dec.toDMSString()));
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check GOTO RA/Dec
        ekos->mountModule()->slew(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raValue->property("text").toString(),
                                      false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of GOTO with Az/alt coordinate
    srand(1);
    coordAzAl->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // set coord input text boxes to random coord
        QTest::qWait(20);
        raText->setProperty("text", QVariant(Az.toDMSString()));
        deText->setProperty("text", QVariant(Alt.toDMSString()));

        // check GOTO Az/Alt
        ekos->mountModule()->slew(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Az.Degrees() -  dms::fromString(azValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision * 120.0, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Alt.Degrees() -  dms::fromString(altValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision * 120.0, 30000);
    }

    // montecarlo test of GOTO with HA/Dec coordinate
    srand(1);
    coordHaDe->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // convert Az/Alt -> HA/Dec
        SkyPoint sp;
        sp.setAz(Az);
        sp.setAlt(Alt);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        sp.HorizontalToEquatorial(&lst, KStars::Instance()->data()->geo()->lat());
        dms RA  = sp.ra();
        dms HA = (lst - RA + dms(360.0)).reduce();
        dms Dec = sp.dec();

        // set coord input text boxes to random coord
        QTest::qWait(20);
        QVariant RAText = raText->property("text");
        QVariant DEText = raText->property("text");
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        raText->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        deText->setProperty("text", QVariant(Dec.toDMSString()));
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check GOTO RA/Dec
        ekos->mountModule()->slew(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(HA.Hours() -  dms::fromString(haValue->property("text").toString(),
                                      false).Hours()) < hourPrecision * 120, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 30000);
    }
}

void TestEkosMount::testMountCtrlSync()
{
    int i;

    // montecarlo test of SYNC with RA/Dec coordinate
    srand(1);
    coordRaDe->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // random UTC range 2000-1-1 -> 2020-12-31
        KStarsDateTime JD(rand() * 7670.0 / RAND_MAX + 2451544.0);
        if (KStars::Instance() != nullptr) \
            if (KStars::Instance()->data() != nullptr) \
                KStars::Instance()->data()->clock()->setUTC(JD);

        // convert Az/Alt -> RA/Dec
        SkyPoint sp;
        sp.setAz(Az);
        sp.setAlt(Alt);
        sp.HorizontalToEquatorial(KStars::Instance()->data()->lst(), KStars::Instance()->data()->geo()->lat());
        dms RA  = sp.ra();
        dms Dec = sp.dec();

        // set coord input text boxes to random coord
        QTest::qWait(20);
        QVariant RAText = raText->property("text");
        QVariant DEText = raText->property("text");
        raText->setProperty("text", QVariant(RA.toHMSString()));
        deText->setProperty("text", QVariant(Dec.toDMSString()));
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check SYNC RA/Dec
        ekos->mountModule()->sync(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raValue->property("text").toString(),
                                      false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of SYNC with Az/alt coordinate
    srand(1);
    coordAzAl->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // set coord input text boxes to random coord
        QTest::qWait(20);
        raText->setProperty("text", QVariant(Az.toDMSString()));
        deText->setProperty("text", QVariant(Alt.toDMSString()));

        // check SYNC Az/Alt
        ekos->mountModule()->sync(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Az.Degrees() -  dms::fromString(azValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision * 20, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Alt.Degrees() -  dms::fromString(altValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision * 20, 20000);
    }

    // montecarlo test of SYNC with HA/Dec coordinate
    srand(1);
    coordHaDe->setProperty("checked", true);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // convert Az/Alt -> HA/Dec
        SkyPoint sp;
        sp.setAz(Az);
        sp.setAlt(Alt);
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        sp.HorizontalToEquatorial(&lst, KStars::Instance()->data()->geo()->lat());
        dms RA  = sp.ra();
        dms HA = (lst - RA + dms(360.0)).reduce();
        dms Dec = sp.dec();

        // set coord input text boxes to random coord
        QTest::qWait(20);
        QVariant RAText = raText->property("text");
        QVariant DEText = raText->property("text");
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        raText->setProperty("text", QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        deText->setProperty("text", QVariant(Dec.toDMSString()));
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check SYNC RA/Dec
        ekos->mountModule()->sync(raText->property("text").toString(), deText->property("text").toString());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(HA.Hours() -  dms::fromString(haValue->property("text").toString(),
                                      false).Hours()) < hourPrecision * 2, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // close Mount Control
    KTRY_MOUNT_CLICK(mountToolBoxB);
#endif
}

QTEST_KSTARS_MAIN(TestEkosMount)

#endif // HAVE_INDI
