/** *************************************************************************
                          clientmanagerlite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 10/07/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef CLIENTMANAGERLITE_H
#define CLIENTMANAGERLITE_H

#include <baseclientqt.h>
#include "inditelescopelite.h"
#include "indicommon.h"

using namespace INDI;
class TelescopeLite;

struct DeviceInfoLite {
    DeviceInfoLite(INDI::BaseDevice *dev);
    INDI::BaseDevice *device;

    //Motion control
    TelescopeLite *telescope;
};

/**
 * @class ClientManagerLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class ClientManagerLite : public INDI::BaseClientQt
{
    Q_OBJECT
    Q_PROPERTY(QString connectedHost READ connectedHost WRITE setConnectedHost NOTIFY connectedHostChanged)
    Q_PROPERTY(bool connected READ isConnected WRITE setConnected NOTIFY connectedChanged)
public:
    ClientManagerLite();
    virtual ~ClientManagerLite();
    Q_INVOKABLE bool setHost(QString ip, unsigned int port);
    Q_INVOKABLE void disconnectHost();

    Q_INVOKABLE TelescopeLite *getTelescope(QString deviceName);

    QString connectedHost() { return m_connectedHost; }
    void setConnectedHost(QString connectedHost);
    void setConnected(bool connected);
    Q_INVOKABLE QString updateLED(QString device, QString property);

    void buildTextGUI(INDI::Property * property);
    void buildNumberGUI(INDI::Property * property);
    void buildMenuGUI(INDI::Property * property);
    void buildSwitchGUI(INDI::Property * property, PGui guiType);
    void buildSwitch(bool buttonGroup, ISwitch *sw, INDI::Property *property, bool exclusive = false, PGui guiType = PG_BUTTONS);

    Q_INVOKABLE void sendNewINDISwitch(QString deviceName, QString propName, QString name);
    Q_INVOKABLE void sendNewINDISwitch(QString deviceName, QString propName, int index);

    bool isConnected() { return m_connected; }
    Q_INVOKABLE bool isDeviceConnected(QString deviceName);

    QList<DeviceInfoLite *>   getDevices() { return m_devices; }

protected:
    virtual void newDevice(INDI::BaseDevice *dp);
    virtual void removeDevice(INDI::BaseDevice *dp);
    virtual void newProperty(INDI::Property *property);
    virtual void removeProperty(INDI::Property *property);
    virtual void newBLOB(IBLOB *bp) {}
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *nvp);
    virtual void newMessage(INDI::BaseDevice *dp, int messageID) { }
    virtual void newText(ITextVectorProperty *tvp) {}
    virtual void newLight(ILightVectorProperty *lvp) {}
    virtual void serverConnected() {}
    virtual void serverDisconnected(int exit_code);
signals:
    //Device
    void newINDIDevice(QString deviceName);
    void removeINDIDevice(QString deviceName);
    void deviceConnected(QString deviceName, bool isConnected);

    void newINDIProperty(QString deviceName, QString propName, QString groupName, QString type, QString label);

    void createINDIText(QString deviceName, QString propName, QString propLabel, QString propText, bool read, bool write);
    void createINDINumber(QString deviceName, QString propName, QString propLabel, QString propText, bool read, bool write, bool scale);

    void createINDIButton(QString deviceName, QString propName, QString propText, QString switchName, bool read, bool write, bool exclusive, bool checked, bool checkable);
    void createINDIRadio(QString deviceName, QString propName, QString propText, QString switchName, bool read, bool write, bool exclusive, bool checked, bool enabled);

    void createINDIMenu(QString deviceName, QString propName, QString switchLabel, QString switchName, bool isSelected);

    void removeINDIProperty(QString groupName, QString propName);

    //Update signals
    void newINDISwitch(QString deviceName, QString propName, QString switchName, bool isOn);

    void connectedHostChanged(QString);
    void connectedChanged(bool);
    void telescopeAdded(TelescopeLite *newTelescope);
private:
    QList<DeviceInfoLite *> m_devices;
    QString m_connectedHost;
    bool m_connected;
};

#endif // CLIENTMANAGERLITE_H
