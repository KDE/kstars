/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "clientmanager.h"

#include "deviceinfo.h"
#include "drivermanager.h"
#include "guimanager.h"
#include "indilistener.h"
#include "Options.h"
#include "servermanager.h"

#include <indi_debug.h>

BlobManager::BlobManager(const QString &host, int port, const QString &device, const QString &prop) : m_Device(device), m_Property(prop)
{
    // Set INDI server params
    setServer(host.toLatin1().constData(), port);

    // We're only interested in a particular device
    watchDevice(m_Device.toLatin1().constData());

    // Connect immediately
    connectServer();
}

void BlobManager::serverDisconnected(int exit_code)
{
    qCDebug(KSTARS_INDI) << "INDI server disconnected from BLOB manager for Device:" << m_Device << "Property:" << m_Property << "Exit code:" << exit_code;
}

void BlobManager::newBLOB(IBLOB *bp)
{    
    emit newINDIBLOB(bp);
}

void BlobManager::newDevice(INDI::BaseDevice *device)
{
    // Got out target device, let's now set to BLOB ONLY for the particular property we want
    if (QString(device->getDeviceName()) == m_Device)
    {
        setBLOBMode(B_ONLY, m_Device.toLatin1().constData(), m_Property.toLatin1().constData());
        emit connected();
    }
}

void BlobManager::setEnabled(bool enabled)
{
    m_Enabled = enabled;
    setBLOBMode(enabled ? B_ONLY : B_NEVER, m_Device.toLatin1().constData(), m_Property.toLatin1().constData());
}
