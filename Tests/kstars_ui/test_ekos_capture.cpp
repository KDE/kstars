/*  KStars UI tests
    Copyright (C) 2020
    Eric Dejouhanet <eric.dejouhanet@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */


#include "test_ekos_capture.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"

TestEkosCapture::TestEkosCapture(QObject *parent) : QObject(parent)
{
}

void TestEkosCapture::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosCapture::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosCapture::init()
{
    Ekos::Manager * const ekos = Ekos::Manager::Instance();

    // Wait for Capture to come up, switch to Focus tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->captureModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->captureModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->captureModule(), 1000);
}

void TestEkosCapture::cleanup()
{
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);
}

QStringList TestEkosCapture::searchFITS(QDir const &dir) const
{
    QStringList list = dir.entryList(QDir::Files);

    //foreach (auto &f, list)
    //    QWARN(QString(dir.path()+'/'+f).toStdString().c_str());

    foreach (auto &d, dir.entryList(QDir::NoDotAndDotDot|QDir::Dirs))
        list.append(searchFITS(QDir(dir.path() + '/' + d)));

    return list;
}

void TestEkosCapture::testAddCaptureJob()
{
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, exposureIN);
    KTRY_CAPTURE_GADGET(QSpinBox, countIN);
    KTRY_CAPTURE_GADGET(QSpinBox, delayIN);
    KTRY_CAPTURE_GADGET(QComboBox, FilterPosCombo);
    KTRY_CAPTURE_GADGET(QComboBox, frameTypeCombo);
    KTRY_CAPTURE_GADGET(QPushButton, addToQueueB);

    QString frameTypes[] = {"Light", "Dark", "Flat"};
    int const frameTypeCount = sizeof(frameTypes)/sizeof(frameTypes[0]);

    QString filterTypes[] = {"Red", "Green", "Blue", "H_Alpha", "OIII", "SII", "LPR", "Luminance"};
    int const filterTypeCount = sizeof(filterTypes)/sizeof(filterTypes[0]);

    // Add a few capture jobs
    int const job_count = 50;
    for (int i = 0; i < job_count; i++)
    {
        exposureIN->setValue(((double)i)/10.0);
        countIN->setValue(i);
        delayIN->setValue(i);
        KTRY_CAPTURE_COMBO_SET(frameTypeCombo, frameTypes[i % frameTypeCount]);
        KTRY_CAPTURE_COMBO_SET(FilterPosCombo, filterTypes[i % filterTypeCount]);
        KTRY_CAPTURE_CLICK(addToQueueB);
    }

    // Count the number of rows
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QVERIFY(queueTable->rowCount() == job_count);

    // Select a few cells and verify the feedback on the left side UI
    srand(42);
    for (int i = 0; i < job_count/2; i++)
    {
        int const index = rand() % job_count;

        QVERIFY(index < queueTable->rowCount());
        queueTable->setCurrentCell(index, 1);
        QTRY_VERIFY_WITH_TIMEOUT(queueTable->currentRow() == index, 1000);
        QCOMPARE(exposureIN->value(), ((double)index)/10.0);
        QCOMPARE(countIN->value(), index);
        QCOMPARE(delayIN->value(), index);
        QCOMPARE(frameTypeCombo->currentText(), frameTypes[index % frameTypeCount]);
        QCOMPARE(FilterPosCombo->currentText(), filterTypes[index % filterTypeCount]);
    }

    // Remove all the rows
    // TODO: test edge cases with the selected row
    KTRY_CAPTURE_GADGET(QPushButton, removeFromQueueB);
    for (int i = job_count; 0 < i; i--)
    {
        KTRY_CAPTURE_CLICK(removeFromQueueB);
        QVERIFY(i-1 == queueTable->rowCount());
    }
}

void TestEkosCapture::testCaptureToTemporary()
{
    QTemporaryDir destination;
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    // Add five exposures
    KTRY_CAPTURE_ADD_LIGHT(0.1, 5, 0, "Red", destination.path());

    // Start capturing and wait for procedure to end (visual icon changing)
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), 5000*2);

    QWARN("When storing to a recognized system temporary folder, only one FITS file is created.");
    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == 1, 1000);
    QCOMPARE(searchFITS(QDir(destination.path()))[0], QString("Light_005.fits"));
}

void TestEkosCapture::testCaptureSingle()
{
    // We cannot use a system temporary due to what testCaptureToTemporary marks
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)+"/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    // Add an exposure
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 0, "Red", destination.path());

    // Start capturing and wait for procedure to end (visual icon changing)
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), 2000);

    // Verify a FITS file was created
    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == 1, 1000);
    QVERIFY(searchFITS(QDir(destination.path())).contains("Light_001.fits"));

    // Reset sequence state - this makes a confirmation dialog appear
    volatile bool dialogValidated = false;
    QTimer::singleShot(200, [&]
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if(dialog != nullptr)
        {
            QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Yes), Qt::LeftButton);
            dialogValidated = true;
        }
    });
    KTRY_CAPTURE_CLICK(resetB);
    QTRY_VERIFY_WITH_TIMEOUT(dialogValidated, 1000);

    // Capture again
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), 2000);

    // Verify an additional FITS file was created - asynchronously eventually
    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == 2, 2000);
    QVERIFY(searchFITS(QDir(destination.path())).contains("Light_001.fits"));
    QVERIFY(searchFITS(QDir(destination.path())).contains("Light_002.fits"));

    // TODO: test storage options
}

void TestEkosCapture::testCaptureMultiple()
{
    // We cannot use a system temporary due to what testCaptureToTemporary marks
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation)+"/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    // Add a few exposures
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 0, "Red", destination.path());
    KTRY_CAPTURE_ADD_LIGHT(0.7, 2, 0, "SII", destination.path());
    KTRY_CAPTURE_ADD_LIGHT(0.2, 5, 0, "Green", destination.path());
    KTRY_CAPTURE_ADD_LIGHT(0.9, 2, 0, "Luminance", destination.path());
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 1, "H_Alpha", destination.path());
    QWARN("A sequence of exposures under 1 second will always take 1 second to capture each of them.");
    //size_t const duration = (500+0)*1+(700+0)*2+(200+0)*5+(900+0)*2+(500+1000)*1;
    size_t const duration = 1000*(1+2+5+2+1);
    size_t const count = 1+2+5+2+1;

    // Start capturing and wait for procedure to end (visual icon changing) - leave enough time for frames to store
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), duration*2);

    // Verify the proper number of FITS file were created
    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == count, 1000);

    // Reset sequence state - this makes a confirmation dialog appear
    volatile bool dialogValidated = false;
    QTimer::singleShot(200, [&]
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if(dialog != nullptr)
        {
            QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Yes), Qt::LeftButton);
            dialogValidated = true;
        }
    });
    KTRY_CAPTURE_CLICK(resetB);
    QTRY_VERIFY_WITH_TIMEOUT(dialogValidated, 500);

    // Capture again
    KTRY_CAPTURE_CLICK(startB);
    QTRY_VERIFY_WITH_TIMEOUT(!startB->icon().name().compare("media-playback-stop"), 500);
    QTRY_VERIFY_WITH_TIMEOUT(!startB->icon().name().compare("media-playback-start"), duration*1.2);

    // Verify the proper number of additional FITS file were created again
    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == 2*count, 1000);

    // TODO: test storage options
}

QTEST_KSTARS_MAIN(TestEkosCapture)

#endif // HAVE_INDI
