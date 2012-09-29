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
    useST4          =false;
    ccdStarted      =false;

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
    auxCombo->addItem("--");

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

        case KSTARS_AUXILIARY:
            auxCombo->addItem(dv->getTreeLabel());
            break;

        default:
            continue;
            break;

        }

        driversList[dv->getTreeLabel()] = dv;
    }

    loadDefaultDrivers();

    connect(toolsWidget, SIGNAL(currentChanged(int)), this, SLOT(updateLog()));

    toolsWidget->setTabEnabled(1, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

    connect(clearB, SIGNAL(clicked()), this, SLOT(clearLog()));

}

EkosManager::~EkosManager()
{
    delete captureProcess;
    delete focusProcess;
    delete guideProcess;
}

void EkosManager::reset()
{
    nDevices=0;
    useGuiderFromCCD=false;
    useFilterFromCCD=false;
    guideStarted    =false;
    useST4          =false;
    ccdStarted      =false;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;
    aux     =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;
    aux_di     = NULL;

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
    QString AuxDriver = Options::auxDriver();

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


    if (AuxDriver.isEmpty() == false && AuxDriver != "--")
    {
        for (int i=0; i < auxCombo->count(); i++)
            if (auxCombo->itemText(i) == AuxDriver)
            {
                auxCombo->setCurrentIndex(i);
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

   Options::setAuxDriver(auxCombo->currentText());

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
    aux_di     = driversList.value(auxCombo->currentText());

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

    if (aux_di != NULL)
        managedDevices.append(aux_di);

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
    connect(INDIListener::Instance(), SIGNAL(newST4(ISD::ST4*)), this, SLOT(setST4(ISD::ST4*)));
    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)));

    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));

    appendLogText("INDI services started. Please connect devices.");
}

void EkosManager::connectDevices()
{
    if (scope)
    {
        connect(scope, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));
         scope->Connect();
    }

    if (ccd)
    {
        ccd->Connect();
        connect(ccd, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));
    }

    if (guider && guider != ccd)
    {
        guider->Connect();
        connect(guider, SIGNAL(propertyDefined(INDI::Property*)), this, SLOT(processNewProperty(INDI::Property*)));
    }

    if (filter && filter != ccd)
        filter->Connect();

    if (focuser)
        focuser->Connect();

    if (aux)
        aux->Connect();


    connectB->setEnabled(false);
    disconnectB->setEnabled(true);

    appendLogText("Connecting INDI devices...");
}

void EkosManager::disconnectDevices()
{

    if (scope)
         scope->Disconnect();

    if (ccd)
    {
        ccdStarted      =false;
        ccd->Disconnect();
    }

    if (guider && guider != ccd)    
        guider->Disconnect();


    if (filter && filter != ccd)
        filter->Disconnect();

    if (focuser)
        focuser->Disconnect();

    if (aux)
        aux->Disconnect();

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

    appendLogText("Disconnecting INDI devices...");

}

void EkosManager::cleanDevices()
{

    INDIListener::Instance()->disconnect(this);

    DriverManager::Instance()->stopDevices(managedDevices);


    nDevices = 0;
    managedDevices.clear();

    reset();

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    appendLogText("INDI services stopped.");
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

             case KSTARS_AUXILIARY:
                if (aux_di == di)
                    aux = devInterface;

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
    scope = scopeDevice;

    appendLogText(QString("%1 is online.").arg(scope->getDeviceName()));

    if (guideProcess)
        guideProcess->setTelescope(scope);
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    // If we have a guider and it's the same as the CCD driver, then let's establish it separately.
    if (useGuiderFromCCD == false && guider_di && (!strcmp(guider_di->getBaseDevice()->getDeviceName(), ccdDevice->getDeviceName())))
    {
        guider = ccdDevice;
        appendLogText(QString("%1 is online.").arg(ccdDevice->getDeviceName()));
    }
    else
    {
        ccd = ccdDevice;

        if (ccdStarted == false)
            appendLogText(QString("%1 is online.").arg(ccdDevice->getDeviceName()));

        ccdStarted = true;

        // If guider is the same driver as the CCD
        if (useGuiderFromCCD == true)
            guider = ccd;
    }

    initCapture();

    captureProcess->addCCD(ccdDevice);

    initFocus();

    focusProcess->addCCD(ccdDevice);


}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{

    if (useFilterFromCCD == false)
    {
       filter = filterDevice;
       appendLogText(QString("%1 is online.").arg(filter->getDeviceName()));
    }
    else
        filter = ccd;

    initCapture();

    captureProcess->addFilter(filter);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    focuser = focuserDevice;

    initFocus();

    focusProcess->setFocuser(focuser);

    appendLogText(QString("%1 is online.").arg(focuser->getDeviceName()));

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

         if (focusProcess)
         {
             for (int i=0; i < toolsWidget->count(); i++)
                 if (i18n("Focus") == toolsWidget->tabText(i))
                     toolsWidget->removeTab(i);

             focuser = NULL;
             delete (focusProcess);
             focusProcess = NULL;

         }



        break;

    case KSTARS_FOCUSER:
    break;

      default:
         break;
    }


    appendLogText(QString("%1 is offline.").arg(devInterface->getDeviceName()));

    foreach(DriverInfo *drvInfo, managedDevices)
    {
        if (drvInfo == devInterface->getDriverInfo())
        {
            managedDevices.removeOne(drvInfo);

            if (managedDevices.count() == 0)
             cleanDevices();

            break;
        }
    }



}

void EkosManager::processNewProperty(INDI::Property* prop)
{
    if (!strcmp(prop->getName(), "CCD_INFO") && guider && !strcmp(guider->getDeviceName(), prop->getDeviceName()))
    {
            // Delay guiderProcess contruction until we receive CCD_INFO property
            initGuide();
            guideProcess->setCCD(guider);

            if (scope && scope->isConnected())
                guideProcess->setTelescope(scope);

    }

}

void EkosManager::updateLog()
{
    if (enableLoggingCheck->isChecked() == false)
        return;

    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
        ekosLogOut->setPlainText(logText.join("\n"));
    else if (currentWidget == captureProcess)
        ekosLogOut->setPlainText(captureProcess->getLogText());
    else if (currentWidget == focusProcess)
        ekosLogOut->setPlainText(focusProcess->getLogText());
    else if (currentWidget == guideProcess)
        ekosLogOut->setPlainText(guideProcess->getLogText());

}

void EkosManager::appendLogText(const QString &text)
{

    logText.insert(0, QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss") + " " + i18n("%1").arg(text));

    updateLog();
}

void EkosManager::clearLog()
{
    QWidget *currentWidget = toolsWidget->currentWidget();

    if (currentWidget == setupTab)
    {
        logText.clear();
        updateLog();
    }
    else if (currentWidget == captureProcess)
        captureProcess->clearLog();
    else if (currentWidget == focusProcess)
        focusProcess->clearLog();
    else if (currentWidget == guideProcess)
        guideProcess->clearLog();

}

void EkosManager::initCapture()
{
    if (captureProcess)
        return;

     captureProcess = new Ekos::Capture();
     toolsWidget->addTab( captureProcess, i18n("CCD"));
     connect(captureProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

}

void EkosManager::initFocus()
{
    if (focusProcess)
        return;

    focusProcess = new Ekos::Focus();
    toolsWidget->addTab( focusProcess, i18n("Focus"));
    connect(focusProcess, SIGNAL(newLog()), this, SLOT(updateLog()));

}

void EkosManager::initGuide()
{    
    if (guideProcess == NULL)
        guideProcess = new Ekos::Guide();

    if (guider && guider->isConnected() && useST4 && guideStarted == false)
    {
        guideStarted = true;
        toolsWidget->addTab( guideProcess, i18n("Guide"));
        connect(guideProcess, SIGNAL(newLog()), this, SLOT(updateLog()));
    }
}

void EkosManager::setST4(ISD::ST4 * st4Driver)
{
    appendLogText(i18n("Guider port from %1 is ready.").arg(st4Driver->getDeviceName()));
     useST4=true;

     initGuide();

     guideProcess->addST4(st4Driver);

}


#include "ekosmanager.moc"
