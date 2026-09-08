/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "opticaltraindbusinterface.h"
#include "opticaltrainmanager.h"

// Qt-generated adaptor header (produced from org.kde.kstars.Ekos.OpticalTrain.xml
// via qt{5,6}_add_dbus_adaptor in CMakeLists.txt).
#include "opticaltrainadaptor.h"

#include <QDBusConnection>

namespace Ekos
{

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainDBusInterface::OpticalTrainDBusInterface(int trainID, QObject *parent)
    : QObject(parent), m_TrainID(trainID)
{
    // Exactly the same three-line pattern used by ISD::DustCap, ISD::Dome, etc.
    new OpticalTrainAdaptor(this);
    m_DBusObjectPath = QString("/KStars/Ekos/OpticalTrain/%1").arg(trainID);
    QDBusConnection::sessionBus().registerObject(m_DBusObjectPath, this);
}

////////////////////////////////////////////////////////////////////////////
///
////////////////////////////////////////////////////////////////////////////
OpticalTrainDBusInterface::~OpticalTrainDBusInterface()
{
    QDBusConnection::sessionBus().unregisterObject(m_DBusObjectPath);
}

////////////////////////////////////////////////////////////////////////////
/// Private helpers
////////////////////////////////////////////////////////////////////////////
QVariant OpticalTrainDBusInterface::field(const QString &key) const
{
    auto train = OpticalTrainManager::Instance()->getOpticalTrain(static_cast<uint8_t>(m_TrainID));
    return train.value(key);
}

bool OpticalTrainDBusInterface::setField(const QString &key, const QVariant &value)
{
    auto train = OpticalTrainManager::Instance()->getOpticalTrain(static_cast<uint8_t>(m_TrainID));
    if (train.isEmpty())
        return false;
    return OpticalTrainManager::Instance()->setOpticalTrainValue(train["name"].toString(), key, value);
}

////////////////////////////////////////////////////////////////////////////
/// Property readers
////////////////////////////////////////////////////////////////////////////
QString OpticalTrainDBusInterface::name() const
{
    return field("name").toString();
}

QString OpticalTrainDBusInterface::mount() const
{
    return field("mount").toString();
}

QString OpticalTrainDBusInterface::camera() const
{
    return field("camera").toString();
}

QString OpticalTrainDBusInterface::focuser() const
{
    return field("focuser").toString();
}

QString OpticalTrainDBusInterface::filterWheel() const
{
    return field("filterwheel").toString();
}

QString OpticalTrainDBusInterface::rotator() const
{
    return field("rotator").toString();
}

QString OpticalTrainDBusInterface::dustCap() const
{
    return field("dustcap").toString();
}

QString OpticalTrainDBusInterface::lightBox() const
{
    return field("lightbox").toString();
}

QString OpticalTrainDBusInterface::scope() const
{
    return field("scope").toString();
}

double OpticalTrainDBusInterface::reducer() const
{
    return field("reducer").toDouble();
}

QString OpticalTrainDBusInterface::guider() const
{
    return field("guider").toString();
}

////////////////////////////////////////////////////////////////////////////
/// Setter methods
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainDBusInterface::setMount(const QString &name)
{
    return setField("mount", name);
}

bool OpticalTrainDBusInterface::setCamera(const QString &name)
{
    return setField("camera", name);
}

bool OpticalTrainDBusInterface::setFocuser(const QString &name)
{
    return setField("focuser", name);
}

bool OpticalTrainDBusInterface::setFilterWheel(const QString &name)
{
    return setField("filterwheel", name);
}

bool OpticalTrainDBusInterface::setRotator(const QString &name)
{
    return setField("rotator", name);
}

bool OpticalTrainDBusInterface::setDustCap(const QString &name)
{
    return setField("dustcap", name);
}

bool OpticalTrainDBusInterface::setLightBox(const QString &name)
{
    return setField("lightbox", name);
}

bool OpticalTrainDBusInterface::setScope(const QString &name)
{
    return setField("scope", name);
}

bool OpticalTrainDBusInterface::setReducer(double value)
{
    return setField("reducer", value);
}

bool OpticalTrainDBusInterface::setGuider(const QString &name)
{
    return setField("guider", name);
}

////////////////////////////////////////////////////////////////////////////
/// Bulk operations
////////////////////////////////////////////////////////////////////////////
bool OpticalTrainDBusInterface::remove()
{
    auto train = OpticalTrainManager::Instance()->getOpticalTrain(static_cast<uint8_t>(m_TrainID));
    if (train.isEmpty())
        return false;

    Q_EMIT removed();
    return OpticalTrainManager::Instance()->removeOpticalTrain(train["name"].toString());
}

int OpticalTrainDBusInterface::cleanup()
{
    return OpticalTrainManager::Instance()->cleanupUnusedDevices();
}

////////////////////////////////////////////////////////////////////////////
/// Called by OpticalTrainManager when it refreshes train data
////////////////////////////////////////////////////////////////////////////
void OpticalTrainDBusInterface::notifyUpdated()
{
    Q_EMIT updated();
}

} // namespace Ekos
