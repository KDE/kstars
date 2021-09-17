/*  mock ekos modules
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "mockmodules.h"
#include "mockfocusadaptor.h"
#include "mockmountadaptor.h"
#include "mockcaptureadaptor.h"
#include "mockalignadaptor.h"
#include "mockguideadaptor.h"
#include "mockekosadaptor.h"

namespace Ekos
{

const QString MockFocus::mockPath = "/KStars/MockEkos/MockFocus";
MockFocus::MockFocus()
{
    qRegisterMetaType<Ekos::FocusState>("Ekos::FocusState");
    qDBusRegisterMetaType<Ekos::FocusState>();
    new MockFocusAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

const QString MockMount::mockPath = "/KStars/MockEkos/MockMount";
MockMount::MockMount()
{
    qRegisterMetaType<ISD::Telescope::Status>("ISD::Telescope::Status");
    qDBusRegisterMetaType<ISD::Telescope::Status>();
    qRegisterMetaType<ISD::ParkStatus>("ISD::ParkStatus");
    qDBusRegisterMetaType<ISD::ParkStatus>();
    new MockMountAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

const QString MockCapture::mockPath = "/KStars/MockEkos/MockCapture";
MockCapture::MockCapture()
{
    qRegisterMetaType<Ekos::CaptureState>("Ekos::CaptureState");
    qDBusRegisterMetaType<Ekos::CaptureState>();
    new MockCaptureAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

const QString MockAlign::mockPath = "/KStars/MockEkos/MockAlign";
MockAlign::MockAlign()
{
    qRegisterMetaType<Ekos::AlignState>("Ekos::AlignState");
    qDBusRegisterMetaType<Ekos::AlignState>();
    new MockAlignAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

const QString MockGuide::mockPath = "/KStars/MockEkos/MockGuide";
MockGuide::MockGuide()
{
    qRegisterMetaType<Ekos::GuideState>("Ekos::GuideState");
    qDBusRegisterMetaType<Ekos::GuideState>();
    new MockGuideAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

const QString MockEkos::mockPath = "/KStars/MockEkos";
MockEkos::MockEkos()
{
    qRegisterMetaType<Ekos::CommunicationStatus>("Ekos::CommunicationStatus");
    qDBusRegisterMetaType<Ekos::CommunicationStatus>();

    new MockEkosAdaptor(this);
    QDBusConnection::sessionBus().unregisterObject(mockPath);
    QDBusConnection::sessionBus().registerObject(mockPath, this);
}

}  // namespace Ekos
