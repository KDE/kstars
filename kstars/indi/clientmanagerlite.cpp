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
#include <QDebug>
#include <QImageReader>
#include <QTemporaryFile>
#include "kspaths.h"
#include "fitsviewer/fitsdata.h"
#include "kstarslite/imageprovider.h"
#include <QProcess>
#include <QFileDialog>
#include "kspaths.h"
#include <QApplication>

const char *libindi_strings_context = "string from libindi, used in the config dialog";

#ifdef Q_OS_ANDROID
#include "libraw/libraw.h"
#endif

DeviceInfoLite::DeviceInfoLite(INDI::BaseDevice *dev)
    :device(dev), telescope(nullptr)
{

}

DeviceInfoLite::~DeviceInfoLite() {
    delete telescope;
}

ClientManagerLite::ClientManagerLite()
    :m_connected(false)
{
#ifdef ANDROID
    defaultImageType = ".jpeg";
    defaultImagesLocation = KSPaths::writableLocation(QStandardPaths::PicturesLocation);
#endif
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
            //Update last used server and port
            setLastUsedServer(ip);
            setLastUsedPort(port);

            return true;
        }
    }
    return false;
}

void ClientManagerLite::disconnectHost() {
    disconnectServer();
    clearDevices();
    setConnectedHost("");
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

QString ClientManagerLite::syncLED(QString device, QString property, QString name) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        if(devInfo->device->getDeviceName() == device) {
            INDI::Property *prop = devInfo->device->getProperty(property.toLatin1());
            if(prop) {
                IPState state = prop->getState();
                if(!name.isEmpty()) {
                    ILight *lights = prop->getLight()->lp;
                    for (int i=0; i < prop->getLight()->nlp; i++) {
                        if(lights[i].name == name) {
                            state = lights[i].s;
                            break;
                        }
                        if(i == prop->getLight()->nlp - 1) return ""; // no Light with name "name" found so return empty string
                    }

                }
                switch (state)
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
            emit createINDIText(property->getDeviceName(), property->getName(), label, name, text, read, write);
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
            emit createINDINumber(property->getDeviceName(), property->getName(), label, name, text, read, write, scale);
        }
    }
}

void ClientManagerLite::buildMenuGUI(INDI::Property * property) {
    /*QStringList menuOptions;
    QString oneOption;
    int onItem=-1;*/
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

void ClientManagerLite::buildLightGUI(INDI::Property *property) {
    ILightVectorProperty *lvp = property->getLight();

    if (lvp == NULL)
        return;

    for (int i=0; i < lvp->nlp; i++)
    {
        ILight *ilp = &(lvp->lp[i]);

        QString name  = ilp->name;
        QString label = i18nc(libindi_strings_context, ilp->label);

        if (label == "(I18N_EMPTY_MESSAGE)")
            label = ilp->label;

        if (label.isEmpty())
            label = i18nc(libindi_strings_context, ilp->name);

        if (label == "(I18N_EMPTY_MESSAGE)")
            label = ilp->name;

        emit createINDILight(property->getDeviceName(), property->getName(), label, name);
    }
}

/*void ClientManagerLite::buildBLOBGUI(INDI::Property *property) {
    IBLOBVectorProperty *ibp = property->getBLOB();

    QString name  = ibp->name;
    QString label = i18nc(libindi_strings_context, ibp->label);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ibp->label;

    if (label.isEmpty())
        label = i18nc(libindi_strings_context, ibp->name);

    if (label == "(I18N_EMPTY_MESSAGE)")
        label = ibp->name;

    text = i18n("INDI DATA STREAM");

    switch (property->getPermission())
    {
    case IP_RW:
        setupElementRead(ELEMENT_READ_WIDTH);
        setupElementWrite(ELEMENT_WRITE_WIDTH);
        setupBrowseButton();
        break;

    case IP_RO:
        setupElementRead(ELEMENT_FULL_WIDTH);
        break;

    case IP_WO:
        setupElementWrite(ELEMENT_FULL_WIDTH);
        setupBrowseButton();
        break;
    }

    guiProp->addLayout(EHBox);
}*/

void ClientManagerLite::sendNewINDISwitch(QString deviceName, QString propName, QString name) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        INDI::BaseDevice *device = devInfo->device;
        if(device->getDeviceName() == deviceName) {
            INDI::Property *property = device->getProperty(propName.toLatin1());
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

void ClientManagerLite::sendNewINDINumber(const QString& deviceName, const QString& propName, const QString &numberName, double value) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        INDI::BaseDevice *device = devInfo->device;
        if(device->getDeviceName() == deviceName) {
            INumberVectorProperty *np = device->getNumber(propName.toLatin1());
            if (np)
            {
                INumber *n = IUFindNumber(np, numberName.toLatin1());
                if (n)
                {
                    n->value = value;
                    sendNewNumber(np);
                    return;
                }

                qDebug() << "Could not find property: " << deviceName << "." << propName << "." << numberName;
                return;
            }

            qDebug() << "Could not find property: " << deviceName << "." << propName << "." << numberName;
            return;
        }
    }
}

void ClientManagerLite::sendNewINDIText(const QString& deviceName, const QString& propName, const QString &fieldName, const QString &text) {
    foreach(DeviceInfoLite *devInfo, m_devices) {
        INDI::BaseDevice *device = devInfo->device;
        if(device->getDeviceName() == deviceName) {
            ITextVectorProperty *tp = device->getText(propName.toLatin1());
            if (tp)
            {
                IText *t = IUFindText(tp, fieldName.toLatin1());
                if (t)
                {
                    IUSaveText(t, text.toLatin1().data());
                    sendNewText(tp);
                    return;
                }

                qDebug() << "Could not find property: " << deviceName << "." << propName << "." << fieldName;
                return;
            }

            qDebug() << "Could not find property: " << deviceName << "." << propName << "." << fieldName;
            return;
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

bool ClientManagerLite::saveDisplayImage() {
    QString dateTime = QDateTime::currentDateTime().toString("dd-MM-yyyy-hh-mm-ss");
    QString fileEnding = "kstars-lite-" + dateTime;
    //QString filename = KSPaths::writableLocation(QStandardPaths::PicturesLocation);
    //#ifndef ANDROID
    QString filename = QFileDialog::getSaveFileName(QApplication::activeWindow(), i18n("Save Image"),
                                                    KSPaths::writableLocation(QStandardPaths::PicturesLocation) + "/" + fileEnding + ".jpeg",
                                                    i18n("JPEG (*.jpeg);;JPG (*.jpg);;PNG (*.png);;BMP (*.bmp)" ));
    //#else
    /*if(imageType.isEmpty() || !(imageType != ".jpeg" || imageType != ".jpg" || imageType != ".png" || imageType != ".bmp")) {
            QString warning = imageType + " is a wrong image type. Switching to \"" + defaultImageType + "\"";
            qDebug() << warning;
            emit newINDIMessage(warning);
            imageType = defaultImageType;
    }*/
    //  QString filename(defaultImagesLocation + "/" + fileEnding + defaultImageType);
    //#endif
    if(!filename.isEmpty()) {
        if(displayImage.save(filename)) {
            emit newINDIMessage("File " + filename + " was successfully saved");
            return true;
        }
    }
    emit newINDIMessage("Couldn't save file " + filename);
    return false;
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
        buildLightGUI(property);
        break;

    case INDI_BLOB:
        //buildBLOBGUI();
        break;

    default:
        break;
    }
}

void ClientManagerLite::removeProperty(INDI::Property *property) {
    emit removeINDIProperty(property->getDeviceName(), property->getGroupName(),property->getName());

    DeviceInfoLite *devInfo = nullptr;
    foreach(DeviceInfoLite *di, m_devices) {
        if(di->device == property->getBaseDevice()) {
            devInfo = di;
        }
    }

    if(devInfo) {
        if ((!strcmp(property->getName(), "EQUATORIAL_EOD_COORD") ||
             !strcmp(property->getName(), "HORIZONTAL_COORD")) ) {
            if(devInfo->telescope) {
                emit telescopeRemoved(devInfo->telescope);
            }
            KStarsLite::Instance()->map()->update(); // Update SkyMap if position of telescope is changed
        }
    }
}

void ClientManagerLite::newBLOB(IBLOB *bp) {
    processBLOBasCCD(bp);
    emit newLEDState(bp->bvp->device , bp->name);
}

bool ClientManagerLite::processBLOBasCCD(IBLOB *bp) {
    enum blobType { BLOB_IMAGE, BLOB_FITS, BLOB_CR2, BLOB_OTHER} BType;

    BType = BLOB_OTHER;

    QString format(bp->format);
    QString deviceName = bp->bvp->device;

    QByteArray fmt = QString(bp->format).toLower().remove(".").toUtf8();

    // If it's not FITS or an image, don't process it.
    if ( (QImageReader::supportedImageFormats().contains(fmt)))
        BType = BLOB_IMAGE;
    else if (format.contains("fits"))
        BType = BLOB_FITS;
    else if (format.contains("cr2"))
        BType = BLOB_CR2;

    if (BType == BLOB_OTHER)
    {
        return false;
    }

    QString currentDir = KSPaths::writableLocation(QStandardPaths::TempLocation);

    int nr, n=0;
    QTemporaryFile tmpFile(QDir::tempPath() + "/fitsXXXXXX");

    if (currentDir.endsWith('/'))
        currentDir.chop(1);

    if (QDir(currentDir).exists() == false)
        QDir().mkpath(currentDir);

    QString filename(currentDir + '/');

    if(true) {
        tmpFile.setAutoRemove(false);

        if (!tmpFile.open())
        {
            qDebug() << "ISD:CCD Error: Unable to open " << filename << endl;
            //emit BLOBUpdated(NULL);
            return false;
        }

        QDataStream out(&tmpFile);

        for (nr=0; nr < (int) bp->size; nr += n)
            n = out.writeRawData( static_cast<char *> (bp->blob) + nr, bp->size - nr);

        tmpFile.close();

        filename = tmpFile.fileName();
    } else {
        //Add support for batch mode
    }

    strncpy(BLOBFilename, filename.toLatin1(), MAXINDIFILENAME);
    bp->aux2 = BLOBFilename;

    /* Test images
    BType = BLOB_IMAGE;
    filename = "/home/polaris/Pictures/351181_0.jpeg";
    */
    /*Test CR2
    BType = BLOB_CR2;

    filename = "/home/polaris/test.CR2";
    filename = "/storage/emulated/0/test.CR2";*/

    if (BType == BLOB_IMAGE || BType == BLOB_CR2)
    {
        if (BType == BLOB_CR2)
        {
#ifdef Q_OS_ANDROID
            LibRaw RawProcessor;
#define OUT RawProcessor.imgdata.params
            OUT.user_qual = 0; // -q
            OUT.use_camera_wb = 1; // -w
            OUT.highlight = 5; // -H
            OUT.bright = 8; // -b

            QString rawFileName = filename;
            rawFileName = rawFileName.remove(0, rawFileName.lastIndexOf(QLatin1Literal("/")));
            QString templateName = QString("%1/%2.XXXXXX").arg(QDir::tempPath()).arg(rawFileName);
            QTemporaryFile jpgPreview(templateName);
            jpgPreview.setAutoRemove(false);
            jpgPreview.open();
            jpgPreview.close();
            QString jpeg_filename = jpgPreview.fileName();

            RawProcessor.open_file(filename.toLatin1());
            RawProcessor.unpack();
            RawProcessor.dcraw_process();
            RawProcessor.dcraw_ppm_tiff_writer(jpeg_filename.toLatin1());
            QFile::remove(filename);
            filename = jpeg_filename;
#else
            if (QStandardPaths::findExecutable("dcraw").isEmpty() == false && QStandardPaths::findExecutable("cjpeg").isEmpty() == false)
            {
                QProcess dcraw;
                QString rawFileName = filename;
                rawFileName = rawFileName.remove(0, rawFileName.lastIndexOf(QLatin1Literal("/")));
                QString templateName = QString("%1/%2.XXXXXX").arg(QDir::tempPath()).arg(rawFileName);
                QTemporaryFile jpgPreview(templateName);
                jpgPreview.setAutoRemove(false);
                jpgPreview.open();
                jpgPreview.close();
                QString jpeg_filename = jpgPreview.fileName();

                QString cmd = QString("/bin/sh -c \"dcraw -c -q 0 -w -H 5 -b 8 %1 | cjpeg -quality 80 > %2\"").arg(filename).arg(jpeg_filename);
                dcraw.start(cmd);
                dcraw.waitForFinished();
                QFile::remove(filename); //Delete raw
                filename = jpeg_filename;
            }
            else
            {
                emit newINDIMessage(i18n("Unable to find dcraw and cjpeg. Please install the required tools to convert CR2 to JPEG."));
                emit newINDIBLOBImage(deviceName, false);
                return false;
            }
#endif
        }

        displayImage.load(filename);
        QFile::remove(filename);
        KStarsLite::Instance()->imageProvider()->addImage("ccdPreview", displayImage);
        emit newINDIBLOBImage(deviceName, true);
        return true;

    } else if (BType == BLOB_FITS) {
        displayImage = FITSData::FITSToImage(filename);
        QFile::remove(filename);
        KStarsLite::Instance()->imageProvider()->addImage("ccdPreview", displayImage);
        emit newINDIBLOBImage(deviceName, true);
        return true;
    }
    emit newINDIBLOBImage(deviceName, false);
    return false;
}

void ClientManagerLite::newSwitch(ISwitchVectorProperty *svp) {
    for(int i = 0; i < svp->nsp; ++i) {
        ISwitch *sw = &(svp->sp[i]);
        if(QString(sw->name) == QString("CONNECT")) {
            emit deviceConnected(svp->device, sw->s == ISS_ON);
        }
        if(sw != NULL) {
            emit newINDISwitch(svp->device, svp->name, sw->name, sw->s == ISS_ON);
            emit newLEDState(svp->device, svp->name);
        }
    }
}

void ClientManagerLite::newNumber(INumberVectorProperty *nvp)
{
    if ((!strcmp(nvp->name, "EQUATORIAL_EOD_COORD") ||
         !strcmp(nvp->name, "HORIZONTAL_COORD")) ) {
        KStarsLite::Instance()->map()->update(); // Update SkyMap if position of telescope is changed
    }

    QString deviceName = nvp->device;
    QString propName = nvp->name;
    for (int i = 0; i < nvp->nnp; ++i) {
        INumber num = nvp->np[i];
        char buf[MAXINDIFORMAT];
        numberFormat(buf, num.format, num.value);
        QString numberName = num.name;

        emit newINDINumber(deviceName, propName, numberName, QString(buf).trimmed());
        emit newLEDState(deviceName, propName);
    }
}

void ClientManagerLite::newText(ITextVectorProperty *tvp) {
    QString deviceName = tvp->device;
    QString propName = tvp->name;
    for (int i = 0; i < tvp->ntp; ++i) {
        IText text = tvp->tp[i];
        QString fieldName = text.name;

        emit newINDIText(deviceName, propName, fieldName, text.text);
        emit newLEDState(deviceName, propName);
    }
}

void ClientManagerLite::newLight(ILightVectorProperty *lvp) {
    emit newINDILight(lvp->device, lvp->name);
    emit newLEDState(lvp->device, lvp->name);
}

void ClientManagerLite::newMessage(INDI::BaseDevice *dp, int messageID) {
    emit newINDIMessage(QString::fromStdString(dp->messageQueue(messageID)));
}

void ClientManagerLite::serverDisconnected(int exit_code) {
    Q_UNUSED(exit_code)
    clearDevices();
    setConnected(false);
}

void ClientManagerLite::clearDevices() {
    //Delete all created devices
    foreach(DeviceInfoLite *devInfo, m_devices) {
        if(devInfo->telescope) {
            emit telescopeRemoved(devInfo->telescope);
        }
        delete devInfo;
    }
    m_devices.clear();
}

QString ClientManagerLite::getLastUsedServer() {
    return Options::lastServer();
}

void ClientManagerLite::setLastUsedServer(QString server) {
    if(getLastUsedServer() != server) {
        Options::setLastServer(server);
        lastUsedServerChanged();
    }
}

int ClientManagerLite::getLastUsedPort() {
    return Options::lastServerPort();
}

void ClientManagerLite::setLastUsedPort(int port) {
    if(getLastUsedPort() != port) {
        Options::setLastServerPort(port);
        lastUsedPortChanged();
    }
}
