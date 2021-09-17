/*
    SPDX-FileCopyrightText: 2014 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include <QtDBus/QtDBus>

/**
 * @class INDIDBus
 *
 * Collection of INDI DBus functions.
 *
 * @author Jasem Mutlaq
 */
class INDIDBus : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.INDI")

  public:
    explicit INDIDBus(QObject *parent = nullptr);

    /** @defgroup INDIDBusInterface INDI DBus Interface
         * INDIDBus interface provides complete scripting capability over INDI servers and devices within KStars.
         *
         * A Python DBus <a href="http://indilib.org/support/tutorials/148-dbus-scripting-with-kstars-python.html">tutorial</a> demonstrates
         * the capabilities of the INDI DBus interface.
        */

    /*@{*/

    /** DBUS interface function.  Start a local INDI server given a list of drivers on the given port.
        * @param port Port used to establish  server. If empty, default port 7624 is used.
        * @param drivers List of drivers executables to run.
        */
    Q_SCRIPTABLE bool start(const QString &port, const QStringList &drivers);

    /** DBUS interface function.  Stops server running on the given port
          @param port Port of existing  server to stop
        */
    Q_SCRIPTABLE bool stop(const QString &port);

    /** DBUS interface function. Connect to an INDI server
        * @param host hostname of  server to connect to.
        * @param port Port of  server.
        */
    Q_SCRIPTABLE bool connect(const QString &host, const QString &port);

    /** DBUS interface function. Disconnect from an INDI server
        * @param host hostname of  server to disconnect.
        * @param port Port of  server.
        */
    Q_SCRIPTABLE bool disconnect(const QString &host, const QString &port);

    /** DBUS interface function. Returns a list of INDI devices
        * @returns List of  device names
        */
    Q_SCRIPTABLE QStringList getDevices();

    /** DBUS interface function. Returns a list of INDI properties
        * @param device device name
        * @returns List of  properties in the format DEVICE.PROPERTY.ELEMENT.
        */
    Q_SCRIPTABLE QStringList getProperties(const QString &device);

    /** DBUS interface function. Returns INDI property state
        * @param device device name
        * @param property property name
        * @returns Idle, Ok, Busy, or Alert. If no property is found, it returns "Invalid"
        */
    Q_SCRIPTABLE QString getPropertyState(const QString &device, const QString &property);

    /** DBUS interface function. Sends property to INDI server
        * @param device device name
        * @param property property name
        * @returns true if property is found and sent to  server, false otherwise.
        */
    Q_SCRIPTABLE bool sendProperty(const QString &device, const QString &property);

    /** DBUS interface function. Set INDI Switch status
        * @param device device name
        * @param property property name
        * @param switchName switch name
        * @param status Either On or Off.
        * /note This function ONLY sets the switch status but does not send it to  server. Use sendProperty to send a switch to  server.
        */
    Q_SCRIPTABLE bool setSwitch(const QString &device, const QString &property, const QString &switchName,
                                const QString &status);

    /** DBUS interface function. Returns INDI switch status
        * @param device device name
        * @param property property name
        * @param switchName switch name
        * @returns On or Off if switch is found. If no switch is found, it returns "Invalid".
        */
    Q_SCRIPTABLE QString getSwitch(const QString &device, const QString &property, const QString &switchName);

    /** DBUS interface function. Set INDI Text
        * @param device device name
        * @param property property name
        * @param textName text element name
        * @param text text value
        * /note This function ONLY sets the text value but does not send it to  server. Use sendProperty to send a text to  server.
        */
    Q_SCRIPTABLE bool setText(const QString &device, const QString &property, const QString &textName,
                              const QString &text);

    /** DBUS interface function. Returns INDI text value
        * @param device device name
        * @param property property name
        * @param textName text element name
        * @returns text value. If no text is found, it returns "Invalid".
        */
    Q_SCRIPTABLE QString getText(const QString &device, const QString &property, const QString &textName);

    /** DBUS interface function. Set INDI  Number
        * @param device device name
        * @param property property name
        * @param numberName number element name
        * @param value number value
        * @returns true if successful, false otherwise.
        * /note This function ONLY sets the number value but does not send it to  server. Use sendProperty to send a number to  server.
        */
    Q_SCRIPTABLE bool setNumber(const QString &device, const QString &property, const QString &numberName,
                                double value);

    /** DBUS interface function. Returns INDI number value
        * @param device device name
        * @param property name
        * @param numberName number element name
        * @returns number value. If no text is found, it returns NAN.
        */
    Q_SCRIPTABLE double getNumber(const QString &device, const QString &property, const QString &numberName);

    /** DBUS interface function. Returns INDI  Light state
        * @param device device name
        * @param property name
        * @param lightName light element name
        * @returns Idle, Ok, Busy, or Alert. If no property is found, it returns "Invalid"
        */
    Q_SCRIPTABLE QString getLight(const QString &device, const QString &property, const QString &lightName);

    /** DBUS interface function. Returns INDI blob data. It can be extremely inefficient transporting large amount of data via DBUS.
        * @param device device name
        * @param property property name
        * @param blobName blob element name
        * @param blobFormat blob element format. It is usually the extension of the blob file.
        * @param size blob element size in bytes. If -1, then there is an error.
        * @returns array of bytes containing blob.
        * @see getBLOBFile
        */
    Q_SCRIPTABLE QByteArray getBLOBData(const QString &device, const QString &property, const QString &blobName,
                                        QString &blobFormat, int &size);

    /** DBUS interface function. Returns INDI blob filename stored on the local file system.
        * @param device device name
        * @param property property name
        * @param blobName blob element name
        * @param blobFormat blob element format. It is usually the extension of a file.
        * @param size blob element size in bytes. If -1, then there is an error.
        * @returns full file name
        */
    Q_SCRIPTABLE QString getBLOBFile(const QString &device, const QString &property, const QString &blobName,
                                     QString &blobFormat, int &size);

    /** @}*/
};
