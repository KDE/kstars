/*  INDI DBUS Interface
    Copyright (C) 2014 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIDBUS_H
#define INDIDBUS_H

#include <QObject>
#include <QtDBus/QtDBus>

class INDIDBUS : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI")

public:
    explicit INDIDBUS(QObject *parent = 0);

    /**DBUS interface function.  Start INDI server locally on the given port and drivers list
    * @param port Port used to establish INDI server. If empty, default port 7624 is used.
    * @param drivers List of drivers executables to run
    */
    Q_SCRIPTABLE bool startINDI (const QString &port, const QStringList &drivers);

    /**DBUS interface function.  Stops INDI server running on the given port
      @param port Port of existing INDI server to stop
    */
    Q_SCRIPTABLE bool stopINDI (const QString &port);

    /**DBUS interface function. Connect to an INDI server
    * @host hostname of INDI server to connect to.
    * @param port Port of INDI server.
    */
    Q_SCRIPTABLE bool connectINDI (const QString &host, const QString &port);

    /**DBUS interface function. Disconnect from an INDI server
    * @host hostname of INDI server to disconnect.
    * @param port Port of INDI server.
    */
    Q_SCRIPTABLE bool disconnectINDI (const QString &host, const QString &port);

    /**DBUS interface function. Returns a list of INDI properties
    * @device device name
    * @returns List of INDI properties in the format DEVICE.PROPERTY.ELEMENT.
    */
    Q_SCRIPTABLE QStringList getINDIProperties(const QString &device);

    /**DBUS interface function. Returns INDI property state
    * @device device name
    * @property property name
    * @returns Idle, Ok, Busy, or Alert. If no property is found, it returns "Invalid"
    */
    Q_SCRIPTABLE QString getINDIPropertyState(const QString &device, const QString &property);

    /**DBUS interface function. Sends propery to INDI server
    * @device device name
    * @property property name
    * @returns true if property is found and sent to INDI server, false otherwise.
    */
    Q_SCRIPTABLE bool sendINDIProperty(const QString &device, const QString &property);

    /**DBUS interface function. Set INDI Switch status
    * @device device name
    * @property property name
    * @switchName switch name
    * @status Either On or Off.
    * /note This function ONLY sets the switch status but does not send it to INDI server. Use sendProperty to send a switch to INDI server.
    */
    Q_SCRIPTABLE bool setINDISwitch(const QString &device, const QString &property, const QString &switchName, const QString &status);

    /**DBUS interface function. Returns INDI switch status
    * @device device name
    * @property property name
    * @switchName switch name
    * @returns On or Off if switch is found. If no switch is found, it returns "Invalid".
    */
    Q_SCRIPTABLE QString getINDISwitch(const QString &device, const QString &property, const QString &switchName);

    /**DBUS interface function. Set INDI Text
    * @device device name
    * @property property name
    * @textName text element name
    * @text text value
    * /note This function ONLY sets the text value but does not send it to INDI server. Use sendProperty to send a text to INDI server.
    */
    Q_SCRIPTABLE bool setINDIText(const QString &device, const QString &property, const QString &textName, const QString &text);

    /**DBUS interface function. Returns INDI text value
    * @device device name
    * @property property name
    * @textName text element name
    * @returns text value. If no text is found, it returns "Invalid".
    */
    Q_SCRIPTABLE QString getINDIText(const QString &device, const QString &property, const QString &textName);

    /**DBUS interface function. Set INDI Number
    * @device device name
    * @property property name
    * @NumberName number element name
    * @value number value
    * /note This function ONLY sets the number value but does not send it to INDI server. Use sendProperty to send a number to INDI server.
    */
    Q_SCRIPTABLE bool setINDINumber(const QString &device, const QString &property, const QString &numberName, double value);

    /**DBUS interface function. Returns INDI number value
    * @device device name
    * @property property name
    * @numberName number element name
    * @returns number value. If no text is found, it returns NAN.
    */
    Q_SCRIPTABLE double getINDINumber(const QString &device, const QString &property, const QString &numberName);

    /**DBUS interface function. Returns INDI blob
    * @device device name
    * @property property name
    * @blobName blob element name
    * @returns array of bytes containing blob.
    */
    Q_SCRIPTABLE QByteArray getINDIBLOB(const QString &device, const QString &property, const QString &blobName, QString &blobFormat, unsigned int & size);


};

#endif
