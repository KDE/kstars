/*  INDI Client Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

*/

#pragma once

#include <QPointer>

#ifdef USE_QT5_INDI
#include <baseclientqt.h>
#else
#include <baseclient.h>
#include <QObject>
#endif

#include "blobmanager.h"

class DeviceInfo;
class DriverInfo;
class ServerManager;

/**
 * @class ClientManager
 * ClientManager manages connection to INDI server, creation of devices, and receiving/sending properties.
 *
 * ClientManager is a subclass of INDI::BaseClient class part of the INDI Library.
 * This enables the class to communicate with INDI server and to receive notification of devices, properties, and messages.
 *
 * @author Jasem Mutlaq
 * @version 1.1
 */
#ifdef USE_QT5_INDI
class ClientManager : public INDI::BaseClientQt
#else
class ClientManager : public QObject, public INDI::BaseClient
#endif
{
        Q_OBJECT

    public:
        ClientManager() = default;
        virtual ~ClientManager() override = default;

        /**
         * @brief appendManagedDriver Add driver to pool of managed drivers by this client manager.
         * @param dv pointer to driver info instance.
         * @note This function is ALWAYS called from the main KStars thread.
         */
        void appendManagedDriver(DriverInfo *dv);

        /**
         * @brief removeManagedDriver Remove managed driver from pool of drivers managed by this client manager.
         * @param dv pointer to driver info instance.
         * @note This function is ALWAYS called from the main KStars thread.
         */
        void removeManagedDriver(DriverInfo *dv);

        int count()
        {
            return managedDrivers.count();
        }

        bool isBLOBEnabled(const QString &device, const QString &property);
        void setBLOBEnabled(bool enabled, const QString &device, const QString &property);

        ServerManager *getServerManager()
        {
            return sManager;
        }

        DriverInfo *findDriverInfoByName(const QString &name);
        DriverInfo *findDriverInfoByLabel(const QString &label);

        bool isDriverManaged(DriverInfo *);

        QList<DriverInfo *> getManagedDrivers() const;

    protected:
        virtual void newDevice(INDI::BaseDevice *dp) override;
        virtual void newProperty(INDI::Property *prop) override;
        virtual void removeProperty(INDI::Property *prop) override;
        virtual void removeDevice(INDI::BaseDevice *dp) override;
        virtual void newBLOB(IBLOB *bp) override;
        virtual void newSwitch(ISwitchVectorProperty *svp) override;
        virtual void newNumber(INumberVectorProperty *) override;
        virtual void newText(ITextVectorProperty *) override;
        virtual void newLight(ILightVectorProperty *) override;
        virtual void newMessage(INDI::BaseDevice *dp, int messageID) override;

#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 5
        virtual void newUniversalMessage(std::string message) override;
#endif

        virtual void serverConnected() override;
        virtual void serverDisconnected(int exit_code) override;

    private:
        QList<DriverInfo *> managedDrivers;
        QList<QPointer<BlobManager>> blobManagers;
        ServerManager *sManager { nullptr };

    signals:
        void connectionSuccessful();
        void connectionFailure(ClientManager *);

        // @note If using INDI Posix client, the following newINDIDevice/Property and removeINDIDevice/Property signals
        // must be connected to slots using Qt::BlockingQueuedConnection to ensure operation is fully completed before
        // resuming the INDI client thread. For Qt Based INDI client, Qt::DirectConnection should be used.

        void newINDIDevice(DeviceInfo *dv);
        void removeINDIDevice(const QString &name);

        void newINDIProperty(INDI::Property *prop);
        void removeINDIProperty(const QString &device, const QString &name);

        void newBLOBManager(const char *device, INDI::Property *prop);

        void newINDIBLOB(IBLOB *bp);
        void newINDISwitch(ISwitchVectorProperty *svp);
        void newINDINumber(INumberVectorProperty *nvp);
        void newINDIText(ITextVectorProperty *tvp);
        void newINDILight(ILightVectorProperty *lvp);
        void newINDIMessage(INDI::BaseDevice *dp, int messageID);
#if INDI_VERSION_MAJOR >= 1 && INDI_VERSION_MINOR >= 5
        void newINDIUniversalMessage(const QString &message);
#endif
};
