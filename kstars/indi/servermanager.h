#ifndef SERVERMANAGER_H
#define SERVERMANAGER_H

/*  INDI Server Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include <QObject>
#include <QTcpSocket>
#include <QProcess>
#include <QFile>

#include <lilxml.h>

#include "indicommon.h"

class DriverInfo;


/**
 * @class ServerManager
 * ServerManager is responsible for starting and shutting local INDI servers.
 *
 * @author Jasem Mutlaq
 */
class ServerManager : public QObject
{

    Q_OBJECT

public:

    enum { INDI_DEVICE_NOT_FOUND=-1, INDI_PROPERTY_INVALID=-2, INDI_PROPERTY_DUPLICATED = -3, INDI_DISPATCH_ERROR=-4 };

    ServerManager(QString inHost, uint inPort);
      ~ServerManager();

    bool start();
    void stop();
    void terminate();

    const QString & getLogBuffer() { return serverBuffer; }
    const QString & getHost() { return host;}
    const QString & getPort() { return port;}

    bool startDriver(DriverInfo *dv);
    void stopDriver(DriverInfo *dv);

    void setMode(ServerMode inMode) { mode = inMode; }
    ServerMode getMode() { return mode; }

    QString errorString();

    int size() { return managedDrivers.size(); }

    bool isDriverManaged(DriverInfo *);

private:

    QTcpSocket		 serverSocket;
    LilXML		 *XMLParser;
    QString		 host;
    QString		 port;
    QString 		 serverBuffer;
    QProcess 		 *serverProcess;

    ServerMode          mode;
    bool        driverCrashed;

    QList<DriverInfo *> managedDrivers;

    QFile               indiFIFO;

public slots:
    void connectionSuccess();
    void processServerError(QProcess::ProcessError);
    void processStandardError();

signals:
    void serverFailure(ServerManager *);
    void newServerLog();
    void started();
    void finished(int exit_code, QProcess::ExitStatus exit_status);


};

#endif // SERVERMANAGER_H
