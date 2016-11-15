/*  INDI STD
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#include <basedevice.h>

#include <QDebug>
#include <QImageReader>
#include <QStatusBar>

#include <KMessageBox>

#include "indistd.h"
#include "indicommon.h"
#include "clientmanager.h"
#include "driverinfo.h"
#include "deviceinfo.h"

#include "imageviewer.h"
#include "skypoint.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "Options.h"

const int MAX_FILENAME_LEN = 1024;

namespace ISD
{

GDSetCommand::GDSetCommand(INDI_PROPERTY_TYPE inPropertyType, const QString &inProperty, const QString &inElement, QVariant qValue, QObject *parent) : QObject(parent)
{
    propType     = inPropertyType;
    indiProperty = inProperty;
    indiElement  = inElement;
    elementValue = qValue;
}


GenericDevice::GenericDevice(DeviceInfo *idv)
{
    connected = false;

    Q_ASSERT(idv != NULL);

    deviceInfo        = idv;
    driverInfo        = idv->getDriverInfo();
    baseDevice        = idv->getBaseDevice();
    clientManager     = driverInfo->getClientManager();

    watchDogTimer     = NULL;

    dType         = KSTARS_UNKNOWN;
}


GenericDevice::~GenericDevice()
{

}

const char * GenericDevice::getDeviceName()
{
    return baseDevice->getDeviceName();
}

void GenericDevice::registerProperty(INDI::Property *prop)
{

    foreach(INDI::Property *pp, properties)
    {
        if (pp == prop)
            return;
    }

    properties.append(prop);

    emit propertyDefined(prop);

    // In case driver already started
    if (!strcmp(prop->getName(), "CONNECTION"))
    {
        ISwitchVectorProperty *svp = prop->getSwitch();
        if (svp == NULL)
            return;
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");
        if (conSP == NULL)
            return;

        if (conSP->s == ISS_ON)
        {
            connected = true;
            emit Connected();
            createDeviceInit();
        }
    }

    if (!strcmp(prop->getName(), "TIME_UTC") && Options::useTimeUpdate() && Options::useComputerSource())
    {
           ITextVectorProperty *tvp = prop->getText();
            if (tvp && tvp->p != IP_RO)
                updateTime();
    }
    else if (!strcmp(prop->getName(), "GEOGRAPHIC_COORD") && Options::useGeographicUpdate() && Options::useComputerSource())
    {
            INumberVectorProperty *nvp = prop->getNumber();
            if (nvp && nvp->p != IP_RO)
                updateLocation();
    }
    else if (!strcmp(prop->getName(), "WATCHDOG_HEARTBEAT"))
    {
        INumberVectorProperty *nvp = prop->getNumber();
        if (nvp)
        {
            if (watchDogTimer == NULL)
            {
                watchDogTimer = new QTimer(this);
                connect(watchDogTimer, SIGNAL(timeout()), this, SLOT(resetWatchdog()));
            }

            if (connected && nvp->np[0].value > 0)
            {
                // Send immediately a heart beat
                clientManager->sendNewNumber(nvp);
                //watchDogTimer->start(0);
            }
        }
    }
}

void GenericDevice::removeProperty(INDI::Property *prop)
{
    properties.removeOne(prop);
}

void GenericDevice::processSwitch(ISwitchVectorProperty * svp)
{

    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");
        if (conSP == NULL)
            return;

        if (conSP->s == ISS_ON)
        {
            connected = true;
            emit Connected();
            createDeviceInit();

            if (watchDogTimer != NULL)
            {
                INumberVectorProperty *nvp = baseDevice->getNumber("WATCHDOG_HEARTBEAT");
                if (nvp && nvp->np[0].value > 0)
                {
                    // Send immediately
                    clientManager->sendNewNumber(nvp);
                    //watchDogTimer->start(0);
                }
            }
        }
        else
        {
            connected = false;
            emit Disconnected();
        }
    }


}

void GenericDevice::processNumber(INumberVectorProperty * nvp)
{
    if (Options::useDeviceSource() && !strcmp(nvp->name, "GEOGRAPHIC_COORD") && nvp->s == IPS_OK)
    {
        // Update KStars Location once we receive update from INDI, if the source is set to DEVICE
        dms lng, lat;
        INumber *np=NULL;

        np = IUFindNumber(nvp, "LONG");
        if (!np)
            return;

        // INDI Longitude convention is 0 to 360. We need to turn it back into 0 to 180 EAST, 0 to -180 WEST
        if (np->value < 180)
            lng.setD(np->value);
        else
            lng.setD(np->value-360.0);

        np = IUFindNumber(nvp, "LAT");
        if (!np)
            return;

        lat.setD(np->value);

        GeoLocation *geo = KStars::Instance()->data()->geo();

        geo->setLong(lng);
        geo->setLat(lat);
        KStars::Instance()->data()->setLocation(*geo);
    }
    else if (!strcmp(nvp->name, "WATCHDOG_HEARTBEAT"))
    {
        if (watchDogTimer == NULL)
        {
            watchDogTimer = new QTimer(this);
            connect(watchDogTimer, SIGNAL(timeout()), this, SLOT(resetWatchdog()));
        }

        if (connected && nvp->np[0].value > 0)
        {
            // Reset timer 15 seconds before it is due
            watchDogTimer->start(nvp->np[0].value*60*1000 - 15*1000);
        }
        else if (nvp->np[0].value == 0)
            watchDogTimer->stop();
    }
}

void GenericDevice::processText(ITextVectorProperty * tvp)
{    

    // Update KStars time once we receive update from INDI, if the source is set to DEVICE
    if (Options::useDeviceSource() && !strcmp(tvp->name, "TIME_UTC") && tvp->s == IPS_OK)
    {
        IText *tp=NULL;
        int d, m, y, min, sec, hour;
        float utcOffset;
        QDate indiDate;
        QTime indiTime;
        KStarsDateTime indiDateTime;

        tp = IUFindText(tvp, "UTC");

        if (!tp)
            return;

        sscanf(tp->text, "%d%*[^0-9]%d%*[^0-9]%dT%d%*[^0-9]%d%*[^0-9]%d", &y, &m, &d, &hour, &min, &sec);
        indiDate.setDate(y, m, d);
        indiTime.setHMS(hour, min, sec);
        indiDateTime.setDate(indiDate);
        indiDateTime.setTime(indiTime);

        tp = IUFindText(tvp, "OFFSET");

        if (!tp)
            return;

        sscanf(tp->text, "%f", &utcOffset);

        KStars::Instance()->data()->changeDateTime(indiDateTime);
        KStars::Instance()->data()->syncLST();

        GeoLocation *geo = KStars::Instance()->data()->geo();
        geo->setTZ(utcOffset);    

    }
}

void GenericDevice::processLight(ILightVectorProperty * lvp)
{
    INDI_UNUSED(lvp);
}

void GenericDevice::processMessage(int messageID)
{
   INDI_UNUSED(messageID);
}

void GenericDevice::processBLOB(IBLOB* bp)
{
    QFile       *data_file = NULL;
    INDIDataTypes dataType;

   if (!strcmp(bp->format, ".ascii"))
        dataType = DATA_ASCII;
    else
        dataType = DATA_OTHER;

    QString currentDir = Options::fitsDir();
    int nr, n=0;

    if (currentDir.endsWith('/'))
        currentDir.truncate(sizeof(currentDir)-1);

    QString filename(currentDir + '/');

    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss");

    filename += QString("file_") + ts +  QString(bp->format).trimmed();

    strncpy(BLOBFilename, filename.toLatin1(), MAXINDIFILENAME);
    bp->aux2 = BLOBFilename;

    if (dataType == DATA_ASCII)
    {
        if (bp->aux0 == NULL)
        {
            bp->aux0 = new int();
            QFile * ascii_data_file = new QFile();
            ascii_data_file->setFileName(filename);
            if (!ascii_data_file->open(QIODevice::WriteOnly))
            {
                        qDebug() << "GenericDevice Error: Unable to open " << ascii_data_file->fileName() << endl;
                        return;
            }

            bp->aux1 = ascii_data_file;
        }

        data_file = (QFile *) bp->aux1;

        QDataStream out(data_file);
        for (nr=0; nr < (int) bp->size; nr += n)
           n = out.writeRawData( static_cast<char *> (bp->blob) + nr, bp->size - nr);

        out.writeRawData( (const char *) "\n" , 1);
        data_file->flush();
   }
    else
    {
        QFile fits_temp_file(filename);
        if (!fits_temp_file.open(QIODevice::WriteOnly))
        {
                qDebug() << "GenericDevice Error: Unable to open " << fits_temp_file.fileName() << endl;
        return;
        }

        QDataStream out(&fits_temp_file);

        for (nr=0; nr < (int) bp->size; nr += n)
            n = out.writeRawData( static_cast<char *> (bp->blob) +nr, bp->size - nr);

        fits_temp_file.close();

        QByteArray fmt = QString(bp->format).toLower().remove(".").toUtf8();
        if (QImageReader::supportedImageFormats().contains(fmt))
        {
            QUrl url (filename);
            url.setScheme("file");
             ImageViewer *iv = new ImageViewer(url, QString(), KStars::Instance());
             if (iv)
                iv->show();
        }
    }

    if (dataType == DATA_OTHER)      
        KStars::Instance()->statusBar()->showMessage(i18n("Data file saved to %1", filename ), 0);

}

bool GenericDevice::setConfig(INDIConfig tConfig)
{

    ISwitchVectorProperty *svp = baseDevice->getSwitch("CONFIG_PROCESS");
    if (svp == NULL)
        return false;

    ISwitch *sp = NULL;

    IUResetSwitch(svp);

    switch (tConfig)
    {
        case LOAD_LAST_CONFIG:
        sp = IUFindSwitch(svp, "CONFIG_LOAD");
        if (sp == NULL)
            return false;

        IUResetSwitch(svp);
        sp->s = ISS_ON;
        break;

        case SAVE_CONFIG:
        sp = IUFindSwitch(svp, "CONFIG_SAVE");
        if (sp == NULL)
            return false;

        IUResetSwitch(svp);
        sp->s = ISS_ON;
        break;

        case LOAD_DEFAULT_CONFIG:
        sp = IUFindSwitch(svp, "CONFIG_DEFAULT");
        if (sp == NULL)
            return false;

        IUResetSwitch(svp);
        sp->s = ISS_ON;
        break;

    }

    clientManager->sendNewSwitch(svp);

    return true;

}

void GenericDevice::createDeviceInit()
{

    if ( Options::showINDIMessages() )
        KStars::Instance()->statusBar()->showMessage( i18n("%1 is online.", baseDevice->getDeviceName()), 0);

    KStars::Instance()->map()->forceUpdateNow();
}

/*********************************************************************************/
/* Update the Driver's Time							 */
/*********************************************************************************/
void GenericDevice::updateTime()
{

    QString offset, isoTS;

    offset = QString().setNum(KStars::Instance()->data()->geo()->TZ(), 'g', 2);

    QTime newTime( KStars::Instance()->data()->ut().time());
    QDate newDate( KStars::Instance()->data()->ut().date());

    isoTS = QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month()).arg(newDate.day()).arg(newTime.hour()).arg(newTime.minute()).arg(newTime.second());

    /* Update Date/Time */
    ITextVectorProperty *timeUTC = baseDevice->getText("TIME_UTC");

    if (timeUTC)
    {

        IText *timeEle = IUFindText(timeUTC, "UTC");
        if (timeEle)
            IUSaveText(timeEle, isoTS.toLatin1().constData());

        IText *offsetEle = IUFindText(timeUTC, "OFFSET");
        if (offsetEle)
            IUSaveText(offsetEle, offset.toLatin1().constData());

        if (timeEle && offsetEle)
            clientManager->sendNewText(timeUTC);
    }
}

/*********************************************************************************/
/* Update the Driver's Geographical Location					 */
/*********************************************************************************/
void GenericDevice::updateLocation()
{
    GeoLocation *geo = KStars::Instance()->data()->geo();
    double longNP;

    if (geo->lng()->Degrees() >= 0)
        longNP = geo->lng()->Degrees();
    else
        longNP = dms(geo->lng()->Degrees() + 360.0).Degrees();

    INumberVectorProperty *nvp = baseDevice->getNumber("GEOGRAPHIC_COORD");

    if (nvp == NULL)
        return;

    INumber *np = IUFindNumber(nvp, "LONG");
    if (np == NULL)
        return;

    np->value = longNP;

    np = IUFindNumber(nvp, "LAT");
    if (np == NULL)
        return;

    np->value = geo->lat()->Degrees();

    clientManager->sendNewNumber(nvp);
}

bool GenericDevice::Connect()
{
    return runCommand(INDI_CONNECT, NULL);
}

bool GenericDevice::Disconnect()
{
    return runCommand(INDI_DISCONNECT, NULL);
}

bool GenericDevice::runCommand(int command, void *ptr)
{
    switch (command)
    {
       case INDI_CONNECT:
        clientManager->connectDevice(baseDevice->getDeviceName());
        break;

      case INDI_DISCONNECT:
        clientManager->disconnectDevice(baseDevice->getDeviceName());
        break;

       case INDI_SET_PORT:
       {
        if (ptr == NULL)
            return false;
        ITextVectorProperty *tvp = baseDevice->getText("DEVICE_PORT");
        if (tvp == NULL)
            return false;

        IText *tp = IUFindText(tvp, "PORT");

        IUSaveText(tp, (static_cast<QString *> (ptr))->toLatin1().constData() );

        clientManager->sendNewText(tvp);
       }
        break;

      // We do it here because it could be either CCD or FILTER interfaces, so no need to duplicate code
      case INDI_SET_FILTER:
      {
        if (ptr == NULL)
            return false;
        INumberVectorProperty *nvp = baseDevice->getNumber("FILTER_SLOT");
        if (nvp == NULL)
            return false;

        int requestedFilter = * ( (int *) ptr);

        if (requestedFilter == nvp->np[0].value)
            break;

        nvp->np[0].value  = requestedFilter;

        clientManager->sendNewNumber(nvp);
      }

        break;

    }

    return true;
}

bool GenericDevice::setProperty(QObject * setPropCommand)
{
    GDSetCommand *indiCommand = static_cast<GDSetCommand *> (setPropCommand);

    //qDebug() << "We are trying to set value for property " << indiCommand->indiProperty << " and element" << indiCommand->indiElement << " and value " << indiCommand->elementValue << endl;

    INDI::Property *pp= baseDevice->getProperty(indiCommand->indiProperty.toLatin1().constData());
    if (pp == NULL)
        return false;

    switch (indiCommand->propType)
    {
        case INDI_SWITCH:
        {
        ISwitchVectorProperty *svp = pp->getSwitch();
        if (svp == NULL)
            return false;

        ISwitch *sp = IUFindSwitch(svp, indiCommand->indiElement.toLatin1().constData());
        if (sp == NULL)
            return false;

        if (svp->r == ISR_1OFMANY || svp->r == ISR_ATMOST1)
            IUResetSwitch(svp);

        sp->s = indiCommand->elementValue.toInt() == 0 ? ISS_OFF : ISS_ON;

        //qDebug() << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off") << endl;
        clientManager->sendNewSwitch(svp);

        return true;
       }
          break;

    case INDI_NUMBER:
    {
        INumberVectorProperty *nvp = pp->getNumber();
        if (nvp == NULL)
            return false;

        INumber *np = IUFindNumber(nvp, indiCommand->indiElement.toLatin1().constData());
        if (np == NULL)
            return false;

        double value = indiCommand->elementValue.toDouble();

        if (value == np->value)
            return true;

        np->value = value;

      //qDebug() << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off") << endl;
      clientManager->sendNewNumber(nvp);
    }
   break;
        // TODO: Add set property for other types of properties
    default:
        break;

    }

    return true;
}

bool GenericDevice::getMinMaxStep(const QString & propName, const QString & elementName, double *min, double *max, double *step)
{
    INumberVectorProperty *nvp = baseDevice->getNumber(propName.toLatin1());

    if (nvp == NULL)
        return false;

    INumber *np = IUFindNumber(nvp, elementName.toLatin1());
    if (np == NULL)
        return false;

    *min = np->min;
    *max = np->max;
    *step = np->step;

    return true;

}

IPState GenericDevice::getState(const QString &propName)
{
    return baseDevice->getPropertyState(propName.toLatin1().constData());
}

IPerm GenericDevice::getPermission(const QString &propName)
{
    return baseDevice->getPropertyPermission(propName.toLatin1().constData());
}

void GenericDevice::resetWatchdog()
{
    INumberVectorProperty *nvp = baseDevice->getNumber("WATCHDOG_HEARTBEAT");

    if (nvp)
       // Send heartbeat to driver
       clientManager->sendNewNumber(nvp);
}

DeviceDecorator::DeviceDecorator(GDInterface *iPtr)
{
    interfacePtr = iPtr;

    baseDevice    = interfacePtr->getBaseDevice();
    clientManager = interfacePtr->getDriverInfo()->getClientManager();
}

DeviceDecorator::~DeviceDecorator()
{
    delete (interfacePtr);
}

bool DeviceDecorator::runCommand(int command, void *ptr)
{
    return interfacePtr->runCommand(command, ptr);
}

bool DeviceDecorator::setProperty(QObject *setPropCommand)
{
    return interfacePtr->setProperty(setPropCommand);
}

void DeviceDecorator::processBLOB(IBLOB *bp)
{
    interfacePtr->processBLOB(bp);
    emit BLOBUpdated(bp);

}

void DeviceDecorator::processLight(ILightVectorProperty *lvp)
{
    interfacePtr->processLight(lvp);
    emit lightUpdated(lvp);

}

void DeviceDecorator::processNumber(INumberVectorProperty *nvp)
{
    interfacePtr->processNumber(nvp);
    emit numberUpdated(nvp);
}

void DeviceDecorator::processSwitch(ISwitchVectorProperty *svp)
{
    interfacePtr->processSwitch(svp);
    emit switchUpdated(svp);
}

void DeviceDecorator::processText(ITextVectorProperty *tvp)
{
    interfacePtr->processText(tvp);
    emit textUpdated(tvp);
}

void DeviceDecorator::processMessage(int messageID)
{
    interfacePtr->processMessage(messageID);
    emit messageUpdated(messageID);
}

void DeviceDecorator::registerProperty(INDI::Property *prop)
{
    interfacePtr->registerProperty(prop);
}

void DeviceDecorator::removeProperty(INDI::Property *prop)
{
    interfacePtr->removeProperty(prop);
    emit propertyDeleted(prop);
}

bool DeviceDecorator::setConfig(INDIConfig tConfig)
{
    return interfacePtr->setConfig(tConfig);
}

DeviceFamily DeviceDecorator::getType()
{
    return interfacePtr->getType();
}

DriverInfo * DeviceDecorator::getDriverInfo()
{
    return interfacePtr->getDriverInfo();
}

DeviceInfo * DeviceDecorator::getDeviceInfo()
{
    return interfacePtr->getDeviceInfo();
}

const char * DeviceDecorator::getDeviceName()
{
    return interfacePtr->getDeviceName();
}

INDI::BaseDevice* DeviceDecorator::getBaseDevice()
{
    return interfacePtr->getBaseDevice();
}

QList<INDI::Property *> DeviceDecorator::getProperties()
{
    return interfacePtr->getProperties();
}

bool DeviceDecorator::isConnected()
{
    return interfacePtr->isConnected();
}

bool DeviceDecorator::Connect()
{
    return interfacePtr->Connect();
}

bool DeviceDecorator::Disconnect()
{
    return interfacePtr->Disconnect();
}


bool DeviceDecorator::getMinMaxStep(const QString & propName, const QString & elementName, double *min, double *max, double *step)
{

    return interfacePtr->getMinMaxStep(propName, elementName, min, max, step);

}

IPState DeviceDecorator::getState(const QString &propName)
{
    return interfacePtr->getState(propName);
}

IPerm DeviceDecorator::getPermission(const QString &propName)
{
    return interfacePtr->getPermission(propName);
}


ST4::ST4(INDI::BaseDevice *bdv, ClientManager *cm)
{
    baseDevice = bdv;
    clientManager = cm;
    swapDEC = false;
}

ST4::~ST4() {}

const char * ST4::getDeviceName()
{
    return baseDevice->getDeviceName();
}

void ST4::setDECSwap(bool enable)
{
    swapDEC = enable;
}

bool ST4::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs )
{
    bool raOK=false, decOK=false;
    raOK  = doPulse(ra_dir, ra_msecs);
    decOK = doPulse(dec_dir, dec_msecs);

    if (raOK && decOK)
        return true;
    else
        return false;

}

bool ST4::doPulse(GuideDirection dir, int msecs )
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INumberVectorProperty *npulse = NULL;
    INumber *dirPulse=NULL;

    if (raPulse == NULL || decPulse == NULL)
        return false;

    if (dir == RA_INC_DIR || dir == RA_DEC_DIR)
        raPulse->np[0].value = raPulse->np[1].value = 0;
    else
        decPulse->np[0].value = decPulse->np[1].value = 0;

    switch(dir)
    {
    case RA_INC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_W");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case RA_DEC_DIR:
    dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_E");
    if (dirPulse == NULL)
        return false;

    npulse = raPulse;
    break;

    case DEC_INC_DIR:
        if (swapDEC)
            dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");
        else
            dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    case DEC_DEC_DIR:
        if (swapDEC)
            dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");
        else
            dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");
    if (dirPulse == NULL)
        return false;

    npulse = decPulse;
    break;

    default:
        return false;

    }

    if (dirPulse == NULL || npulse == NULL)
        return false;

    dirPulse->value = msecs;

    clientManager->sendNewNumber(npulse);

    //qDebug() << "Sending pulse for " << npulse->name << " in direction " << dirPulse->name << " for " << msecs << " ms " << endl;

    return true;

}

}


