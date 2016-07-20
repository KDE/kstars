/** *************************************************************************
                          clientmanagerlite.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 10/07/2016
    copyright            : (C) 2016 by Artem Fedoskin
    email                : afedoskin3@gmail.com
 ***************************************************************************/
/** *************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "clientmanagerlite.h"
#include "Options.h"
#include "basedevice.h"
#include "kstarslite.h"
#include <QQmlApplicationEngine>
#include <KLocalizedString>
#include "indicom.h"
#include "skymaplite.h"
#include "kstarslite/skyitems/telescopesymbolsitem.h"


DeviceInfoLite::DeviceInfoLite(INDI::BaseDevice *dev)
    :device(dev), telescope(nullptr)
{

}

ClientManagerLite::ClientManagerLite()
    :m_connected(false)
{
    qmlRegisterType<TelescopeLite>("TelescopeLiteEnums", 1, 0, "TelescopeNS");
    qmlRegisterType<TelescopeLite>("TelescopeLiteEnums", 1, 0, "TelescopeWE");
    qmlRegisterType<TelescopeLite>("TelescopeLiteEnums", 1, 0, "TelescopeCommand");
}

ClientManagerLite::~ClientManagerLite()
{

}

bool ClientManagerLite::setHost(QString ip, unsigned int port) {
    if(!isConnected()) {
        setServer(ip.toStdString().c_str(), port);
        qDebug() << ip << port;
        if(connectServer()) {
            setConnectedHost(ip + ":" + QString::number(port));
            return true;
        }
    }
    return false;
}

void ClientManagerLite::disconnectHost() {
    disconnectServer();
    setConnectedHost("");
    m_devices.clear();
}

TelescopeLite *ClientManagerLite::getTelescope(QString deviceName) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        if(devInfo->device->getDeviceName() == deviceName) {
            return devInfo->telescope;
        }
    }
    return nullptr;
}

void ClientManagerLite::setConnectedHost(QString connectedHost) {
    m_connectedHost = connectedHost;
    setConnected(m_connectedHost.size() > 0);

    emit connectedHostChanged(connectedHost);
}

void ClientManagerLite::setConnected(bool connected) {
    m_connected = connected;
    emit connectedChanged(connected);
}

QString ClientManagerLite::updateLED(QString device, QString property) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        if(devInfo->device->getDeviceName() == device) {
            INDI::Property *prop = devInfo->device->getProperty(property.toStdString().c_str());
            if(prop) {
                switch (prop->getState())
                {
                case IPS_IDLE:
                    return "grey";
                    break;

                case IPS_OK:
                    return "green";
                    break;

                case IPS_BUSY:
                    return "yellow";
                    break;

                case IPS_ALERT:
                    return "red";
                    break;

                default:
                    return "grey";
                    break;
                }
            }
        }
    }
    return "grey";
}

void ClientManagerLite::buildTextGUI(Property * property) {
    {
        ITextVectorProperty *tvp = property->getText();
        if (tvp == NULL)
            return;

        for (int i=0; i < tvp->ntp; i++)
        {
            IText *tp = &(tvp->tp[i]);
            QString name  = tp->name;
            QString label = tp->label;
            QString text = tp->text;
            bool read = false;
            bool write = false;
            /*if (tp->label[0])
                    label = i18nc(libindi_strings_context, itp->label);

                if (label == "(I18N_EMPTY_MESSAGE)")
                    label = itp->label;*/

            if (label.isEmpty())
                label = tvp->name;
            /*label = i18nc(libindi_strings_context, itp->name);

                if (label == "(I18N_EMPTY_MESSAGE)")*/

            //setupElementLabel();

            /*if (tp->text[0])
                    text = i18nc(libindi_strings_context, tp->text);*/

            switch (property->getPermission())
            {
            case IP_RW:
                read = true;
                write = true;

                break;

            case IP_RO:
                read = true;
                write = false;
                break;

            case IP_WO:
                read = false;
                write = true;
                break;
            }
            emit createINDIText(property->getDeviceName(), property->getName(), label, text, read, write);
        }
    }
}

void ClientManagerLite::buildNumberGUI(Property * property) {
    {
        INumberVectorProperty *nvp = property->getNumber();
        if (nvp == NULL)
            return;

        for (int i=0; i < nvp->nnp; i++)
        {
            bool scale = false;
            char iNumber[MAXINDIFORMAT];

            INumber *np = &(nvp->np[i]);
            QString name  = np->name;
            QString label = np->label;
            QString text;
            bool read = false;
            bool write = false;
            /*if (tp->label[0])
                    label = i18nc(libindi_strings_context, itp->label);

                if (label == "(I18N_EMPTY_MESSAGE)")
                    label = itp->label;*/

            if (label.isEmpty())
                label = np->name;

            numberFormat(iNumber, np->format, np->value);
            text = iNumber;

            /*label = i18nc(libindi_strings_context, itp->name);

                if (label == "(I18N_EMPTY_MESSAGE)")*/

            //setupElementLabel();

            /*if (tp->text[0])
                    text = i18nc(libindi_strings_context, tp->text);*/

            if (np->step != 0 && (np->max - np->min)/np->step <= 100)
                scale = true;

            switch (property->getPermission())
            {
            case IP_RW:
                read = true;
                write = true;

                break;

            case IP_RO:
                read = true;
                write = false;
                break;

            case IP_WO:
                read = false;
                write = true;
                break;
            }
            emit createINDINumber(property->getDeviceName(), property->getName(), label, text, read, write, scale);
        }
    }
}

void ClientManagerLite::buildMenuGUI(INDI::Property * property) {
    QStringList menuOptions;
    QString oneOption;
    int onItem=-1;
    ISwitchVectorProperty *svp = property->getSwitch();

    if (svp == NULL)
        return;

    for (int i=0; i < svp->nsp; i++)
    {
        ISwitch *tp = &(svp->sp[i]);

        buildSwitch(false,tp,property);

        /*if (tp->s == ISS_ON)
            onItem = i;

        lp = new INDI_E(this, dataProp);

        lp->buildMenuItem(tp);

        oneOption = i18nc(libindi_strings_context, lp->getLabel().toUtf8());

        if (oneOption == "(I18N_EMPTY_MESSAGE)")
            oneOption =  lp->getLabel().toUtf8();

        menuOptions.append(oneOption);

        elementList.append(lp);*/
    }
}

void ClientManagerLite::buildSwitchGUI(INDI::Property * property, PGui guiType) {
    ISwitchVectorProperty *svp = property->getSwitch();
    bool exclusive = false;

    if (svp == NULL)
        return;

    if (guiType == PG_BUTTONS)
    {
        if (svp->r == ISR_1OFMANY)
            exclusive = true;
        else
            exclusive = false;
    }
    else if (guiType == PG_RADIO)
        exclusive = false;

    /*if (svp->p != IP_RO)
        QObject::connect(groupB, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(newSwitch(QAbstractButton *)));*/

    for (int i=0; i < svp->nsp; i++)
    {
        ISwitch *sp = &(svp->sp[i]);
        buildSwitch(true, sp, property, exclusive, guiType);
    }
}

void ClientManagerLite::buildSwitch(bool buttonGroup, ISwitch *sw, INDI::Property * property, bool exclusive, PGui guiType) {
    QString name  = sw->name;
    QString label = sw->label;//i18nc(libindi_strings_context, sw->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = sw->label;

    if (label.isEmpty())
        label = sw->name;
    //label = i18nc(libindi_strings_context, sw->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = sw->name;

    if (!buttonGroup) {
        bool isSelected = false;
        if(sw->s == ISS_ON) isSelected = true;
        emit createINDIMenu(property->getDeviceName(),property->getName(), label, sw->name, isSelected);
        return;
    }

    bool enabled = true;

    if (sw->svp->p == IP_RO)
        enabled = (sw->s == ISS_ON);

    switch (guiType)
    {
    case PG_BUTTONS:
        emit createINDIButton(property->getDeviceName(),property->getName(),label,name,true,true,exclusive,sw->s == ISS_ON, enabled);
        break;

    case PG_RADIO:
        emit createINDIRadio(property->getDeviceName(),property->getName(),label,name,true,true,exclusive, sw->s == ISS_ON, enabled);
        /*check_w = new QCheckBox(label, guiProp->getGroup()->getContainer());
        groupB->addButton(check_w);

        syncSwitch();

        guiProp->addWidget(check_w);

        check_w->show();

        if (sw->svp->p == IP_RO)
            check_w->setEnabled(sw->s == ISS_ON);

        break;*/

    default:
        break;

    }
}

void ClientManagerLite::sendNewINDISwitch(QString deviceName, QString propName, QString name) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        INDI::BaseDevice *device = devInfo->device;
        if(device->getDeviceName() == deviceName) {
            INDI::Property *property = device->getProperty(propName.toStdString().c_str());
            if(property) {

                ISwitchVectorProperty *svp = property->getSwitch();

                if (svp == NULL)
                    return;

                ISwitch *sp = IUFindSwitch(svp, name.toLatin1().constData());

                if (sp == NULL)
                    return;

                if (svp->r == ISR_1OFMANY)
                {
                    IUResetSwitch(svp);
                    sp->s  = ISS_ON;
                }
                else
                {
                    if (svp->r == ISR_ATMOST1)
                    {
                        ISState prev_state = sp->s;
                        IUResetSwitch(svp);
                        sp->s = prev_state;
                    }

                    sp->s = (sp->s == ISS_ON) ? ISS_OFF : ISS_ON;
                }
                sendNewSwitch(svp);
            }
        }
    }
}

void ClientManagerLite::sendNewINDISwitch(QString deviceName, QString propName, int index) {
    if(index >= 0) {
        foreach(DeviceInfoLite *devInfo, m_devices) {
            INDI::BaseDevice *device = devInfo->device;
            if(device->getDeviceName() == deviceName) {
                INDI::Property *property = device->getProperty(propName.toStdString().c_str());
                if(property) {

                    ISwitchVectorProperty *svp = property->getSwitch();

                    if (svp == NULL)
                        return;

                    if (index >= svp->nsp)
                        return;

                    ISwitch *sp = &(svp->sp[index]);

                    IUResetSwitch(svp);
                    sp->s = ISS_ON;

                    sendNewSwitch(svp);
                }
            }
        }
    }
}

bool ClientManagerLite::isDeviceConnected(QString deviceName) {
    INDI::BaseDevice * device = getDevice(deviceName.toStdString().c_str());
    if(device != NULL) {
        return device->isConnected();
    }
    return false;
}

void ClientManagerLite::newDevice(INDI::BaseDevice *dp)
{
    setBLOBMode(B_ALSO, dp->getDeviceName());

    QString deviceName = dp->getDeviceName();

    if (deviceName.isEmpty())
    {
        qWarning() << "Received invalid device with empty name! Ignoring the device...";
        return;
    }

    if (Options::verboseLogging())
        qDebug() << "Received new device " << deviceName;
    emit newINDIDevice(deviceName);


    DeviceInfoLite *devInfo = new DeviceInfoLite(dp);
    //Think about it!
    //devInfo->telescope = new TelescopeLite(dp);
    m_devices.append(devInfo);
}

void ClientManagerLite::removeDevice(BaseDevice *dp) {
    emit removeINDIDevice(QString(dp->getDeviceName()));
}

void ClientManagerLite::newProperty(INDI::Property *property)
{
    QString deviceName = property->getDeviceName();
    QString name = property->getName();
    QString groupName = property->getGroupName();
    QString type = QString(property->getType());
    QString label = property->getLabel();

    DeviceInfoLite *devInfo = NULL;

    foreach(DeviceInfoLite *di, m_devices) {
        if(di->device->getDeviceName() == deviceName) {
            devInfo = di;
        }
    }

    if(devInfo) {
        if ((!strcmp(property->getName(), "EQUATORIAL_EOD_COORD") ||
             !strcmp(property->getName(), "HORIZONTAL_COORD")) ) {
            devInfo->telescope = new TelescopeLite(devInfo->device);
            emit telescopeAdded(devInfo->telescope);
        }
    }

    emit newINDIProperty(deviceName, name, groupName, type, label);
    PGui guiType;
    switch (property->getType())
    {
    case INDI_SWITCH:
        if (property->getSwitch()->r == ISR_NOFMANY)
            guiType = PG_RADIO;
        else if (property->getSwitch()->nsp > 4)
            guiType = PG_MENU;
        else
            guiType = PG_BUTTONS;

        if (guiType == PG_MENU)
            buildMenuGUI(property);
        else
            buildSwitchGUI(property, guiType);
        break;

    case INDI_TEXT:
        buildTextGUI(property);
        break;
    case INDI_NUMBER:
        buildNumberGUI(property);
        break;

    case INDI_LIGHT:
        //buildLightGUI();
        break;

    case INDI_BLOB:
        //buildBLOBGUI();
        break;

    default:
        break;
    }
}

void ClientManagerLite::removeProperty(INDI::Property *property) {
    emit removeINDIProperty(property->getGroupName(),property->getName());
}

void ClientManagerLite::newSwitch(ISwitchVectorProperty *svp) {
    for(int i = 0; i < svp->nsp; ++i) {
        ISwitch *sw = &(svp->sp[i]);
        if(QString(sw->name) == QString("CONNECT")) {
            emit deviceConnected(svp->device, sw->s == ISS_ON);
        }
        if(sw != NULL) {
            emit newINDISwitch(svp->device, svp->name, sw->name, sw->s == ISS_ON);
        }
    }
}

void ClientManagerLite::newNumber(INumberVectorProperty *nvp)
{
    if ((!strcmp(nvp->name, "EQUATORIAL_EOD_COORD") ||
         !strcmp(nvp->name, "HORIZONTAL_COORD")) ) {
        KStarsLite::Instance()->map()->update(); // Update SkyMap if position of telescope is changed
    }
}

void ClientManagerLite::serverDisconnected(int exit_code) {
    setConnected(false);
}
