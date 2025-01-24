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
#include <QMutex>

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

        void setPendingDrivers(QList<QSharedPointer<DriverInfo>> drivers)
        {
            QMutexLocker locker(&m_PendingMutex);
            m_PendingDrivers = drivers;
        }
        QList<QSharedPointer<DriverInfo>> pendingDrivers() const
        {
            QMutexLocker locker(&m_PendingMutex);
            return m_PendingDrivers;
        }

        void startDriver(const QSharedPointer<DriverInfo> &driver);
        void stopDriver(const QSharedPointer<DriverInfo> &driver);
        bool restartDriver(const QSharedPointer<DriverInfo> &driver);

        QList<QSharedPointer<DriverInfo>> managedDrivers() const
        {
            QMutexLocker locker(&m_DriverMutex);
            return m_ManagedDrivers;
        }
        bool contains(const QSharedPointer<DriverInfo> &driver)
        {
            QMutexLocker locker(&m_DriverMutex);
            return m_ManagedDrivers.contains(driver);
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
            QMutexLocker locker(&m_DriverMutex);
            return m_ManagedDrivers.size();
        }

    public slots:
        void processServerError(QProcess::ProcessError);
        void processStandardError();
        void connectScriptDriver(const QSharedPointer<DriverInfo> &driver);

    private:
        QTcpSocket serverSocket;
        QString host;
        int port;
        QTemporaryFile serverBuffer;
        std::unique_ptr<QProcess> serverProcess;

        void insertEnvironmentPath(QProcessEnvironment *env, const QString &variable, const QString &relativePath);

        ServerMode mode { SERVER_CLIENT };

        // Protected by m_DriverMutex for concurrent access from main thread and QtConcurrent thread
        QList<QSharedPointer<DriverInfo>> m_ManagedDrivers;
        QList<QSharedPointer<DriverInfo>> m_PendingDrivers;

        mutable QMutex m_DriverMutex;
        mutable QMutex m_PendingMutex;


        QFile indiFIFO;

    signals:
        void started();
        void stopped();
        void failed(const QString &message);
        void terminated(const QString &message);
        void newServerLog();

        // Driver Signals
        void driverStarted(const QSharedPointer<DriverInfo> &driver);
        void driverStopped(const QSharedPointer<DriverInfo> &driver);
        void driverRestarted(const QSharedPointer<DriverInfo> &driver);
        void scriptDriverStarted(const QSharedPointer<DriverInfo> &driver);
        void driverFailed(const QSharedPointer<DriverInfo> &driver, const QString &message);
};
