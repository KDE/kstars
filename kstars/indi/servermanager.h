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
#include <QFuture>

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
        ServerManager(const QString &inHost, int inPort);
        ~ServerManager() override;

        bool start();
        void stop();

        QString getLogBuffer();
        const QString &getHost() const
        {
            return host;
        }
        int getPort() const
        {
            return port;
        }

        void setPendingDrivers(QList<DriverInfo *> drivers)
        {
            m_PendingDrivers = drivers;
        }
        const QList<DriverInfo *> &pendingDrivers() const
        {
            return m_PendingDrivers;
        }

        void startDriver(DriverInfo *dv);
        void stopDriver(DriverInfo *dv);
        bool restartDriver(DriverInfo *dv);

        const QList<DriverInfo *> &managedDrivers() const
        {
            return m_ManagedDrivers;
        }
        bool contains(DriverInfo *dv)
        {
            return m_ManagedDrivers.contains(dv);
        }

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
            return m_ManagedDrivers.size();
        }

    public slots:
        void processServerError(QProcess::ProcessError);
        void processStandardError();

    private:
        QTcpSocket serverSocket;
        QString host;
        int port;
        QTemporaryFile serverBuffer;
        std::unique_ptr<QProcess> serverProcess;

        void insertEnvironmentPath(QProcessEnvironment *env, const QString &variable, const QString &relativePath);

        ServerMode mode { SERVER_CLIENT };

        QList<DriverInfo *> m_ManagedDrivers;

        QList<DriverInfo *> m_PendingDrivers;

        QFile indiFIFO;

    signals:
        void started();
        void stopped();
        void failed(const QString &message);
        void terminated(const QString &message);


        void newServerLog();

        // Driver Signals
        void driverStarted(DriverInfo *driver);
        void driverStopped(DriverInfo *driver);
        void driverRestarted(DriverInfo *driver);
        void driverFailed(DriverInfo *driver, const QString &message);
};
