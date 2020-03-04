/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "test_ekos_simulator.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "ekos/manager.h"
#include "kstars.h"
#include "ksmessagebox.h"

TestEkosSimulator::TestEkosSimulator(QObject *parent) : QObject(parent)
{

}

void TestEkosSimulator::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

}

void TestEkosSimulator::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosSimulator::init()
{

}

void TestEkosSimulator::cleanup()
{

}


void TestEkosSimulator::testMountSlew_data()
{
    QTest::addColumn<QString>("NAME");
    QTest::addColumn<QString>("RA");
    QTest::addColumn<QString>("DEC");

    // Altitude computation taken from SchedulerJob::findAltitude
    GeoLocation * const geo = KStarsData::Instance()->geo();
    KStarsDateTime const now(KStarsData::Instance()->lt());
    KSNumbers const numbers(now.djd());
    CachingDms const LST = geo->GSTtoLST(geo->LTtoUT(now).gst());

    // Build a list of Messier objects, 5 degree over the horizon
    for (int i = 1; i < 103; i++)
    {
        QString name = QString("M %1").arg(i);
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (5.0 < so->alt().Degrees())
                QTest::addRow("%s", name.toStdString().c_str())
                    << name
                    << so->ra().toHMSString()
                    << so->dec().toDMSString();
        }
    }
}

void TestEkosSimulator::testMountSlew()
{
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
    QTRY_VERIFY_WITH_TIMEOUT(abs(clampRA(RA) - clampRA(raOut->text())) < 2, 15000);
    QTest::qWait(100);
    if (clampRA(RA) != clampRA(raOut->text()))
        QWARN(QString("Target '%1' slewed to with offset RA %2").arg(NAME).arg(RA).toStdString().c_str());

    QLineEdit * deOut = Ekos::Manager::Instance()->findChild<QLineEdit*>("decOUT");
    QVERIFY(deOut != nullptr);
    QTRY_VERIFY_WITH_TIMEOUT(abs(clampDE(DEC) - clampDE(deOut->text())) < 2, 15000);
    QTest::qWait(100);
    if (clampRA(DEC) != clampRA(deOut->text()))
        QWARN(QString("Target '%1' slewed to with coordinate offset DEC %2").arg(NAME).arg(DEC).toStdString().c_str());

    QVERIFY(Ekos::Manager::Instance()->mountModule()->abort());
}

#endif
