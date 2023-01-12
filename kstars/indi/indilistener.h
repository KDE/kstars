/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

    Handle INDI Standard properties.
*/

#pragma once

#include "indi/indistd.h"

#include <indiproperty.h>

#include <QObject>

class ClientManager;
class FITSViewer;
class DeviceInfo;

/**
 * @class INDIListener
 * INDIListener is responsible for creating ISD::GDInterface generic devices as new devices arrive from
 * ClientManager. It can support multiple ClientManagers and will first create a generic INDI device.
 * Upon arrival of INDI properties, INDIListener can create specialized devices (e.g. Telescope) if it
 * detects key Standard INDI property that signifies a particular device family. The generic device
 * functionality is extended via the Decorator design pattern.
 *
 * INDIListener also delegates INDI properties as they are received from ClientManager to the appropriate
 * device to be processed.
 *
 * @author Jasem Mutlaq
 */
class INDIListener : public QObject
{
        Q_OBJECT

    public:
        static INDIListener *Instance();
        // Convenience function
        static const QList<QSharedPointer<ISD::GenericDevice>> &devices()
        {
            return INDIListener::Instance()->getDevices();
        }
        static const QList<QSharedPointer<ISD::GenericDevice>> devicesByInterface(uint32_t interface)
        {
            QList<QSharedPointer<ISD::GenericDevice>> filteredDevices;
            auto all = devices();
            std::copy_if(all.begin(), all.end(), std::back_inserter(filteredDevices), [interface](auto & oneDevice)
            {
                return oneDevice->getDriverInterface() & interface;
            });
            return filteredDevices;
        }
        static bool findDevice(const QString &name, QSharedPointer<ISD::GenericDevice> &device);

        void addClient(ClientManager *cm);
        void removeClient(ClientManager *cm);

        bool getDevice(const QString &name, QSharedPointer<ISD::GenericDevice> &device) const;
        const QList<QSharedPointer<ISD::GenericDevice>> &getDevices() const
        {
            return m_Devices;
        }

        int size() const
        {
            return m_Devices.size();
        }

    public slots:
        void processDevice(DeviceInfo *dv);
        void removeDevice(const QString &deviceName);

        void registerProperty(INDI::Property prop);
        void updateProperty(INDI::Property prop);
        void removeProperty(INDI::Property prop);

        void processMessage(INDI::BaseDevice dp, int messageID);
        void processUniversalMessage(const QString &message);

    private:
        explicit INDIListener(QObject *parent);

        static INDIListener *_INDIListener;

        QList<ClientManager *> clients;
        QList<QSharedPointer<ISD::GenericDevice>> m_Devices;


    signals:
        void newDevice(const QSharedPointer<ISD::GenericDevice> &device);
        void deviceRemoved(const QSharedPointer<ISD::GenericDevice> &device);
};
