/*
    SPDX-FileCopyrightText: 2018 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#ifdef USE_QT5_INDI
#include <baseclientqt.h>
#else
#include <baseclient.h>
#include <QObject>
#endif

class DeviceInfo;
class DriverInfo;
class ServerManager;

/**
 * @class BlobManager
 * BlobManager manages connection to INDI server to handle a specific BLOB.
 *
 * BlobManager is a subclass of INDI::BaseClient class part of the INDI Library.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
#ifdef USE_QT5_INDI
class BlobManager : public INDI::BaseClientQt
#else
class BlobManager : public QObject, public INDI::BaseClient
#endif
{
    Q_OBJECT
    Q_PROPERTY(QString device MEMBER m_Device)
    Q_PROPERTY(QString property MEMBER m_Property)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled)

  public:
    BlobManager(QObject *parent, const QString &host, int port, const QString &device, const QString &prop);
    virtual ~BlobManager() override = default;

    bool enabled() { return m_Enabled; }
    void setEnabled(bool enabled);

  protected:
    virtual void newDevice(INDI::BaseDevice device) override;
    virtual void updateProperty(INDI::Property prop) override;

    virtual void serverConnected() override {}
    virtual void serverDisconnected(int exit_code) override;

  signals:   
    void propertyUpdated(INDI::Property prop);
    void connected();
    void connectionFailure();

  private:
    QString m_Device;
    QString m_Property;
    bool m_Enabled { true };
};
