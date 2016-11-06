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
#include "Options.h"
#include "indistd.h"
#include <QImage>

class QFileDialog;

using namespace INDI;
class TelescopeLite;

struct DeviceInfoLite {
    DeviceInfoLite(INDI::BaseDevice *dev);
    ~DeviceInfoLite();
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

    /** A wrapper for Options::lastServer(). Used to store last used server if user successfully
     * connected to some server at least once.**/
    Q_PROPERTY(QString lastUsedServer READ getLastUsedServer WRITE setLastUsedServer NOTIFY lastUsedServerChanged)

    /** A wrapper for Options::lastServer(). Used to store last used port if user successfully
     * connected to some server at least once.**/
    Q_PROPERTY(int lastUsedPort READ getLastUsedPort WRITE setLastUsedPort NOTIFY lastUsedPortChanged)
public:
    typedef enum { UPLOAD_CLIENT, UPLOAD_LOCAL, UPLOAD_BOTH } UploadMode;
    ClientManagerLite();
    virtual ~ClientManagerLite();
    Q_INVOKABLE bool setHost(QString ip, unsigned int port);
    Q_INVOKABLE void disconnectHost();

    Q_INVOKABLE TelescopeLite *getTelescope(QString deviceName);

    QString connectedHost() { return m_connectedHost; }
    void setConnectedHost(QString connectedHost);
    void setConnected(bool connected);
    /**
     * @brief syncLED
     * @param name of Light which LED needs to be synced
     * @return color of state
     */
    Q_INVOKABLE QString syncLED(QString device, QString property, QString name = "");

    void buildTextGUI(INDI::Property * property);
    void buildNumberGUI(INDI::Property * property);
    void buildMenuGUI(INDI::Property * property);
    void buildSwitchGUI(INDI::Property * property, PGui guiType);
    void buildSwitch(bool buttonGroup, ISwitch *sw, INDI::Property *property, bool exclusive = false, PGui guiType = PG_BUTTONS);
    void buildLightGUI(INDI::Property *property);
    //void buildBLOBGUI(INDI::Property *property);

    Q_INVOKABLE void sendNewINDISwitch(QString deviceName, QString propName, QString name);
    Q_INVOKABLE void sendNewINDISwitch(QString deviceName, QString propName, int index);

    Q_INVOKABLE void sendNewINDINumber(const QString& deviceName, const QString& propName, const QString &numberName, double value);
    Q_INVOKABLE void sendNewINDIText(const QString& deviceName, const QString& propName, const QString &fieldName, const QString& text);

    bool isConnected() { return m_connected; }
    Q_INVOKABLE bool isDeviceConnected(QString deviceName);

    QList<DeviceInfoLite *> getDevices() { return m_devices; }

    Q_INVOKABLE QString getLastUsedServer();
    Q_INVOKABLE void setLastUsedServer(QString server);

    Q_INVOKABLE int getLastUsedPort();
    Q_INVOKABLE void setLastUsedPort(int port);
    /**
     * @brief saveDisplayImage
     * @return true if image was saved false otherwise
     */
    Q_INVOKABLE bool saveDisplayImage();

    void clearDevices();

protected:
    virtual void newDevice(INDI::BaseDevice *dp);
    virtual void removeDevice(INDI::BaseDevice *dp);
    virtual void newProperty(INDI::Property *property);
    virtual void removeProperty(INDI::Property *property);
    virtual void newBLOB(IBLOB *bp);
    virtual void newSwitch(ISwitchVectorProperty *svp);
    virtual void newNumber(INumberVectorProperty *nvp);
    virtual void newMessage(INDI::BaseDevice *dp, int messageID);
    virtual void newText(ITextVectorProperty *tvp);
    virtual void newLight(ILightVectorProperty *lvp);
    virtual void serverConnected() {}
    virtual void serverDisconnected(int exit_code);
signals:
    //Device
    void newINDIDevice(QString deviceName);
    void removeINDIDevice(QString deviceName);
    void deviceConnected(QString deviceName, bool isConnected);

    void newINDIProperty(QString deviceName, QString propName, QString groupName, QString type, QString label);

    void createINDIText(QString deviceName, QString propName, QString propLabel, QString fieldName, QString propText, bool read, bool write);

    void createINDINumber(QString deviceName, QString propName, QString propLabel, QString numberName, QString propText, bool read, bool write, bool scale);

    void createINDIButton(QString deviceName, QString propName, QString propText, QString switchName, bool read, bool write, bool exclusive, bool checked, bool checkable);
    void createINDIRadio(QString deviceName, QString propName, QString propText, QString switchName, bool read, bool write, bool exclusive, bool checked, bool enabled);

    void createINDIMenu(QString deviceName, QString propName, QString switchLabel, QString switchName, bool isSelected);

    void createINDILight(QString deviceName, QString propName, QString label, QString lightName);

    void removeINDIProperty(QString deviceName, QString groupName, QString propName);

    //Update signals
    void newINDISwitch(QString deviceName, QString propName, QString switchName, bool isOn);
    void newINDINumber(QString deviceName, QString propName, QString numberName, QString value);
    void newINDIText(QString deviceName, QString propName, QString fieldName, QString text);
    void newINDIMessage(QString message);
    void newINDILight(QString deviceName, QString propName);
    void newINDIBLOBImage(QString deviceName, bool isLoaded);
    void newLEDState(QString deviceName, QString propName); // to sync LED for properties

    void connectedHostChanged(QString);
    void connectedChanged(bool);
    void telescopeAdded(TelescopeLite *newTelescope);
    void telescopeRemoved(TelescopeLite *delTelescope);

    void lastUsedServerChanged();
    void lastUsedPortChanged();
private:
    bool processBLOBasCCD(IBLOB *bp);

    QList<DeviceInfoLite *> m_devices;
    QString m_connectedHost;
    bool m_connected;    
    char BLOBFilename[MAXINDIFILENAME];
    QImage displayImage;
#ifdef ANDROID
    QString defaultImageType;
    QString defaultImagesLocation;
#endif
};

#endif // CLIENTMANAGERLITE_H
