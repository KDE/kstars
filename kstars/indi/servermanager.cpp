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

#include <KMessageBox>
#include <QUuid>

#include <sys/stat.h>

#include <indi_debug.h>

ServerManager::ServerManager(const QString &inHost, uint inPort)
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
        serverBuffer.open();

        serverProcess.reset(new QProcess(this));
#ifdef Q_OS_OSX
        QString driversDir = Options::indiDriversDir();
        if (Options::indiDriversAreInternal())
            driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
        QString indiServerDir = Options::indiServer();
        if (Options::indiServerIsInternal())
            indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
        else
            indiServerDir = QFileInfo(Options::indiServer()).dir().path();
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("PATH", driversDir + ':' + indiServerDir + ":/usr/local/bin:/usr/bin:/bin");
        QString gscDirPath = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("gsc");
        env.insert("GSCDAT", gscDirPath);

        insertEnvironmentPath(&env, "INDIPREFIX", "/../../");
        insertEnvironmentPath(&env, "IOLIBS", "/../Resources/DriverSupport/gphoto/IOLIBS");
        insertEnvironmentPath(&env, "CAMLIBS", "/../Resources/DriverSupport/gphoto/CAMLIBS");

        serverProcess->setProcessEnvironment(env);
#endif
    }

    QStringList args;

    args << "-v" << "-p" << port;

    QString fifoFile = QString("/tmp/indififo%1").arg(QUuid::createUuid().toString().mid(1, 8));

    if ((fd = mkfifo(fifoFile.toLatin1(), S_IRUSR | S_IWUSR)) < 0)
    {
        KSNotification::error(i18n("Error making FIFO file %1: %2.", fifoFile, strerror(errno)));
        return false;
    }

    indiFIFO.setFileName(fifoFile);

    //driverCrashed = false;

    if (!indiFIFO.open(QIODevice::ReadWrite | QIODevice::Text))
    {
        qCCritical(KSTARS_INDI) << "Unable to create INDI FIFO file: " << fifoFile;
        return false;
    }

    args << "-m" << QString::number(Options::serverTransferBufferSize()) << "-r" << "0" << "-f" << fifoFile;

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
        KSNotification::error(i18n("INDI server failed to start: %1", serverProcess->errorString()));

    qCDebug(KSTARS_INDI) << "INDI Server Started? " << connected;

    return connected;
#endif
}

void ServerManager::insertEnvironmentPath(QProcessEnvironment *env, QString variable, QString relativePath)
{
    QString environmentPath = QCoreApplication::applicationDirPath() + relativePath;
    if (QFileInfo::exists(environmentPath) && Options::indiDriversAreInternal())
        env->insert(variable, QDir(environmentPath).absolutePath());
}

bool ServerManager::startDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    // Check for duplicates within existing clients
    if (dv->getUniqueLabel().isEmpty() && dv->getLabel().isEmpty() == false)
        dv->setUniqueLabel(DriverManager::Instance()->getUniqueDeviceLabel(dv->getLabel()));

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
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    if (Options::indiDriversAreInternal())
        driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();
#endif

    if (dv->getRemoteHost().isEmpty() == false)
    {
        QString driverString = dv->getName() + "@" + dv->getRemoteHost() + ":" + dv->getRemotePort();
        qCDebug(KSTARS_INDI) << "Starting Remote INDI Driver" << driverString;
        out << "start " << driverString;
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        out << Qt::endl;
#else
        out << endl;
#endif
        out.flush();
    }
    else
    {
        QStringList paths;
        paths << "/usr/bin"
              << "/usr/local/bin" << driversDir << indiServerDir;

        if (QStandardPaths::findExecutable(dv->getExecutable()).isEmpty())
        {
            if (QStandardPaths::findExecutable(dv->getExecutable(), paths).isEmpty())
            {
                KSNotification::error(i18n("Driver %1 was not found on the system. Please make sure the package that "
                                           "provides the '%1' binary is installed.",
                                           dv->getExecutable()));
                return false;
            }
        }

        qCDebug(KSTARS_INDI) << "Starting INDI Driver " << dv->getExecutable();

        out << "start " << dv->getExecutable();
        if (dv->getUniqueLabel().isEmpty() == false)
            out << " -n \"" << dv->getUniqueLabel() << "\"";
        if (dv->getSkeletonFile().isEmpty() == false)
            out << " -s \"" << driversDir << QDir::separator() << dv->getSkeletonFile() << "\"";
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        out << Qt::endl;
#else
        out << endl;
#endif

        out.flush();

        dv->setServerState(true);

        dv->setPort(port);
    }

    return true;
}

void ServerManager::stopDriver(DriverInfo *dv)
{
    QTextStream out(&indiFIFO);

    managedDrivers.removeOne(dv);

    qCDebug(KSTARS_INDI) << "Stopping INDI Driver " << dv->getExecutable();

    if (dv->getUniqueLabel().isEmpty() == false)
        out << "stop " << dv->getExecutable() << " -n \"" << dv->getUniqueLabel() << "\"";
    else
        out << "stop " << dv->getExecutable();

#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    out << Qt::endl;
#else
    out << endl;
#endif

    out.flush();
    dv->setServerState(false);
    dv->setPort(dv->getUserPort());
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
        managedDrivers.append(dv);
        dv->setServerManager(this);

        QTextStream out(&indiFIFO);

        QString driversDir    = Options::indiDriversDir();
        QString indiServerDir = Options::indiServer();

#ifdef Q_OS_OSX
        if (Options::indiServerIsInternal())
            indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
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
            out << endl;
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
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            out << Qt::endl;
#else
            out << endl;
#endif

            out.flush();

            dv->setServerState(true);
            dv->setPort(port);
        }
    });

    return true;
}

void ServerManager::stop()
{
    if (serverProcess.get() == nullptr)
        return;

    for (auto device : managedDrivers)
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
    for (auto device : managedDrivers)
        device->setServerState(true);

    connect(serverProcess.get(), &QProcess::readyReadStandardError, this, &ServerManager::processStandardError);

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

        auto crashedDriver = std::find_if(managedDrivers.begin(), managedDrivers.end(),
                                          [driverExec](DriverInfo * dv)
        {
            return dv->getExecutable() == driverExec;
        });

        if (crashedDriver != managedDrivers.end())
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
