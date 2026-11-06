/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/cameraprocess.h"
#include "ekos/capture/camerastate.h"
#include "ekos/capture/capturedeviceadaptor.h"
#include "ekos/capture/sequencejob.h"
#include "ekos/manager/meridianflipstate.h"
#include "indi/clientmanager.h"
#include "indi/deviceinfo.h"
#include "indi/driverinfo.h"
#include "indi/indicamera.h"
#include "indi/indistd.h"
#include "kstarsdata.h"

#include <parentdevice.h>

#include "../../testhelpers.h"

#include <QSignalSpy>
#include <QTest>

class TestCameraProcess : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testResetFrameWithoutCamera();
        void testActiveCameraDeleted();

        void testCaptureTimeoutMS_data();
        void testCaptureTimeoutMS();

        void testIsRecording_data();
        void testIsRecording();

        void testVideoTimeoutWhileRecording();
        void testVideoTimeoutWhenNotRecording();

    private:
        QSharedPointer<ISD::GenericDevice> createDevice(const QString &name);
        QSharedPointer<ISD::GenericDevice> createRecordingDevice(const QString &name, IPState state,
                const QString &onSwitch);
        QSharedPointer<Ekos::SequenceJob> createVideoJob(const QSharedPointer<Ekos::CaptureDeviceAdaptor> &devices,
                const QSharedPointer<Ekos::CameraState> &state, double exposure, int frames);

        ClientManager m_ClientManager;
        QList<QSharedPointer<INDI::ParentDevice>> m_BaseDevices;
};

void TestCameraProcess::initTestCase()
{
    KTEST_BEGIN();
    // INDI devices follow the KStars clock
    QVERIFY(KStarsData::Create() != nullptr);
}

void TestCameraProcess::cleanupTestCase()
{
    KTEST_END();
}

// Create an INDI device that is not connected to any server
QSharedPointer<ISD::GenericDevice> TestCameraProcess::createDevice(const QString &name)
{
    QSharedPointer<DriverInfo> driver(new DriverInfo(name));
    INDI::ParentDevice baseDevice(INDI::ParentDevice::Valid);
    baseDevice.setDeviceName(name.toLatin1().constData());
    QSharedPointer<DeviceInfo> deviceInfo(new DeviceInfo(driver, baseDevice));
    return QSharedPointer<ISD::GenericDevice>(new ISD::GenericDevice(deviceInfo, &m_ClientManager));
}

// Create an INDI device with the RECORD_STREAM property of the INDI stream manager
QSharedPointer<ISD::GenericDevice> TestCameraProcess::createRecordingDevice(const QString &name, IPState state,
        const QString &onSwitch)
{
    QSharedPointer<INDI::ParentDevice> baseDevice(new INDI::ParentDevice(INDI::ParentDevice::Valid));
    baseDevice->setDeviceName(name.toLatin1().constData());
    INDI::PropertySwitch record(4);
    const QStringList names = {"RECORD_ON", "RECORD_DURATION_ON", "RECORD_FRAME_ON", "RECORD_OFF"};
    for (int i = 0; i < names.count(); i++)
        record[i].fill(names[i].toLatin1().constData(), names[i].toLatin1().constData(),
                       names[i] == onSwitch ? ISS_ON : ISS_OFF);
    record.fill(name.toLatin1().constData(), "RECORD_STREAM", "Video Record", "Streaming", IP_RW, ISR_1OFMANY, 0, state);
    baseDevice->registerProperty(record);
    // The device info only keeps a copy of the base device, which shares the properties
    m_BaseDevices.append(baseDevice);

    QSharedPointer<DriverInfo> driver(new DriverInfo(name));
    QSharedPointer<DeviceInfo> deviceInfo(new DeviceInfo(driver, *baseDevice));
    return QSharedPointer<ISD::GenericDevice>(new ISD::GenericDevice(deviceInfo, &m_ClientManager));
}

QSharedPointer<Ekos::SequenceJob> TestCameraProcess::createVideoJob(const QSharedPointer<Ekos::CaptureDeviceAdaptor>
        &devices, const QSharedPointer<Ekos::CameraState> &state, double exposure, int frames)
{
    QSharedPointer<Ekos::SequenceJob> job(new Ekos::SequenceJob(devices, state, Ekos::SequenceJob::JOBTYPE_BATCH));
    job->setFrameType(FRAME_VIDEO);
    job->setCoreProperty(Ekos::SequenceJob::SJ_Exposure, exposure);
    job->setCoreProperty(Ekos::SequenceJob::SJ_Count, frames);
    return job;
}

// The reset frame button is connected to CameraProcess::resetFrame and can be clicked
// while no camera is selected.
void TestCameraProcess::testResetFrameWithoutCamera()
{
    QSharedPointer<Ekos::CameraState> state(new Ekos::CameraState());
    QSharedPointer<Ekos::CaptureDeviceAdaptor> devices(new Ekos::CaptureDeviceAdaptor());
    Ekos::CameraProcess process(state, devices);
    QVERIFY(devices->getActiveCamera() == nullptr);

    QSignalSpy frameSpy(&process, &Ekos::CameraProcess::updateFrameProperties);
    process.resetFrame();

    QCOMPARE(frameSpy.count(), 0);
    QVERIFY(devices->getActiveChip() == nullptr);
}

// The camera is owned by its INDI device and is deleted along with it, e.g. when the INDI
// services stop. The capture module must not keep using it (Sentry KSTARS-J3, KSTARS-XF).
void TestCameraProcess::testActiveCameraDeleted()
{
    QSharedPointer<Ekos::CameraState> state(new Ekos::CameraState());
    QSharedPointer<Ekos::CaptureDeviceAdaptor> devices(new Ekos::CaptureDeviceAdaptor());
    Ekos::CameraProcess process(state, devices);

    auto device = createDevice("Test Camera");
    auto camera = new ISD::Camera(device.get());
    QVERIFY(process.setCamera(camera));
    devices->setActiveChip(camera->getChip(ISD::CameraChip::PRIMARY_CCD));
    QVERIFY(devices->getActiveCamera() == camera);
    QVERIFY(devices->getActiveChip() != nullptr);

    delete camera;

    QVERIFY(devices->getActiveCamera() == nullptr);
    QVERIFY(devices->getActiveChip() == nullptr);

    // Switching to another camera disconnects the previous one first
    auto otherDevice = createDevice("Other Camera");
    ISD::Camera otherCamera(otherDevice.get());
    QVERIFY(process.setCamera(&otherCamera));
    QVERIFY(devices->getActiveCamera() == &otherCamera);
    QVERIFY(process.setCamera(nullptr));
}

void TestCameraProcess::testCaptureTimeoutMS_data()
{
    QTest::addColumn<double>("exposure");
    QTest::addColumn<int>("frames");
    QTest::addColumn<bool>("isVideo");
    QTest::addColumn<int>("expected");

    QTest::newRow("still, count is ignored") << 0.5 << 600 << false << 180500;
    QTest::newRow("still, long exposure") << 300.0 << 1 << false << 480000;
    // bug 528290: 600 x 0.5s takes 300s, longer than CAPTURE_TIMEOUT_THRESHOLD
    QTest::newRow("video, 600 x 0.5s") << 0.5 << 600 << true << 480000;
    QTest::newRow("video, 360 x 0.5s") << 0.5 << 360 << true << 360000;
    QTest::newRow("video, no frame count") << 0.5 << 0 << true << 180500;
    QTest::newRow("video, longer than an int") << 10.0 << 99999999 << true << std::numeric_limits<int>::max();
}

void TestCameraProcess::testCaptureTimeoutMS()
{
    QFETCH(double, exposure);
    QFETCH(int, frames);
    QFETCH(bool, isVideo);
    QFETCH(int, expected);

    QCOMPARE(Ekos::CameraProcess::captureTimeoutMS(exposure, frames, isVideo), expected);
}

void TestCameraProcess::testIsRecording_data()
{
    QTest::addColumn<IPState>("state");
    QTest::addColumn<QString>("onSwitch");
    QTest::addColumn<bool>("expected");

    QTest::newRow("recording frames") << IPS_BUSY << "RECORD_FRAME_ON" << true;
    QTest::newRow("recording duration") << IPS_BUSY << "RECORD_DURATION_ON" << true;
    QTest::newRow("recording") << IPS_BUSY << "RECORD_ON" << true;
    QTest::newRow("recording finished") << IPS_IDLE << "RECORD_OFF" << false;
    QTest::newRow("recording failed") << IPS_ALERT << "RECORD_OFF" << false;
    QTest::newRow("record frames not busy") << IPS_OK << "RECORD_FRAME_ON" << false;
}

void TestCameraProcess::testIsRecording()
{
    QFETCH(IPState, state);
    QFETCH(QString, onSwitch);
    QFETCH(bool, expected);

    auto device = createRecordingDevice("Record Camera", state, onSwitch);
    ISD::Camera camera(device.get());
    QCOMPARE(camera.isRecording(), expected);

    // A camera without video recording is never recording
    auto plainDevice = createDevice("Plain Camera");
    ISD::Camera plainCamera(plainDevice.get());
    QVERIFY(!plainCamera.isRecording());
}

// bug 528290: a video recording reports no progress until it ends. The capture timeout must
// keep waiting while the driver records, and not restart it as a still exposure.
void TestCameraProcess::testVideoTimeoutWhileRecording()
{
    QSharedPointer<Ekos::CameraState> state(new Ekos::CameraState());
    QSharedPointer<Ekos::CaptureDeviceAdaptor> devices(new Ekos::CaptureDeviceAdaptor());
    // The capture timeout handling sets the capture state, which reads the meridian flip state
    state->setMeridianFlipState(QSharedPointer<Ekos::MeridianFlipState>(new Ekos::MeridianFlipState()));
    Ekos::CameraProcess process(state, devices);

    auto device = createRecordingDevice("Video Camera", IPS_BUSY, "RECORD_FRAME_ON");
    ISD::Camera camera(device.get());
    QVERIFY(process.setCamera(&camera));
    devices->setActiveChip(camera.getChip(ISD::CameraChip::PRIMARY_CCD));
    state->setActiveJob(createVideoJob(devices, state, 0.5, 600));

    QSignalSpy logSpy(&process, &Ekos::CameraProcess::newLog);
    QSignalSpy stopSpy(&process, &Ekos::CameraProcess::stopCapture);
    QSignalSpy restartSpy(&process, &Ekos::CameraProcess::driverTimedout);

    // Each expiry while recording only extends the wait, even beyond the restart limit
    for (int i = 0; i < 5; i++)
    {
        process.processCaptureTimeout();
        QVERIFY(state->getCaptureTimeout().isActive());
        QCOMPARE(state->getCaptureTimeout().interval(), CAPTURE_TIMEOUT_THRESHOLD);
    }

    QCOMPARE(state->captureTimeoutCounter(), 0);
    QCOMPARE(logSpy.count(), 0);
    QCOMPARE(stopSpy.count(), 0);
    QCOMPARE(restartSpy.count(), 0);

    state->getCaptureTimeout().stop();
    state->setActiveJob(nullptr);
    QVERIFY(process.setCamera(nullptr));
}

// A video timeout without a recording in progress restarts the recording, not a still exposure
void TestCameraProcess::testVideoTimeoutWhenNotRecording()
{
    QSharedPointer<Ekos::CameraState> state(new Ekos::CameraState());
    QSharedPointer<Ekos::CaptureDeviceAdaptor> devices(new Ekos::CaptureDeviceAdaptor());
    // The capture timeout handling sets the capture state, which reads the meridian flip state
    state->setMeridianFlipState(QSharedPointer<Ekos::MeridianFlipState>(new Ekos::MeridianFlipState()));
    Ekos::CameraProcess process(state, devices);

    auto device = createRecordingDevice("Idle Camera", IPS_IDLE, "RECORD_OFF");
    ISD::Camera camera(device.get());
    QVERIFY(process.setCamera(&camera));
    devices->setActiveChip(camera.getChip(ISD::CameraChip::PRIMARY_CCD));
    state->setActiveJob(createVideoJob(devices, state, 0.5, 600));

    QSignalSpy logSpy(&process, &Ekos::CameraProcess::newLog);
    process.processCaptureTimeout();

    QCOMPARE(state->captureTimeoutCounter(), 1);
    QCOMPARE(logSpy.count(), 1);
    QCOMPARE(logSpy.at(0).at(0).toString(), QString("Video recording timeout. Restarting recording..."));

    state->getCaptureTimeout().stop();
    state->setActiveJob(nullptr);
    QVERIFY(process.setCamera(nullptr));
}

QTEST_MAIN(TestCameraProcess)

#include "test_cameraprocess.moc"
