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

    // Get Mount Control widget
    QTRY_VERIFY_WITH_TIMEOUT([&]
    {
        for (QWidget *w : qApp->topLevelWidgets())
        {
            if (w && w->windowTitle().startsWith("Mount Control"))
            {
                mountControl = w;
                return true;
            }
        }
        return false;
    }(), 5000);

    raLabel = mountControl->findChild<QLabel *>("targetRALabel");
    QVERIFY(raLabel != nullptr);
    deLabel = mountControl->findChild<QLabel *>("targetDECLabel");
    QVERIFY(deLabel != nullptr);

    coordRaDe = mountControl->findChild<QRadioButton*>("equatorialCheckObject");
    QVERIFY(coordRaDe != nullptr);
    coordAzAl = mountControl->findChild<QRadioButton*>("horizontalCheckObject");
    QVERIFY(coordAzAl != nullptr);
    coordHaDe = mountControl->findChild<QRadioButton*>("haEquatorialCheckObject");
    QVERIFY(coordHaDe != nullptr);

    raText = mountControl->findChild<dmsBox*>("targetRATextObject");
    QVERIFY(raText != nullptr);
    deText = mountControl->findChild<dmsBox*>("targetDETextObject");
    QVERIFY(deText != nullptr);

    raValue = mountControl->findChild<QLabel*>("raValueObject");
    QVERIFY(raValue != nullptr);
    deValue = mountControl->findChild<QLabel*>("deValueObject");
    QVERIFY(deValue != nullptr);
    azValue = mountControl->findChild<QLabel*>("azValueObject");
    QVERIFY(azValue != nullptr);
    altValue = mountControl->findChild<QLabel*>("altValueObject");
    QVERIFY(altValue != nullptr);
    haValue = mountControl->findChild<QLabel*>("haValueObject");
    QVERIFY(haValue != nullptr);
    zaValue = mountControl->findChild<QLabel*>("zaValueObject");
    QVERIFY(zaValue != nullptr);
    gotoButton = mountControl->findChild<QPushButton*>("gotoButtonObject");
    QVERIFY(gotoButton != nullptr);
    syncButton = mountControl->findChild<QPushButton*>("syncButtonObject");
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
    coordAzAl->click();
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("AZ:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("AL:"), 1000);

    // check transition AZ/ALT -> HA/DE
    coordHaDe->click();
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("HA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition HA/DE -> RA/DE
    coordRaDe->click();
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("RA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition RA/DE -> HA/DE
    coordHaDe->click();
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("HA:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("DE:"), 1000);

    // check transition HA/DE -> AZ/AL
    coordAzAl->click();
    QTRY_COMPARE_WITH_TIMEOUT(raLabel->property("text"), QVariant("AZ:"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(deLabel->property("text"), QVariant("AL:"), 1000);

    // check transition AZ/AL -> RA/DE
    coordRaDe->click();
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

    // montecarlo test with cyclic coords transforms among all coord types
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
        coordRaDe->click();
        raText->setFocus();
        raText->setText(RA.toHMSString());
        deText->setFocus();
        deText->setText(Dec.toDMSString());
        deText->clearFocus();
        KTELL("Using RA = " + RA.toHMSString() + " Dec = " + Dec.toDMSString());

        // forward chain RA/Dec -> Az/Alt -> HA/Dec -> RA/Dec

        // RA/Dec -> Alt/Az
        QString RAText = raText->text();
        QString DEText = raText->text();
        coordAzAl->click();
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->text(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->text(), 1000);
        KTELL("Recalculated Alt = " + raText->text() + " Az = " + deText->text());

        // Alt/Az -> HA/Dec
        RAText = raText->text();
        DEText = raText->text();
        coordHaDe->click();
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->text(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->text(), 1000);
        KTELL("Recalculated Ha = " + raText->text() + " Dec = " + deText->text());

        // HA/Dec -> RA/Dec
        coordRaDe->click();
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raText->text(),
                                      false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deText->text(),
                                      true).Degrees()) < degreePrecision, 1000);
        KTELL("Back to RA = " + raText->text() + " Dec = " + deText->text());

        // backward chain RA/Dec -> HA/Dec -> Az/Alt -> RA/Dec

        // RA/Dec -> HA/Dec
        RAText = raText->text();
        DEText = raText->text();
        coordHaDe->click();
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->text(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->text(), 1000);
        KTELL("Recalculated Ha = " + raText->text() + " Dec = " + deText->text());

        // HA/Dec -> Az/Alt
        RAText = raText->text();
        DEText = raText->text();
        coordAzAl->click();
        QTest::qWait(2000);
        KTELL("Recalculated Alt = " + raText->text() + " Az = " + deText->text());
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->text(), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->text(), 1000);

        // Az/Alt -> RA/Dec
        RAText = raText->text();
        DEText = raText->text();
        coordRaDe->click();
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raText->text(),
                                      false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deText->text(),
                                      true).Degrees()) < degreePrecision, 1000);
        KTELL("Back to RA = " + raText->text() + " Dec = " + deText->text());
    }
}

void TestEkosMount::testMountCtrlGoto()
{
    int i;

    // montecarlo test of GOTO with RA/Dec coordinate
    srand(1);
    coordRaDe->click();
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
        QString RAText = raText->text();
        QString DEText = raText->text();
        raText->setFocus();
        raText->setText(RA.toHMSString());
        deText->setFocus();
        deText->setText(Dec.toDMSString());
        deText->clearFocus();

        // Goto
        gotoButton->click();

        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raValue->text(),
                                      false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->text(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of GOTO with Az/alt coordinate
    srand(1);
    coordAzAl->click();
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // set coord input text boxes to random coord
        QString RAText = raText->text();
        QString DEText = raText->text();
        raText->setFocus();
        raText->setText(Alt.toDMSString());
        deText->setFocus();
        deText->setText(Az.toDMSString());
        deText->clearFocus();
        QTest::qWait(100);
        // Goto
        KTELL("Using ALT = " + Alt.toDMSString() + "AZ = " + Az.toDMSString());
        gotoButton->click();

        QTRY_VERIFY_WITH_TIMEOUT(fabs(Az.Degrees() - dms::fromString(azValue->text(), true).Degrees()) +
                                 fabs(Alt.Degrees() - dms::fromString(altValue->text(), true).Degrees()) < degreePrecision * 7200.0, 30000);
    }

    // montecarlo test of GOTO with HA/Dec coordinate
    srand(1);
    coordHaDe->click();
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
        QString RAText = raText->text();
        QString DEText = raText->text();
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        raText->setFocus();
        raText->setText(QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        deText->setFocus();
        deText->setText(Dec.toDMSString());
        deText->clearFocus();

        // Goto
        gotoButton->click();
        KTELL("Using ALT = " + Alt.toDMSString() + "AZ = " + Az.toDMSString());

        QTRY_VERIFY_WITH_TIMEOUT(fabs(HA.Hours() -  dms::fromString(haValue->text(),
                                      false).Hours()) < hourPrecision * 120, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->text(),
                                      true).Degrees()) < degreePrecision, 30000);
    }
}

void TestEkosMount::testMountCtrlSync()
{
    int i;

    // montecarlo test of SYNC with RA/Dec coordinate
    srand(1);
    coordRaDe->click();
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
        QString RAText = raText->text();
        QString DEText = raText->text();
        raText->setFocus();
        raText->setText(RA.toHMSString());
        deText->setFocus();
        deText->setText(Dec.toDMSString());
        deText->clearFocus();

        // check SYNC RA/Dec
        dms ra = dms::fromString(raText->text(), false);
        auto de = dms::fromString(deText->text(), true);
        syncButton->click();
        QTRY_VERIFY_WITH_TIMEOUT(fabs(RA.Hours() -  dms::fromString(raValue->text(),
                                      false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->text(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of SYNC with Az/alt coordinate
    srand(1);
    coordAzAl->click();
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);
        KTELL("Using Alt = " + Alt.toDMSString() + " Az = " + Az.toDMSString());

        // set coord input text boxes to random coord
        QTest::qWait(20);
        raText->setFocus();
        raText->setText(Alt.toDMSString());
        deText->setFocus();
        deText->setText(Az.toDMSString());
        deText->clearFocus();

        // check SYNC Az/Alt
        syncButton->click();
        QTest::qWait(1000);
        KTELL("Synched to Alt = " + altValue->text() + " Az = " + azValue->text());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Az.Degrees() -  dms::fromString(azValue->text(),
                                      true).Degrees()) < degreePrecision * 20, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Alt.Degrees() -  dms::fromString(altValue->text(),
                                      true).Degrees()) < degreePrecision * 20, 20000);
    }

    // montecarlo test of SYNC with HA/Dec coordinate
    srand(1);
    coordHaDe->click();
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
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        KTELL("Using HA = " + sgn + HA.toHMSString() + " Dec = " + Dec.toDMSString());
        raText->setFocus();
        raText->setText(QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        deText->setFocus();
        deText->setText(Dec.toDMSString());
        deText->clearFocus();

        // check SYNC RA/Dec
        syncButton->click();
        QTest::qWait(1000);
        KTELL("Synched to HA = " + haValue->text() + " Dec = " + deValue->text());
        QTRY_VERIFY_WITH_TIMEOUT(fabs(HA.Hours() -  dms::fromString(haValue->text(),
                                      false).Hours()) < hourPrecision * 2, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(fabs(Dec.Degrees() -  dms::fromString(deValue->text(),
                                      true).Degrees()) < degreePrecision, 30000);
    }

    // close Mount Control
    KTRY_MOUNT_CLICK(mountToolBoxB);
#endif
}

QTEST_KSTARS_MAIN(TestEkosMount)

#endif // HAVE_INDI
