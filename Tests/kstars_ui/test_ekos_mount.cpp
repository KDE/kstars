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

#include <QLineEdit>
#include <QMetaObject>

namespace
{
bool g_mountTestDisabled = false;
QString g_mountTestDisabledReason;

QAbstractButton *asButton(QObject *object)
{
    return qobject_cast<QAbstractButton *>(object);
}

QLineEdit *asLineEdit(QObject *object)
{
    return qobject_cast<QLineEdit *>(object);
}

void clickControl(QObject *object)
{
    if (auto button = asButton(object))
        QTest::mouseClick(button, Qt::LeftButton);
    else
        object->setProperty("checked", true);
    QTest::qWait(20);
}

void clickButton(QObject *object)
{
    if (auto button = asButton(object))
        QTest::mouseClick(button, Qt::LeftButton);
    else
        QMetaObject::invokeMethod(object, "click", Qt::DirectConnection);
    QTest::qWait(20);
}

void setLineEditText(QObject *object, const QString &value)
{
    if (auto lineEdit = asLineEdit(object))
    {
        lineEdit->setText(value);
        QMetaObject::invokeMethod(lineEdit, "editingFinished", Qt::DirectConnection);
    }
    else
    {
        object->setProperty("text", value);
    }
    QTest::qWait(20);
}
}

#define SKIP_IF_MOUNT_TEST_DISABLED() \
    do \
    { \
        if (g_mountTestDisabled) \
            QSKIP(qPrintable(g_mountTestDisabledReason)); \
    } while (false)

TestEkosMount::TestEkosMount(QObject *parent) : QObject(parent)
{
}

void TestEkosMount::initTestCase()
{
    if (qEnvironmentVariableIsSet("CI") || !kstarsTestRequiresActiveWindow())
    {
        g_mountTestDisabled = true;
        g_mountTestDisabledReason = qEnvironmentVariableIsSet("CI")
                                    ? QStringLiteral("Mount control tests are skipped in CI because the active-window/GL setup is not reliable there.")
                                    : QStringLiteral("Mount control tests require an active window and cannot run headless/offscreen.");
        QWARN(qPrintable(g_mountTestDisabledReason));
        return;
    }

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
    auto findMountControl = []() -> QWidget *
    {
        for (QWidget *widget : QApplication::topLevelWidgets())
        {
            if (!widget)
                continue;
            const QString title = widget->windowTitle();
            if (widget->objectName() == QStringLiteral("MountControlPanel") ||
                    title == QStringLiteral("Mount Control Panel") ||
                    title == QStringLiteral("Mount Control"))
            {
                return widget;
            }
        }
        return nullptr;
    };

    QTRY_VERIFY_WITH_TIMEOUT((mountControl = findMountControl()) != nullptr, 5000);
    QTRY_VERIFY_WITH_TIMEOUT(mountControl->isVisible(), 1000);

    // get access to Mount Control widgets

    auto findObject = [&](const char *primary, const char *fallback) -> QObject *
    {
        QObject *object = mountControl->findChild<QObject*>(primary);
        if (!object && fallback)
            object = mountControl->findChild<QObject * >(fallback);
        return object;
    };

    raLabel = findObject("targetRALabel", "targetRALabelObject");
    QVERIFY(raLabel != nullptr);
    deLabel = findObject("targetDECLabel", "targetDELabelObject");
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
    if (g_mountTestDisabled)
        return;

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
    SKIP_IF_MOUNT_TEST_DISABLED();
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
    SKIP_IF_MOUNT_TEST_DISABLED();
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
        clickControl(coordRaDe);
        setLineEditText(raText, RA.toHMSString());
        setLineEditText(deText, Dec.toDMSString());

        // forward chain RA/Dec -> Az/Alt -> HA/Dec -> RA/Dec

        // RA/Dec -> Az/Alt
        QVariant RAText = raText->property("text");
        QVariant DEText = deText->property("text");
        clickControl(coordAzAl);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // Az/Alt -> HA/Dec
        RAText = raText->property("text");
        DEText = deText->property("text");
        clickControl(coordHaDe);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // HA/Dec -> RA/Dec
        clickControl(coordRaDe);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(RA.Hours() -  dms::fromString(raText->property("text").toString(),
                                          false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deText->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 1000);

        // backward chain RA/Dec -> HA/Dec -> Az/Alt -> RA/Dec

        // RA/Dec -> HA/Dec
        RAText = raText->property("text");
        DEText = deText->property("text");
        clickControl(coordHaDe);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // HA/Dec -> Az/Alt
        RAText = raText->property("text");
        DEText = deText->property("text");
        clickControl(coordAzAl);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // Az/Alt -> RA/Dec
        RAText = raText->property("text");
        DEText = deText->property("text");
        clickControl(coordRaDe);
        QTest::qWait(20);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(RA.Hours() -  dms::fromString(raText->property("text").toString(),
                                          false).Hours()) < hourPrecision, 1000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deText->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 1000);
    }
}

void TestEkosMount::testMountCtrlGoto()
{
    SKIP_IF_MOUNT_TEST_DISABLED();
    int i;

    // montecarlo test of GOTO with RA/Dec coordinate
    srand(1);
    clickControl(coordRaDe);
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
        QVariant DEText = deText->property("text");
        setLineEditText(raText, RA.toHMSString());
        setLineEditText(deText, Dec.toDMSString());
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check GOTO RA/Dec
        clickButton(gotoButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(RA.Hours() -  dms::fromString(raValue->property("text").toString(),
                                          false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of GOTO with Az/alt coordinate
    srand(1);
    clickControl(coordAzAl);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);

        // set coord input text boxes to random coord
        QTest::qWait(20);
        setLineEditText(raText, Alt.toDMSString());
        setLineEditText(deText, Az.toDMSString());

        // check GOTO Az/Alt
        clickButton(gotoButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Az.Degrees() -  dms::fromString(azValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision * 120.0, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Alt.Degrees() -  dms::fromString(altValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision * 120.0, 30000);
    }

    // montecarlo test of GOTO with HA/Dec coordinate
    srand(1);
    clickControl(coordHaDe);
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
        QVariant DEText = deText->property("text");
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        setLineEditText(raText, QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        setLineEditText(deText, Dec.toDMSString());
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check GOTO RA/Dec
        clickButton(gotoButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(HA.Hours() -  dms::fromString(haValue->property("text").toString(),
                                          false).Hours()) < hourPrecision * 120, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 30000);
    }
}

void TestEkosMount::testMountCtrlSync()
{
    SKIP_IF_MOUNT_TEST_DISABLED();
    int i;

    // montecarlo test of SYNC with RA/Dec coordinate
    srand(1);
    clickControl(coordRaDe);
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
        QVariant DEText = deText->property("text");
        setLineEditText(raText, RA.toHMSString());
        setLineEditText(deText, Dec.toDMSString());
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check SYNC RA/Dec
        clickButton(syncButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(RA.Hours() -  dms::fromString(raValue->property("text").toString(),
                                          false).Hours()) < hourPrecision, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 30000);
    }

    // montecarlo test of SYNC with Az/alt coordinate
    srand(1);
    clickControl(coordAzAl);
    for (i = 0; i < 3; i++)
    {
        // random coordinates above horizon
        dms Az(rand() * 359.999999999 / RAND_MAX);
        dms Alt(rand() * 89.0 / RAND_MAX + 1.0);
        KTELL("Using Alt = " + Alt.toDMSString() + " Az = " + Az.toDMSString());

        // set coord input text boxes to random coord
        QTest::qWait(20);
        setLineEditText(raText, Alt.toDMSString());
        setLineEditText(deText, Az.toDMSString());

        // check SYNC Az/Alt
        clickButton(syncButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Az.Degrees() -  dms::fromString(azValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision * 20, 20000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Alt.Degrees() -  dms::fromString(altValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision * 20, 20000);
    }

    // montecarlo test of SYNC with HA/Dec coordinate
    srand(1);
    clickControl(coordHaDe);
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
        QVariant DEText = deText->property("text");
        QChar sgn('+');
        if (HA.Hours() > 12.0)
        {
            HA.setH(24.0 - HA.Hours());
            sgn = '-';
        }
        setLineEditText(raText, QString("%1%2").arg(sgn).arg(HA.toHMSString()));
        setLineEditText(deText, Dec.toDMSString());
        QTRY_VERIFY_WITH_TIMEOUT(RAText != raText->property("text"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(DEText != deText->property("text"), 1000);

        // check SYNC RA/Dec
        clickButton(syncButton);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(HA.Hours() -  dms::fromString(haValue->property("text").toString(),
                                          false).Hours()) < hourPrecision * 2, 30000);
        QTRY_VERIFY_WITH_TIMEOUT(std::abs(Dec.Degrees() -  dms::fromString(deValue->property("text").toString(),
                                          true).Degrees()) < degreePrecision, 30000);
    }

    // close Mount Control
    KTRY_MOUNT_CLICK(mountToolBoxB);
#endif
}

QTEST_KSTARS_MAIN(TestEkosMount)

#endif // HAVE_INDI
