/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "profileeditor.h"

#include "geolocation.h"
#include "kstarsdata.h"
#include "ksnotification.h"
#include "Options.h"
#include "guide/guide.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "profilescriptdialog.h"

#include "ekos_debug.h"

#include <QNetworkInterface>
#include <QListView>
#include <QDesktopServices>
#include <QStandardItemModel>
#include <QStringListModel>

ProfileEditorUI::ProfileEditorUI(QWidget *p) : QFrame(p)
{
    setupUi(this);
}

ProfileEditor::ProfileEditor(QWidget *w) : QDialog(w)
{
    setObjectName("profileEditorDialog");
#ifdef Q_OS_MACOS
    setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
#endif
    ui = new ProfileEditorUI(this);

    pi = nullptr;

    driversModel = new QStandardItemModel(this);
    profileDriversModel = new QStandardItemModel(this);

    ui->driversTree->setModel(driversModel);
    ui->profileDriversList->setModel(profileDriversModel);

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    setWindowTitle(i18nc("@title:window", "Profile Editor"));

    // Create button box and link it to save and reject functions
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Close, this);

    buttonBox->setObjectName("dialogButtons");
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(saveProfile()));

    connect(ui->scriptsB, &QPushButton::clicked, this, &ProfileEditor::executeScriptEditor);

    connect(ui->openWebManagerB, &QPushButton::clicked, this, [this]()
    {
        QUrl url(QString("http://" + ui->remoteHost->text() + ":8624"));
        QDesktopServices::openUrl(url);
    });

    connect(ui->INDIWebManagerCheck, &QCheckBox::toggled, this, [&](bool enabled)
    {
        ui->openWebManagerB->setEnabled(enabled);
        ui->remoteDrivers->setEnabled(enabled || ui->localMode->isChecked());
        ui->scriptsB->setEnabled(enabled || ui->localMode->isChecked());
    });

    connect(ui->guideTypeCombo, SIGNAL(activated(int)), this, SLOT(updateGuiderSelection(int)));

    connect(ui->scanB, &QPushButton::clicked, this, &ProfileEditor::scanNetwork);

    connect(ui->addDriverB, &QPushButton::clicked, this, QOverload<>::of(&ProfileEditor::addDriver));
    connect(ui->removeDriverB, &QPushButton::clicked, this, &ProfileEditor::removeDriver);
    connect(ui->driversTree, &QTreeView::doubleClicked, this, QOverload<const QModelIndex &>::of(&ProfileEditor::addDriver));
    connect(ui->driverSearchEdit, &QLineEdit::textChanged, this, &ProfileEditor::filterDrivers);
    connect(ui->profileSearchEdit, &QLineEdit::textChanged, this, &ProfileEditor::filterProfileDrivers);
    connect(profileDriversModel, &QStandardItemModel::rowsInserted, this, &ProfileEditor::updateDriverCount);
    connect(profileDriversModel, &QStandardItemModel::rowsRemoved, this, &ProfileEditor::updateDriverCount);

#ifdef Q_OS_WIN
    ui->remoteMode->setChecked(true);
    ui->localMode->setEnabled(false);
    setRemoteMode(true);
#else
    connect(ui->remoteMode, SIGNAL(toggled(bool)), this, SLOT(setRemoteMode(bool)));
#endif

    // Load all drivers
    loadDrivers();

    // Shared tooltips
    ui->remoteDrivers->setToolTip(ui->remoteDriversLabel->toolTip());
    ui->localMode->setToolTip(ui->modeLabel->toolTip());
    ui->remoteMode->setToolTip(ui->modeLabel->toolTip());
    ui->remoteHostLabel->setToolTip(ui->remoteHost->toolTip());
    ui->remotePortLabel->setToolTip(ui->remotePort->toolTip());
    ui->externalGuidePortLabel->setToolTip(ui->externalGuidePort->toolTip());
    ui->INDIWebManagerPortLabel->setToolTip(ui->INDIWebManagerPort->toolTip());
    ui->guideTypeCombo->setToolTip(ui->guidingTypeLabel->toolTip());
    ui->externalGuideHostLabel->setToolTip(ui->externalGuideHost->toolTip());
}

void ProfileEditor::saveProfile()
{
    bool newProfile = (pi.isNull());

    if (ui->profileIN->text().isEmpty())
    {
        KSNotification::error(i18n("Cannot save an empty profile."));
        return;
    }

    if (newProfile)
    {
        QList<QSharedPointer<ProfileInfo >> existingProfiles;
        KStarsData::Instance()->userdb()->GetAllProfiles(existingProfiles);
        for (auto &profileInfo : existingProfiles)
        {
            if (ui->profileIN->text() == profileInfo->name)
            {
                KSNotification::error(i18n("Profile name already exists."));
                return;
            }
        }
        int id = KStarsData::Instance()->userdb()->AddProfile(ui->profileIN->text());
        pi.reset(new ProfileInfo(id, ui->profileIN->text()));
    }
    else
        pi->name = ui->profileIN->text();

    // Local Mode
    if (ui->localMode->isChecked())
    {
        pi->host.clear();
        pi->port               = -1;
        pi->INDIWebManagerPort = -1;
        //pi->customDrivers = ui->customDriversIN->text();
    }
    // Remote Mode
    else
    {
        pi->host = ui->remoteHost->text().trimmed();
        pi->port = ui->remotePort->text().toInt();
        if (ui->INDIWebManagerCheck->isChecked())
            pi->INDIWebManagerPort = ui->INDIWebManagerPort->text().toInt();
        else
            pi->INDIWebManagerPort = -1;
        //pi->customDrivers.clear();
    }

    // City Info
    if (ui->loadSiteCheck->isEnabled() && ui->loadSiteCheck->isChecked())
    {
        pi->city     = KStarsData::Instance()->geo()->name();
        pi->province = KStarsData::Instance()->geo()->province();
        pi->country  = KStarsData::Instance()->geo()->country();
    }
    else
    {
        pi->city.clear();
        pi->province.clear();
        pi->country.clear();
    }

    // Auto Connect
    pi->autoConnect = ui->autoConnectCheck->isChecked();
    // Port Selector
    pi->portSelector = ui->portSelectorCheck->isChecked();

    // Guider Type
    pi->guidertype = ui->guideTypeCombo->currentIndex();
    if (pi->guidertype != Ekos::Guide::GUIDE_INTERNAL)
    {
        pi->guiderhost = ui->externalGuideHost->text();
        pi->guiderport = ui->externalGuidePort->text().toInt();

        if (pi->guidertype == Ekos::Guide::GUIDE_PHD2)
        {
            Options::setPHD2Host(pi->guiderhost);
            Options::setPHD2Port(pi->guiderport);
        }
        else if (pi->guidertype == Ekos::Guide::GUIDE_LINGUIDER)
        {
            Options::setLinGuiderHost(pi->guiderhost);
            Options::setLinGuiderPort(pi->guiderport);
        }
    }

    pi->drivers.clear();

    for (int i = 0; i < profileDriversModel->rowCount(); ++i)
    {
        QStandardItem *item = profileDriversModel->item(i);
        QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByLabel(item->text());
        if (driverInfo)
            pi->addDriver(driverInfo->getType(), driverInfo->getLabel());
    }

    pi->remotedrivers = ui->remoteDrivers->text();

    KStarsData::Instance()->userdb()->SaveProfile(pi);

    // Ekos manager will reload and new profiles will be created
    if (newProfile)
        pi.clear();

    accept();
}

void ProfileEditor::setRemoteMode(bool enable)
{
    //This is needed to reload the drivers because some may not be available locally
    loadDrivers();

    ui->remoteHost->setEnabled(enable);
    ui->remoteHostLabel->setEnabled(enable);
    ui->remotePort->setEnabled(enable);
    ui->remotePortLabel->setEnabled(enable);

    //ui->customLabel->setEnabled(!enable);
    //ui->customDriversIN->setEnabled(!enable);

    ui->remoteDrivers->setEnabled(!enable);

    ui->loadSiteCheck->setEnabled(enable);

    ui->INDIWebManagerCheck->setEnabled(enable);
    if (enable == false)
        ui->INDIWebManagerCheck->setChecked(false);
    ui->INDIWebManagerPort->setEnabled(enable);

    ui->scriptsB->setEnabled(!enable || ui->INDIWebManagerCheck->isChecked());
}

void ProfileEditor::setPi(const QSharedPointer<ProfileInfo> &newProfile)
{
    pi = newProfile;

    ui->profileIN->setText(pi->name);

    ui->loadSiteCheck->setChecked(!pi->city.isEmpty());
    ui->autoConnectCheck->setChecked(pi->autoConnect);
    ui->portSelectorCheck->setChecked(pi->portSelector);

    if (pi->city.isEmpty() == false)
    {
        if (pi->province.isEmpty())
            ui->loadSiteCheck->setText(ui->loadSiteCheck->text() + QString(" (%1, %2)").arg(pi->country, pi->city));
        else
            ui->loadSiteCheck->setText(ui->loadSiteCheck->text() +
                                       QString(" (%1, %2, %3)").arg(pi->country, pi->province, pi->city));
    }

    if (pi->host.isEmpty() == false)
    {
        ui->remoteHost->setText(pi->host);
        ui->remotePort->setText(QString::number(pi->port));

        ui->remoteMode->setChecked(true);

        if (pi->INDIWebManagerPort > 0)
        {
            ui->INDIWebManagerCheck->setChecked(true);
            ui->INDIWebManagerPort->setText(QString::number(pi->INDIWebManagerPort));
        }
        else
        {
            ui->INDIWebManagerCheck->setChecked(false);
            ui->INDIWebManagerPort->setText("8624");
        }
    }

    if (pi->remotedrivers.isEmpty() == false)
        ui->remoteDrivers->setText(pi->remotedrivers);

    ui->guideTypeCombo->setCurrentIndex(pi->guidertype);
    updateGuiderSelection(ui->guideTypeCombo->currentIndex());
    if (pi->guidertype == Ekos::Guide::GUIDE_PHD2)
    {
        Options::setPHD2Host(pi->guiderhost);
        Options::setPHD2Port(pi->guiderport);
    }
    else if (pi->guidertype == Ekos::Guide::GUIDE_LINGUIDER)
    {
        Options::setLinGuiderHost(pi->guiderhost);
        Options::setLinGuiderPort(pi->guiderport);
    }

    QStringList profileDriverLabels;
    QMapIterator<DeviceFamily, QList<QString >> i(pi->drivers);
    while (i.hasNext())
    {
        i.next();
        for (const QString &driverName : i.value())
        {
            QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByLabel(driverName);
            if (driverInfo)
                profileDriverLabels.append(driverInfo->getLabel());
        }
    }

    profileDriversModel->clear();
    for (const QString &label : profileDriverLabels)
    {
        QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByLabel(label);
        if (driverInfo)
        {
            QStandardItem *item = new QStandardItem(getIconForFamily(driverInfo->getType()), label);
            item->setEditable(false);
            profileDriversModel->appendRow(item);
        }
    }
    profileDriversModel->sort(0);
    updateDriverCount();
}

QString ProfileEditor::getTooltip(const QSharedPointer<DriverInfo> &driver)
{
    bool locallyAvailable = false;
    if (driver->getAuxInfo().contains("LOCALLY_AVAILABLE"))
        locallyAvailable = driver->getAuxInfo().value("LOCALLY_AVAILABLE", false).toBool();
    QString toolTipText;
    if (!locallyAvailable)
        toolTipText = i18n(
                          "<nobr>Available as <b>Remote</b> Driver. To use locally, install the corresponding driver.<nobr/>");
    else
        toolTipText = i18n("<nobr><b>Label</b>: %1 &#9473; <b>Driver</b>: %2 &#9473; <b>Exec</b>: %3<nobr/>",
                           driver->getLabel(), driver->getName(), driver->getExecutable());

    return toolTipText;
}

void ProfileEditor::loadDrivers()
{
    driversModel->clear();
    driversModel->setHorizontalHeaderLabels({i18n("Drivers")});

    QMap<DeviceFamily, QMap<QString, QList<QSharedPointer<DriverInfo >>> > categorizedDrivers;

    for (QSharedPointer<DriverInfo> driver : DriverManager::Instance()->getDrivers())
    {
        categorizedDrivers[driver->getType()][driver->manufacturer()] << driver;
    }

    for (auto it = categorizedDrivers.constBegin(); it != categorizedDrivers.constEnd(); ++it)
    {
        QStandardItem *familyItem = new QStandardItem(fromDeviceFamily(it.key()));
        familyItem->setEditable(false);
        driversModel->appendRow(familyItem);

        for (auto it2 = it.value().constBegin(); it2 != it.value().constEnd(); ++it2)
        {
            QStandardItem *manufacturerItem = new QStandardItem(it2.key());
            manufacturerItem->setEditable(false);
            familyItem->appendRow(manufacturerItem);

            for (const QSharedPointer<DriverInfo> &driver : it2.value())
            {
                if (driver->getLabel().isEmpty())
                    continue;
                QStandardItem *driverItem = new QStandardItem(driver->getLabel());
                driverItem->setToolTip(getTooltip(driver));
                driverItem->setEditable(false);
                manufacturerItem->appendRow(driverItem);
            }
        }
    }
}

void ProfileEditor::setProfileName(const QString &name)
{
    ui->profileIN->setText(name);
}

void ProfileEditor::setAuxDrivers(const QStringList &aux)
{
    for (const QString &driverLabel : aux)
    {
        QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByLabel(driverLabel);
        if (driverInfo)
        {
            QStandardItem *item = new QStandardItem(getIconForFamily(driverInfo->getType()), driverInfo->getLabel());
            item->setEditable(false);
            profileDriversModel->appendRow(item);
        }
    }
    profileDriversModel->sort(0);
    updateDriverCount();
}

void ProfileEditor::setHostPort(const QString &host, const QString &port)
{
    ui->remoteMode->setChecked(true);
    ui->remoteHost->setText(host);
    ui->remotePort->setText(port);
}

void ProfileEditor::setWebManager(bool enabled, const QString &port)
{
    ui->INDIWebManagerCheck->setChecked(enabled);
    ui->INDIWebManagerPort->setText(port);
}

void ProfileEditor::setGuiderType(int type)
{
    ui->guideTypeCombo->setCurrentIndex(type);
    if (type != Ekos::Guide::GUIDE_INTERNAL)
    {
        ui->externalGuideHostLabel->setEnabled(true);
        ui->externalGuideHost->setEnabled(true);
        ui->externalGuidePortLabel->setEnabled(true);
        ui->externalGuidePort->setEnabled(true);
    }
}

void ProfileEditor::setConnectionOptionsEnabled(bool enable)
{
    // Enable or disable connection related options
    ui->modeLabel->setEnabled(enable);
    ui->localMode->setEnabled(enable);
    ui->remoteMode->setEnabled(enable);
    ui->remoteHostLabel->setEnabled(enable);
    ui->remoteHost->setEnabled(enable);
    ui->remotePortLabel->setEnabled(enable);
    ui->remotePort->setEnabled(enable);
    ui->INDIWebManagerCheck->setEnabled(enable);
    ui->INDIWebManagerPort->setEnabled(enable);
    ui->INDIWebManagerPortLabel->setEnabled(enable);
    ui->guidingTypeLabel->setEnabled(enable);
    ui->guideTypeCombo->setEnabled(enable);
    ui->remoteDrivers->setEnabled(enable);

    updateGuiderSelection(ui->guideTypeCombo->currentIndex());

    if (enable == false)
        ui->driversTree->setFocus();
}

void ProfileEditor::updateGuiderSelection(int id)
{

    if (id == Ekos::Guide::GUIDE_INTERNAL)
    {
        ui->externalGuideHost->setText("localhost");
        ui->externalGuidePort->clear();

        ui->externalGuideHost->setEnabled(false);
        ui->externalGuideHostLabel->setEnabled(false);
        ui->externalGuidePort->setEnabled(false);
        ui->externalGuidePortLabel->setEnabled(false);
        return;
    }

    QString host;
    int port = -1;

    ui->externalGuideHost->setEnabled(true);
    ui->externalGuideHostLabel->setEnabled(true);
    ui->externalGuidePort->setEnabled(true);
    ui->externalGuidePortLabel->setEnabled(true);

    if (pi && pi->guidertype == id)
    {
        host = pi->guiderhost;
        port = pi->guiderport;
    }

    if (id == Ekos::Guide::GUIDE_PHD2)
    {
        if (host.isEmpty())
            host = Options::pHD2Host();
        if (port < 0)
            port = Options::pHD2Port();
    }
    else if (id == Ekos::Guide::GUIDE_LINGUIDER)
    {
        if (host.isEmpty())
            host = Options::linGuiderHost();
        if (port < 0)
            port = Options::linGuiderPort();
    }

    ui->externalGuideHost->setText(host);
    ui->externalGuidePort->setText(QString::number(port));

}

void ProfileEditor::setSettings(const QJsonObject &profile)
{
    ui->profileIN->setText(profile["name"].toString());
    ui->autoConnectCheck->setChecked(profile["auto_connect"].toBool(true));
    ui->portSelectorCheck->setChecked(profile["port_selector"].toBool(false));
    ui->localMode->setChecked(profile["mode"].toString() == "local");
    ui->remoteMode->setChecked(profile["mode"].toString() == "remote");
    ui->remoteHost->setText(profile["remote_host"].toString("localhost"));
    ui->remotePort->setText(profile["remote_port"].toString("7624"));
    ui->guideTypeCombo->setCurrentText(profile["guiding"].toString(i18n("Internal")));
    ui->externalGuideHost->setText(profile["remote_guiding_host"].toString(("localhost")));
    ui->externalGuidePort->setText(profile["remote_guiding_port"].toString("4400"));
    ui->INDIWebManagerCheck->setChecked(profile["use_web_manager"].toBool());
    ui->remoteDrivers->setText(profile["remote"].toString(ui->remoteDrivers->text()));

    QStringList profileDrivers;
    for (const auto &key : profile.keys())
    {
        if (key != "name" && key != "auto_connect" && key != "port_selector" && key != "mode" &&
                key != "remote_host" && key != "remote_port" && key != "guiding" && key != "remote_guiding_host" &&
                key != "remote_guiding_port" && key != "use_web_manager" && key != "remote")
        {
            QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByName(profile[key].toString());
            if (driverInfo)
            {
                QStandardItem *item = new QStandardItem(getIconForFamily(driverInfo->getType()), driverInfo->getLabel());
                item->setEditable(false);
                profileDriversModel->appendRow(item);
            }
        }
    }
    profileDriversModel->sort(0);
    updateDriverCount();
}

void ProfileEditor::removeDriver()
{
    QModelIndexList indexes = ui->profileDriversList->selectionModel()->selectedIndexes();
    for (const QModelIndex &index : indexes)
    {
        profileDriversModel->removeRow(index.row());
    }
    profileDriversModel->sort(0);
    updateDriverCount();
}

void ProfileEditor::addDriver(const QModelIndex &index)
{
    if (!index.isValid() || !index.parent().isValid() || !index.parent().parent().isValid())
        return;

    QString driverLabel = driversModel->data(index).toString();
    for (int i = 0; i < profileDriversModel->rowCount(); ++i)
    {
        if (profileDriversModel->item(i)->text() == driverLabel)
            return;
    }

    QSharedPointer<DriverInfo> driverInfo = DriverManager::Instance()->findDriverByLabel(driverLabel);
    if (driverInfo)
    {
        QStandardItem *item = new QStandardItem(getIconForFamily(driverInfo->getType()), driverLabel);
        item->setEditable(false);
        profileDriversModel->appendRow(item);
        profileDriversModel->sort(0);
    }
}

void ProfileEditor::addDriver()
{
    addDriver(ui->driversTree->currentIndex());
}

void ProfileEditor::filterDrivers(const QString &text)
{
    if (text.length() < 2)
    {
        for (int i = 0; i < driversModel->rowCount(); ++i)
        {
            QModelIndex familyIndex = driversModel->index(i, 0);
            ui->driversTree->setRowHidden(i, QModelIndex(), false);
            QStandardItem *familyItem = driversModel->itemFromIndex(familyIndex);

            for (int j = 0; j < familyItem->rowCount(); ++j)
            {
                QModelIndex manufacturerIndex = familyItem->child(j)->index();
                ui->driversTree->setRowHidden(j, familyIndex, false);
                QStandardItem *manufacturerItem = familyItem->child(j);

                for (int k = 0; k < manufacturerItem->rowCount(); ++k)
                {
                    ui->driversTree->setRowHidden(k, manufacturerIndex, false);
                }
            }
            ui->driversTree->collapse(familyIndex);
        }
        return;
    }

    QRegularExpression regex(text, QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < driversModel->rowCount(); ++i)
    {
        QStandardItem *familyItem = driversModel->item(i);
        bool familyHasVisibleChildren = false;
        for (int j = 0; j < familyItem->rowCount(); ++j)
        {
            QStandardItem *manufacturerItem = familyItem->child(j);
            bool manufacturerHasVisibleChildren = false;
            for (int k = 0; k < manufacturerItem->rowCount(); ++k)
            {
                QStandardItem *driverItem = manufacturerItem->child(k);
                bool match = driverItem->text().contains(regex);
                ui->driversTree->setRowHidden(k, manufacturerItem->index(), !match);
                if (match)
                {
                    manufacturerHasVisibleChildren = true;
                }
            }

            ui->driversTree->setRowHidden(j, familyItem->index(), !manufacturerHasVisibleChildren);
            if (manufacturerHasVisibleChildren)
            {
                familyHasVisibleChildren = true;
                ui->driversTree->expand(manufacturerItem->index());
            }
        }
        ui->driversTree->setRowHidden(i, QModelIndex(), !familyHasVisibleChildren);
        if (familyHasVisibleChildren)
        {
            ui->driversTree->expand(familyItem->index());
        }
    }
}

void ProfileEditor::filterProfileDrivers(const QString &text)
{
    QRegularExpression regex(text, QRegularExpression::CaseInsensitiveOption);
    for (int i = 0; i < profileDriversModel->rowCount(); ++i)
    {
        QStandardItem *item = profileDriversModel->item(i);
        bool match = item->text().contains(regex);
        ui->profileDriversList->setRowHidden(i, !match);
    }
}

void ProfileEditor::updateDriverCount()
{
    ui->driverCount->setText(QString::number(profileDriversModel->rowCount()));
}

QIcon ProfileEditor::getIconForFamily(DeviceFamily family)
{
    switch (family)
    {
        case KSTARS_TELESCOPE:
            return QIcon(":/categories/telescope.svg");
        case KSTARS_CCD:
            return QIcon(":/categories/camera.svg");
        case KSTARS_FOCUSER:
            return QIcon(":/categories/focuser.svg");
        case KSTARS_FILTER:
            return QIcon(":/categories/filterwheel.svg");
        case KSTARS_ADAPTIVE_OPTICS:
            return QIcon(":/categories/adaptiveoptics.svg");
        case KSTARS_DOME:
            return QIcon(":/categories/dome.svg");
        case KSTARS_WEATHER:
            return QIcon(":/categories/weather.svg");
        case KSTARS_ROTATOR:
            return QIcon(":/categories/rotator.svg");
        case KSTARS_POWER:
            return QIcon(":/categories/power.svg");
        case KSTARS_SPECTROGRAPHS:
            return QIcon(":/categories/spectrograph.svg");
        case KSTARS_AGENT:
            return QIcon(":/categories/agent.svg");
        case KSTARS_AUXILIARY:
            return QIcon(":/categories/auxiliary.svg");
        default:
            return QIcon();
    }
}

void ProfileEditor::scanNetwork()
{
    delete (m_ProgressDialog);
    m_ProgressDialog = new QProgressDialog(this);
    m_ProgressDialog->setWindowTitle(i18nc("@title:window", "Scanning Network"));
    m_ProgressDialog->setLabelText(i18n("Scanning network for INDI Web Managers..."));
    connect(m_ProgressDialog, &QProgressDialog::canceled, this, [this]()
    {
        m_CancelScan = true;
        clearAllRequests();
    });
    m_ProgressDialog->setMinimum(0);
    m_ProgressDialog->setMaximum(0);
    m_ProgressDialog->show();
    m_ProgressDialog->raise();

    m_CancelScan = false;

    QList<QHostAddress> addresses = QNetworkInterface::allAddresses();
    std::sort(addresses.begin(), addresses.end(), [](const QHostAddress & a, const QHostAddress & b) -> bool
    { return a.toString() < b.toString();});

    for(QHostAddress address : addresses)
    {
        if (address.isLoopback() || address.protocol() & QAbstractSocket::IPv6Protocol)
            continue;

        QString ipv4 = address.toString();

        if (ipv4.startsWith("10.250"))
        {
            scanIP("10.250.250.1");
        }
        else
        {
            QString prefixIP = ipv4.remove(ipv4.lastIndexOf("."), 10);
            // Blind search all over subnet
            // TODO better subnet detection instead of assuming it finishes at 254
            for (int i = 1; i <= 254; i++)
            {
                scanIP(prefixIP + "." + QString::number(i));
            }
        }
    }

}

void ProfileEditor::scanIP(const QString &ip)
{
    QUrl url(QString("http://%1:8624/api/server/status").arg(ip));

    qCDebug(KSTARS_EKOS) << "Scanning" << url;

    QNetworkReply *response = m_Manager.get(QNetworkRequest(url));
    m_Replies.append(response);
    connect(response, &QNetworkReply::finished, [this, response, ip]()
    {
        m_Replies.removeOne(response);
        response->deleteLater();
        if (m_CancelScan)
            return;
        if (response->error() == QNetworkReply::NoError)
        {
            clearAllRequests();
            m_ProgressDialog->close();
            ui->remoteHost->setText(ip);

            qCDebug(KSTARS_EKOS) << "Found Web Manager server at" << ip;

            KSNotification::info(i18n("Found INDI Web Manager at %1", ip));
        }
    });
}

void ProfileEditor::clearAllRequests()
{
    for (QNetworkReply *oneReply : m_Replies)
    {
        oneReply->abort();
        oneReply->deleteLater();
    }

    m_Replies.clear();
}

void ProfileEditor::executeScriptEditor()
{
    if (pi == nullptr)
        return;
    QStringList currentDrivers;
    for (int i = 0; i < profileDriversModel->rowCount(); ++i)
    {
        currentDrivers << profileDriversModel->item(i)->text();
    }
    for (auto &oneDriver : ui->remoteDrivers->text().split(","))
        currentDrivers << oneDriver;
    currentDrivers.removeAll("--");
    currentDrivers.removeAll("");
    currentDrivers.sort();
    ProfileScriptDialog dialog(currentDrivers, pi->scripts, this);
    dialog.exec();
    auto settings = dialog.jsonSettings();
    pi->scripts = QJsonDocument(settings).toJson(QJsonDocument::Compact);
}
