/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/capture/cameraprocess.h"
#include "ekos/capture/camerastate.h"
#include "ekos/capture/capturedeviceadaptor.h"

#include <QSignalSpy>
#include <QTest>

class TestCameraProcess : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void testResetFrameWithoutCamera();
};

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

QTEST_GUILESS_MAIN(TestCameraProcess)

#include "test_cameraprocess.moc"
