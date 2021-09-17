/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "indicommon.h"

#include <QFile>
#include <QObject>
#include <QProcess>
#include <QTcpSocket>
#include <QTemporaryFile>

#include <memory>

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
        ServerManager(const QString &inHost, uint inPort);
        ~ServerManager();

        bool start();
        void stop();
        void terminate();

        QString getLogBuffer();
        const QString &getHost()
        {
            return host;
        }
        const QString &getPort()
        {
            return port;
        }

        bool startDriver(DriverInfo *dv);
        void stopDriver(DriverInfo *dv);
        bool restartDriver(DriverInfo *dv);
        bool contains(DriverInfo *dv)
        {
            return managedDrivers.contains(dv);
        };

        void setMode(ServerMode inMode)
        {
            mode = inMode;
        }
        ServerMode getMode()
        {
            return mode;
        }

        QString errorString();

        int size()
        {
            return managedDrivers.size();
        }

    public slots:
        void connectionSuccess();
        void processServerError(QProcess::ProcessError);
        void processStandardError();

    private:
        QTcpSocket serverSocket;
        QString host;
        QString port;
        QTemporaryFile serverBuffer;
        std::unique_ptr<QProcess> serverProcess;

        void insertEnvironmentPath(QProcessEnvironment *env, QString variable, QString relativePath);

        ServerMode mode { SERVER_CLIENT };
        //bool driverCrashed { false };

        QList<DriverInfo *> managedDrivers;

        QFile indiFIFO;

    signals:
        void serverFailure(ServerManager *);
        void newServerLog();
        void started();
        void finished(int exit_code, QProcess::ExitStatus exit_status);
};
