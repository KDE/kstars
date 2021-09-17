/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "drivermanager.h"

#include "config-kstars.h"

#include "clientmanager.h"
#include "driverinfo.h"
#include "guimanager.h"
#include "indilistener.h"
#include "kspaths.h"
#include "kstars.h"
#include "indidbus.h"
#include "kstarsdata.h"
#include "Options.h"
#include "servermanager.h"
#include "ui_indihostconf.h"
#include "auxiliary/ksnotification.h"

#include <basedevice.h>

#ifndef KSTARS_LITE
#include <KMessageBox>
#include <KActionCollection>
#include <KNotifications/KNotification>
#endif

#include <QTcpServer>
#include <indi_debug.h>

#define INDI_MAX_TRIES 2
#define ERRMSG_SIZE    1024

DriverManagerUI::DriverManagerUI(QWidget *parent) : QFrame(parent)
{
    setupUi(this);

    localTreeWidget->setSortingEnabled(false);
    localTreeWidget->setRootIsDecorated(true);

    clientTreeWidget->setSortingEnabled(false);

    runningPix = QIcon::fromTheme("system-run");
    stopPix    = QIcon::fromTheme("dialog-cancel");
    localMode  = QIcon::fromTheme("computer");
    serverMode = QIcon::fromTheme("network-server");

    connected    = QIcon::fromTheme("network-connect");
    disconnected = QIcon::fromTheme("network-disconnect");

    connect(localTreeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this,
            SLOT(makePortEditable(QTreeWidgetItem*, int)));
}

void DriverManagerUI::makePortEditable(QTreeWidgetItem *selectedItem, int column)
{
    // If it's the port column, then make it user-editable
    if (column == ::DriverManager::LOCAL_PORT_COLUMN)
        selectedItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEditable | Qt::ItemIsEnabled);

    localTreeWidget->editItem(selectedItem, ::DriverManager::LOCAL_PORT_COLUMN);
}

DriverManager *DriverManager::_DriverManager = nullptr;

DriverManager *DriverManager::Instance()
{
    if (_DriverManager == nullptr)
    {
        _DriverManager = new DriverManager(KStars::Instance());
        INDIDBus *indiDBUS = new INDIDBus(KStars::Instance());
        Q_UNUSED(indiDBUS)
    }

    return _DriverManager;
}

DriverManager::DriverManager(QWidget *parent) : QDialog(parent)
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif

    currentPort = Options::serverPortStart() - 1;

    QVBoxLayout *mainLayout = new QVBoxLayout;
    ui                      = new DriverManagerUI(this);
    mainLayout->addWidget(ui);
    setLayout(mainLayout);
    setWindowTitle(i18nc("@title:window", "Device Manager"));

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, SIGNAL(rejected()), this, SLOT(close()));

    connect(ui->addB, SIGNAL(clicked()), this, SLOT(addINDIHost()));
    connect(ui->modifyB, SIGNAL(clicked()), this, SLOT(modifyINDIHost()));
    connect(ui->removeB, SIGNAL(clicked()), this, SLOT(removeINDIHost()));

    connect(ui->connectHostB, SIGNAL(clicked()), this, SLOT(activateHostConnection()));
    connect(ui->disconnectHostB, SIGNAL(clicked()), this, SLOT(activateHostDisconnection()));
    connect(ui->runServiceB, SIGNAL(clicked()), this, SLOT(activateRunService()));
    connect(ui->stopServiceB, SIGNAL(clicked()), this, SLOT(activateStopService()));
    connect(ui->localTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(updateLocalTab()));
    connect(ui->clientTreeWidget, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this, SLOT(updateClientTab()));
    connect(ui->localTreeWidget, SIGNAL(expanded(QModelIndex)), this, SLOT(resizeDeviceColumn()));

    // Do not use KSPaths here, this is for INDI
    if (Options::indiDriversDir().isEmpty())
        Options::setIndiDriversDir(
            QStandardPaths::locate(QStandardPaths::GenericDataLocation, "indi", QStandardPaths::LocateDirectory));

    readXMLDrivers();

    readINDIHosts();

    m_CustomDrivers = new CustomDrivers(this, driversList);

    updateCustomDrivers();

#ifdef Q_OS_WIN
    ui->localTreeWidget->setEnabled(false);
#endif
}

DriverManager::~DriverManager()
{
    clearServers();
    qDeleteAll(driversList);
}

void DriverManager::processDeviceStatus(DriverInfo *dv)
{
    if (dv == nullptr)
        return;

    if (dv->getDriverSource() == GENERATED_SOURCE)
        return;

    QString currentDriver;
    ServerMode mode        = connectionMode;
    ServerManager *manager = dv->getServerManager();
    bool dState            = false;
    bool cState            = false;

    if (dv->getDriverSource() != HOST_SOURCE)
    {
        if (ui->localTreeWidget->currentItem())
            currentDriver = ui->localTreeWidget->currentItem()->text(LOCAL_NAME_COLUMN);

        for (auto &item : ui->localTreeWidget->findItems(dv->getLabel(), Qt::MatchExactly | Qt::MatchRecursive))
        {
            item->setText(LOCAL_VERSION_COLUMN, dv->getVersion());

            if (manager)
                mode = manager->getMode();

            dState = dv->getServerState();
            cState = dv->getClientState() && dState;

            bool locallyAvailable = false;
            if (dv->getAuxInfo().contains("LOCALLY_AVAILABLE"))
                locallyAvailable = dv->getAuxInfo().value("LOCALLY_AVAILABLE", false).toBool();

            switch (mode)
            {
                case SERVER_ONLY:
                    if (locallyAvailable)
                    {
                        ui->runServiceB->setEnabled(!dState);
                        ui->stopServiceB->setEnabled(dState);
                        item->setIcon(LOCAL_STATUS_COLUMN, dState ? ui->runningPix : ui->stopPix);
                    }
                    else
                    {
                        ui->runServiceB->setEnabled(false);
                        ui->stopServiceB->setEnabled(false);
                    }

                    if (dState)
                    {
                        item->setIcon(LOCAL_MODE_COLUMN, ui->serverMode);
                        if (manager)
                            item->setText(LOCAL_PORT_COLUMN, QString(manager->getPort()));
                    }
                    else
                    {
                        item->setIcon(LOCAL_MODE_COLUMN, QIcon());
                        item->setText(LOCAL_PORT_COLUMN, dv->getUserPort());
                    }

                    break;

                case SERVER_CLIENT:
                    if (locallyAvailable)
                    {
                        ui->runServiceB->setEnabled(!cState);
                        ui->stopServiceB->setEnabled(cState);
                        item->setIcon(LOCAL_STATUS_COLUMN, cState ? ui->runningPix : ui->stopPix);
                    }
                    else
                    {
                        ui->runServiceB->setEnabled(false);
                        ui->stopServiceB->setEnabled(false);
                    }

                    if (cState)
                    {
                        item->setIcon(LOCAL_MODE_COLUMN, ui->localMode);

                        if (manager)
                            item->setText(LOCAL_PORT_COLUMN, QString(manager->getPort()));
                    }
                    else
                    {
                        item->setIcon(LOCAL_MODE_COLUMN, QIcon());
                        item->setText(LOCAL_PORT_COLUMN, dv->getUserPort() == "-1" ? "" : dv->getUserPort());
                    }

                    break;
            }

            // Only update the log if the current driver is selected
            if (currentDriver == dv->getLabel())
            {
                ui->serverLogText->clear();
                ui->serverLogText->append(dv->getServerBuffer());
            }
        }
    }
    else
    {
        for (auto &item : ui->clientTreeWidget->findItems(dv->getName(), Qt::MatchExactly, HOST_NAME_COLUMN))
        {
            if (dv->getClientState())
            {
                item->setIcon(HOST_STATUS_COLUMN, ui->connected);
                ui->connectHostB->setEnabled(false);
                ui->disconnectHostB->setEnabled(true);
            }
            else
            {
                item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
                ui->connectHostB->setEnabled(true);
                ui->disconnectHostB->setEnabled(false);
            }
        }
    }
}

void DriverManager::getUniqueHosts(QList<DriverInfo *> &dList, QList<QList<DriverInfo *>> &uHosts)
{
    bool found = false;

    // Iterate over all drivers
    for (DriverInfo *dv : dList)
    {
        QList<DriverInfo *> uList;

        // Let's see for drivers with identical hosts and ports
        for (DriverInfo *idv : dList)
        {
            // If we get a match between port and hostname, we add it to the list
            if ( (dv->getHost() == idv->getHost() && dv->getPort() == idv->getPort()))
            {
                // Check if running already
                if (dv->getClientState() || dv->getServerState())
                {
                    int ans = KMessageBox::warningContinueCancel(
                                  nullptr, i18n("Driver %1 is already running, do you want to restart it?", dv->getLabel()));
                    if (ans == KMessageBox::Cancel)
                        continue;
                    else
                    {
                        QList<DriverInfo *> stopList;
                        stopList.append(dv);
                        stopDevices(stopList);
                    }
                }

                found = false;

                // Check to see if the driver already been added elsewhere
                for (auto &qdi : uHosts)
                {
                    for (DriverInfo *di : qdi)
                    {
                        if (di == idv)
                        {
                            found = true;
                            break;
                        }
                    }
                }

                if (found == false)
                    uList.append(idv);
            }
        }

        if (uList.empty() == false)
            uHosts.append(uList);
    }
}

bool DriverManager::startDevices(QList<DriverInfo *> &dList)
{
    ServerManager *serverManager = nullptr;
    ClientManager *clientManager = nullptr;
    int port                     = -1;

    QList<QList<DriverInfo *>> uHosts;

    bool connectionToServer = false;

    getUniqueHosts(dList, uHosts);

    qCDebug(KSTARS_INDI) << "INDI: Starting local drivers...";

    for (auto &qdv : uHosts)
    {
        if (qdv.empty())
            continue;

        port = qdv.at(0)->getPort().toInt();

        // Select random port within range is none specified.
        if (port == -1)
            port = getINDIPort(port);

        if (port <= 0)
        {
            KSNotification::error(i18n("Cannot start INDI server: port error."));
            return false;
        }

        serverManager = new ServerManager(qdv.at(0)->getHost(), port);

        if (serverManager == nullptr)
        {
            qWarning() << "Warning: device manager has not been established properly";
            return false;
        }

        serverManager->setMode(connectionMode);

        connect(serverManager, SIGNAL(newServerLog()), this, SLOT(updateLocalTab()));
        connect(serverManager, &ServerManager::started, [&]()
        {
            emit serverStarted(qdv.at(0)->getHost(), QString::number(port));
        });

        if (serverManager->start())
            servers.append(serverManager);
        else
        {
            delete serverManager;
            return false;
        }

        qCDebug(KSTARS_INDI) << "INDI: INDI Server started locally on port " << port;

        for (DriverInfo *dv : qdv)
        {
            if (serverManager->startDriver(dv) == false)
            {
                servers.removeOne(serverManager);
                serverManager->stop();
                emit serverTerminated(serverManager->getHost(), serverManager->getPort());
                delete serverManager;
                return false;
            }
        }

        // Nothing to do more if SERVER ONLY
        if (connectionMode == SERVER_ONLY)
            continue;

        clientManager = new ClientManager();

        for (DriverInfo *dv : qdv)
            clientManager->appendManagedDriver(dv);

        connect(clientManager, &ClientManager::connectionFailure, this, &DriverManager::processClientTermination,
                Qt::BlockingQueuedConnection);

        clientManager->setServer(qdv.at(0)->getHost().toLatin1().constData(), port);

        GUIManager::Instance()->addClient(clientManager);
        INDIListener::Instance()->addClient(clientManager);

        for (int i = 0; i < INDI_MAX_TRIES; i++)
        {
            qCDebug(KSTARS_INDI) << "INDI: Connecting to local INDI server on port " << port << " ...";

            connectionToServer = clientManager->connectServer();

            if (connectionToServer)
                break;

            qApp->processEvents();

            //usleep(100000);
            QThread::usleep(100000);
        }

        if (connectionToServer)
        {
            qCDebug(KSTARS_INDI) << "Connection to INDI server is successful";

            clients.append(clientManager);
            updateMenuActions();
        }
        else
        {
            qCDebug(KSTARS_INDI) << "INDI: Connection to local INDI server on port " << port << " failed!";

            KNotification::beep();
            QPointer<QMessageBox> msgBox = new QMessageBox();
            msgBox->setAttribute(Qt::WA_DeleteOnClose);
            msgBox->setStandardButtons(QMessageBox::Ok);
            msgBox->setWindowTitle(i18nc("@title:window", "Error"));
            msgBox->setText(i18n("Connection to INDI server locally on port %1 failed.", port));
            msgBox->setModal(false);
            msgBox->setIcon(QMessageBox::Critical);
            msgBox->show();

            for (DriverInfo *dv : qdv)
                processDeviceStatus(dv);

            GUIManager::Instance()->removeClient(clientManager);
            INDIListener::Instance()->removeClient(clientManager);

            return false;
        }
    }

    return true;
}

void DriverManager::stopDevices(const QList<DriverInfo *> &dList)
{
    qCDebug(KSTARS_INDI) << "INDI: Stopping local drivers...";

    // #1 Disconnect all clients
    for (DriverInfo *dv : dList)
    {
        ClientManager *cm = dv->getClientManager();

        if (cm == nullptr)
            continue;

        cm->removeManagedDriver(dv);

        if (cm->count() == 0)
        {
            GUIManager::Instance()->removeClient(cm);
            INDIListener::Instance()->removeClient(cm);
            cm->disconnectServer();
            clients.removeOne(cm);
            cm->deleteLater();

            KStars::Instance()->slotSetTelescopeEnabled(false);
            KStars::Instance()->slotSetDomeEnabled(false);
        }
    }

    // #2 Disconnect all servers
    for (DriverInfo *dv : dList)
    {
        ServerManager *sm = dv->getServerManager();

        if (sm != nullptr)
        {
            sm->stopDriver(dv);

            if (sm->size() == 0)
            {
                sm->stop();
                servers.removeOne(sm);
                sm->deleteLater();
            }
        }
    }

    // Reset current port
    currentPort = Options::serverPortStart() - 1;

    updateMenuActions();
}

void DriverManager::clearServers()
{
    foreach (ServerManager *serverManager, servers)
        serverManager->terminate();

    qDeleteAll(servers);
}

void DriverManager::activateRunService()
{
    processLocalTree(true);
}

void DriverManager::activateStopService()
{
    processLocalTree(false);
}

void DriverManager::activateHostConnection()
{
    processRemoteTree(true);
}

void DriverManager::activateHostDisconnection()
{
    processRemoteTree(false);
}

ClientManager *DriverManager::getClientManager(DriverInfo *dv)
{
    return dv->getClientManager();
}

void DriverManager::updateLocalTab()
{
    if (ui->localTreeWidget->currentItem() == nullptr)
        return;

    QString currentDriver = ui->localTreeWidget->currentItem()->text(LOCAL_NAME_COLUMN);

    foreach (DriverInfo *device, driversList)
    {
        if (currentDriver == device->getLabel())
        {
            processDeviceStatus(device);
            break;
        }
    }
}

void DriverManager::updateClientTab()
{
    QTreeWidgetItem *item = ui->clientTreeWidget->currentItem();

    if (item == nullptr)
        return;

    QString hostname = item->text(HOST_NAME_COLUMN);
    QString hostport = item->text(HOST_PORT_COLUMN);

    for (auto &dv : driversList)
    {
        if (hostname == dv->getName() && hostport == dv->getPort())
        {
            processDeviceStatus(dv);
            break;
        }
    }
}

void DriverManager::processLocalTree(bool dState)
{
    QList<DriverInfo *> processed_devices;

    int port    = -1;
    bool portOK = false;

    connectionMode = ui->localR->isChecked() ? SERVER_CLIENT : SERVER_ONLY;

    foreach (QTreeWidgetItem *item, ui->localTreeWidget->selectedItems())
    {
        foreach (DriverInfo *device, driversList)
        {
            port = -1;

            //device->state = (dev_request == DriverInfo::DEV_TERMINATE) ? DriverInfo::DEV_START : DriverInfo::DEV_TERMINATE;
            if (item->text(LOCAL_NAME_COLUMN) == device->getLabel() && device->getServerState() != dState)
            {
                processed_devices.append(device);

                // N.B. If multiple devices are selected to run under one device manager
                // then we select the port for the first device that has a valid port
                // entry, the rest are ignored.
                if (port == -1 && item->text(LOCAL_PORT_COLUMN).isEmpty() == false)
                {
                    port = item->text(LOCAL_PORT_COLUMN).toInt(&portOK);
                    // If we encounter conversion error, we abort
                    if (portOK == false)
                    {
                        KSNotification::error(i18n("Invalid port entry: %1", item->text(LOCAL_PORT_COLUMN)));
                        return;
                    }
                }

                device->setHostParameters("localhost", QString("%1").arg(port));
            }
        }
    }

    if (processed_devices.empty())
        return;

    if (dState)
        startDevices(processed_devices);
    else
        stopDevices(processed_devices);
}

void DriverManager::processClientTermination(ClientManager *client)
{
    if (client == nullptr)
        return;

    ServerManager *manager = client->getServerManager();

    if (manager)
    {
        servers.removeOne(manager);
        delete manager;
    }

    emit serverTerminated(client->getHost(), QString("%1").arg(client->getPort()));

    GUIManager::Instance()->removeClient(client);
    INDIListener::Instance()->removeClient(client);

    KSNotification::event(QLatin1String("IndiServerMessage"),
                          i18n("Connection to INDI host at %1 on port %2 lost. Server disconnected.", client->getHost(),
                               client->getPort()), KSNotification::EVENT_ALERT);

    clients.removeOne(client);
    client->deleteLater();

    updateMenuActions();
    updateLocalTab();
}

void DriverManager::processServerTermination(ServerManager *server)
{
    if (server == nullptr)
        return;

    for (auto &dv : driversList)
        if (dv->getServerManager() == server)
        {
            dv->reset();
        }

    if (server->getMode() == SERVER_ONLY)
    {
        KSNotification::error(i18n("Connection to INDI host at %1 on port %2 encountered an error: %3.",
                                   server->getHost(), server->getPort(), server->errorString()));
    }

    emit serverTerminated(server->getHost(), server->getPort());
    servers.removeOne(server);
    server->deleteLater();

    updateLocalTab();
}

void DriverManager::processRemoteTree(bool dState)
{
    QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();
    if (!currentItem)
        return;

    for (auto &dv : driversList)
    {
        if (dv->getDriverSource() != HOST_SOURCE)
            continue;

        //qDebug() << "Current item port " << currentItem->text(HOST_PORT_COLUMN) << " current dev " << dv->getName() << " -- port " << dv->getPort() << endl;
        //qDebug() << "dState is : " << (dState ? "True" : "False") << endl;

        if (currentItem->text(HOST_NAME_COLUMN) == dv->getName() &&
                currentItem->text(HOST_PORT_COLUMN) == dv->getPort())
        {
            // Nothing changed, return
            if (dv->getClientState() == dState)
                return;

            // connect to host
            if (dState)
                connectRemoteHost(dv);
            // Disconnect form host
            else
                disconnectRemoteHost(dv);
        }
    }
}

bool DriverManager::connectRemoteHost(DriverInfo *dv)
{
    bool hostPortOk              = false;
    bool connectionToServer      = false;
    ClientManager *clientManager = nullptr;

    dv->getPort().toInt(&hostPortOk);

    if (hostPortOk == false)
    {
        KSNotification::error(i18n("Invalid host port %1", dv->getPort()));
        return false;
    }

    clientManager = new ClientManager();

    clientManager->appendManagedDriver(dv);

    connect(clientManager, &ClientManager::connectionFailure, this, &DriverManager::processClientTermination,
            Qt::BlockingQueuedConnection);

    clientManager->setServer(dv->getHost().toLatin1().constData(), static_cast<uint>(dv->getPort().toInt()));

    GUIManager::Instance()->addClient(clientManager);
    INDIListener::Instance()->addClient(clientManager);

    for (int i = 0; i < INDI_MAX_TRIES; i++)
    {
        connectionToServer = clientManager->connectServer();

        if (connectionToServer)
            break;

        QThread::usleep(100000);
    }

    if (connectionToServer)
    {
        clients.append(clientManager);
        updateMenuActions();

        KSNotification::event(QLatin1String("ConnectionSuccessful"), i18n("Connected to INDI server"), KSNotification::EVENT_ALERT);
    }
    else
    {
        GUIManager::Instance()->removeClient(clientManager);
        INDIListener::Instance()->removeClient(clientManager);

        KNotification::beep();
        QMessageBox *msgBox = new QMessageBox();
        msgBox->setAttribute(Qt::WA_DeleteOnClose);
        msgBox->setStandardButtons(QMessageBox::Ok);
        msgBox->setWindowTitle(i18nc("@title:window", "Error"));
        msgBox->setText(
            i18n("Connection to INDI server at host %1 with port %2 failed.", dv->getHost(), dv->getPort()));
        KSNotification::event(QLatin1String("ConnectionFailed"), msgBox->text(), KSNotification::EVENT_ALERT);
        msgBox->setModal(false);
        msgBox->setIcon(QMessageBox::Critical);
        msgBox->show();

        processDeviceStatus(dv);
        return false;
    }

    return true;
}

bool DriverManager::disconnectRemoteHost(DriverInfo *dv)
{
    ClientManager *clientManager = dv->getClientManager();

    if (clientManager)
    {
        clientManager->removeManagedDriver(dv);
        clientManager->disconnectAll();
        GUIManager::Instance()->removeClient(clientManager);
        INDIListener::Instance()->removeClient(clientManager);
        clients.removeOne(clientManager);
        clientManager->deleteLater();
        updateMenuActions();
        return true;
    }

    return false;
}

void DriverManager::resizeDeviceColumn()
{
    ui->localTreeWidget->resizeColumnToContents(0);
}

void DriverManager::updateMenuActions()
{
    // We iterate over devices, we enable INDI Control Panel if we have any active device
    // We enable capture image sequence if we have any imaging device

    QAction *tmpAction = nullptr;
    bool activeDevice  = false;

    if (clients.size() > 0)
        activeDevice = true;

    tmpAction = KStars::Instance()->actionCollection()->action("indi_cpl");
    if (tmpAction != nullptr)
    {
        //qDebug() << "indi_cpl action set to active" << endl;
        tmpAction->setEnabled(activeDevice);
    }
}

int DriverManager::getINDIPort(int customPort)
{
#ifdef Q_OS_WIN
    qWarning() << "INDI server is currently not supported on Windows.";
    return -1;
#else
    int lastPort = Options::serverPortEnd();
    bool success = false;
    currentPort++;

    // recycle
    if (currentPort > lastPort)
        currentPort = Options::serverPortStart();

    QTcpServer temp_server;

    if (customPort != -1)
    {
        success = temp_server.listen(QHostAddress::LocalHost, customPort);
        if (success)
        {
            temp_server.close();
            return customPort;
        }
        else
            return -1;
    }

    for (; currentPort <= lastPort; currentPort++)
    {
        success = temp_server.listen(QHostAddress::LocalHost, currentPort);
        if (success)
        {
            temp_server.close();
            return currentPort;
        }
    }
    return -1;
#endif
}

bool DriverManager::readINDIHosts()
{
    QString indiFile("indihosts.xml");
    //QFile localeFile;
    QFile file;
    char errmsg[1024];
    char c;
    LilXML *xmlParser = newLilXML();
    XMLEle *root      = nullptr;
    XMLAtt *ap        = nullptr;
    QString hName, hHost, hPort;

    lastGroup = nullptr;
    file.setFileName(KSPaths::locate(QStandardPaths::AppDataLocation, indiFile));
    if (file.fileName().isEmpty() || !file.open(QIODevice::ReadOnly))
    {
        delLilXML(xmlParser);
        return false;
    }

    while (file.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            // Get host name
            ap = findXMLAtt(root, "name");
            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hName = QString(valuXMLAtt(ap));

            // Get host name
            ap = findXMLAtt(root, "hostname");

            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hHost = QString(valuXMLAtt(ap));

            ap = findXMLAtt(root, "port");

            if (!ap)
            {
                delLilXML(xmlParser);
                return false;
            }

            hPort = QString(valuXMLAtt(ap));

            DriverInfo *dv = new DriverInfo(hName);
            dv->setHostParameters(hHost, hPort);
            dv->setDriverSource(HOST_SOURCE);

            connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));

            driversList.append(dv);

            QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget, lastGroup);
            lastGroup             = item;
            item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
            item->setText(HOST_NAME_COLUMN, hName);
            item->setText(HOST_PORT_COLUMN, hPort);

            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            qDebug() << errmsg;
            delLilXML(xmlParser);
            return false;
        }
    }

    delLilXML(xmlParser);

    return true;
}

bool DriverManager::readXMLDrivers()
{
    QDir indiDir;
    QString driverName;

    // This is the XML file shipped with KStars that contains all supported INDI drivers.
    /*QString indiDriversXML = KSPaths::locate(QStandardPaths::AppDataLocation, "indidrivers.xml");
    if (indiDriversXML.isEmpty() == false)
        processXMLDriver(indiDriversXML);
    */

    processXMLDriver(QLatin1String(":/indidrivers.xml"));

    QString driversDir = Options::indiDriversDir();
#ifdef Q_OS_OSX
    if (Options::indiDriversAreInternal())
        driversDir = QCoreApplication::applicationDirPath() + "/../Resources/DriverSupport";
#endif

    if (indiDir.cd(driversDir) == false)
    {
        KSNotification::error(i18n("Unable to find INDI drivers directory: %1\nPlease make sure to set the correct "
                                   "path in KStars configuration",
                                   driversDir));
        return false;
    }

    indiDir.setNameFilters(QStringList() << "indi_*.xml" << "drivers.xml");
    indiDir.setFilter(QDir::Files | QDir::NoSymLinks);
    QFileInfoList list = indiDir.entryInfoList();

    for (auto &fileInfo : list)
    {
        // libindi 0.7.1: Skip skeleton files
        if (fileInfo.fileName().endsWith(QLatin1String("_sk.xml")))
            continue;

        //        if (fileInfo.fileName() == "drivers.xml")
        //        {
        //            // Let first attempt to load the local version of drivers.xml
        //            driverName = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("drivers.xml");

        //            // If found, we continue, otherwise, we load the system file
        //            if (driverName.isEmpty() == false && QFile(driverName).exists())
        //            {
        //                processXMLDriver(driverName);
        //                continue;
        //            }
        //        }

        processXMLDriver(fileInfo.absoluteFilePath());
    }

    return true;
}

void DriverManager::processXMLDriver(const QString &driverName)
{
    QFile file(driverName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        KSNotification::error(i18n("Failed to open INDI Driver file: %1", driverName));
        return;
    }

    char errmsg[ERRMSG_SIZE];
    char c;
    LilXML *xmlParser = newLilXML();
    XMLEle *root      = nullptr;
    XMLEle *ep        = nullptr;

    if (driverName.endsWith(QLatin1String("drivers.xml")))
        driverSource = PRIMARY_XML;
    else
        driverSource = THIRD_PARTY_XML;

    while (file.getChar(&c))
    {
        root = readXMLEle(xmlParser, c, errmsg);

        if (root)
        {
            // If the XML file is using the INDI Library v1.3+ format
            if (!strcmp(tagXMLEle(root), "driversList"))
            {
                for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
                {
                    if (!buildDeviceGroup(ep, errmsg))
                        prXMLEle(stderr, ep, 0);
                }
            }
            // If using the older format
            else
            {
                if (!buildDeviceGroup(root, errmsg))
                    prXMLEle(stderr, root, 0);
            }

            delXMLEle(root);
        }
        else if (errmsg[0])
        {
            qCDebug(KSTARS_INDI) << QString(errmsg);
            delLilXML(xmlParser);
            return;
        }
    }

    delLilXML(xmlParser);
}

bool DriverManager::buildDeviceGroup(XMLEle *root, char errmsg[])
{
    XMLAtt *ap;
    XMLEle *ep;
    QString groupName;
    QTreeWidgetItem *group;
    DeviceFamily groupType = KSTARS_TELESCOPE;

    // avoid overflow
    if (strlen(tagXMLEle(root)) > 1024)
        return false;

    // Get device grouping name
    ap = findXMLAtt(root, "group");

    if (!ap)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a group attribute", tagXMLEle(root));
        return false;
    }

    groupName = valuXMLAtt(ap);
    groupType = DeviceFamilyLabels.key(groupName);

#ifndef HAVE_CFITSIO
    // We do not create these groups if we don't have CFITSIO support
    if (groupType == KSTARS_CCD || groupType == KSTARS_VIDEO)
        return true;
#endif

    // Find if the group already exists
    QList<QTreeWidgetItem *> treeList = ui->localTreeWidget->findItems(groupName, Qt::MatchExactly);
    if (!treeList.isEmpty())
        group = treeList[0];
    else
        group = new QTreeWidgetItem(ui->localTreeWidget, lastGroup);

    group->setText(0, groupName);
    lastGroup = group;

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!buildDriverElement(ep, group, groupType, errmsg))
            return false;
    }

    return true;
}

bool DriverManager::buildDriverElement(XMLEle *root, QTreeWidgetItem *DGroup, DeviceFamily groupType, char errmsg[])
{
    XMLAtt *ap;
    XMLEle *el;
    DriverInfo *dv;
    QString label;
    QString driver;
    QString version;
    // N.B. NOT an i18n string.
    QString manufacturer("Others");
    QString name;
    QString port;
    QString skel;
    QVariantMap vMap;
    //double focal_length(-1), aperture(-1);

    ap = findXMLAtt(root, "label");
    if (!ap)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a label attribute", tagXMLEle(root));
        return false;
    }

    label = valuXMLAtt(ap);

    // Label is unique, so if we have the same label, we simply ignore
    if (findDriverByLabel(label) != nullptr)
        return true;

    ap = findXMLAtt(root, "manufacturer");
    if (ap)
        manufacturer = valuXMLAtt(ap);

    // Search for optional port attribute
    ap = findXMLAtt(root, "port");
    if (ap)
        port = valuXMLAtt(ap);

    // Search for skel file, if any
    // Search for optional port attribute
    ap = findXMLAtt(root, "skel");
    if (ap)
        skel = valuXMLAtt(ap);

    // Let's look for telescope-specific attributes: focal length and aperture
    //    ap = findXMLAtt(root, "focal_length");
    //    if (ap)
    //    {
    //        focal_length = QString(valuXMLAtt(ap)).toDouble();
    //        if (focal_length > 0)
    //            vMap.insert("TELESCOPE_FOCAL_LENGTH", focal_length);
    //    }

    // Find MDPD: Multiple Devices Per Driver
    ap = findXMLAtt(root, "mdpd");
    if (ap)
    {
        bool mdpd = false;
        mdpd      = (QString(valuXMLAtt(ap)) == QString("true")) ? true : false;
        vMap.insert("mdpd", mdpd);
    }

    //    ap = findXMLAtt(root, "aperture");
    //    if (ap)
    //    {
    //        aperture = QString(valuXMLAtt(ap)).toDouble();
    //        if (aperture > 0)
    //            vMap.insert("TELESCOPE_APERTURE", aperture);
    //    }

    el = findXMLEle(root, "driver");

    if (!el)
        return false;

    driver = pcdataXMLEle(el);

    ap = findXMLAtt(el, "name");
    if (!ap)
    {
        snprintf(errmsg, ERRMSG_SIZE, "Tag %.64s does not have a name attribute", tagXMLEle(el));
        return false;
    }

    name = valuXMLAtt(ap);

    el = findXMLEle(root, "version");

    if (!el)
        return false;

    version = pcdataXMLEle(el);
    bool versionOK = false;
    version.toDouble(&versionOK);
    if (versionOK == false)
        version = "1.0";

    bool driverIsAvailable = checkDriverAvailability(driver);

    vMap.insert("LOCALLY_AVAILABLE", driverIsAvailable);
    QIcon remoteIcon = QIcon::fromTheme("network-modem");

    QTreeWidgetItem *device = new QTreeWidgetItem(DGroup);

    device->setText(LOCAL_NAME_COLUMN, label);
    if (driverIsAvailable)
        device->setIcon(LOCAL_STATUS_COLUMN, ui->stopPix);
    else
        device->setIcon(LOCAL_STATUS_COLUMN, remoteIcon);
    device->setText(LOCAL_VERSION_COLUMN, version);
    device->setText(LOCAL_PORT_COLUMN, port);

    //if ((driverSource == PRIMARY_XML) && driversStringList.contains(driver) == false)
    if (groupType == KSTARS_TELESCOPE && driversStringList.contains(driver) == false)
        driversStringList.append(driver);

    dv = new DriverInfo(name);

    dv->setLabel(label);
    dv->setVersion(version);
    dv->setExecutable(driver);
    dv->setManufacturer(manufacturer);
    dv->setSkeletonFile(skel);
    dv->setType(groupType);
    dv->setDriverSource(driverSource);
    dv->setUserPort(port);
    dv->setAuxInfo(vMap);

    connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));

    driversList.append(dv);

    return true;
}

bool DriverManager::checkDriverAvailability(const QString &driver)
{
    QString indiServerDir = Options::indiServer();
    if (Options::indiServerIsInternal())
        indiServerDir = QCoreApplication::applicationDirPath() + "/indi";
    else
        indiServerDir = QFileInfo(Options::indiServer()).dir().path();

    QFile driverFile(indiServerDir + '/' + driver);

    if (driverFile.exists() == false)
        return (!QStandardPaths::findExecutable(indiServerDir + '/' + driver).isEmpty());

    return true;
}

void DriverManager::updateCustomDrivers()
{
    for (const QVariantMap &oneDriver : m_CustomDrivers->customDrivers())
    {
        DriverInfo *dv = new DriverInfo(oneDriver["Name"].toString());
        dv->setLabel(oneDriver["Label"].toString());
        dv->setUniqueLabel(dv->getLabel());
        dv->setExecutable(oneDriver["Exec"].toString());
        dv->setVersion(oneDriver["Version"].toString());
        dv->setManufacturer(oneDriver["Manufacturer"].toString());
        dv->setType(DeviceFamilyLabels.key(oneDriver["Family"].toString()));
        dv->setDriverSource(CUSTOM_SOURCE);

        bool driverIsAvailable = checkDriverAvailability(oneDriver["Exec"].toString());
        QVariantMap vMap;
        vMap.insert("LOCALLY_AVAILABLE", driverIsAvailable);
        dv->setAuxInfo(vMap);

        driversList.append(dv);
    }
}


// JM 2018-07-23: Disabling the old custom drivers method
#if 0
void DriverManager::updateCustomDrivers()
{
    QString label;
    QString driver;
    QString version;
    QString name;
    QTreeWidgetItem *group     = nullptr;
    QTreeWidgetItem *widgetDev = nullptr;
    QVariantMap vMap;
    DriverInfo *drv = nullptr;

    // Find if the group already exists
    QList<QTreeWidgetItem *> treeList = ui->localTreeWidget->findItems("Telescopes", Qt::MatchExactly);
    if (!treeList.isEmpty())
        group = treeList[0];
    else
        return;

    KStarsData::Instance()->logObject()->readAll();

    // Find custom telescope to ADD/UPDATE
    foreach (OAL::Scope *s, *(KStarsData::Instance()->logObject()->scopeList()))
    {
        name = label = s->name();

        if (s->driver() == i18n("None"))
            continue;

        // If driver already exists, just update values
        if ((drv = findDriverByLabel(label)))
        {
            if (s->aperture() > 0 && s->focalLength() > 0)
            {
                vMap.insert("TELESCOPE_APERTURE", s->aperture());
                vMap.insert("TELESCOPE_FOCAL_LENGTH", s->focalLength());
                drv->setAuxInfo(vMap);
            }

            drv->setExecutable(s->driver());

            continue;
        }

        driver  = s->driver();
        version = QString("1.0");

        QTreeWidgetItem *device = new QTreeWidgetItem(group);
        device->setText(LOCAL_NAME_COLUMN, QString(label));
        device->setIcon(LOCAL_STATUS_COLUMN, ui->stopPix);
        device->setText(LOCAL_VERSION_COLUMN, QString(version));

        DriverInfo *dv = new DriverInfo(name);

        dv->setLabel(label);
        dv->setExecutable(driver);
        dv->setVersion(version);
        dv->setType(KSTARS_TELESCOPE);
        dv->setDriverSource(EM_XML);

        if (s->aperture() > 0 && s->focalLength() > 0)
        {
            vMap.insert("TELESCOPE_APERTURE", s->aperture());
            vMap.insert("TELESCOPE_FOCAL_LENGTH", s->focalLength());
            dv->setAuxInfo(vMap);
        }

        connect(dv, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));
        driversList.append(dv);
    }

    // Find custom telescope to REMOVE
    foreach (DriverInfo *dev, driversList)
    {
        // If it's from primary xml file or it is in a running state, continue.
        if (dev->getDriverSource() != EM_XML || dev->getClientState())
            continue;

        if (KStarsData::Instance()->logObject()->findScopeByName(dev->getName()))
            continue;

        // Find if the group already exists
        QList<QTreeWidgetItem *> devList =
            ui->localTreeWidget->findItems(dev->getLabel(), Qt::MatchExactly | Qt::MatchRecursive);
        if (!devList.isEmpty())
        {
            widgetDev = devList[0];
            group->removeChild(widgetDev);
        }
        else
            return;

        driversList.removeOne(dev);
        delete (dev);
    }
}
#endif

void DriverManager::addINDIHost()
{
    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18nc("@title:window", "Add Host"));
    bool portOk = false;

    if (hostConfDialog.exec() == QDialog::Accepted)
    {
        DriverInfo *hostItem = new DriverInfo(hostConf.nameIN->text());

        hostConf.portnumber->text().toInt(&portOk);

        if (portOk == false)
        {
            KSNotification::error(i18n("Error: the port number is invalid."));
            delete hostItem;
            return;
        }

        hostItem->setHostParameters(hostConf.hostname->text(), hostConf.portnumber->text());

        //search for duplicates
        //for (uint i=0; i < ksw->data()->INDIHostsList.count(); i++)
        foreach (DriverInfo *host, driversList)
            if (hostItem->getName() == host->getName() && hostItem->getPort() == host->getPort())
            {
                KSNotification::error(
                    i18n("Host: %1 Port: %2 already exists.", hostItem->getName(), hostItem->getPort()));
                delete hostItem;
                return;
            }

        hostItem->setDriverSource(HOST_SOURCE);

        connect(hostItem, SIGNAL(deviceStateChanged(DriverInfo*)), this, SLOT(processDeviceStatus(DriverInfo*)));

        driversList.append(hostItem);

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->clientTreeWidget);
        item->setIcon(HOST_STATUS_COLUMN, ui->disconnected);
        item->setText(HOST_NAME_COLUMN, hostConf.nameIN->text());
        item->setText(HOST_PORT_COLUMN, hostConf.portnumber->text());
    }

    saveHosts();
}

void DriverManager::modifyINDIHost()
{
    QDialog hostConfDialog;
    Ui::INDIHostConf hostConf;
    hostConf.setupUi(&hostConfDialog);
    hostConfDialog.setWindowTitle(i18nc("@title:window", "Modify Host"));

    QTreeWidgetItem *currentItem = ui->clientTreeWidget->currentItem();

    if (currentItem == nullptr)
        return;

    foreach (DriverInfo *host, driversList)
    {
        if (currentItem->text(HOST_NAME_COLUMN) == host->getName() &&
                currentItem->text(HOST_PORT_COLUMN) == host->getPort())
        {
            hostConf.nameIN->setText(host->getName());
            hostConf.hostname->setText(host->getHost());
            hostConf.portnumber->setText(host->getPort());

            if (hostConfDialog.exec() == QDialog::Accepted)
            {
                //INDIHostsInfo *hostItem = new INDIHostsInfo;
                host->setName(hostConf.nameIN->text());
                host->setHostParameters(hostConf.hostname->text(), hostConf.portnumber->text());

                currentItem->setText(HOST_NAME_COLUMN, hostConf.nameIN->text());
                currentItem->setText(HOST_PORT_COLUMN, hostConf.portnumber->text());

                //ksw->data()->INDIHostsList.replace(i, hostItem);

                saveHosts();
                return;
            }
        }
    }
}

void DriverManager::removeINDIHost()
{
    if (ui->clientTreeWidget->currentItem() == nullptr)
        return;

    foreach (DriverInfo *host, driversList)
        if (ui->clientTreeWidget->currentItem()->text(HOST_NAME_COLUMN) == host->getName() &&
                ui->clientTreeWidget->currentItem()->text(HOST_PORT_COLUMN) == host->getPort())
        {
            if (host->getClientState())
            {
                KSNotification::error(i18n("You need to disconnect the client before removing it."));
                return;
            }

            if (KMessageBox::warningContinueCancel(nullptr,
                                                   i18n("Are you sure you want to remove the %1 client?",
                                                           ui->clientTreeWidget->currentItem()->text(HOST_NAME_COLUMN)),
                                                   i18n("Delete Confirmation"),
                                                   KStandardGuiItem::del()) != KMessageBox::Continue)
                return;

            driversList.removeOne(host);
            delete host;
            delete (ui->clientTreeWidget->currentItem());
            break;
        }

    saveHosts();
}

void DriverManager::saveHosts()
{
    QFile file;
    QString hostData;

    //determine filename in local user KDE directory tree.
    file.setFileName(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("indihosts.xml"));

    if (!file.open(QIODevice::WriteOnly))
    {
        KSNotification::sorry(
            i18n(
                "Unable to write to file 'indihosts.xml'\nAny changes to INDI hosts configurations will not be saved."),
            i18n("Could Not Open File"));
        return;
    }

    QTextStream outstream(&file);

    //for (uint i= 0; i < ksw->data()->INDIHostsList.count(); i++)
    foreach (DriverInfo *host, driversList)
    {
        if (host->getDriverSource() != HOST_SOURCE)
            continue;

        hostData = "<INDIHost name='";
        hostData += host->getName();
        hostData += "' hostname='";
        hostData += host->getHost();
        hostData += "' port='";
        hostData += host->getPort();
        hostData += "' />\n";

        outstream << hostData;
    }

    file.close();
}

DriverInfo *DriverManager::findDriverByName(const QString &name)
{
    for (auto &dv : driversList)
    {
        if (dv->getName() == name)
            return dv;
    }

    return nullptr;
}

DriverInfo *DriverManager::findDriverByLabel(const QString &label)
{
    for (auto &dv : driversList)
    {
        if (dv->getLabel() == label)
            return dv;
    }

    return nullptr;
}

DriverInfo *DriverManager::findDriverByExec(const QString &exec)
{
    for (auto &dv : driversList)
    {
        if (dv->getExecutable() == exec)
            return dv;
    }

    return nullptr;
}

QString DriverManager::getUniqueDeviceLabel(const QString &label)
{
    int nset            = 0;
    QString uniqueLabel = label;

    for (auto &cm : clients)
    {
        auto &devices = cm->getDevices();

        for (auto &dv : devices)
        {
            if (label == QString(dv->getDeviceName()))
                nset++;
        }
    }
    if (nset > 0)
        uniqueLabel = QString("%1 %2").arg(label).arg(nset + 1);

    return uniqueLabel;
}

QJsonArray DriverManager::getDriverList() const
{
    QJsonArray driverArray;

    for (const auto &drv : driversList)
        driverArray.append(drv->toJson());

    return driverArray;
}

bool DriverManager::restartDriver(DriverInfo *dv)
{
    for (auto &oneServer : servers)
    {
        if (oneServer->contains(dv))
        {
            return oneServer->restartDriver(dv);
        }
    }

    return false;
}
