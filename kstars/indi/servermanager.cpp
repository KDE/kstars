/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "servermanager.h"

#include "driverinfo.h"
#include "clientmanager.h"
#include "drivermanager.h"
#include "auxiliary/kspaths.h"
#include "auxiliary/ksmessagebox.h"
#include "Options.h"
#include "ksnotification.h"

#include <indidevapi.h>
#include <thread>

#include <KMessageBox>
#include <QUuid>

#include <sys/stat.h>

#include <indi_debug.h>

// Qt version calming
#include <qtendl.h>

ServerManager::ServerManager(const QString &inHost, int inPort) : host(inHost), port(inPort)
{
}

ServerManager::~ServerManager()
{
    serverSocket.close();
    indiFIFO.close();

    QFile::remove(indiFIFO.fileName());

    if (serverProcess.get() != nullptr)
        serverProcess->close();
}

bool ServerManager::start()
{
#ifdef Q_OS_WIN
    qWarning() << "INDI server is currently not supported on Windows.";
    return false;
#else
    bool connected = false;

    if (serverProcess.get() == nullptr)
    {
        serverBuffer.open();

        serverProcess.reset(new QProcess(this));
#ifdef Q_OS_OSX
        QString driversDir = Options::indiDriversDir();
        if (Options::indiDriversAreInternal())
            driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
        QString indiServerDir;
        if (Options::indiServerIsInternal())
            indiServerDir = QCoreApplication::applicationDirPath();
        else
            indiServerDir = QFileInfo(Options::indiServer()).dir().path();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PATH", driversDir + ':' + indiServerDir + ":/usr/local/bin:/usr/bin:/bin");
        QString gscDirPath = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation)).filePath("gsc");
        env.insert("GSCDAT", gscDirPath);

        insertEnvironmentPath(&env, "INDIPREFIX", "/../../");
        insertEnvironmentPath(&env, "IOLIBS", "/../Resources/DriverSupport/gphoto/IOLIBS");
        insertEnvironmentPath(&env, "CAMLIBS", "/../Resources/DriverSupport/gphoto/CAMLIBS");

        serverProcess->setProcessEnvironment(env);
#endif
    }

    QStringList args;

    args << "-v" << "-p" << QString::number(port);

    QString fifoFile = QString("/tmp/indififo%1").arg(QUuid::createUuid().toString().mid(1, 8));

    if (mkfifo(fifoFile.toLatin1(), S_IRUSR | S_IWUSR) < 0)
    {
        emit failed(i18n("Error making FIFO file %1: %2.", fifoFile, strerror(errno)));
        return false;
    }

    indiFIFO.setFileName(fifoFile);

    if (!indiFIFO.open(QIODevice::ReadWrite | QIODevice::Text))
    {
        qCCritical(KSTARS_INDI) << "Unable to create INDI FIFO file: " << fifoFile;
        emit failed(i18n("Unable to create INDI FIFO file %1", fifoFile));
        return false;
    }

    args << "-m" << QString::number(Options::serverTransferBufferSize()) << "-r" << "0" << "-f" << fifoFile;

    qCDebug(KSTARS_INDI) << "Starting INDI Server: " << args << "-f" << fifoFile;

    serverProcess->setProcessChannelMode(QProcess::SeparateChannels);
    serverProcess->setReadChannel(QProcess::StandardError);

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        serverProcess->start(QCoreApplication::applicationDirPath() + "/indiserver", args);
    else
#endif
        serverProcess->start(Options::indiServer(), args);

    connected = serverProcess->waitForStarted();

    if (connected)
    {
        connect(serverProcess.get(), &QProcess::errorOccurred, this, &ServerManager::processServerError);
        connect(serverProcess.get(), &QProcess::readyReadStandardError, this, &ServerManager::processStandardError);
        emit started();
    }
    else
    {
        emit failed(i18n("INDI server failed to start: %1", serverProcess->errorString()));
    }

    qCDebug(KSTARS_INDI) << "INDI Server Started? " << connected;

    return connected;
#endif
}

void ServerManager::insertEnvironmentPath(QProcessEnvironment *env, const QString &variable, const QString &relativePath)
{
    QString environmentPath = QCoreApplication::applicationDirPath() + relativePath;
    if (QFileInfo::exists(environmentPath) && Options::indiDriversAreInternal())
        env->insert(variable, QDir(environmentPath).absolutePath());
}

void ServerManager::startDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    // Check for duplicates within existing clients
    if (dv->getUniqueLabel().isEmpty() && dv->getLabel().isEmpty() == false)
        dv->setUniqueLabel(DriverManager::Instance()->getUniqueDeviceLabel(dv->getLabel()));

    // Check for duplicates within managed drivers
    if (dv->getUniqueLabel().isEmpty() == false)
    {
        QString uniqueLabel;
        QString label = dv->getUniqueLabel();
        int nset = std::count_if(m_ManagedDrivers.begin(), m_ManagedDrivers.end(), [label](auto & oneDriver)
        {
            return label == oneDriver->getUniqueLabel();
        });
        if (nset > 0)
        {
            uniqueLabel = QString("%1 %2").arg(label).arg(nset + 1);
            dv->setUniqueLabel(uniqueLabel);
        }
    }

    m_ManagedDrivers.append(dv);
    dv->setServerManager(this);

    QString driversDir    = Options::indiDriversDir();
    QString indiServerDir = QFileInfo(Options::indiServer()).dir().path();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath();
    if (Options::indiDriversAreInternal())
        driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
#endif

    QJsonObject startupRule = dv->startupRule();
    auto PreDelay = startupRule["PreDelay"].toInt(0);

    // Sleep for PreDelay seconds if required.
    if (PreDelay > 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(PreDelay));
    }

    // Startup Script?
    auto PreScript = startupRule["PreScript"].toString();
    if (!PreScript.isEmpty())
    {
        QProcess script;
        QEventLoop loop;
        QObject::connect(&script, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished), &loop, &QEventLoop::quit);
        QObject::connect(&script, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
        script.start(PreScript, QStringList());
        loop.exec();

        if (script.exitCode() != 0)
        {
            emit driverFailed(dv, i18n("Pre driver startup script failed with exit code: %1", script.exitCode()));
            return;
        }
    }

    // Remote host?
    if (dv->getRemoteHost().isEmpty() == false)
    {
        QString driverString = dv->getName() + "@" + dv->getRemoteHost() + ":" + dv->getRemotePort();
        qCDebug(KSTARS_INDI) << "Starting Remote INDI Driver" << driverString;
        out << "start " << driverString << Qt::endl;
        out.flush();
    }
    // Local?
    else
    {
        QStringList paths;
        paths << "/usr/bin"
              << "/usr/local/bin" << driversDir << indiServerDir;

        if (QStandardPaths::findExecutable(dv->getExecutable()).isEmpty())
        {
            if (QStandardPaths::findExecutable(dv->getExecutable(), paths).isEmpty())
            {
                emit driverFailed(dv, i18n("Driver %1 was not found on the system. Please make sure the package that "
                                           "provides the '%1' binary is installed.",
                                           dv->getExecutable()));
                return;
            }
        }

        qCDebug(KSTARS_INDI) << "Starting INDI Driver " << dv->getExecutable();

        out << "start " << dv->getExecutable();
        if (dv->getUniqueLabel().isEmpty() == false)
            out << " -n \"" << dv->getUniqueLabel() << "\"";
        if (dv->getSkeletonFile().isEmpty() == false)
            out << " -s \"" << driversDir << QDir::separator() << dv->getSkeletonFile() << "\"";
        out << Qt::endl;
        out.flush();

        dv->setServerState(true);

        dv->setPort(port);
    }

    auto PostDelay = startupRule["PostDelay"].toInt(0);

    // Sleep for PostDelay seconds if required.
    if (PostDelay > 0)
    {
        std::this_thread::sleep_for(std::chrono::seconds(PostDelay));
    }

    // Startup Script?
    auto PostScript = startupRule["PostScript"].toString();
    if (!PostScript.isEmpty())
    {
        QProcess script;
        QEventLoop loop;
        QObject::connect(&script, static_cast<void (QProcess::*)(int exitCode, QProcess::ExitStatus status)>(&QProcess::finished), &loop, &QEventLoop::quit);
        QObject::connect(&script, &QProcess::errorOccurred, &loop, &QEventLoop::quit);
        script.start(PostScript, QStringList());
        loop.exec();

        if (script.exitCode() != 0)
        {
            emit driverFailed(dv, i18n("Post driver startup script failed with exit code: %1", script.exitCode()));
            return;
        }
    }

    // Remove driver from pending list.
    m_PendingDrivers.erase(std::remove_if(m_PendingDrivers.begin(), m_PendingDrivers.end(), [dv](const auto & oneDriver)
    {
        return dv == oneDriver;
    }), m_PendingDrivers.end());
    emit driverStarted(dv);
}

void ServerManager::stopDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    m_ManagedDrivers.removeOne(dv);

    qCDebug(KSTARS_INDI) << "Stopping INDI Driver " << dv->getExecutable();

    if (dv->getUniqueLabel().isEmpty() == false)
        out << "stop " << dv->getExecutable() << " -n \"" << dv->getUniqueLabel() << "\"";
    else
        out << "stop " << dv->getExecutable();
    out << Qt::endl;
    out.flush();
    dv->setServerState(false);
    dv->setPort(dv->getUserPort());

    emit driverStopped(dv);
}


bool ServerManager::restartDriver(DriverInfo *dv)
{
    ClientManager *cm = dv->getClientManager();

    // N.B. This MUST be called BEFORE stopping driver below
    // Since it requires the driver device pointer.
    cm->removeManagedDriver(dv);

    // Stop driver.
    stopDriver(dv);

    // Wait 1 second before starting the driver again.
    QTimer::singleShot(1000, this, [this, dv, cm]()
    {
        cm->appendManagedDriver(dv);
        m_ManagedDrivers.append(dv);
        dv->setServerManager(this);

        QTextStream out(&indiFIFO);

        QString driversDir    = Options::indiDriversDir();
        QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
        if (Options::indiServerIsInternal())
            indiServerDir = QCoreApplication::applicationDirPath();
        if (Options::indiDriversAreInternal())
            driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
        else
            indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

        if (dv->getRemoteHost().isEmpty() == false)
        {
            QString driverString = dv->getName() + "@" + dv->getRemoteHost() + ":" + dv->getRemotePort();
            qCDebug(KSTARS_INDI) << "Restarting Remote INDI Driver" << driverString;
            out << "start " << driverString;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            out << Qt::endl;
#else
            out << Qt::endl;
#endif
            out.flush();
        }
        else
        {
            QStringList paths;
            paths << "/usr/bin"
                  << "/usr/local/bin" << driversDir << indiServerDir;

            qCDebug(KSTARS_INDI) << "Starting INDI Driver " << dv->getExecutable();

            out << "start " << dv->getExecutable();
            if (dv->getUniqueLabel().isEmpty() == false)
                out << " -n \"" << dv->getUniqueLabel() << "\"";
            if (dv->getSkeletonFile().isEmpty() == false)
                out << " -s \"" << driversDir << QDir::separator() << dv->getSkeletonFile() << "\"";
            out << Qt::endl;
            out.flush();

            dv->setServerState(true);
            dv->setPort(port);
        }
    });

    emit driverRestarted(dv);
    return true;
}

void ServerManager::stop()
{
    if (serverProcess.get() == nullptr)
        return;

    for (auto &device : m_ManagedDrivers)
    {
        device->reset();
    }

    qCDebug(KSTARS_INDI) << "Stopping INDI Server " << host << "@" << port;

    serverProcess->disconnect(this);

    serverBuffer.close();

    serverProcess->terminate();

    serverProcess->waitForFinished();

    serverProcess.reset();

    indiFIFO.close();
    QFile::remove(indiFIFO.fileName());
    emit stopped();

}

void ServerManager::processServerError(QProcess::ProcessError err)
{
    Q_UNUSED(err)
    emit terminated(i18n("Connection to INDI server %1:%2 terminated: %3.",
                         getHost(), getPort(), serverProcess.get()->errorString()));
}

void ServerManager::processStandardError()
{
#ifdef Q_OS_WIN
    qWarning() << "INDI server is currently not supported on Windows.";
    return;
#else
    QString stderr = serverProcess->readAllStandardError();

    for (auto &msg : stderr.split('\n'))
        qCDebug(KSTARS_INDI) << "INDI Server: " << msg;

    serverBuffer.write(stderr.toLatin1());
    emit newServerLog();

    //if (driverCrashed == false && (stderr.contains("stdin EOF") || stderr.contains("stderr EOF")))
    QRegularExpression re("Driver (.*): Terminated after #0 restarts");
    QRegularExpressionMatch match = re.match(stderr);
    if (match.hasMatch())
    {
        QString driverExec = match.captured(1);
        qCCritical(KSTARS_INDI) << "INDI driver " << driverExec << " crashed!";

        //KSNotification::info(i18n("KStars detected INDI driver %1 crashed. Please check INDI server log in the Device Manager.", driverName));

        auto crashedDriver = std::find_if(m_ManagedDrivers.begin(), m_ManagedDrivers.end(),
                                          [driverExec](DriverInfo * dv)
        {
            return dv->getExecutable() == driverExec;
        });

        if (crashedDriver != m_ManagedDrivers.end())
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [this, crashedDriver]()
            {
                //QObject::disconnect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, nullptr);
                KSMessageBox::Instance()->disconnect(this);
                restartDriver(*crashedDriver);
            });

            QString label = (*crashedDriver)->getUniqueLabel();
            if (label.isEmpty())
                label = (*crashedDriver)->getExecutable();
            KSMessageBox::Instance()->warningContinueCancel(i18n("INDI Driver <b>%1</b> crashed. Restart it?",
                    label), i18n("Driver crash"), 10);
        }
    }
#endif
}

QString ServerManager::errorString()
{
    if (serverProcess.get() != nullptr)
        return serverProcess->errorString();

    return nullptr;
}

QString ServerManager::getLogBuffer()
{
    serverBuffer.flush();
    serverBuffer.close();

    serverBuffer.open();
    return serverBuffer.readAll();
}
