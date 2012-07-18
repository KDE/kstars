/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "ekos.h"

#include "kstars.h"

#include <KMessageBox>

#include <config-kstars.h>


#include "indi/devicemanager.h"
#include "indi/indimenu.h"
#include "indi/indielement.h"
#include "indi/indidriver.h"
#include "indi/indiproperty.h"
#include "indi/indidevice.h"
#include "indi/indistd.h"

#include "ekos/telescope.h"
#include "ekos/ccd.h"
#include "ekos/filter.h"
#include "ekos/focuser.h"

Ekos::Ekos( KStars *_ks )
        : QDialog( _ks )
{
    setupUi(this);

    deviceManager = NULL;
    nDevices=0;
    useGuiderFromCCD=false;
    useFilterFromCCD=false;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");

    foreach(IDevice *dv, KStars::Instance()->indiDriver()->devices)
    {
        switch (dv->type)
        {
        case KSTARS_TELESCOPE:
             telescopeCombo->addItem(dv->tree_label);
             break;

        case KSTARS_CCD:
            ccdCombo->addItem(dv->tree_label);
            guiderCombo->addItem(dv->tree_label);
            break;

        case KSTARS_FOCUSER:
            focuserCombo->addItem(dv->tree_label);
            break;

        case KSTARS_FILTER:
            filterCombo->addItem(dv->tree_label);
            break;

        }
    }

    toolsWidget->setTabEnabled(1, false);
    toolsWidget->setTabEnabled(2, false);
    toolsWidget->setTabEnabled(3, false);
    toolsWidget->setTabEnabled(4, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), KStars::Instance()->indiMenu(), SLOT(show()));

}

Ekos::~Ekos() {}

void Ekos::processINDI()
{
    int port=0;

    if (processINDIB->text() == i18n("Stop INDI"))
    {
        cleanDevices();
        return;
    }

    IDevice *scope_dv=NULL, *ccd_dv=NULL, *guider_dv=NULL, *filter_dv=NULL, *focuser_dv=NULL;

    // Don't start INDI if it's already started
    if (deviceManager != NULL)
        return;

    // Let's clear cache
    nDevices=0;
    processed_devices.clear();

    scope_dv   = KStars::Instance()->indiDriver()->findDeviceByLabel(telescopeCombo->currentText());
    ccd_dv     = KStars::Instance()->indiDriver()->findDeviceByLabel(ccdCombo->currentText());
    guider_dv  = KStars::Instance()->indiDriver()->findDeviceByLabel(guiderCombo->currentText());
    filter_dv  = KStars::Instance()->indiDriver()->findDeviceByLabel(filterCombo->currentText());
    focuser_dv = KStars::Instance()->indiDriver()->findDeviceByLabel(focuserCombo->currentText());

    if (guider_dv == ccd_dv)
        useGuiderFromCCD = true;

    if (filter_dv == ccd_dv)
        useFilterFromCCD = true;

    if (scope_dv != NULL)
        processed_devices.append(scope_dv);
    if (ccd_dv != NULL)
        processed_devices.append(ccd_dv);
    if (guider_dv != NULL && useGuiderFromCCD == false)
        processed_devices.append(guider_dv);
    if (filter_dv != NULL && useFilterFromCCD == false)
        processed_devices.append(filter_dv);

    if (focuser_dv != NULL)
        processed_devices.append(focuser_dv);


    if (processed_devices.empty()) return;

    nDevices = processed_devices.size();

    // Select random port within range is none specified.
     port = KStars::Instance()->indiDriver()->getINDIPort(port);

     if (port <= 0)
     {
           KMessageBox::error(0, i18n("Cannot start INDI server: port error."));
          return;
     }

         deviceManager = KStars::Instance()->indiMenu()->initDeviceManager("localhost", ((uint) port), localR->isChecked() ? DeviceManager::M_LOCAL : DeviceManager::M_SERVER);

         if (deviceManager == NULL)
         {
                 kWarning() << "Warning: device manager has not been established properly";
                 return;
         }

         deviceManager->appendManagedDevices(processed_devices);
         deviceManager->startServer();
         //connect(deviceManager, SIGNAL(deviceManagerError(DeviceManager *)), this, SLOT(disconnectDevices()));

         connect(deviceManager, SIGNAL(deviceManagerError(DeviceManager *)), this, SLOT(cleanDevices()));

         connect(deviceManager, SIGNAL(newDevice(INDI_D*)), this, SLOT(processNewDevice(INDI_D*)));

         connectB->setEnabled(false);
         disconnectB->setEnabled(false);
         controlPanelB->setEnabled(false);

         processINDIB->setText(i18n("Stop INDI"));

}

void Ekos::connectDevices()
{
    if (scope != NULL)
        scope->Connect();

    if (ccd != NULL)
        ccd->Connect();

    if (guider != NULL)
        guider->Connect();

    if (focuser != NULL)
        focuser->Connect();

    if (filter != NULL)
        filter->Connect();

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);
}

void Ekos::disconnectDevices()
{

    if (scope != NULL)
        scope->Disconnect();

    if (ccd != NULL)
        ccd->Disconnect();

    if (guider != NULL)
        guider->Disconnect();

    if (focuser != NULL)
        focuser->Disconnect();

    if (filter != NULL)
        filter->Disconnect();

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

}

void Ekos::cleanDevices()
{
    KStars::Instance()->indiMenu()->stopDeviceManager(processed_devices);

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
}

void Ekos::processNewDevice(INDI_D *dp)
{
    foreach (IDevice *dv, processed_devices)
    {
        if (dv->name == dp->name)
        {
            switch (dv->type)
            {
               case KSTARS_TELESCOPE:
                if (scope == NULL && dp->name == telescopeCombo->currentText())
                {
                    scope = new EkoDevice::Telescope(dp);
                    connect(dp, SIGNAL(newProperty(INDI_P*)), scope, SLOT(processNewProperty(INDI_P*)));
                }
                break;

            case KSTARS_CCD:
                if (ccd == NULL && dp->name == ccdCombo->currentText())
                {
                     ccd = new EkoDevice::CCD(dp);
                     connect(dp, SIGNAL(newProperty(INDI_P*)), ccd, SLOT(processNewProperty(INDI_P*)));
                }
               else if (guider == NULL && useGuiderFromCCD == false && dp->name == guiderCombo->currentText())
               {
                     guider = new EkoDevice::CCD(dp);
                     connect(dp, SIGNAL(newProperty(INDI_P*)), guider, SLOT(processNewProperty(INDI_P*)));
               }
               break;

            case KSTARS_FILTER:
                if (filter == NULL && useFilterFromCCD == false && dp->name == filterCombo->currentText())
                {
                     filter = new EkoDevice::Filter(dp);
                     connect(dp, SIGNAL(newProperty(INDI_P*)), filter, SLOT(processNewProperty(INDI_P*)));
                 }
                 break;

            case KSTARS_FOCUSER:
                if (focuser == NULL && dp->name == focuserCombo->currentText())
                {
                     focuser = new EkoDevice::Focuser(dp);
                     connect(dp, SIGNAL(newProperty(INDI_P*)), focuser, SLOT(processNewProperty(INDI_P*)));
                }
                break;

            }
        }
    }

    nDevices--;

    if (nDevices == 0)
    {
        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
    }

}

#include "ekos.moc"
