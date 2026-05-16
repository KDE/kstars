/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "kstars_ui_tests.h"
#include "test_ekos_simulator.h"
#include "ekos/guide/guide.h"

#include <cmath>
#include <limits>

#if defined(HAVE_INDI)

#include "test_ekos.h"
#include "ksmessagebox.h"

#include <QLabel>

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
    for (auto d : KStars::Instance()->findChildren<QDialog * >())
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

    const bool headless = !kstarsTestRequiresActiveWindow();
    int added = 0;
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
            {
                QTest::addRow("%s", name.toStdString().c_str())
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                if (headless && ++added >= 1)
                    break;
            }
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
            Q_EMIT dialog->reject();
        }
    });
    dms ra = dms::fromString(RA, false);
    dms de = dms::fromString(DEC, true);
    bool const slew_result = ekos->mountModule()->slew(ra.Hours(), de.Degrees());
    if (under_horizon_or_close_to_sun)
        QEXPECT_FAIL(NAME.toStdString().c_str(),
                     QString("Slew target '%1' is expected to be over the horizon during night time.").arg(NAME).toStdString().c_str(), Abort);
    QVERIFY(slew_result);
#endif

    // DEC slews are precise at plus/minus one arcsecond - expected or not?
    auto clampRA = [](const QString & value)
    {
        return CachingDms(value, false).arcsec();
    };
    auto clampDE = [](const QString & value)
    {
        return CachingDms(value, true).arcsec();
    };
    auto labelHasCoordinateText = [](const QLabel * label)
    {
        if (label == nullptr)
            return false;

        const QString text = label->text().trimmed();
        return !text.isEmpty() && text != QStringLiteral("--") && text != QStringLiteral("-");
    };
    const double targetRA = clampRA(RA);
    QLabel *raOut = ekos->findChild<QLabel *>("raOUT");
    if (raOut == nullptr)
        QWARN("raOUT label missing; falling back to mount equatorial coords.");
    auto raTolerance = [&]() -> double
    {
        return labelHasCoordinateText(raOut) ? 2.0 : 120.0;
    };
    auto currentRaArcsec = [&]() -> double
    {
        if (labelHasCoordinateText(raOut))
            return clampRA(raOut->text());

        QList<double> coords = ekos->mountModule()->equatorialCoords();
        if (coords.size() >= 2)
            return dms(coords[0] * 15.0).arcsec();
        return std::numeric_limits<double>::quiet_NaN();
    };
    QTRY_VERIFY_WITH_TIMEOUT(!std::isnan(currentRaArcsec()) && std::abs(targetRA - currentRaArcsec()) <= raTolerance(), 15000);
    QTest::qWait(100);
    const double currentRA = currentRaArcsec();
    if (!std::isnan(currentRA) && targetRA != currentRA)
        QWARN(QString("Target '%1', RA %2, slewed to RA %3 with offset RA %4")
              .arg(NAME)
              .arg(targetRA)
              .arg(currentRA)
              .arg(std::abs(targetRA - currentRA)).toStdString().c_str());

    const double targetDE = clampDE(DEC);
    QLabel *deOut = ekos->findChild<QLabel *>("decOUT");
    if (deOut == nullptr)
        QWARN("decOUT label missing; falling back to mount equatorial coords.");
    auto deTolerance = [&]() -> double
    {
        return labelHasCoordinateText(deOut) ? 2.0 : 120.0;
    };
    auto currentDeArcsec = [&]() -> double
    {
        if (labelHasCoordinateText(deOut))
            return clampDE(deOut->text());

        QList<double> coords = ekos->mountModule()->equatorialCoords();
        if (coords.size() >= 2)
            return dms(coords[1]).arcsec();
        return std::numeric_limits<double>::quiet_NaN();
    };
    QTRY_VERIFY_WITH_TIMEOUT(!std::isnan(currentDeArcsec()) && std::abs(targetDE - currentDeArcsec()) <= deTolerance(), 20000);
    QTest::qWait(100);
    const double currentDE = currentDeArcsec();
    if (!std::isnan(currentDE) && targetDE != currentDE)
        QWARN(QString("Target '%1', DEC %2, slewed to DEC %3 with coordinate offset DEC %4")
              .arg(NAME)
              .arg(targetDE)
              .arg(currentDE)
              .arg(std::abs(targetDE - currentDE)).toStdString().c_str());

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
