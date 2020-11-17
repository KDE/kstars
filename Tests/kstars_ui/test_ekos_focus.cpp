/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_focus.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

TestEkosFocus::TestEkosFocus(QObject *parent) : QObject(parent)
{
}

void TestEkosFocus::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosFocus::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosFocus::init()
{

}

void TestEkosFocus::cleanup()
{

}

void TestEkosFocus::testDuplicateFocusRequest()
{
    KTRY_FOCUS_GADGET(QPushButton, startFocusB);
    KTRY_FOCUS_GADGET(QPushButton, stopFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(startFocusB->isEnabled(), 1000);
    QTRY_VERIFY_WITH_TIMEOUT(!stopFocusB->isEnabled(), 1000);
    QCOMPARE(Ekos::Manager::Instance()->focusModule()->status(), Ekos::FOCUS_IDLE);

    KTRY_FOCUS_DETECT(2, 1);

    // Prepare to detect the beginning of the autofocus_procedure
    volatile bool autofocus_started = false;
    connect(Ekos::Manager::Instance()->focusModule(), &Ekos::Focus::autofocusStarting, this, [&]() { autofocus_started = true; });

    // If we click the autofocus button, we receive a signal that the procedure starts, the state change and the button is disabled
    KTRY_FOCUS_CLICK(startFocusB);
    QTRY_VERIFY_WITH_TIMEOUT(autofocus_started, 500);
    QVERIFY(Ekos::Manager::Instance()->focusModule()->status() != Ekos::FOCUS_IDLE);
    QVERIFY(!startFocusB->isEnabled());
    QVERIFY(stopFocusB->isEnabled());

    // If we issue an autofocus command at that point (bypassing d-bus), no procedure should start
    autofocus_started = false;
    Ekos::Manager::Instance()->focusModule()->start();
    QTest::qWait(500);
    QVERIFY(!autofocus_started);

    // Stop the running autofocus
    KTRY_FOCUS_CLICK(stopFocusB);
    QTRY_COMPARE_WITH_TIMEOUT(Ekos::Manager::Instance()->focusModule()->status(), Ekos::FOCUS_ABORTED, 5000);
}

void TestEkosFocus::testStarDetection_data()
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

    std::list<char const *> Objects = { "Polaris", "Mizar", "M 51", "M 13", "M 47", "Vega", "NGC 2238", "M 81" };
    size_t count = 0;

    foreach (char const *name, Objects)
    {
        SkyObject const * const so = KStars::Instance()->data()->objectNamed(name);
        if (so != nullptr)
        {
            SkyObject o(*so);
            o.updateCoordsNow(&numbers);
            o.EquatorialToHorizontal(&LST, geo->lat());
            if (10.0 < o.alt().Degrees())
            {
                QTest::addRow("%s", name)
                        << name
                        << o.ra().toHMSString()
                        << o.dec().toDMSString();
                count++;
            }
            else QWARN(QString("Fixture '%1' altitude is '%2' degrees, discarding.").arg(name).arg(so->alt().Degrees()).toStdString().c_str());
        }
    }

    if (!count)
        QSKIP("No usable fixture objects, bypassing test.");
#endif
}

void TestEkosFocus::testStarDetection()
{
#if QT_VERSION < 0x050900
    QSKIP("Skipping fixture-based test on old QT version.");
#else
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    QFETCH(QString, NAME);
    QFETCH(QString, RA);
    QFETCH(QString, DEC);
    qDebug("Test focusing on '%s' RA '%s' DEC '%s'",
           NAME.toStdString().c_str(),
           RA.toStdString().c_str(),
           DEC.toStdString().c_str());

    // Just sync to RA/DEC to make the mount teleport to the object
    QWARN("During this test, the mount is not tracking - we leave it as is for the feature in the CCD simulator to trigger a failure.");
    QTRY_VERIFY_WITH_TIMEOUT(ekos->mountModule() != nullptr, 5000);
    QVERIFY(ekos->mountModule()->sync(RA, DEC));

    // Wait for Focus to come up, switch to Focus tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->focusModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->focusModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->focusModule(), 1000);

    QWARN("The Focus capture button toggles after Ekos is started, leave a bit of time for it to settle.");
    QTest::qWait(500);

    KTRY_FOCUS_GADGET(QLineEdit, starsOut);

    // Run the focus procedure for SEP
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0);
    KTRY_FOCUS_DETECT(1, 3);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Centroid
    KTRY_FOCUS_CONFIGURE("Centroid", "Iterative", 0.0, 100.0);
    KTRY_FOCUS_DETECT(1, 3);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Threshold - disable full-field
    KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0.0, 0.0);
    KTRY_FOCUS_DETECT(1, 3);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure for Gradient - disable full-field
    KTRY_FOCUS_CONFIGURE("Gradient", "Iterative", 0.0, 0.0);
    KTRY_FOCUS_DETECT(1, 3);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Longer exposure
    KTRY_FOCUS_CONFIGURE("SEP", "Iterative", 0.0, 100.0);
    KTRY_FOCUS_DETECT(8, 1);
    QTRY_VERIFY_WITH_TIMEOUT(starsOut->text().toInt() >= 1, 5000);

    // Run the focus procedure again to cover more code
    // Filtering annulus is independent of the detection method
    // Run the HFR average over three frames with SEP to avoid
    for (double inner = 0.0; inner < 100.0; inner += 43.0)
    {
        for (double outer = 100.0; inner < outer; outer -= 42.0)
        {
            KTRY_FOCUS_CONFIGURE("SEP", "Iterative", inner, outer);
            KTRY_FOCUS_DETECT(0.1, 2);
        }
    }

    // Test threshold - disable full-field
    for (double threshold = 80.0; threshold < 200.0; threshold += 13.3)
    {
        KTRY_FOCUS_GADGET(QDoubleSpinBox, thresholdSpin);
        thresholdSpin->setValue(threshold);
        KTRY_FOCUS_CONFIGURE("Threshold", "Iterative", 0, 0.0);
        KTRY_FOCUS_DETECT(0.1, 1);
    }
#endif
}

QTEST_KSTARS_MAIN(TestEkosFocus)

#endif // HAVE_INDI
