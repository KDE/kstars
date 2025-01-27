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
#include "ui_indihub.h"

#include "ekos_debug.h"

#include <QNetworkInterface>
#include <QListView>
#include <QDesktopServices>

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

    // Initialize device map
    m_Devices =
    {
        {"Mount", {ui->mountCombo, new QStandardItemModel(this), QString(), {KSTARS_TELESCOPE}}},
        {"CCD", {ui->ccdCombo, new QStandardItemModel(this), QString(), {KSTARS_CCD}}},
        {"Guider", {ui->guiderCombo, new QStandardItemModel(this), QString(), {KSTARS_CCD}}},
        {"Focuser", {ui->focuserCombo, new QStandardItemModel(this), QString(), {KSTARS_FOCUSER}}},
        {"Filter", {ui->filterCombo, new QStandardItemModel(this), QString(), {KSTARS_FILTER}}},
        {"AO", {ui->AOCombo, new QStandardItemModel(this), QString(), {KSTARS_ADAPTIVE_OPTICS}}},
        {"Dome", {ui->domeCombo, new QStandardItemModel(this), QString(), {KSTARS_DOME}}},
        {"Weather", {ui->weatherCombo, new QStandardItemModel(this), QString(), {KSTARS_WEATHER}}},
        {
            "Aux1", {
                ui->aux1Combo, new QStandardItemModel(this), QString(),
                {KSTARS_AUXILIARY, KSTARS_CCD, KSTARS_FOCUSER, KSTARS_FILTER, KSTARS_WEATHER, KSTARS_SPECTROGRAPHS, KSTARS_DETECTORS}
            }
        },
        {
            "Aux2", {
                ui->aux2Combo, new QStandardItemModel(this), QString(),
                {KSTARS_AUXILIARY, KSTARS_CCD, KSTARS_FOCUSER, KSTARS_FILTER, KSTARS_WEATHER, KSTARS_SPECTROGRAPHS, KSTARS_DETECTORS}
            }
        },
        {
            "Aux3", {
                ui->aux3Combo, new QStandardItemModel(this), QString(),
                {KSTARS_AUXILIARY, KSTARS_CCD, KSTARS_FOCUSER, KSTARS_FILTER, KSTARS_WEATHER, KSTARS_SPECTROGRAPHS, KSTARS_DETECTORS}
            }
        },
        {
            "Aux4", {
                ui->aux4Combo, new QStandardItemModel(this), QString(),
                {KSTARS_AUXILIARY, KSTARS_CCD, KSTARS_FOCUSER, KSTARS_FILTER, KSTARS_WEATHER, KSTARS_SPECTROGRAPHS, KSTARS_DETECTORS}
            }
        }
    };

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

#ifdef Q_OS_WIN
    ui->remoteMode->setChecked(true);
    ui->localMode->setEnabled(false);
    setRemoteMode(true);
#else
    connect(ui->remoteMode, SIGNAL(toggled(bool)), this, SLOT(setRemoteMode(bool)));
#endif

    connect(ui->indihubB, &QPushButton::clicked, this, &ProfileEditor::showINDIHub);

    // Load all drivers
    loadDrivers();

    // Shared tooltips
    ui->remoteDrivers->setToolTip(ui->remoteDriversLabel->toolTip());
    ui->aux1Combo->setToolTip(ui->aux1Label->toolTip());
    ui->aux2Combo->setToolTip(ui->aux2Label->toolTip());
    ui->aux3Combo->setToolTip(ui->aux3Label->toolTip());
    ui->aux4Combo->setToolTip(ui->aux4Label->toolTip());
    ui->filterCombo->setToolTip(ui->filterLabel->toolTip());
    ui->AOCombo->setToolTip(ui->AOLabel->toolTip());
    ui->domeCombo->setToolTip(ui->domeLabel->toolTip());
    ui->weatherCombo->setToolTip(ui->weatherLabel->toolTip());
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
        QList<QSharedPointer<ProfileInfo>> existingProfiles;
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

    pi->indihub = m_INDIHub;

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

    if (ui->mountCombo->currentText().isEmpty() || ui->mountCombo->currentText() == "--")
        pi->drivers.remove("Mount");
    else
        pi->drivers["Mount"] = ui->mountCombo->currentText();

    if (ui->ccdCombo->currentText().isEmpty() || ui->ccdCombo->currentText() == "--")
        pi->drivers.remove("CCD");
    else
        pi->drivers["CCD"] = ui->ccdCombo->currentText();

    if (ui->guiderCombo->currentText().isEmpty() || ui->guiderCombo->currentText() == "--")
        pi->drivers.remove("Guider");
    else
        pi->drivers["Guider"] = ui->guiderCombo->currentText();

    if (ui->focuserCombo->currentText().isEmpty() || ui->focuserCombo->currentText() == "--")
        pi->drivers.remove("Focuser");
    else
        pi->drivers["Focuser"] = ui->focuserCombo->currentText();

    if (ui->filterCombo->currentText().isEmpty() || ui->filterCombo->currentText() == "--")
        pi->drivers.remove("Filter");
    else
        pi->drivers["Filter"] = ui->filterCombo->currentText();

    if (ui->AOCombo->currentText().isEmpty() || ui->AOCombo->currentText() == "--")
        pi->drivers.remove("AO");
    else
        pi->drivers["AO"] = ui->AOCombo->currentText();

    if (ui->domeCombo->currentText().isEmpty() || ui->domeCombo->currentText() == "--")
        pi->drivers.remove("Dome");
    else
        pi->drivers["Dome"] = ui->domeCombo->currentText();

    if (ui->weatherCombo->currentText().isEmpty() || ui->weatherCombo->currentText() == "--")
        pi->drivers.remove("Weather");
    else
        pi->drivers["Weather"] = ui->weatherCombo->currentText();

    if (ui->aux1Combo->currentText().isEmpty() || ui->aux1Combo->currentText() == "--")
        pi->drivers.remove("Aux1");
    else
        pi->drivers["Aux1"] = ui->aux1Combo->currentText();

    if (ui->aux2Combo->currentText().isEmpty() || ui->aux2Combo->currentText() == "--")
        pi->drivers.remove("Aux2");
    else
        pi->drivers["Aux2"] = ui->aux2Combo->currentText();

    if (ui->aux3Combo->currentText().isEmpty() || ui->aux3Combo->currentText() == "--")
        pi->drivers.remove("Aux3");
    else
        pi->drivers["Aux3"] = ui->aux3Combo->currentText();

    if (ui->aux4Combo->currentText().isEmpty() || ui->aux4Combo->currentText() == "--")
        pi->drivers.remove("Aux4");
    else
        pi->drivers["Aux4"] = ui->aux4Combo->currentText();

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

    ui->mountCombo->setEditable(enable);
    ui->ccdCombo->setEditable(enable);
    ui->guiderCombo->setEditable(enable);
    ui->focuserCombo->setEditable(enable);
    ui->filterCombo->setEditable(enable);
    ui->AOCombo->setEditable(enable);
    ui->domeCombo->setEditable(enable);
    ui->weatherCombo->setEditable(enable);
    ui->aux1Combo->setEditable(enable);
    ui->aux2Combo->setEditable(enable);
    ui->aux3Combo->setEditable(enable);
    ui->aux4Combo->setEditable(enable);

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

    QMapIterator<QString, QString> i(pi->drivers);

    while (i.hasNext())
    {
        int row = 0;
        i.next();

        QString key   = i.key();
        QString value = i.value();

        if (key == "Mount")
        {
            // If driver doesn't exist, let's add it to the list
            if ((row = ui->mountCombo->findText(value)) == -1)
            {
                ui->mountCombo->addItem(value);
                row = ui->mountCombo->count() - 1;
            }

            // Set index to our driver
            ui->mountCombo->setCurrentIndex(row);
        }
        else if (key == "CCD")
        {
            if ((row = ui->ccdCombo->findText(value)) == -1)
            {
                ui->ccdCombo->addItem(value);
                row = ui->ccdCombo->count() - 1;
            }

            ui->ccdCombo->setCurrentIndex(row);
        }
        else if (key == "Guider")
        {
            if ((row = ui->guiderCombo->findText(value)) == -1)
            {
                ui->guiderCombo->addItem(value);
                row = ui->guiderCombo->count() - 1;
            }

            ui->guiderCombo->setCurrentIndex(row);
        }
        else if (key == "Focuser")
        {
            if ((row = ui->focuserCombo->findText(value)) == -1)
            {
                ui->focuserCombo->addItem(value);
                row = ui->focuserCombo->count() - 1;
            }

            ui->focuserCombo->setCurrentIndex(row);
        }
        else if (key == "Filter")
        {
            if ((row = ui->filterCombo->findText(value)) == -1)
            {
                ui->filterCombo->addItem(value);
                row = ui->filterCombo->count() - 1;
            }

            ui->filterCombo->setCurrentIndex(row);
        }
        else if (key == "AO")
        {
            if ((row = ui->AOCombo->findText(value)) == -1)
            {
                ui->AOCombo->addItem(value);
                row = ui->AOCombo->count() - 1;
            }

            ui->AOCombo->setCurrentIndex(row);
        }
        else if (key == "Dome")
        {
            if ((row = ui->domeCombo->findText(value)) == -1)
            {
                ui->domeCombo->addItem(value);
                row = ui->domeCombo->count() - 1;
            }

            ui->domeCombo->setCurrentIndex(row);
        }
        else if (key == "Weather")
        {
            if ((row = ui->weatherCombo->findText(value)) == -1)
            {
                ui->weatherCombo->addItem(value);
                row = ui->weatherCombo->count() - 1;
            }

            ui->weatherCombo->setCurrentIndex(row);
        }
        else if (key == "Aux1")
        {
            if ((row = ui->aux1Combo->findText(value)) == -1)
            {
                ui->aux1Combo->addItem(value);
                row = ui->aux1Combo->count() - 1;
            }

            ui->aux1Combo->setCurrentIndex(row);
        }
        else if (key == "Aux2")
        {
            if ((row = ui->aux2Combo->findText(value)) == -1)
            {
                ui->aux2Combo->addItem(value);
                row = ui->aux2Combo->count() - 1;
            }

            ui->aux2Combo->setCurrentIndex(row);
        }
        else if (key == "Aux3")
        {
            if ((row = ui->aux3Combo->findText(value)) == -1)
            {
                ui->aux3Combo->addItem(value);
                row = ui->aux3Combo->count() - 1;
            }

            ui->aux3Combo->setCurrentIndex(row);
        }
        else if (key == "Aux4")
        {
            if ((row = ui->aux4Combo->findText(value)) == -1)
            {
                ui->aux4Combo->addItem(value);
                row = ui->aux4Combo->count() - 1;
            }

            ui->aux4Combo->setCurrentIndex(row);
        }
    }

    m_INDIHub = pi->indihub;
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
    const bool isLocal = ui->localMode->isChecked();

    // Save current selections and update models
    for (auto &device : m_Devices)
    {
        device.selectedDriver = device.combo->currentText();
        delete device.model;
        device.model = new QStandardItemModel(this);
        populateManufacturerCombo(device.model, device.combo, device.selectedDriver, isLocal, device.families);
    }
}

void ProfileEditor::setProfileName(const QString &name)
{
    ui->profileIN->setText(name);
}

void ProfileEditor::setAuxDrivers(const QStringList &aux)
{
    QStringList auxList(aux);

    if (auxList.isEmpty())
        return;
    ui->aux1Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())
        return;
    ui->aux2Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())
        return;
    ui->aux3Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())
        return;
    ui->aux4Combo->setCurrentText(auxList.first());
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
        ui->mountCombo->setFocus();
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

    m_INDIHub = profile["indihub"].toInt(m_INDIHub);

    // Drivers
    const QString mount = profile["mount"].toString("--");
    if (mount == "--")
        ui->mountCombo->setCurrentIndex(0);
    else
    {
        ui->mountCombo->addItem(mount);
        ui->mountCombo->setCurrentIndex(ui->mountCombo->count() - 1);
    }

    const QString ccd = profile["ccd"].toString("--");
    if (ccd == "--")
        ui->ccdCombo->setCurrentIndex(0);
    else
    {
        ui->ccdCombo->addItem(ccd);
        ui->ccdCombo->setCurrentIndex(ui->ccdCombo->count() - 1);
    }

    const QString guider = profile["guider"].toString("--");
    if (guider == "--")
        ui->guiderCombo->setCurrentIndex(0);
    else
    {
        ui->guiderCombo->addItem(guider);
        ui->guiderCombo->setCurrentIndex(ui->guiderCombo->count() - 1);
    }

    const QString focuser = profile["focuser"].toString("--");
    if (focuser == "--")
        ui->focuserCombo->setCurrentIndex(0);
    else
    {
        ui->focuserCombo->addItem(focuser);
        ui->focuserCombo->setCurrentIndex(ui->focuserCombo->count() - 1);
    }

    ui->filterCombo->setCurrentText(profile["filter"].toString("--"));
    ui->AOCombo->setCurrentText(profile["ao"].toString("--"));
    ui->domeCombo->setCurrentText(profile["dome"].toString("--"));
    ui->weatherCombo->setCurrentText(profile["weather"].toString("--"));

    const auto aux1 = profile["aux1"].toString("--");
    if (aux1.isEmpty() || aux1 == "--")
        ui->aux1Combo->setCurrentIndex(0);
    else
    {
        ui->aux1Combo->addItem(aux1);
        ui->aux1Combo->setCurrentIndex(ui->aux1Combo->count() - 1);
    }

    const auto aux2 = profile["aux2"].toString("--");
    if (aux2.isEmpty() || aux2 == "--")
        ui->aux2Combo->setCurrentIndex(0);
    else
    {
        ui->aux2Combo->addItem(aux2);
        ui->aux2Combo->setCurrentIndex(ui->aux2Combo->count() - 1);
    }

    const auto aux3 = profile["aux3"].toString("--");
    if (aux3.isEmpty() || aux3 == "--")
        ui->aux3Combo->setCurrentIndex(0);
    else
    {
        ui->aux3Combo->addItem(aux3);
        ui->aux3Combo->setCurrentIndex(ui->aux3Combo->count() - 1);
    }

    const auto aux4 = profile["aux4"].toString("--");
    if (aux4.isEmpty() || aux4 == "--")
        ui->aux4Combo->setCurrentIndex(0);
    else
    {
        ui->aux4Combo->addItem(aux4);
        ui->aux4Combo->setCurrentIndex(ui->aux4Combo->count() - 1);
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

void ProfileEditor::showINDIHub()
{
    QDialog hub;
    Ui::INDIHub indihub;
    indihub.setupUi(&hub);

    indihub.modeButtonGroup->setId(indihub.offR, 0);
    indihub.modeButtonGroup->setId(indihub.solorR, 1);
    indihub.modeButtonGroup->setId(indihub.shareR, 2);
    indihub.modeButtonGroup->setId(indihub.roboticR, 3);

    indihub.logoLabel->setPixmap(QIcon(":/icons/indihub_logo.svg").pixmap(QSize(128, 128)));

    indihub.modeButtonGroup->button(m_INDIHub)->setChecked(true);
    connect(indihub.closeB, &QPushButton::clicked, &hub, &QDialog::close);

    hub.exec();

    m_INDIHub = indihub.modeButtonGroup->checkedId();
}

void ProfileEditor::populateManufacturerCombo(QStandardItemModel *model, QComboBox *combo, const QString &selectedDriver,
        bool isLocal, const QList<DeviceFamily> &families)
{
    if (isLocal)
    {
        QStandardItem *selectedItem = nullptr;
        model->appendRow(new QStandardItem("--"));
        for (QSharedPointer<DriverInfo>driver : DriverManager::Instance()->getDrivers())
        {
            if (!families.contains(driver->getType()))
                continue;

            QString manufacturer = driver->manufacturer();
            QList<QStandardItem*> manufacturers = model->findItems(manufacturer);

            QStandardItem *parentItem = nullptr;
            if (model->findItems(manufacturer).empty())
            {
                parentItem = new QStandardItem(manufacturer);
                parentItem->setSelectable(false);
                model->appendRow(parentItem);
            }
            else
            {
                parentItem = manufacturers.first();
            }

            QStandardItem *item = new QStandardItem(driver->getLabel());
            item->setData(getTooltip(driver), Qt::ToolTipRole);
            parentItem->appendRow(item);
            if (selectedDriver == driver->getLabel())
                selectedItem = item;
        }
        QTreeView *view = new QTreeView(this);
        view->setModel(model);
        view->sortByColumn(0, Qt::AscendingOrder);
        combo->setView(view);
        combo->setModel(model);
        if (selectedItem)
        {
            // JM: Only way to make it the QTreeView sets the current index
            // in the combo way

            QModelIndex index = model->indexFromItem(selectedItem);

            // First set current index to the child
            combo->setRootModelIndex(index.parent());
            combo->setModelColumn(index.column());
            combo->setCurrentIndex(index.row());

            // Now reset
            combo->setRootModelIndex(QModelIndex());
            view->setCurrentIndex(index);
        }
    }
    else
    {
        QIcon remoteIcon = QIcon::fromTheme("network-modem");
        combo->setView(new QListView(this));
        model->appendRow(new QStandardItem("--"));
        QIcon icon;
        for (QSharedPointer<DriverInfo>driver : DriverManager::Instance()->getDrivers())
        {
            if (!families.contains(driver->getType()))
                continue;

            bool locallyAvailable = false;
            if (driver->getAuxInfo().contains("LOCALLY_AVAILABLE"))
                locallyAvailable = driver->getAuxInfo().value("LOCALLY_AVAILABLE", false).toBool();
            icon = locallyAvailable ? QIcon() : remoteIcon;

            QStandardItem *mount = new QStandardItem(icon, driver->getLabel());
            mount->setData(getTooltip(driver), Qt::ToolTipRole);
            model->appendRow(mount);
        }
        combo->setModel(model);
        combo->setCurrentText(selectedDriver);
    }
}

void ProfileEditor::executeScriptEditor()
{
    if (pi == nullptr)
        return;
    QStringList currentDrivers;
    for (auto &oneCombo : ui->driversGroupBox->findChildren<QComboBox *>())
        currentDrivers << oneCombo->currentText();
    currentDrivers.removeAll("--");
    currentDrivers.removeAll("");
    for (auto &oneDriver : ui->remoteDrivers->text().split(","))
        currentDrivers << oneDriver;
    currentDrivers.sort();
    ProfileScriptDialog dialog(currentDrivers, pi->scripts, this);
    dialog.exec();
    auto settings = dialog.jsonSettings();
    pi->scripts = QJsonDocument(settings).toJson(QJsonDocument::Compact);
}
