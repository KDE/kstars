/*  INDI STD
    Copyright (C) 2012 Jasem Mutlaq (mutlaqja@ikarustech.com)

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    Handle INDI Standard properties.
 */

#include "indistd.h"

#include "clientmanager.h"
#include "driverinfo.h"
#include "deviceinfo.h"
#include "imageviewer.h"
#include "indi_debug.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"

#include <QImageReader>
#include <QStatusBar>

namespace ISD
{

GDSetCommand::GDSetCommand(INDI_PROPERTY_TYPE inPropertyType, const QString &inProperty, const QString &inElement,
                           QVariant qValue, QObject *parent)
    : QObject(parent)
{
    propType     = inPropertyType;
    indiProperty = inProperty;
    indiElement  = inElement;
    elementValue = qValue;
}

GenericDevice::GenericDevice(DeviceInfo &idv)
{
    deviceInfo    = &idv;
    driverInfo    = idv.getDriverInfo();
    baseDevice    = idv.getBaseDevice();
    clientManager = driverInfo->getClientManager();

    dType = KSTARS_UNKNOWN;

    registerDBusType();
}

void GenericDevice::registerDBusType()
{
#ifndef KSTARS_LITE
    static bool isRegistered = false;

    if (isRegistered == false)
    {
        qRegisterMetaType<ISD::ParkStatus>("ISD::ParkStatus");
        qDBusRegisterMetaType<ISD::ParkStatus>();
        isRegistered = true;
    }
#endif
}

const char *GenericDevice::getDeviceName()
{
    return baseDevice->getDeviceName();
}

void GenericDevice::registerProperty(INDI::Property *prop)
{
    foreach (INDI::Property *pp, properties)
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

        if (svp == nullptr)
            return;

        // Still connecting/disconnecting...
        if (svp->s == IPS_BUSY)
            return;

        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");

        if (conSP == nullptr)
            return;

        if (svp->s == IPS_OK && conSP->s == ISS_ON)
        {
            connected = true;
            emit Connected();
            createDeviceInit();
        }
    }

    if (!strcmp(prop->getName(), "DRIVER_INFO"))
    {
        ITextVectorProperty *tvp = prop->getText();
        if (tvp)
        {
            IText *tp = IUFindText(tvp, "DRIVER_INTERFACE");
            if (tp)
            {
                driverInterface = static_cast<uint32_t>(atoi(tp->text));
                emit interfaceDefined();
            }

            tp = IUFindText(tvp, "DRIVER_VERSION");
            if (tp)
            {
                driverVersion = QString(tp->text);
            }
        }
    }
    else if (!strcmp(prop->getName(), "SYSTEM_PORTS"))
    {
        // Check if our current port is set to one of the system ports. This indicates that the port
        // is not mapped yet to a permenant designation
        ISwitchVectorProperty *svp = prop->getSwitch();
        ITextVectorProperty *port = baseDevice->getText("DEVICE_PORT");
        if (svp && port)
        {
            for (int i = 0; i < svp->nsp; i++)
            {
                if (!strcmp(port->tp[0].text, svp->sp[i].name))
                {
                    emit systemPortDetected();
                    break;
                }
            }
        }
    }
    else if (!strcmp(prop->getName(), "TIME_UTC") && Options::useTimeUpdate() && Options::useKStarsSource())
    {
        ITextVectorProperty *tvp = prop->getText();
        if (tvp && tvp->p != IP_RO)
            updateTime();
    }
    else if (!strcmp(prop->getName(), "GEOGRAPHIC_COORD") && Options::useGeographicUpdate() && Options::useKStarsSource())
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
            if (watchDogTimer == nullptr)
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

    emit propertyDeleted(prop);
}

void GenericDevice::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");

        if (conSP == nullptr)
            return;

        // Still connecting/disconnecting...
        if (svp->s == IPS_BUSY)
            return;

        if (svp->s == IPS_OK && conSP->s == ISS_ON)
        {
            connected = true;
            emit Connected();
            createDeviceInit();

            if (watchDogTimer != nullptr)
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

    emit switchUpdated(svp);
}

void GenericDevice::processNumber(INumberVectorProperty *nvp)
{
    QString deviceName = getDeviceName();
    //    uint32_t interface = getDriverInterface();
    //    Q_UNUSED(interface);

    if (!strcmp(nvp->name, "GEOGRAPHIC_COORD") && nvp->s == IPS_OK &&
            ( (Options::useMountSource() && (getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)) ||
              (Options::useGPSSource() && (getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE))))
    {
        // Update KStars Location once we receive update from INDI, if the source is set to DEVICE
        dms lng, lat;
        double elev = 0;

        INumber *np = IUFindNumber(nvp, "LONG");
        if (!np)
            return;

        // INDI Longitude convention is 0 to 360. We need to turn it back into 0 to 180 EAST, 0 to -180 WEST
        if (np->value < 180)
            lng.setD(np->value);
        else
            lng.setD(np->value - 360.0);

        np = IUFindNumber(nvp, "LAT");
        if (!np)
            return;

        lat.setD(np->value);

        // Double check we have valid values
        if (lng.Degrees() == 0 && lat.Degrees() == 0)
        {
            qCWarning(KSTARS_INDI) << "Ignoring invalid device coordinates.";
            return;
        }

        np = IUFindNumber(nvp, "ELEV");
        if (np)
            elev = np->value;

        GeoLocation *geo = KStars::Instance()->data()->geo();
        std::unique_ptr<GeoLocation> tempGeo;

        QString newLocationName;
        if (getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE)
            newLocationName = i18n("GPS Location");
        else
            newLocationName = i18n("Mount Location");

        if (geo->name() != newLocationName)
        {
            double TZ0 = geo->TZ0();
            TimeZoneRule *rule = geo->tzrule();
            tempGeo.reset(new GeoLocation(lng, lat, newLocationName, "", "", TZ0, rule, elev));
            geo = tempGeo.get();
        }
        else
        {
            geo->setLong(lng);
            geo->setLat(lat);
        }

        qCInfo(KSTARS_INDI) << "Setting location from device:" << deviceName << "Longitude:" << lng.toDMSString() << "Latitude:" << lat.toDMSString();

        KStars::Instance()->data()->setLocation(*geo);
    }
    else if (!strcmp(nvp->name, "WATCHDOG_HEARTBEAT"))
    {
        if (watchDogTimer == nullptr)
        {
            watchDogTimer = new QTimer(this);
            connect(watchDogTimer, SIGNAL(timeout()), this, SLOT(resetWatchdog()));
        }

        if (connected && nvp->np[0].value > 0)
        {
            // Reset timer 15 seconds before it is due
            watchDogTimer->start(nvp->np[0].value * 60 * 1000 - 15 * 1000);
        }
        else if (nvp->np[0].value == 0)
            watchDogTimer->stop();
    }

    emit numberUpdated(nvp);
}

void GenericDevice::processText(ITextVectorProperty *tvp)
{
    // Update KStars time once we receive update from INDI, if the source is set to DEVICE
    if (!strcmp(tvp->name, "TIME_UTC") && tvp->s == IPS_OK &&
            ( (Options::useMountSource() && (getDriverInterface() & INDI::BaseDevice::TELESCOPE_INTERFACE)) ||
              (Options::useGPSSource() && (getDriverInterface() & INDI::BaseDevice::GPS_INTERFACE))))
    {
        int d, m, y, min, sec, hour;
        float utcOffset;
        QDate indiDate;
        QTime indiTime;

        IText *tp = IUFindText(tvp, "UTC");

        if (!tp)
        {
            qCWarning(KSTARS_INDI) << "UTC property missing from TIME_UTC";
            return;
        }

        sscanf(tp->text, "%d%*[^0-9]%d%*[^0-9]%dT%d%*[^0-9]%d%*[^0-9]%d", &y, &m, &d, &hour, &min, &sec);
        indiDate.setDate(y, m, d);
        indiTime.setHMS(hour, min, sec);

        KStarsDateTime indiDateTime(QDateTime(indiDate, indiTime, Qt::UTC));

        tp = IUFindText(tvp, "OFFSET");

        if (!tp)
        {
            qCWarning(KSTARS_INDI) << "Offset property missing from TIME_UTC";
            return;
        }

        sscanf(tp->text, "%f", &utcOffset);

        qCInfo(KSTARS_INDI) << "Setting UTC time from device:" << getDeviceName() << indiDateTime.toString();

        KStars::Instance()->data()->changeDateTime(indiDateTime);
        KStars::Instance()->data()->syncLST();

        GeoLocation *geo = KStars::Instance()->data()->geo();
        if (geo->tzrule())
            utcOffset -= geo->tzrule()->deltaTZ();

        // TZ0 is the timezone WTIHOUT any DST offsets. Above, we take INDI UTC Offset (with DST already included)
        // and subtract from it the deltaTZ from the current TZ rule.
        geo->setTZ0(utcOffset);
    }

    emit textUpdated(tvp);
}

void GenericDevice::processLight(ILightVectorProperty *lvp)
{
    emit lightUpdated(lvp);
}

void GenericDevice::processMessage(int messageID)
{
    emit messageUpdated(messageID);
}

void GenericDevice::processBLOB(IBLOB *bp)
{
    // Ignore write-only BLOBs since we only receive it for state-change
    if (bp->bvp->p == IP_WO)
        return;

    QFile *data_file = nullptr;
    INDIDataTypes dataType;

    if (!strcmp(bp->format, ".ascii"))
        dataType = DATA_ASCII;
    else
        dataType = DATA_OTHER;

    QString currentDir = Options::fitsDir();
    int nr, n = 0;

    if (currentDir.endsWith('/'))
        currentDir.truncate(sizeof(currentDir) - 1);

    QString filename(currentDir + '/');

    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");

    filename += QString("%1_").arg(bp->label) + ts + QString(bp->format).trimmed();

    strncpy(BLOBFilename, filename.toLatin1(), MAXINDIFILENAME);
    bp->aux2 = BLOBFilename;

    if (dataType == DATA_ASCII)
    {
        if (bp->aux0 == nullptr)
        {
            bp->aux0               = new int();
            QFile *ascii_data_file = new QFile();
            ascii_data_file->setFileName(filename);
            if (!ascii_data_file->open(QIODevice::WriteOnly))
            {
                qCCritical(KSTARS_INDI) << "GenericDevice Error: Unable to open " << ascii_data_file->fileName() << endl;
                return;
            }

            bp->aux1 = ascii_data_file;
        }

        data_file = (QFile *)bp->aux1;

        QDataStream out(data_file);
        for (nr = 0; nr < (int)bp->size; nr += n)
            n = out.writeRawData(static_cast<char *>(bp->blob) + nr, bp->size - nr);

        out.writeRawData((const char *)"\n", 1);
        data_file->flush();
    }
    else
    {
        QFile fits_temp_file(filename);
        if (!fits_temp_file.open(QIODevice::WriteOnly))
        {
            qCCritical(KSTARS_INDI) << "GenericDevice Error: Unable to open " << fits_temp_file.fileName() << endl;
            return;
        }

        QDataStream out(&fits_temp_file);

        for (nr = 0; nr < (int)bp->size; nr += n)
            n = out.writeRawData(static_cast<char *>(bp->blob) + nr, bp->size - nr);

        fits_temp_file.flush();
        fits_temp_file.close();

        QByteArray fmt = QString(bp->format).toLower().remove('.').toUtf8();
        if (QImageReader::supportedImageFormats().contains(fmt))
        {
            QUrl url(filename);
            url.setScheme("file");
            ImageViewer *iv = new ImageViewer(url, QString(), KStars::Instance());
            if (iv)
                iv->show();
        }
    }

    if (dataType == DATA_OTHER)
        KStars::Instance()->statusBar()->showMessage(i18n("Data file saved to %1", filename), 0);

    emit BLOBUpdated(bp);
}

bool GenericDevice::setConfig(INDIConfig tConfig)
{
    ISwitchVectorProperty *svp = baseDevice->getSwitch("CONFIG_PROCESS");

    if (svp == nullptr)
        return false;

    ISwitch *sp = nullptr;

    IUResetSwitch(svp);

    switch (tConfig)
    {
        case LOAD_LAST_CONFIG:
            sp = IUFindSwitch(svp, "CONFIG_LOAD");
            if (sp == nullptr)
                return false;

            IUResetSwitch(svp);
            sp->s = ISS_ON;
            break;

        case SAVE_CONFIG:
            sp = IUFindSwitch(svp, "CONFIG_SAVE");
            if (sp == nullptr)
                return false;

            IUResetSwitch(svp);
            sp->s = ISS_ON;
            break;

        case LOAD_DEFAULT_CONFIG:
            sp = IUFindSwitch(svp, "CONFIG_DEFAULT");
            if (sp == nullptr)
                return false;

            IUResetSwitch(svp);
            sp->s = ISS_ON;
            break;

        case PURGE_CONFIG:
            sp = IUFindSwitch(svp, "CONFIG_PURGE");
            if (sp == nullptr)
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
    if (Options::showINDIMessages())
        KStars::Instance()->statusBar()->showMessage(i18n("%1 is online.", baseDevice->getDeviceName()), 0);

    KStars::Instance()->map()->forceUpdateNow();
}

/*********************************************************************************/
/* Update the Driver's Time							 */
/*********************************************************************************/
void GenericDevice::updateTime()
{
    QString offset, isoTS;

    offset = QString().setNum(KStars::Instance()->data()->geo()->TZ(), 'g', 2);

    //QTime newTime( KStars::Instance()->data()->ut().time());
    //QDate newDate( KStars::Instance()->data()->ut().date());

    //isoTS = QString("%1-%2-%3T%4:%5:%6").arg(newDate.year()).arg(newDate.month()).arg(newDate.day()).arg(newTime.hour()).arg(newTime.minute()).arg(newTime.second());

    isoTS = KStars::Instance()->data()->ut().toString(Qt::ISODate).remove('Z');

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

    if (nvp == nullptr)
        return;

    INumber *np = IUFindNumber(nvp, "LONG");

    if (np == nullptr)
        return;

    np->value = longNP;

    np = IUFindNumber(nvp, "LAT");
    if (np == nullptr)
        return;

    np->value = geo->lat()->Degrees();

    np = IUFindNumber(nvp, "ELEV");
    if (np == nullptr)
        return;

    np->value = geo->elevation();

    clientManager->sendNewNumber(nvp);
}

bool GenericDevice::Connect()
{
    return runCommand(INDI_CONNECT, nullptr);
}

bool GenericDevice::Disconnect()
{
    return runCommand(INDI_DISCONNECT, nullptr);
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
            if (ptr == nullptr)
                return false;

            ITextVectorProperty *tvp = baseDevice->getText("DEVICE_PORT");

            if (tvp == nullptr)
                return false;

            IText *tp = IUFindText(tvp, "PORT");

            IUSaveText(tp, (static_cast<QString *>(ptr))->toLatin1().constData());

            clientManager->sendNewText(tvp);
        }
        break;

        // We do it here because it could be either CCD or FILTER interfaces, so no need to duplicate code
        case INDI_SET_FILTER:
        {
            if (ptr == nullptr)
                return false;

            INumberVectorProperty *nvp = baseDevice->getNumber("FILTER_SLOT");

            if (nvp == nullptr)
                return false;

            int requestedFilter = *((int *)ptr);

            if (requestedFilter == nvp->np[0].value)
                break;

            nvp->np[0].value = requestedFilter;

            clientManager->sendNewNumber(nvp);
        }
        break;

        case INDI_CONFIRM_FILTER:
        {
            ISwitchVectorProperty *svp = baseDevice->getSwitch("CONFIRM_FILTER_SET");
            if (svp == nullptr)
                return false;
            svp->sp[0].s = ISS_ON;
            clientManager->sendNewSwitch(svp);
        }
        break;

        // We do it here because it could be either FOCUSER or ROTATOR interfaces, so no need to duplicate code
        case INDI_SET_ROTATOR_ANGLE:
        {
            if (ptr == nullptr)
                return false;

            INumberVectorProperty *nvp = baseDevice->getNumber("ABS_ROTATOR_ANGLE");

            if (nvp == nullptr)
                return false;

            double requestedAngle = *((double *)ptr);

            if (requestedAngle == nvp->np[0].value)
                break;

            nvp->np[0].value = requestedAngle;

            clientManager->sendNewNumber(nvp);
        }
        break;

        // We do it here because it could be either FOCUSER or ROTATOR interfaces, so no need to duplicate code
        case INDI_SET_ROTATOR_TICKS:
        {
            if (ptr == nullptr)
                return false;

            INumberVectorProperty *nvp = baseDevice->getNumber("ABS_ROTATOR_POSITION");

            if (nvp == nullptr)
                return false;

            int32_t requestedTicks = *((int32_t *)ptr);

            if (requestedTicks == nvp->np[0].value)
                break;

            nvp->np[0].value = requestedTicks;

            clientManager->sendNewNumber(nvp);
        }
        break;

    }

    return true;
}

bool GenericDevice::setProperty(QObject *setPropCommand)
{
    GDSetCommand *indiCommand = static_cast<GDSetCommand *>(setPropCommand);

    //qDebug() << "We are trying to set value for property " << indiCommand->indiProperty << " and element" << indiCommand->indiElement << " and value " << indiCommand->elementValue << endl;

    INDI::Property *pp = baseDevice->getProperty(indiCommand->indiProperty.toLatin1().constData());

    if (pp == nullptr)
        return false;

    switch (indiCommand->propType)
    {
        case INDI_SWITCH:
        {
            ISwitchVectorProperty *svp = pp->getSwitch();

            if (svp == nullptr)
                return false;

            ISwitch *sp = IUFindSwitch(svp, indiCommand->indiElement.toLatin1().constData());

            if (sp == nullptr)
                return false;

            if (svp->r == ISR_1OFMANY || svp->r == ISR_ATMOST1)
                IUResetSwitch(svp);

            sp->s = indiCommand->elementValue.toInt() == 0 ? ISS_OFF : ISS_ON;

            //qDebug() << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off") << endl;
            clientManager->sendNewSwitch(svp);

            return true;
        }

        case INDI_NUMBER:
        {
            INumberVectorProperty *nvp = pp->getNumber();

            if (nvp == nullptr)
                return false;

            INumber *np = IUFindNumber(nvp, indiCommand->indiElement.toLatin1().constData());

            if (np == nullptr)
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

bool GenericDevice::getMinMaxStep(const QString &propName, const QString &elementName, double *min, double *max,
                                  double *step)
{
    INumberVectorProperty *nvp = baseDevice->getNumber(propName.toLatin1());

    if (nvp == nullptr)
        return false;

    INumber *np = IUFindNumber(nvp, elementName.toLatin1());

    if (np == nullptr)
        return false;

    *min  = np->min;
    *max  = np->max;
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

INDI::Property *GenericDevice::getProperty(const QString &propName)
{
    for (auto &oneProp : properties)
    {
        if (propName == QString(oneProp->getName()))
            return oneProp;
    }

    return nullptr;
}

bool GenericDevice::setJSONProperty(const QString &propName, const QJsonArray &propValue)
{
    for (auto &oneProp : properties)
    {
        if (propName == QString(oneProp->getName()))
        {
            switch (oneProp->getType())
            {
                case INDI_SWITCH:
                {
                    ISwitchVectorProperty *svp = oneProp->getSwitch();
                    if (svp->r == ISR_1OFMANY || svp->r == ISR_ATMOST1)
                        IUResetSwitch(svp);

                    for (auto oneElement : propValue)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        ISwitch *sp = IUFindSwitch(svp, oneElementObject["name"].toString().toLatin1().constData());
                        if (sp)
                        {
                            sp->s = static_cast<ISState>(oneElementObject["state"].toInt());
                        }
                    }

                    clientManager->sendNewSwitch(svp);
                    return true;
                }

                case INDI_NUMBER:
                {
                    INumberVectorProperty *nvp = oneProp->getNumber();
                    for (auto oneElement : propValue)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        INumber *np = IUFindNumber(nvp, oneElementObject["name"].toString().toLatin1().constData());
                        if (np)
                            np->value = oneElementObject["value"].toDouble();
                    }

                    clientManager->sendNewNumber(nvp);
                    return true;
                }

                case INDI_TEXT:
                {
                    ITextVectorProperty *tvp = oneProp->getText();
                    for (auto oneElement : propValue)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        IText *tp = IUFindText(tvp, oneElementObject["name"].toString().toLatin1().constData());
                        if (tp)
                            IUSaveText(tp, oneElementObject["text"].toString().toLatin1().constData());
                    }

                    clientManager->sendNewText(tvp);
                    return true;
                }

                case INDI_BLOB:
                    // TODO
                    break;

                default:
                    break;
            }
        }
    }

    return false;
}

QJsonObject GenericDevice::getJSONProperty(const QString &propName, bool compact)
{
    for (auto &oneProp : properties)
    {
        if (propName == QString(oneProp->getName()))
        {
            switch (oneProp->getType())
            {
                case INDI_SWITCH:
                {
                    ISwitchVectorProperty *svp = oneProp->getSwitch();
                    QJsonArray switches;
                    for (int i = 0; i < svp->nsp; i++)
                    {
                        QJsonObject oneSwitch = {{"name", svp->sp[i].name}, {"state", svp->sp[i].s}};
                        if (!compact)
                            oneSwitch.insert("label", svp->sp[i].label);
                        switches.append(oneSwitch);
                    }

                    QJsonObject switchVector = {{"name", svp->name}, {"state", svp->s}, {"switches", switches}};
                    if (!compact)
                    {
                        switchVector.insert("label", svp->label);
                        switchVector.insert("group", svp->group);
                        switchVector.insert("perm", svp->p);
                        switchVector.insert("rule", svp->r);
                    };

                    return switchVector;
                }

                case INDI_NUMBER:
                {
                    INumberVectorProperty *nvp = oneProp->getNumber();
                    QJsonArray numbers;
                    for (int i = 0; i < nvp->nnp; i++)
                    {
                        QJsonObject oneNumber = {{"name", nvp->np[i].name}, {"value", nvp->np[i].value}};
                        if (!compact)
                        {
                            oneNumber.insert("label", nvp->np[i].label);
                            oneNumber.insert("min", nvp->np[i].min);
                            oneNumber.insert("mix", nvp->np[i].max);
                            oneNumber.insert("step", nvp->np[i].step);
                        }
                        numbers.append(oneNumber);
                    }

                    QJsonObject numberVector = {{"name", nvp->name}, {"state", nvp->s}, {"numbers", numbers}};
                    if (!compact)
                    {
                        numberVector.insert("label", nvp->label);
                        numberVector.insert("group", nvp->group);
                        numberVector.insert("perm", nvp->p);
                    };

                    return numberVector;
                }

                case INDI_TEXT:
                {
                    ITextVectorProperty *tvp = oneProp->getText();
                    QJsonArray Texts;
                    for (int i = 0; i < tvp->ntp; i++)
                    {
                        QJsonObject oneText = {{"name", tvp->tp[i].name}, {"value", tvp->tp[i].text}};
                        if (!compact)
                        {
                            oneText.insert("label", tvp->tp[i].label);
                        }
                        Texts.append(oneText);
                    }

                    QJsonObject TextVector = {{"name", tvp->name}, {"state", tvp->s}, {"texts", Texts}};
                    if (!compact)
                    {
                        TextVector.insert("label", tvp->label);
                        TextVector.insert("group", tvp->group);
                        TextVector.insert("perm", tvp->p);
                    };

                    return TextVector;
                }


                case INDI_LIGHT:
                {
                    ILightVectorProperty *tvp = oneProp->getLight();
                    QJsonArray Lights;
                    for (int i = 0; i < tvp->nlp; i++)
                    {
                        QJsonObject oneLight = {{"name", tvp->lp[i].name}, {"state", tvp->lp[i].s}};
                        if (!compact)
                        {
                            oneLight.insert("label", tvp->lp[i].label);
                        }
                        Lights.append(oneLight);
                    }

                    QJsonObject LightVector = {{"name", tvp->name}, {"state", tvp->s}, {"lights", Lights}};
                    if (!compact)
                    {
                        LightVector.insert("label", tvp->label);
                        LightVector.insert("group", tvp->group);
                    };

                    return LightVector;
                }

                case INDI_BLOB:
                    // TODO
                    break;

                default:
                    break;
            }
        }
    }

    return QJsonObject();
}

void GenericDevice::resetWatchdog()
{
    INumberVectorProperty *nvp = baseDevice->getNumber("WATCHDOG_HEARTBEAT");

    if (nvp)
        // Send heartbeat to driver
        clientManager->sendNewNumber(nvp);
}

DeviceDecorator::DeviceDecorator(GDInterface * iPtr)
{
    interfacePtr = iPtr;
    baseDevice    = interfacePtr->getBaseDevice();
    clientManager = interfacePtr->getDriverInfo()->getClientManager();

    connect(iPtr, SIGNAL(Connected()), this, SIGNAL(Connected()));
    connect(iPtr, SIGNAL(Disconnected()), this, SIGNAL(Disconnected()));
    connect(iPtr, SIGNAL(propertyDefined(INDI::Property*)), this, SIGNAL(propertyDefined(INDI::Property*)));
    connect(iPtr, SIGNAL(propertyDeleted(INDI::Property*)), this, SIGNAL(propertyDeleted(INDI::Property*)));
    connect(iPtr, SIGNAL(messageUpdated(int)), this, SIGNAL(messageUpdated(int)));
    connect(iPtr, SIGNAL(switchUpdated(ISwitchVectorProperty*)), this, SIGNAL(switchUpdated(ISwitchVectorProperty*)));
    connect(iPtr, SIGNAL(numberUpdated(INumberVectorProperty*)), this, SIGNAL(numberUpdated(INumberVectorProperty*)));
    connect(iPtr, SIGNAL(textUpdated(ITextVectorProperty*)), this, SIGNAL(textUpdated(ITextVectorProperty*)));
    connect(iPtr, SIGNAL(BLOBUpdated(IBLOB*)), this, SIGNAL(BLOBUpdated(IBLOB*)));
    connect(iPtr, SIGNAL(lightUpdated(ILightVectorProperty*)), this, SIGNAL(lightUpdated(ILightVectorProperty*)));
}

DeviceDecorator::~DeviceDecorator()
{
    delete (interfacePtr);
}

bool DeviceDecorator::runCommand(int command, void *ptr)
{
    return interfacePtr->runCommand(command, ptr);
}

bool DeviceDecorator::setProperty(QObject * setPropCommand)
{
    return interfacePtr->setProperty(setPropCommand);
}

void DeviceDecorator::processBLOB(IBLOB * bp)
{
    interfacePtr->processBLOB(bp);
}

void DeviceDecorator::processLight(ILightVectorProperty * lvp)
{
    interfacePtr->processLight(lvp);
}

void DeviceDecorator::processNumber(INumberVectorProperty * nvp)
{
    interfacePtr->processNumber(nvp);
}

void DeviceDecorator::processSwitch(ISwitchVectorProperty * svp)
{
    interfacePtr->processSwitch(svp);
}

void DeviceDecorator::processText(ITextVectorProperty * tvp)
{
    interfacePtr->processText(tvp);
}

void DeviceDecorator::processMessage(int messageID)
{
    interfacePtr->processMessage(messageID);
}

void DeviceDecorator::registerProperty(INDI::Property * prop)
{
    interfacePtr->registerProperty(prop);
}

void DeviceDecorator::removeProperty(INDI::Property * prop)
{
    interfacePtr->removeProperty(prop);
}

bool DeviceDecorator::setConfig(INDIConfig tConfig)
{
    return interfacePtr->setConfig(tConfig);
}

DeviceFamily DeviceDecorator::getType()
{
    return interfacePtr->getType();
}

DriverInfo *DeviceDecorator::getDriverInfo()
{
    return interfacePtr->getDriverInfo();
}

DeviceInfo *DeviceDecorator::getDeviceInfo()
{
    return interfacePtr->getDeviceInfo();
}

const char *DeviceDecorator::getDeviceName()
{
    return interfacePtr->getDeviceName();
}

INDI::BaseDevice *DeviceDecorator::getBaseDevice()
{
    return interfacePtr->getBaseDevice();
}

uint32_t DeviceDecorator::getDriverInterface()
{
    return interfacePtr->getDriverInterface();
}

QString DeviceDecorator::getDriverVersion()
{
    return interfacePtr->getDriverVersion();
}

QList<INDI::Property *> DeviceDecorator::getProperties()
{
    return interfacePtr->getProperties();
}

INDI::Property *DeviceDecorator::getProperty(const QString &propName)
{
    return interfacePtr->getProperty(propName);
}

QJsonObject DeviceDecorator::getJSONProperty(const QString &propName, bool compact)
{
    return interfacePtr->getJSONProperty(propName, compact);
}

bool DeviceDecorator::setJSONProperty(const QString &propName, const QJsonArray &propValue)
{
    return interfacePtr->setJSONProperty(propName, propValue);
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

bool DeviceDecorator::getMinMaxStep(const QString &propName, const QString &elementName, double * min, double * max,
                                    double * step)
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

ST4::ST4(INDI::BaseDevice * bdv, ClientManager * cm)
{
    baseDevice    = bdv;
    clientManager = cm;
}

const char *ST4::getDeviceName()
{
    return baseDevice->getDeviceName();
}

void ST4::setDECSwap(bool enable)
{
    swapDEC = enable;
}

bool ST4::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    bool raOK = false, decOK = false;
    raOK  = doPulse(ra_dir, ra_msecs);
    decOK = doPulse(dec_dir, dec_msecs);

    if (raOK && decOK)
        return true;
    else
        return false;
}

bool ST4::doPulse(GuideDirection dir, int msecs)
{
    INumberVectorProperty *raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    INumberVectorProperty *decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INumberVectorProperty *npulse   = nullptr;
    INumber *dirPulse               = nullptr;

    if (raPulse == nullptr || decPulse == nullptr)
        return false;

    if (dir == RA_INC_DIR || dir == RA_DEC_DIR)
        raPulse->np[0].value = raPulse->np[1].value = 0;
    else
        decPulse->np[0].value = decPulse->np[1].value = 0;

    switch (dir)
    {
        case RA_INC_DIR:
            dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_W");
            if (dirPulse == nullptr)
                return false;

            npulse = raPulse;
            break;

        case RA_DEC_DIR:
            dirPulse = IUFindNumber(raPulse, "TIMED_GUIDE_E");
            if (dirPulse == nullptr)
                return false;

            npulse = raPulse;
            break;

        case DEC_INC_DIR:
            if (swapDEC)
                dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");
            else
                dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");

            if (dirPulse == nullptr)
                return false;

            npulse = decPulse;
            break;

        case DEC_DEC_DIR:
            if (swapDEC)
                dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_N");
            else
                dirPulse = IUFindNumber(decPulse, "TIMED_GUIDE_S");

            if (dirPulse == nullptr)
                return false;

            npulse = decPulse;
            break;

        default:
            return false;
    }

    dirPulse->value = msecs;

    clientManager->sendNewNumber(npulse);

    //qDebug() << "Sending pulse for " << npulse->name << " in direction " << dirPulse->name << " for " << msecs << " ms " << endl;

    return true;
}
}

#ifndef KSTARS_LITE
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::ParkStatus &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::ParkStatus &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::ParkStatus>(a);
    return argument;
}
#endif
