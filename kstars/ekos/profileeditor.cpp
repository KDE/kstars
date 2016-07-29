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

ProfileEditorUI::ProfileEditorUI( QWidget *p ) : QFrame( p )
{
    setupUi( this );
}


ProfileEditor::ProfileEditor(QWidget *w )  : QDialog( w )
{
    ui = new ProfileEditorUI( this );

    pi = NULL;

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(ui);
    setLayout(mainLayout);

    setWindowTitle(i18n("Profile Editor"));

    // Create button box and link it to save and reject functions
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save|QDialogButtonBox::Close);
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

   if (ui->mountCombo->currentText() != "--")
       pi->drivers["Mount"] = ui->mountCombo->currentText();
   else
       pi->drivers.remove("Mount");

   if (ui->ccdCombo->currentText() != "--")
       pi->drivers["CCD"] = ui->ccdCombo->currentText();
   else
       pi->drivers.remove("CCD");

   if (ui->guiderCombo->currentText() != "--")
       pi->drivers["Guider"] = ui->guiderCombo->currentText();
   else
       pi->drivers.remove("Guider");

   if (ui->focuserCombo->currentText() != "--")
       pi->drivers["Focuser"] = ui->focuserCombo->currentText();
   else
       pi->drivers.remove("Focuser");

   if (ui->filterCombo->currentText() != "--")
       pi->drivers["Filter"] = ui->filterCombo->currentText();
   else
       pi->drivers.remove("Filter");

   if (ui->AOCombo->currentText() != "--")
       pi->drivers["AO"] = ui->AOCombo->currentText();
   else
       pi->drivers.remove("AO");

   if (ui->domeCombo->currentText() != "--")
       pi->drivers["Dome"] = ui->domeCombo->currentText();
   else
       pi->drivers.remove("Dome");

   if (ui->weatherCombo->currentText() != "--")
       pi->drivers["Weather"] = ui->weatherCombo->currentText();
   else
       pi->drivers.remove("Weather");

   if (ui->aux1Combo->currentText() != "--")
       pi->drivers["Aux1"] = ui->aux1Combo->currentText();
   else
       pi->drivers.remove("Aux1");

   if (ui->aux2Combo->currentText() != "--")
       pi->drivers["Aux2"] = ui->aux2Combo->currentText();
   else
       pi->drivers.remove("Aux2");

   if (ui->aux3Combo->currentText() != "--")
       pi->drivers["Aux3"] = ui->aux3Combo->currentText();
   else
       pi->drivers.remove("Aux3");

   if (ui->aux4Combo->currentText() != "--")
       pi->drivers["Aux4"] = ui->aux4Combo->currentText();
   else
       pi->drivers.remove("Aux4");

   KStarsData::Instance()->userdb()->SaveProfile(pi);

   // Ekos manager will reload and new profiles will be created
   if (newProfile)
    delete (pi);

   accept();
}

ProfileInfo *ProfileEditor::getPi() const
{
    return pi;
}

void ProfileEditor::setRemoteMode(bool enable)
{
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

void ProfileEditor::setPi(ProfileInfo *value)
{
    pi = value;

    ui->profileIN->setText(pi->name);

    ui->loadSiteCheck->setChecked(!pi->city.isEmpty());

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
    ui->mountCombo->addItem("--");
    ui->ccdCombo->addItem("--");
    ui->guiderCombo->addItem("--");
    ui->AOCombo->addItem("--");
    ui->focuserCombo->addItem("--");
    ui->filterCombo->addItem("--");
    ui->domeCombo->addItem("--");
    ui->weatherCombo->addItem("--");
    ui->aux1Combo->addItem("--");
    ui->aux2Combo->addItem("--");
    ui->aux3Combo->addItem("--");
    ui->aux4Combo->addItem("--");

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
        {
            ui->mountCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_CCD:
        {
            ui->ccdCombo->addItem(dv->getTreeLabel());
            ui->guiderCombo->addItem(dv->getTreeLabel());

            ui->aux1Combo->addItem(dv->getTreeLabel());
            ui->aux2Combo->addItem(dv->getTreeLabel());
            ui->aux3Combo->addItem(dv->getTreeLabel());
            ui->aux4Combo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_ADAPTIVE_OPTICS:
        {
            ui->AOCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_FOCUSER:
        {
            ui->focuserCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_FILTER:
        {
            ui->filterCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_DOME:
        {
            ui->domeCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_WEATHER:
        {
            ui->weatherCombo->addItem(dv->getTreeLabel());
        }
        break;

        case KSTARS_AUXILIARY:
        {
            ui->aux1Combo->addItem(dv->getTreeLabel());
            ui->aux2Combo->addItem(dv->getTreeLabel());
            ui->aux3Combo->addItem(dv->getTreeLabel());
            ui->aux4Combo->addItem(dv->getTreeLabel());
        }
        break;

        default:
            continue;
            break;
        }
    }

    //ui->mountCombo->setCurrentIndex(-1);
    ui->mountCombo->model()->sort(0);
    ui->ccdCombo->model()->sort(0);
    ui->guiderCombo->model()->sort(0);
    ui->AOCombo->model()->sort(0);
    ui->focuserCombo->model()->sort(0);
    ui->filterCombo->model()->sort(0);
    ui->domeCombo->model()->sort(0);
    ui->weatherCombo->model()->sort(0);
    ui->aux1Combo->model()->sort(0);
    ui->aux2Combo->model()->sort(0);
    ui->aux3Combo->model()->sort(0);
    ui->aux4Combo->model()->sort(0);
}

