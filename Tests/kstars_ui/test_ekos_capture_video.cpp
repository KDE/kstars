/*  KStars UI tests
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "test_ekos_capture_video.h"

#if defined(HAVE_INDI)

#include "kstars_ui_tests.h"
#include "test_ekos.h"
#include "test_ekos_simulator.h"
#include "ekos/capture/capture.h"
#include "ekos/capture/capturetypes.h"

#include <QDirIterator>

TestEkosCaptureVideo::TestEkosCaptureVideo(QObject *parent) : QObject(parent)
{
}

void TestEkosCaptureVideo::initTestCase()
{
    KVERIFY_EKOS_IS_HIDDEN();
    KTRY_OPEN_EKOS();
    KVERIFY_EKOS_IS_OPENED();
    KTRY_EKOS_START_SIMULATORS();

    // HACK: Reset clock to initial conditions
    KHACK_RESET_EKOS_TIME();
}

void TestEkosCaptureVideo::cleanupTestCase()
{
    KTRY_EKOS_STOP_SIMULATORS();
    KTRY_CLOSE_EKOS();
    KVERIFY_EKOS_IS_HIDDEN();
}

void TestEkosCaptureVideo::init()
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

void TestEkosCaptureVideo::cleanup()
{
    Ekos::Manager::Instance()->captureModule()->clearSequenceQueue();
    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 0, 2000);
}

void TestEkosCaptureVideo::testCaptureVideoLongRecording()
{
    QTemporaryDir destination(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/test-XXXXXX");
    QVERIFY(destination.isValid());
    QVERIFY(destination.autoRemove());

    // 400 frames of 0.5s each record for 200s, which is longer than CAPTURE_TIMEOUT_THRESHOLD (180s)
    const double exposure = 0.5;
    const int frames = 400;

    KTRY_CAPTURE_GADGET(QComboBox, captureTypeS);
    KTRY_CAPTURE_COMBO_SET(captureTypeS, CAPTURE_TYPE_VIDEO);
    KTRY_CAPTURE_GADGET(QComboBox, captureFormatS);
    KTRY_CAPTURE_COMBO_SET(captureFormatS, "SER");
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, captureExposureN);
    captureExposureN->setValue(exposure);
    KTRY_CAPTURE_GADGET(QComboBox, videoDurationUnitCB);
    // index 1 is the duration in number of frames
    videoDurationUnitCB->setCurrentIndex(1);
    KTRY_CAPTURE_GADGET(QDoubleSpinBox, videoDurationSB);
    videoDurationSB->setValue(frames);
    KTRY_CAPTURE_GADGET(QLineEdit, targetNameT);
    targetNameT->setText("video");
    KTRY_CAPTURE_GADGET(QLineEdit, fileDirectoryT);
    fileDirectoryT->setText(destination.path());
    KTRY_CAPTURE_GADGET(QLineEdit, fileRemoteDirT);
    fileRemoteDirT->setText(destination.path());

    KTRY_CAPTURE_GADGET(QTableWidget, queueTable);
    KTRY_CAPTURE_CLICK(addToQueueB);
    QTRY_VERIFY_WITH_TIMEOUT(queueTable->rowCount() == 1, 1000);

    Ekos::Capture *capture = Ekos::Manager::Instance()->captureModule();
    QStringList logs;
    QMetaObject::Connection logConnection = connect(capture, &Ekos::Capture::newLog, this, [&logs](const QString & text)
    {
        logs.append(text);
    });

    KTRY_CAPTURE_GADGET(QPushButton, startB);
    QCOMPARE(startB->icon().name(), QString("media-playback-start"));
    KTRY_CAPTURE_CLICK(startB);
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-stop"), 5000);

    // The recording must complete on its own, without the exposure watchdog restarting it as a still exposure
    QTRY_COMPARE_WITH_TIMEOUT(startB->icon().name(), QString("media-playback-start"),
                              static_cast<int>(frames * exposure * 1000) + 120000);
    disconnect(logConnection);

    for (const auto &line : logs)
        QVERIFY2(!line.contains("Exposure timeout"), qPrintable(line));

    // The SER file is the only file recorded
    const QStringList files = m_CaptureHelper->searchFITS(QDir(destination.path()));
    QCOMPARE(files.count(), 1);
    QVERIFY2(files[0].endsWith(".ser"), qPrintable(files[0]));

    // A restarted still exposure saves a FITS file in the previous remote directory of the camera, the home folder
    QDirIterator it(QStandardPaths::writableLocation(QStandardPaths::HomeLocation), QStringList() << "*.fits",
                    QDir::Files);
    QVERIFY2(!it.hasNext(), qPrintable(it.hasNext() ? it.next() : QString()));
}

QTEST_KSTARS_MAIN(TestEkosCaptureVideo)

#endif // HAVE_INDI
