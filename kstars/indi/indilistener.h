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

        void addClient(ClientManager *cm);
        void removeClient(ClientManager *cm);

        ISD::GDInterface *getDevice(const QString &name);
        QList<ISD::GDInterface *> getDevices()
        {
            return devices;
        }

        int size()
        {
            return devices.size();
        }

        bool isStandardProperty(const QString &name);

    public slots:

        void registerProperty(INDI::Property prop);
        void removeProperty(const QString &device, const QString &name);
        void processDevice(DeviceInfo *dv);
        void processSwitch(ISwitchVectorProperty *svp);
        void processText(ITextVectorProperty *tvp);
        void processNumber(INumberVectorProperty *nvp);
        void processLight(ILightVectorProperty *lvp);
        void processBLOB(IBLOB *bp);
        void processMessage(INDI::BaseDevice *dp, int messageID);
        void processUniversalMessage(const QString &message);
        //void removeDevice(DeviceInfo *dv);
        void removeDevice(const QString &deviceName);

    private:
        explicit INDIListener(QObject *parent);
        ~INDIListener();

        static INDIListener *_INDIListener;

        QList<ClientManager *> clients;
        QList<ISD::GDInterface *> devices;
        QList<ISD::ST4 *> st4Devices;

    signals:
        void newDevice(ISD::GDInterface *);
        void newTelescope(ISD::GDInterface *);
        void newCCD(ISD::GDInterface *);
        void newFilter(ISD::GDInterface *);
        void newFocuser(ISD::GDInterface *);
        void newDome(ISD::GDInterface *);
        void newWeather(ISD::GDInterface *);
        void newDustCap(ISD::GDInterface *);
        void newLightBox(ISD::GDInterface *);
        void newST4(ISD::ST4 *);
        void deviceRemoved(ISD::GDInterface *);
};
