/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <libindi/basedevice.h>

#include "Options.h"
#include "ekosmanager.h"
#include "kstars.h"

#include <KMessageBox>
#include <KComboBox>

#include <config-kstars.h>

#include "indi/clientmanager.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "indi/guimanager.h"

EkosManager::EkosManager()
        : QDialog(KStars::Instance())
{
    setupUi(this);

    nDevices=0;
    useGuiderFromCCD=false;
    useFilterFromCCD=false;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
             telescopeCombo->addItem(dv->getTreeLabel());
             break;

        case KSTARS_CCD:
            ccdCombo->addItem(dv->getTreeLabel());
            guiderCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FOCUSER:
            focuserCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FILTER:
            filterCombo->addItem(dv->getTreeLabel());
            break;

        default:
            continue;
            break;

        }

        driversList[dv->getTreeLabel()] = dv;
    }

    loadDefaultDrivers();

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(tabChanged(int)));

    toolsWidget->setTabEnabled(1, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

}

EkosManager::~EkosManager() {}

void EkosManager::reset()
{
    nDevices=0;
    useGuiderFromCCD=false;
    useFilterFromCCD=false;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;

    captureProcess = NULL;
    focusProcess   = NULL;
    guideProcess   = NULL;
}

void EkosManager::loadDefaultDrivers()
{
    QString TelescopeDriver = Options::telescopeDriver();
    QString CCDDriver = Options::cCDDriver();
    QString GuiderDriver = Options::guiderDriver();
    QString FocuserDriver = Options::focuserDriver();
    QString FilterDriver = Options::filterDriver();

    if (TelescopeDriver.isEmpty() == false && TelescopeDriver != "--")
    {
        for (int i=0; i < telescopeCombo->count(); i++)
            if (telescopeCombo->itemText(i) == TelescopeDriver)
            {
                telescopeCombo->setCurrentIndex(i);
                break;
            }
    }

    if (CCDDriver.isEmpty() == false && CCDDriver != "--")
    {
        for (int i=0; i < ccdCombo->count(); i++)
            if (ccdCombo->itemText(i) == CCDDriver)
            {
                ccdCombo->setCurrentIndex(i);
                break;
            }
    }

    if (GuiderDriver.isEmpty() == false && GuiderDriver != "--")
    {
        for (int i=0; i < guiderCombo->count(); i++)
            if (guiderCombo->itemText(i) == GuiderDriver)
            {
                guiderCombo->setCurrentIndex(i);
                break;
            }
    }

    if (FilterDriver.isEmpty() == false && FilterDriver != "--")
    {
        for (int i=0; i < filterCombo->count(); i++)
            if (filterCombo->itemText(i) == FilterDriver)
            {
                filterCombo->setCurrentIndex(i);
                break;
            }
    }

    if (FocuserDriver.isEmpty() == false && FocuserDriver != "--")
    {
        for (int i=0; i < focuserCombo->count(); i++)
            if (focuserCombo->itemText(i) == FocuserDriver)
            {
                focuserCombo->setCurrentIndex(i);
                break;
            }
    }

}

void EkosManager::saveDefaultDrivers()
{

     Options::setTelescopeDriver(telescopeCombo->currentText());

    Options::setCCDDriver(ccdCombo->currentText());

    Options::setGuiderDriver(guiderCombo->currentText());

    Options::setFilterDriver(filterCombo->currentText());

   Options::setFocuserDriver(focuserCombo->currentText());

}

void EkosManager::processINDI()
{
    if (managedDevices.count() > 0)
    {
        cleanDevices();
        return;
    }

    managedDevices.clear();
    reset();

    scope_di   = driversList.value(telescopeCombo->currentText());
    ccd_di     = driversList.value(ccdCombo->currentText());
    guider_di  = driversList.value(guiderCombo->currentText());
    filter_di  = driversList.value(filterCombo->currentText());
    focuser_di = driversList.value(focuserCombo->currentText());

    if (guider_di)
    {
        if (guider_di == ccd_di)
        {
            useGuiderFromCCD = true;
            guider_di = NULL;
        }
    }

    if (filter_di)
    {
        if (filter_di == ccd_di)
        {
            useFilterFromCCD = true;
            filter_di = NULL;
        }
    }

    if (scope_di != NULL)
        managedDevices.append(scope_di);
    if (ccd_di != NULL)
        managedDevices.append(ccd_di);
    if (guider_di != NULL)
        managedDevices.append(guider_di);
    if (filter_di != NULL)
        managedDevices.append(filter_di);

    if (focuser_di != NULL)
        managedDevices.append(focuser_di);

    if (managedDevices.empty()) return;

    nDevices = managedDevices.count();

    saveDefaultDrivers();

    if (DriverManager::Instance()->startDevices(managedDevices) == false)
        return;

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));

    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)));
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));
}

void EkosManager::connectDevices()
{
    if (scope)
    {
        connect(scope, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));
         scope->runCommand(INDI_CONNECT);
    }

    if (ccd)
        ccd->runCommand(INDI_CONNECT);

    if (guider && guider != ccd)
    {
        connect(guider, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));
        guider->runCommand(INDI_CONNECT);
    }

    if (filter && filter != ccd)
        filter->runCommand(INDI_CONNECT);

    if (focuser)
        focuser->runCommand(INDI_CONNECT);

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);
}

void EkosManager::disconnectDevices()
{

    if (scope)
         scope->runCommand(INDI_DISCONNECT);

    if (ccd)
        ccd->runCommand(INDI_DISCONNECT);

    if (guider && guider != ccd)    
        guider->runCommand(INDI_DISCONNECT);


    if (filter && filter != ccd)
        filter->runCommand(INDI_DISCONNECT);

    if (focuser)
        focuser->runCommand(INDI_DISCONNECT);

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

}

void EkosManager::cleanDevices()
{

    DriverManager::Instance()->stopDevices(managedDevices);

    nDevices = 0;
    managedDevices.clear();

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
}

void EkosManager::processNewDevice(ISD::GDInterface *devInterface)
{
    foreach(DriverInfo *di, driversList.values())
    {
        if (di == devInterface->getDriverInfo())
        {
            switch (di->getType())
            {
               case KSTARS_TELESCOPE:
                scope = devInterface;
                break;


               case KSTARS_CCD:
                if (guider_di == di)
                    guider = devInterface;
                else
                    ccd = devInterface;

                break;

            case KSTARS_FOCUSER:
                focuser = devInterface;
                break;

             case KSTARS_FILTER:
                if (filter_di == di)
                    filter = devInterface;
                break;

              default:
                break;
            }

            nDevices--;
            break;
        }
    }

    if (nDevices == 0)
    {
        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
    }

}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
   // qDebug() << "Received set telescope scope device " << endl;
    scope = scopeDevice;
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    //qDebug() << "Received set CCD device " << endl;

    //if (useGuiderFromCCD == false && guider_di)
    if (useGuiderFromCCD == false && guider_di && (!strcmp(guider_di->getBaseDevice()->getDeviceName(), ccdDevice->getDeviceName())))
        guider = ccdDevice;
    else
    {
        ccd = ccdDevice;
        if (useGuiderFromCCD == true)
            guider = ccd;
    }

    if (captureProcess == NULL)
    {
         captureProcess = new Ekos::Capture();
         toolsWidget->addTab( captureProcess, i18n("CCD"));
    }

    captureProcess->addCCD(ccdDevice);

    if (focusProcess != NULL)
        focusProcess->addCCD(ccdDevice);

}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{

    if (useFilterFromCCD == false)
       filter = filterDevice;
    else
        filter = ccd;

    if (captureProcess == NULL)
    {
         captureProcess = new Ekos::Capture();
         toolsWidget->addTab( captureProcess, i18n("CCD"));
    }

    captureProcess->addFilter(filter);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    //qDebug() << "Received set focuser device " << endl;
    focuser = focuserDevice;

    if (focusProcess == NULL)
    {
         focusProcess = new Ekos::Focus();
         toolsWidget->addTab( focusProcess, i18n("Focus"));
    }

    focusProcess->addFocuser(focuser);

    if (ccd != NULL)
        focusProcess->addCCD(ccd);
}

void EkosManager::removeDevice(ISD::GDInterface* devInterface)
{
    switch (devInterface->getType())
    {
        case KSTARS_CCD:
        if (devInterface == ccd  || devInterface == guider)
        {
            for (int i=0; i < toolsWidget->count(); i++)
                if (i18n("CCD") == toolsWidget->tabText(i))
                    toolsWidget->removeTab(i);

            ccd = NULL;
            delete (captureProcess);
            captureProcess = NULL;
         }
         if (useGuiderFromCCD || devInterface == guider)
         {
                for (int i=0; i < toolsWidget->count(); i++)
                    if (i18n("Guide") == toolsWidget->tabText(i))
                        toolsWidget->removeTab(i);

                guider = NULL;
                delete (guideProcess);
                guideProcess = NULL;

        }



        break;

    case KSTARS_FOCUSER:
    if (devInterface == focuser)
    {
        for (int i=0; i < toolsWidget->count(); i++)
            if (i18n("Focus") == toolsWidget->tabText(i))
                toolsWidget->removeTab(i);

        focuser = NULL;
        delete (focusProcess);
        focusProcess = NULL;
    }

    break;

      default:
         break;
    }


    foreach(DriverInfo *drvInfo, managedDevices)
    {
        if (drvInfo == devInterface->getDriverInfo())
        {
            managedDevices.removeOne(drvInfo);

            if (managedDevices.count() == 0)
            {
                processINDIB->setText(i18n("Start INDI"));
                connectB->setEnabled(false);
                disconnectB->setEnabled(false);
                controlPanelB->setEnabled(false);
            }

            break;
        }
    }



}

void EkosManager::processNewProperty(INDI::Property* prop)
{

    // Delay guiderProcess contruction until we receive CCD_INFO property
    if (guider != NULL && scope != NULL && guideProcess == NULL)
    {
        if (!strcmp(guider->getDeviceName(), prop->getDeviceName()) && !strcmp(prop->getName(), "CCD_INFO"))
        {
            guideProcess = new Ekos::Guide();
            toolsWidget->addTab( guideProcess, i18n("Guide"));

            guideProcess->addCCD(guider);
            guideProcess->addTelescope(scope);
        }
    }


}

void EkosManager::tabChanged(int index)
{
    if (index == -1)
        return;


//    resize(toolsWidget->currentWidget()->size());

}


#include "ekosmanager.moc"
