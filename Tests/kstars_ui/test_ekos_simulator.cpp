/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstars_ui_tests.h"
#include "test_ekos_simulator.h"

#if defined(HAVE_INDI)

#include "test_ekos.h"
#include "ksmessagebox.h"

TestEkosSimulator::TestEkosSimulator(QObject *parent) : QObject(parent)
{
}

void TestEkosSimulator::initTestCase()
{
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
}

void TestEkosSimulator::cleanupTestCase()
{
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosSimulator::init()
{
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosSimulator::cleanup()
{
    foreach (QDialog * d, KStars::Instance()->findChildren<QDialog*>())
        if (d->isVisible())
            d->hide();

    KTRY_EKOS_STOP_SIMULATORS();
}


void TestEkosSimulator::testMountSlew_data()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    // Build a list of Messier objects, 5 degree over the horizon
    for (int i = 1; i < 103; i += 20)
    {
        QString name = QString("M %1").arg(i);
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (5.0 < o.alt().Degrees())
                QTest::addRow("%s", name.toStdString().c_str())
                    << name
                    << o.ra().toHMSString()
                    << o.dec().toDMSString();
        }
    }
#endif
}

void TestEkosSimulator::testMountSlew()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);
    qDebug("Test slewing to '%s' RA '%s' DEC '%s'",
           NAME.toStdString().c_str(),
           RA.toStdString().c_str(),
           DEC.toStdString().c_str());

#if 0
    // In the mount tab, open the mount control
    KTRY_EKOS_CLICK("mountToolBoxB");
    QTextObject * raInput = Ekos::Manager::Instance()->findChild<QTextObject*>("targetRATextObject");
    QVERIFY(raInput != nullptr);
    raInput->setProperty("text", QVariant("07h 37m 30s"));
    QTest::qWait(1000);
    QTextObject * deInput = Ekos::Manager::Instance()->findChild<QTextObject*>("targetDETextObject");
    QVERIFY(deInput != nullptr);
    deInput->setProperty("text", QVariant("-14° 31' 50"));
    QTest::qWait(1000);

    // Too bad, not accessible...
    QPushButton * gotoButton = Ekos::Manager::Instance()->findChild<QPushButton*>("");

    QTest::qWait(5000);
    KTRY_EKOS_CLICK("mountToolBoxB");
#else
    QVERIFY(ekos->mountModule()->abort());
    // Catch the unexpected "under horizon" and the "sun is close" dialogs
    // This will not catch the altitude security interval
    bool under_horizon_or_close_to_sun = false;
    QTimer::singleShot(1000, [&]
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if(dialog != nullptr)
        {
            under_horizon_or_close_to_sun = true;
            emit dialog->reject();
        }
    });
    bool const slew_result = ekos->mountModule()->slew(RA, DEC);
    if (under_horizon_or_close_to_sun)
        QEXPECT_FAIL(NAME.toStdString().c_str(), QString("Slew target '%1' is expected to be over the horizon during night time.").arg(NAME).toStdString().c_str(), Abort);
    QVERIFY(slew_result);
#endif

    // DEC slews are precise at plus/minus one arcsecond - expected or not?
    auto clampRA = [](QString v) { return CachingDms(v, false).arcsec(); };
    auto clampDE = [](QString v) { return CachingDms(v, true).arcsec(); };

    QLineEdit * raOut = ekos->findChild<QLineEdit*>("raOUT");
    QVERIFY(raOut != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(abs(clampRA(RA) - clampRA(raOut->text())) <= 2, 15000);
    QTest::qWait(100);
    if (clampRA(RA) != clampRA(raOut->text()))
        QWARN(QString("Target '%1', RA %2, slewed to RA %3 with offset RA %4")
              .arg(NAME)
              .arg(clampRA(RA))
              .arg(clampRA(raOut->text()))
              .arg(abs(clampRA(RA) - clampRA(raOut->text()))).toStdString().c_str());

    QLineEdit * deOut = Ekos::Manager::Instance()->findChild<QLineEdit*>("decOUT");
    QVERIFY(deOut != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(abs(clampDE(DEC) - clampDE(deOut->text())) <= 2, 20000);
    QTest::qWait(100);
    if (clampDE(DEC) != clampDE(deOut->text()))
        QWARN(QString("Target '%1', DEC %2, slewed to DEC %3 with coordinate offset DEC %4")
              .arg(NAME)
              .arg(clampDE(DEC))
              .arg(clampDE(deOut->text()))
              .arg(abs(clampDE(DEC) - clampDE(deOut->text()))).toStdString().c_str());

    QVERIFY(Ekos::Manager::Instance()->mountModule()->abort());
#endif
}

void TestEkosSimulator::testColorSchemes_data()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("FILENAME");

    QTest::addRow("Classic, friendly name") << "Default Colors" << "classic.colors";
    QTest::addRow("Chart, friendly name") << "Star Chart" << "chart.colors",
    QTest::addRow("Night, friendly name") << "Night Vision" << "night.colors",
    QTest::addRow("Moonless, friendly name") << "Moonless Night" << "moonless-night.colors";

    QTest::addRow("Classic, short name") << "classic" << "classic.colors";
    QTest::addRow("Chart, short name") << "chart" << "chart.colors",
    QTest::addRow("Night, short name") << "night" << "night.colors",
    QTest::addRow("Moonless, short name") << "moonless-night" << "moonless-night.colors";

    QTest::addRow("Classic, full name") << "classic.colors" << "classic.colors";
    QTest::addRow("Chart, full name") << "chart.colors" << "chart.colors",
    QTest::addRow("Night, full name") << "night.colors" << "night.colors",
    QTest::addRow("Moonless, full name") << "moonless-night.colors" << "moonless-night.colors";
#endif
}

void TestEkosSimulator::testColorSchemes()
{
#if QT_VERSION < QT_VERSION_CHECK(5,9,0)
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    QFETCH(QString, NAME);
    QFETCH(QString, FILENAME);

    KStars::Instance()->loadColorScheme(NAME);
    QTRY_COMPARE_WITH_TIMEOUT(KStars::Instance()->colorScheme(), FILENAME, 1000);
    QVERIFY(KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError").isValid());
    QTRY_VERIFY_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule() != nullptr, 5000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(0)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(1)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("DEGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(2)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("RAGuideError"), 1000);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->guideModule()->driftGraph->graph(3)->pen().color(),
                              KStars::Instance()->data()->colorScheme()->colorNamed("DEGuideError"), 1000);
#endif
}

QTEST_KSTARS_MAIN(TestEkosSimulator)

#endif // HAVE_INDI
