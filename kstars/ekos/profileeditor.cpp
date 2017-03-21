/*  Profile Editor
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include <QDialogButtonBox>

#include <KMessageBox>

#include "kstarsdata.h"
#include "geolocation.h"
#include "auxiliary/ksuserdb.h"

#include "indi/drivermanager.h"
#include "indi/driverinfo.h"

#
#include "profileeditor.h"
#include "profileinfo.h"

ProfileEditorUI::ProfileEditorUI( QWidget * p ) : QFrame( p )
{
    setupUi( this );
}


ProfileEditor::ProfileEditor(QWidget * w )  : QDialog( w )
{
#ifdef Q_OS_OSX
    setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
#endif
    ui = new ProfileEditorUI( this );

    pi = NULL;

    QVBoxLayout * mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    setWindowTitle(i18n("Profile Editor"));

    // Create button box and link it to save and reject functions
    QDialogButtonBox * buttonBox = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttonBox, SIGNAL(accepted()), this, SLOT(saveProfile()));

#ifdef Q_OS_WIN
    ui->remoteMode->setChecked(true);
    ui->localMode->setEnabled(false);
    setRemoteMode(true);
#else
    connect(ui->remoteMode, SIGNAL(toggled(bool)), this, SLOT(setRemoteMode(bool)));
#endif

    // Load all drivers
    loadDrivers();
}

ProfileEditor::~ProfileEditor()
{

}

void ProfileEditor::saveProfile()
{
    bool newProfile = (pi == NULL);

    if (ui->profileIN->text().isEmpty())
    {
        KMessageBox::error(this, i18n("Cannot save an empty profile!"));
        return;
    }

    if (newProfile)
    {
        int id = KStarsData::Instance()->userdb()->AddProfile(ui->profileIN->text());
        pi = new ProfileInfo(id, ui->profileIN->text());
    }
    else
        pi->name = ui->profileIN->text();

    // Local Mode
    if (ui->localMode->isChecked())
    {
        pi->host.clear();
        pi->port=-1;
        pi->INDIWebManagerPort=-1;
        //pi->customDrivers = ui->customDriversIN->text();
    }
    // Remote Mode
    else
    {
        pi->host = ui->remoteHost->text();
        pi->port = ui->remotePort->text().toInt();
        if (ui->INDIWebManagerCheck->isChecked())
            pi->INDIWebManagerPort = ui->INDIWebManagerPort->text().toInt();
        else
            pi->INDIWebManagerPort=-1;
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

    KStarsData::Instance()->userdb()->SaveProfile(pi);

    // Ekos manager will reload and new profiles will be created
    if (newProfile)
        delete (pi);

    accept();
}

ProfileInfo * ProfileEditor::getPi() const
{
    return pi;
}

void ProfileEditor::setRemoteMode(bool enable)
{
    loadDrivers();//This is needed to reload the drivers because some may not be available locally

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

    ui->loadSiteCheck->setEnabled(enable);

    ui->INDIWebManagerCheck->setEnabled(enable);
    ui->INDIWebManagerPort->setEnabled(enable);

}

void ProfileEditor::setPi(ProfileInfo * value)
{
    pi = value;

    ui->profileIN->setText(pi->name);

    ui->loadSiteCheck->setChecked(!pi->city.isEmpty());
    ui->autoConnectCheck->setChecked(pi->autoConnect);

    if (pi->city.isEmpty() == false)
    {
        if (pi->province.isEmpty())
            ui->loadSiteCheck->setText(ui->loadSiteCheck->text() + QString(" (%1, %2)").arg(pi->country).arg(pi->city));
        else
            ui->loadSiteCheck->setText(ui->loadSiteCheck->text() + QString(" (%1, %2, %3)").arg(pi->country).arg(pi->province).arg(pi->city));
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

    QMapIterator<QString, QString> i(pi->drivers);
    int row=0;

    while (i.hasNext())
    {
        i.next();

        QString key   = i.key();
        QString value = i.value();

        if (key == "Mount")
        {
            // If driver doesn't exist, let's add it to the list
            if ( (row = ui->mountCombo->findText(value)) == -1)
            {
                ui->mountCombo->addItem(value);
                row = ui->mountCombo->count() - 1;
            }

            // Set index to our driver
            ui->mountCombo->setCurrentIndex(row);
        }
        else if (key == "CCD")
        {
            if ( (row = ui->ccdCombo->findText(value)) == -1)
            {
                ui->ccdCombo->addItem(value);
                row = ui->ccdCombo->count() - 1;
            }

            ui->ccdCombo->setCurrentIndex(row);
        }
        else if (key == "Guider")
        {
            if ( (row = ui->guiderCombo->findText(value)) == -1)
            {
                ui->guiderCombo->addItem(value);
                row = ui->guiderCombo->count() - 1;
            }
            else
                ui->guiderCombo->setCurrentIndex(row);
        }
        else if (key == "Focuser")
        {
            if ( (row = ui->focuserCombo->findText(value)) == -1)
            {
                ui->focuserCombo->addItem(value);
                row = ui->focuserCombo->count() - 1;
            }

            ui->focuserCombo->setCurrentIndex(row);
        }
        else if (key == "Filter")
        {
            if ( (row = ui->filterCombo->findText(value)) == -1)
            {
                ui->filterCombo->addItem(value);
                row = ui->filterCombo->count() - 1;
            }

            ui->filterCombo->setCurrentIndex(row);
        }
        else if (key == "AO")
        {
            if ( (row = ui->AOCombo->findText(value)) == -1)
            {
                ui->AOCombo->addItem(value);
                row = ui->AOCombo->count() - 1;
            }

            ui->AOCombo->setCurrentIndex(row);
        }
        else if (key == "Dome")
        {
            if ( (row = ui->domeCombo->findText(value)) == -1)
            {
                ui->domeCombo->addItem(value);
                row = ui->domeCombo->count() - 1;
            }

            ui->domeCombo->setCurrentIndex(row);
        }
        else if (key == "Weather")
        {
            if ( (row = ui->weatherCombo->findText(value)) == -1)
            {
                ui->weatherCombo->addItem(value);
                row = ui->weatherCombo->count() - 1;
            }

            ui->weatherCombo->setCurrentIndex(row);
        }
        else if (key == "Aux1")
        {
            if ( (row = ui->aux1Combo->findText(value)) == -1)
            {
                ui->aux1Combo->addItem(value);
                row = ui->aux1Combo->count() - 1;
            }

            ui->aux1Combo->setCurrentIndex(row);
        }
        else if (key == "Aux2")
        {
            if ( (row = ui->aux2Combo->findText(value)) == -1)
            {
                ui->aux2Combo->addItem(value);
                row = ui->aux2Combo->count() - 1;
            }

            ui->aux2Combo->setCurrentIndex(row);
        }
        else if (key == "Aux3")
        {
            if ( (row = ui->aux3Combo->findText(value)) == -1)
            {
                ui->aux3Combo->addItem(value);
                row = ui->aux3Combo->count() - 1;
            }

            ui->aux3Combo->setCurrentIndex(row);
        }
        else if (key == "Aux4")
        {
            if ( (row = ui->aux4Combo->findText(value)) == -1)
            {
                ui->aux4Combo->addItem(value);
                row = ui->aux4Combo->count() - 1;
            }

            ui->aux4Combo->setCurrentIndex(row);
        }
    }
}

void ProfileEditor::loadDrivers()
{

    QVector<QComboBox *> boxes;
    boxes.append(ui->mountCombo);
    boxes.append(ui->ccdCombo);
    boxes.append(ui->guiderCombo);
    boxes.append(ui->AOCombo);
    boxes.append(ui->focuserCombo);
    boxes.append(ui->filterCombo);
    boxes.append(ui->domeCombo);
    boxes.append(ui->weatherCombo);
    boxes.append(ui->aux1Combo);
    boxes.append(ui->aux2Combo);
    boxes.append(ui->aux3Combo);
    boxes.append(ui->aux4Combo);

    QVector<QString> selectedItems;

    foreach(QComboBox * box,boxes)
    {
        selectedItems.append(box->currentText());
        box->clear();
        box->addItem("--");
        box->setMaxVisibleItems(20);
    }

    QIcon remoteIcon=QIcon::fromTheme("modem", QIcon(":/icons/breeze/default/modem.svg"));

    foreach(DriverInfo * dv, DriverManager::Instance()->getDrivers())
    {
        bool locallyAvailable=false;
        QIcon icon;
        if (dv->getAuxInfo().contains("LOCALLY_AVAILABLE"))
            locallyAvailable = dv->getAuxInfo().value("LOCALLY_AVAILABLE", false).toBool();
        if(!locallyAvailable)
        {
            if(ui->localMode->isChecked())
                continue;
            else
                icon=remoteIcon;
        }

        switch (dv->getType())
        {
            case KSTARS_TELESCOPE:
            {
                ui->mountCombo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_CCD:
            {
                ui->ccdCombo->addItem(icon, dv->getTreeLabel());
                ui->guiderCombo->addItem(icon, dv->getTreeLabel());

                ui->aux1Combo->addItem(icon, dv->getTreeLabel());
                ui->aux2Combo->addItem(icon, dv->getTreeLabel());
                ui->aux3Combo->addItem(icon, dv->getTreeLabel());
                ui->aux4Combo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_ADAPTIVE_OPTICS:
            {
                ui->AOCombo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_FOCUSER:
            {
                ui->focuserCombo->addItem(icon, dv->getTreeLabel());

                ui->aux1Combo->addItem(icon, dv->getTreeLabel());
                ui->aux2Combo->addItem(icon, dv->getTreeLabel());
                ui->aux3Combo->addItem(icon, dv->getTreeLabel());
                ui->aux4Combo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_FILTER:
            {
                ui->filterCombo->addItem(icon, dv->getTreeLabel());

                ui->aux1Combo->addItem(icon, dv->getTreeLabel());
                ui->aux2Combo->addItem(icon, dv->getTreeLabel());
                ui->aux3Combo->addItem(icon, dv->getTreeLabel());
                ui->aux4Combo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_DOME:
            {
                ui->domeCombo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_WEATHER:
            {
                ui->weatherCombo->addItem(icon, dv->getTreeLabel());

                ui->aux1Combo->addItem(icon, dv->getTreeLabel());
                ui->aux2Combo->addItem(icon, dv->getTreeLabel());
                ui->aux3Combo->addItem(icon, dv->getTreeLabel());
                ui->aux4Combo->addItem(icon, dv->getTreeLabel());
            }
            break;

            case KSTARS_AUXILIARY:
            {
                ui->aux1Combo->addItem(icon, dv->getTreeLabel());
                ui->aux2Combo->addItem(icon, dv->getTreeLabel());
                ui->aux3Combo->addItem(icon, dv->getTreeLabel());
                ui->aux4Combo->addItem(icon, dv->getTreeLabel());
            }
            break;

            default:
                continue;
                break;
        }
    }

    //ui->mountCombo->setCurrentIndex(-1);

    for(int i=0; i<boxes.count(); i++)
    {
        QComboBox * box=boxes.at(i);
        QString selectedItemText=selectedItems.at(i);
        int index=box->findText(selectedItemText);
        if(index==-1)
        {
            if(ui->localMode->isChecked())
                box->setCurrentIndex(0);
            else
                box->addItem(remoteIcon,selectedItemText);

        }
        else
        {
            box->setCurrentIndex(index);
        }

        box->model()->sort(0);
    }

    ui->guiderCombo->addItem(QIcon::fromTheme("crosshairs", QIcon(":/icons/breeze/default/crosshairs.svg")), "PHD2" );
    ui->guiderCombo->addItem(QIcon::fromTheme("crosshairs", QIcon(":/icons/breeze/default/crosshairs.svg")), "LinGuider" );
}

void ProfileEditor::setProfileName(const QString &name)
{
    ui->profileIN->setText(name);
}

void ProfileEditor::setAuxDrivers(const QStringList &aux)
{
    QStringList auxList(aux);

    if (auxList.isEmpty())  return;
    ui->aux1Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())  return;
    ui->aux2Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())  return;
    ui->aux3Combo->setCurrentText(auxList.first());
    auxList.removeFirst();

    if (auxList.isEmpty())  return;
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

void ProfileEditor::setExternalGuider(const QString &name)
{
    ui->guiderCombo->setCurrentText(name);
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

    if (enable == false)
        ui->mountCombo->setFocus();
}
