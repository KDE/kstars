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
    Q_SCRIPTABLE Q_NOREPLY void startINDI (const QString &port, const QStringList &drivers);

    /**DBUS interface function.  Stops INDI server running on the given port
      @param port Port of existing INDI server to stop
    */
    Q_SCRIPTABLE Q_NOREPLY void stopINDI (const QString &port);

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

};

#endif
