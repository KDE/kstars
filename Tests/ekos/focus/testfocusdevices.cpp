/*
    SPDX-FileCopyrightText: 2026 KStars Team

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "ekos/focus/focus.h"
#include "indi/clientmanager.h"
#include "indi/deviceinfo.h"
#include "indi/driverinfo.h"
#include "indi/indicamera.h"
#include "indi/indifocuser.h"
#include "indi/indistd.h"
#include "kstarsdata.h"

#include "../../testhelpers.h"

#include <parentdevice.h>

#include <QTest>

class TestFocusDevices : public QObject
{
        Q_OBJECT

    private Q_SLOTS:
        void initTestCase();
        void cleanupTestCase();

        void testDevicesDeleted();

    private:
        QSharedPointer<ISD::GenericDevice> createDevice(const QString &name);

        ClientManager m_ClientManager;
};

void TestFocusDevices::initTestCase()
{
    KTEST_BEGIN();
    KStarsData *data = KStarsData::Create();
    QVERIFY(data != nullptr);
    QVERIFY(data->initialize());
}

void TestFocusDevices::cleanupTestCase()
{
    KTEST_END();
}

// Create an INDI device that is not connected to any server
QSharedPointer<ISD::GenericDevice> TestFocusDevices::createDevice(const QString &name)
{
    QSharedPointer<DriverInfo> driver(new DriverInfo(name));
    INDI::ParentDevice baseDevice(INDI::ParentDevice::Valid);
    baseDevice.setDeviceName(name.toLatin1().constData());
    QSharedPointer<DeviceInfo> deviceInfo(new DeviceInfo(driver, baseDevice));
    return QSharedPointer<ISD::GenericDevice>(new ISD::GenericDevice(deviceInfo, &m_ClientManager));
}

// The camera and focuser are owned by their INDI devices, which are deleted when INDI stops,
// possibly before the focus module hears about it. Stopping or aborting focus afterwards must
// not use them (Sentry KSTARS-Y2, KSTARS-SQ, KSTARS-X9).
void TestFocusDevices::testDevicesDeleted()
{
    Ekos::Focus focus(0);

    auto cameraDevice = createDevice("Test Camera");
    auto focuserDevice = createDevice("Test Focuser");
    auto camera = new ISD::Camera(cameraDevice.get());
    auto focuser = new ISD::Focuser(focuserDevice.get());
    focus.setCamera(camera);
    focus.setFocuser(focuser);
    QCOMPARE(focus.camera(), QString("Test Camera"));
    QCOMPARE(focus.focuser(), QString("Test Focuser"));

    delete camera;
    delete focuser;
    QCOMPARE(focus.camera(), QString());
    QCOMPARE(focus.focuser(), QString());

    // Ends with Focus::stop() and Focus::resetButtons()
    focus.checkStopFocus(true);
    focus.abort();
}

QTEST_MAIN(TestFocusDevices)

#include "testfocusdevices.moc"
