/*  KStars UI tests
    SPDX-FileCopyrightText: 2020 Eric Dejouhanet <eric.dejouhanet@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/


#include "test_ekos_capture.h"

#if defined(HAVE_INDI)

#include "Options.h"
#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "ekos/capture/capture.h"

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

    // Wait for Capture to come up, switch to Capture tab
    QTRY_VERIFY_WITH_TIMEOUT(ekos->captureModule() != nullptr, 5000);
    KTRY_EKOS_GADGET(QTabWidget, toolsWidget);
    toolsWidget->setCurrentWidget(ekos->captureModule());
    QTRY_COMPARE_WITH_TIMEOUT(toolsWidget->currentWidget(), ekos->captureModule(), 1000);
    // wait 0.5 sec to ensure that the capture module has been initialized
    QTest::qWait(500);
}

void TestEkosCapture::cleanup()
{
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);
}


void TestEkosCapture::testAddCaptureJob()
{
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, captureExposureN);
    KTRY_CAPTURE_GADGET(QSpinBox, captureCountN);
    KTRY_CAPTURE_GADGET(QSpinBox, captureDelayN);
    KTRY_CAPTURE_GADGET(QComboBox, FilterPosCombo);
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_GADGET(QPushButton, addToQueueB);
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);

    // These are the expected exhaustive list of frame names
    QString frameTypes[] = {"Light", "Bias", "Dark", "Flat"};
    int const frameTypeCount = sizeof(frameTypes) / sizeof(frameTypes[0]);

    // Verify our assumption about those frame types is correct
    QTRY_COMPARE_WITH_TIMEOUT(captureTypeS->count(), frameTypeCount, 5000);
    for (QString &frameType : frameTypes)
        if(captureTypeS->findText(frameType) < 0)
            QFAIL(qPrintable(QString("Frame '%1' expected by the test is not in the Capture frame list").arg(frameType)));

    // These are the expected exhaustive list of filter names from the default Filter Wheel Simulator
    QString filterTypes[] = {"Red", "Green", "Blue", "H_Alpha", "OIII", "SII", "LPR", "Luminance"};
    int const filterTypeCount = sizeof(filterTypes) / sizeof(filterTypes[0]);

    // Verify our assumption about those filters is correct - but wait for properties to be read from the device
    QTRY_COMPARE_WITH_TIMEOUT(FilterPosCombo->count(), filterTypeCount, 5000);
    for (QString &filterType : filterTypes)
        if(FilterPosCombo->findText(filterType) < 0)
            QFAIL(qPrintable(QString("Filter '%1' expected by the test is not in the Capture filter list").arg(filterType)));

    // Add a few capture jobs
    int const job_count = 50;
    QWARN("When clicking the 'Add' button, immediately starting to fill the next job overwrites the job being added.");
    for (int i = 0; i < job_count; i++)
    {
        captureExposureN->setValue(((double)i) / 10.0);
        captureCountN->setValue(i);
        captureDelayN->setValue(i);
        KTRY_CAPTURE_COMBO_SET(captureTypeS, frameTypes[i % frameTypeCount]);
        KTRY_CAPTURE_COMBO_SET(FilterPosCombo, filterTypes[i % filterTypeCount]);
        KTRY_CAPTURE_CLICK(addToQueueB);
        // Wait for the job to be added, else the next loop will overwrite the current job
        QTRY_COMPARE_WITH_TIMEOUT(queueTable->rowCount(), i + 1, 100);
    }

    // Count the number of rows
    QVERIFY(queueTable->rowCount() == job_count);

    // Check first capture job item, which could not accept exposure duration 0 and count 0
    QWARN("This test assumes that minimal exposure is 0.01 for the CCD Simulator.");
    queueTable->setCurrentCell(0, 1);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->currentRow() == 0, 1000);

    // It actually takes time before all signals syncing UI are processed, so wait for situation to settle
    QTRY_COMPARE_WITH_TIMEOUT(captureExposureN->value(), 0.01, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(captureCountN->value(), 1, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(captureDelayN->value(), 0, 1000);
    QTRY_COMPARE_WITH_TIMEOUT(captureTypeS->currentText(), frameTypes[0], 1000);
    QTRY_COMPARE_WITH_TIMEOUT(FilterPosCombo->currentText(), filterTypes[0], 1000);

    // Select a few cells and verify the feedback on the left side UI
    srand(42);
    for (int index = 1; index < job_count / 2; index += rand() % 4 + 1)
    {
        QVERIFY(index < queueTable->rowCount());
        queueTable->setCurrentCell(index, 1);
        QTRY_VERIFY_WITH_TIMEOUT(queueTable->currentRow() == index, 1000);

        // It actually takes time before all signals syncing UI are processed, so wait for situation to settle
        QTRY_VERIFY_WITH_TIMEOUT(std::fabs(captureExposureN->value() - static_cast<double>(index) / 10.0) < 0.1, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(captureCountN->value(), index, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(captureDelayN->value(), index, 1000);
        QTRY_COMPARE_WITH_TIMEOUT(captureTypeS->currentText(), frameTypes[index % frameTypeCount], 1000);
        QTRY_COMPARE_WITH_TIMEOUT(FilterPosCombo->currentText(), filterTypes[index % filterTypeCount], 1000);
    }

    // Remove all the rows
    // TODO: test edge cases with the selected row
    KTRY_CAPTURE_GADGET(QPushButton, removeFromQueueB);
    for (int i = job_count; 0 < i; i--)
    {
        KTRY_CAPTURE_CLICK(removeFromQueueB);
        QVERIFY(i - 1 == queueTable->rowCount());
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
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), 30000);

    QWARN("Test capturing to temporary is no longer valid since we don't create temporary files any more.");
    //    QWARN("When storing to a recognized system temporary folder, only one FITS file is created.");
    //    QTRY_VERIFY_WITH_TIMEOUT(searchFITS(QDir(destination.path())).count() == 1, 1000);
    //    QCOMPARE(searchFITS(QDir(destination.path()))[0], QString("Light_005.fits"));
}

void TestEkosCapture::testCaptureSingle()
{
    // We cannot use a system temporary due to what testCaptureToTemporary marks
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
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
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->searchFITS(QDir(destination.path())).count() == 1, 1000);
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[0].startsWith("Light_"));
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[0].endsWith("001.fits"));

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
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->searchFITS(QDir(destination.path())).count() == 2, 2000);
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[0].startsWith("Light_"));
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[0].endsWith("001.fits"));
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[1].startsWith("Light_"));
    QVERIFY(m_CaptureHelper->searchFITS(QDir(destination.path()))[1].endsWith("002.fits"));

    // TODO: test storage options
}

void TestEkosCapture::testCaptureMultiple()
{
    // We cannot use a system temporary due to what testCaptureToTemporary marks
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    // Add a few exposures
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 0, "Red", destination.path() + "/%T");
    KTRY_CAPTURE_ADD_LIGHT(0.7, 2, 0, "SII", destination.path() + "/%T");
    KTRY_CAPTURE_ADD_LIGHT(0.2, 5, 0, "Green", destination.path() + "/%T");
    KTRY_CAPTURE_ADD_LIGHT(0.9, 2, 0, "Luminance", destination.path() + "/%T");
    KTRY_CAPTURE_ADD_LIGHT(0.5, 1, 1, "H_Alpha", destination.path() + "/%T");
    QWARN("A sequence of exposures under 1 second will always take 1 second to capture each of them.");
    //size_t const duration = (500+0)*1+(700+0)*2+(200+0)*5+(900+0)*2+(500+1000)*1;
    size_t const duration = 1000 * (1 + 2 + 5 + 2 + 1);
    size_t const count = 1 + 2 + 5 + 2 + 1;

    // Start capturing and wait for procedure to end (visual icon changing) - leave enough time for frames to store
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"), duration * 2);

    // Verify the proper number of FITS file were created
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->searchFITS(QDir(destination.path())).count() == count, 1000);

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
    QTRY_VERIFY_WITH_TIMEOUT(!startB->icon().name().compare("media-playback-start"), duration * 2);

    // Verify the proper number of additional FITS file were created again
    QTRY_VERIFY_WITH_TIMEOUT(m_CaptureHelper->searchFITS(QDir(destination.path())).count() == 2 * count, 1000);

    // TODO: test storage options
}

void TestEkosCapture::testCaptureDarkFlats()
{
    // ensure that we know that the CCD has a shutter
    m_CaptureHelper->ensureCCDShutter(true);
    // We cannot use a system temporary due to what testCaptureToTemporary marks
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);

    // Set to flat first so that the flat calibration button is enabled
    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, "Flat");

    volatile bool dialogValidated = false;
    QTimer::singleShot(200, [&]
    {
        QDialog * const dialog = qobject_cast <QDialog*> (QApplication::activeModalWidget());
        if(dialog != nullptr)
        {
            // Set Flat duration to ADU
            QRadioButton *ADUC = dialog->findChild<QRadioButton *>("ADUC");
            QVERIFY(ADUC);
            ADUC->setChecked(true);

            // Set ADU to 4000
            QSpinBox *ADUValue = dialog->findChild<QSpinBox *>("ADUValue");
            QVERIFY(ADUValue);
            ADUValue->setValue(4000);


            QTest::mouseClick(dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok), Qt::LeftButton);
            dialogValidated = true;
        }
    });

    // Toggle flat calibration dialog
    KTRY_CAPTURE_CLICK(calibrationB);

    QTRY_VERIFY_WITH_TIMEOUT(dialogValidated, 1000);

    // Add flats
    KTRY_CAPTURE_ADD_FLAT(1, 2, 0, "Red", destination.path());
    KTRY_CAPTURE_ADD_FLAT(0.1, 3, 0, "Green", destination.path());

    // Generate Dark Flats
    KTRY_CAPTURE_CLICK(generateDarkFlatsB);

    // We should have 4 jobs in total (2 flats and 2 dark flats)
    QTRY_VERIFY2_WITH_TIMEOUT(queueTable->rowCount() == 4,
                              QString("Row number wrong, %1 expected, %2 found!").arg(4).arg(queueTable->rowCount()).toStdString().c_str(), 1000);

    // Start capturing and wait for procedure to end (visual icon changing) - leave enough time for frames to store
    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 500);

    // Accept any dialogs
    QTimer::singleShot(5000, [&]
    {
        auto dialog = qobject_cast <QMessageBox*> (QApplication::activeModalWidget());
        if(dialog)
        {
            dialog->accept();
        }
    });

    QTRY_VERIFY_WITH_TIMEOUT(!startB->icon().name().compare("media-playback-start"), 120000);

    // Verify the proper number of FITS file were created
    QTRY_COMPARE_WITH_TIMEOUT(m_CaptureHelper->searchFITS(QDir(destination.path())).count(), 10, 1000);

    // Verify dark flat job (3) matches flat job (1) time
    queueTable->selectRow(0);
    KTRY_CAPTURE_GADGET(NonLinearDoubleSpinBox, captureExposureN);
    auto flatDuration = captureExposureN->value();

    queueTable->selectRow(2);
    auto darkDuration = captureExposureN->value();

    QTRY_COMPARE(darkDuration, flatDuration);


}

QTEST_KSTARS_MAIN(TestEkosCapture)

#endif // HAVE_INDI
