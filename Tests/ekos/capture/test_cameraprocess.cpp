/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/cameraprocess.h"
#include "ekos/capture/camerastate.h"
#include "ekos/capture/capturedeviceadaptor.h"
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

    private:
        QSharedPointer<ISD::GenericDevice> createDevice(const QString &name);

        ClientManager m_ClientManager;
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

QTEST_MAIN(TestCameraProcess)

#include "test_cameraprocess.moc"
