/*  INDI Server Manager
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

 */

#include "servermanager.h"

#include "driverinfo.h"
#include "drivermanager.h"
#include "auxiliary/kspaths.h"
#include "Options.h"

#include <indidevapi.h>

#include <KMessageBox>
#include <QUuid>

#include <sys/stat.h>

#include <indi_debug.h>

ServerManager::ServerManager(const QString& inHost, uint inPort)
{
    host          = inHost;
    port          = QString::number(inPort);

    //qDebug() << "We got port unit with value of " << inPort << " and now as tring it is equal to #" << port << "#" << endl;
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
    int fd         = 0;

    if (serverProcess.get() == nullptr)
    {
        serverProcess.reset(new QProcess(this));
#ifdef Q_OS_OSX
        QString driversDir = Options::indiDriversDir();
        if (Options::indiDriversAreInternal())
            driversDir = QCoreApplication::applicationDirPath() + "/indi";
        QString indiServerDir = Options::indiServer();
        if (Options::indiServerIsInternal())
            indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
        else
            indiServerDir = QFileInfo(Options::indiServer()).dir().path();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PATH", driversDir + ':' + indiServerDir + ":/usr/local/bin:/usr/bin:/bin");
        QString gscDirPath = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "gsc";
        env.insert("GSCDAT", gscDirPath);
        QString gphoto_iolibs = QCoreApplication::applicationDirPath() + "/../PlugIns/libgphoto2_port";
        if (QFileInfo(gphoto_iolibs).exists() && Options::indiDriversAreInternal())
            env.insert("IOLIBS", QDir(gphoto_iolibs).absolutePath());
        QString gphoto_camlibs = QCoreApplication::applicationDirPath() + "/../PlugIns/libgphoto2";
        if (QFileInfo(gphoto_camlibs).exists() && Options::indiDriversAreInternal())
            env.insert("CAMLIBS", QDir(gphoto_camlibs).absolutePath());
        QString qhyFirmwarePath = QCoreApplication::applicationDirPath() + "/../PlugIns/qhy";
        if (QFileInfo(qhyFirmwarePath).exists() && Options::indiDriversAreInternal())
            env.insert("QHY_FIRMWARE_DIR", QDir(qhyFirmwarePath).absolutePath());
        env.insert("INDIPREFIX", indiServerDir);
        serverProcess->setProcessEnvironment(env);
#endif
    }

    QStringList args;

    args << "-v"
         << "-p" << port << "-m"
         << "100";

    QString fifoFile = QString("/tmp/indififo%1").arg(QUuid::createUuid().toString().mid(1, 8));

    if ((fd = mkfifo(fifoFile.toLatin1(), S_IRUSR | S_IWUSR) < 0))
    {
        KMessageBox::error(nullptr, i18n("Error making FIFO file %1: %2.", fifoFile, strerror(errno)));
        return false;
    }

    indiFIFO.setFileName(fifoFile);

    driverCrashed = false;

    if (!indiFIFO.open(QIODevice::ReadWrite | QIODevice::Text))
    {
        qCCritical(KSTARS_INDI) << "Unable to create INDI FIFO file: " << fifoFile << endl;
        return false;
    }

    args << "-f" << fifoFile;

    qCDebug(KSTARS_INDI) << "Starting INDI Server: " << args << "-f" << fifoFile;

    serverProcess->setProcessChannelMode(QProcess::SeparateChannels);
    serverProcess->setReadChannel(QProcess::StandardError);

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        serverProcess->start(QCoreApplication::applicationDirPath() + "/indi/indiserver", args);
    else
#endif
        serverProcess->start(Options::indiServer(), args);

    connected = serverProcess->waitForStarted();

    if (connected)
    {
        connect(serverProcess.get(), SIGNAL(error(QProcess::ProcessError)), this,
                SLOT(processServerError(QProcess::ProcessError)));
        connect(serverProcess.get(), SIGNAL(readyReadStandardError()), this, SLOT(processStandardError()));

        emit started();
    }
    else
        KMessageBox::error(0, i18n("INDI server failed to start: %1", serverProcess->errorString()));

    qCDebug(KSTARS_INDI) << "INDI Server Started? " << connected;

    return connected;
#endif
}

bool ServerManager::startDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    // Check for duplicates within existing clients
    if (dv->getUniqueLabel().isEmpty() && dv->getTreeLabel().isEmpty() == false)
        dv->setUniqueLabel(DriverManager::Instance()->getUniqueDeviceLabel(dv->getTreeLabel()));

    // Check for duplicates within managed drivers
    if (dv->getUniqueLabel().isEmpty() == false)
    {
        int nset = 0;
        QString uniqueLabel;
        QString label = dv->getUniqueLabel();
        foreach (DriverInfo *drv, managedDrivers)
        {
            if (label == drv->getUniqueLabel())
                nset++;
        }
        if (nset > 0)
        {
            uniqueLabel = QString("%1 %2").arg(label).arg(nset + 1);
            dv->setUniqueLabel(uniqueLabel);
        }
    }

    managedDrivers.append(dv);
    dv->setServerManager(this);

    QString driversDir    = Options::indiDriversDir();
    QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
    if (Options::indiServerIsInternal())
        driversDir = QCoreApplication::applicationDirPath() + "/indi";
    if (Options::indiDriversAreInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    QStringList paths;
    paths << "/usr/bin"
          << "/usr/local/bin" << driversDir << indiServerDir;

    if (QStandardPaths::findExecutable(dv->getDriver()).isEmpty())
    {
        if (QStandardPaths::findExecutable(dv->getDriver(), paths).isEmpty())
        {
            KMessageBox::error(nullptr, i18n("Driver %1 was not found on the system. Please make sure the package that "
                                             "provides the '%1' binary is installed.",
                                             dv->getDriver()));
            return false;
        }
    }

    qCDebug(KSTARS_INDI) << "Starting INDI Driver " << dv->getDriver();

    out << "start " << dv->getDriver();
    if (dv->getUniqueLabel().isEmpty() == false)
        out << " -n \"" << dv->getUniqueLabel() << "\"";
    if (dv->getSkeletonFile().isEmpty() == false)
        out << " -s \"" << driversDir << QDir::separator() << dv->getSkeletonFile() << "\"";
    out << endl;

    out.flush();

    dv->setServerState(true);

    dv->setPort(port);

    return true;
}

void ServerManager::stopDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    managedDrivers.removeOne(dv);

    qCDebug(KSTARS_INDI) << "Stopping INDI Driver " << dv->getDriver();

    if (dv->getUniqueLabel().isEmpty() == false)
        out << "stop " << dv->getDriver() << " -n \"" << dv->getUniqueLabel() << "\"" << endl;
    else
        out << "stop " << dv->getDriver() << endl;

    out.flush();
    dv->setServerState(false);
    dv->setPort(dv->getUserPort());
}

void ServerManager::stop()
{
    if (serverProcess.get() == nullptr)
        return;

    foreach (DriverInfo *device, managedDrivers)
    {
        device->setServerState(false);
        device->clear();
    }

    qCDebug(KSTARS_INDI) << "Stopping INDI Server " << host << "@" << port;

    serverProcess->disconnect(SIGNAL(error(QProcess::ProcessError)));

    serverProcess->terminate();

    serverProcess->waitForFinished();

    serverProcess.reset();
}

void ServerManager::terminate()
{
    if (serverProcess.get() == nullptr)
        return;

    serverProcess->terminate();

    serverProcess->waitForFinished();
}

void ServerManager::connectionSuccess()
{
    foreach (DriverInfo *device, managedDrivers)
        device->setServerState(true);

    connect(serverProcess.get(), SIGNAL(readyReadStandardError()), this, SLOT(processStandardError()));

    emit started();
}

void ServerManager::processServerError(QProcess::ProcessError err)
{
    INDI_UNUSED(err);
    emit serverFailure(this);
}

void ServerManager::processStandardError()
{
#ifdef Q_OS_WIN
    qWarning() << "INDI server is currently not supported on Windows.";
    return;
#else
    QString stderr = serverProcess->readAllStandardError();

    serverBuffer.append(stderr);

   for (auto &msg : stderr.split('\n'))
            qCDebug(KSTARS_INDI) << "INDI Server: " << msg;

    if (driverCrashed == false && (serverBuffer.contains("stdin EOF") || serverBuffer.contains("stderr EOF")))
    {
        QStringList parts = serverBuffer.split("Driver");
        for (auto &driver : parts)
        {
            if (driver.contains("stdin EOF") || driver.contains("stderr EOF"))
            {
                driverCrashed      = true;
                QString driverName = driver.left(driver.indexOf(':')).trimmed();
                qCCritical(KSTARS_INDI) << "INDI driver " << driverName << " crashed!";
                KMessageBox::information(
                    0,
                    i18n("KStars detected INDI driver %1 crashed. Please check INDI server log in the Device Manager.",
                         driverName));
                break;
            }
        }
    }

    emit newServerLog();
#endif
}

QString ServerManager::errorString()
{
    if (serverProcess.get() != nullptr)
        return serverProcess->errorString();

    return nullptr;
}
