/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later

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

#include <indicom.h>
#include <QImageReader>
#include <QStatusBar>

namespace ISD
{

GDSetCommand::GDSetCommand(INDI_PROPERTY_TYPE inPropertyType, const QString &inProperty, const QString &inElement,
                           QVariant qValue, QObject *parent)
    : QObject(parent), propType(inPropertyType), indiProperty((inProperty)), indiElement(inElement), elementValue(qValue)
{
}

GenericDevice::GenericDevice(DeviceInfo &idv, ClientManager *cm)
{
    deviceInfo    = &idv;
    driverInfo    = idv.getDriverInfo();
    baseDevice    = idv.getBaseDevice();
    clientManager = cm;

    Q_ASSERT_X(baseDevice, __FUNCTION__, "Base device is invalid.");
    Q_ASSERT_X(clientManager, __FUNCTION__, "Client manager is invalid.");

    m_Name        = baseDevice->getDeviceName();

    dType = KSTARS_UNKNOWN;

    registerDBusType();

    // JM 2020-09-05: In case KStars time change, update driver time if applicable.
    connect(KStarsData::Instance()->clock(), &SimClock::timeChanged, this, [this]()
    {
        if (Options::useTimeUpdate() && Options::useKStarsSource())
        {
            if (dType != KSTARS_UNKNOWN && baseDevice != nullptr && baseDevice->isConnected())
            {
                auto tvp = baseDevice->getText("TIME_UTC");
                if (tvp && tvp->getPermission() != IP_RO)
                    updateTime();
            }
        }
    });
}

GenericDevice::~GenericDevice()
{
    for (auto &metadata : streamFileMetadata)
        metadata.file->close();
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

const QString &GenericDevice::getDeviceName() const
{
    return m_Name;
}

void GenericDevice::registerProperty(INDI::Property prop)
{
    if (!prop->getRegistered())
        return;

    const QString name = prop->getName();
    //    if (properties.contains(name))
    //        return;

    //    properties[name] = prop;

    emit propertyDefined(prop);

    // In case driver already started
    if (name == "CONNECTION")
    {
        auto svp = prop->getSwitch();

        if (!svp)
            return;

        // Still connecting/disconnecting...
        if (svp->getState() == IPS_BUSY)
            return;

        auto conSP = svp->findWidgetByName("CONNECT");

        if (!conSP)
            return;

        if (svp->getState() == IPS_OK && conSP->getState() == ISS_ON)
        {
            connected = true;
            emit Connected();
            createDeviceInit();
        }
    }
    else if (name == "DRIVER_INFO")
    {
        auto tvp = prop->getText();
        if (tvp)
        {
            auto tp = tvp->findWidgetByName("DRIVER_INTERFACE");
            if (tp)
            {
                driverInterface = static_cast<uint32_t>(atoi(tp->getText()));
                emit interfaceDefined();
            }

            tp = tvp->findWidgetByName("DRIVER_VERSION");
            if (tp)
            {
                driverVersion = QString(tp->getText());
            }
        }
    }
    else if (name == "SYSTEM_PORTS")
    {
        // Check if our current port is set to one of the system ports. This indicates that the port
        // is not mapped yet to a permenant designation
        auto svp = prop->getSwitch();
        auto port = baseDevice->getText("DEVICE_PORT");
        if (svp && port)
        {
            for (const auto &it: *svp)
            {
                if (it.isNameMatch(port->at(0)->getText()))
                {
                    emit systemPortDetected();
                    break;
                }
            }
        }
    }
    else if (name == "TIME_UTC" && Options::useTimeUpdate() && Options::useKStarsSource())
    {
        const auto &tvp = prop->getText();
        if (tvp && tvp->getPermission() != IP_RO)
            updateTime();
    }
    else if (name == "GEOGRAPHIC_COORD" && Options::useGeographicUpdate() && Options::useKStarsSource())
    {
        const auto &nvp = prop->getNumber();
        if (nvp && nvp->getPermission() != IP_RO)
            updateLocation();
    }
    else if (name == "WATCHDOG_HEARTBEAT")
    {
        const auto &nvp = prop->getNumber();
        if (nvp)
        {
            if (watchDogTimer == nullptr)
            {
                watchDogTimer = new QTimer(this);
                connect(watchDogTimer, SIGNAL(timeout()), this, SLOT(resetWatchdog()));
            }

            if (connected && nvp->at(0)->getValue() > 0)
            {
                // Send immediately a heart beat
                clientManager->sendNewNumber(nvp);
            }
        }
    }
}

void GenericDevice::removeProperty(const QString &name)
{
    emit propertyDeleted(name);
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
                auto nvp = baseDevice->getNumber("WATCHDOG_HEARTBEAT");
                if (nvp && nvp->at(0)->getValue() > 0)
                {
                    // Send immediately
                    clientManager->sendNewNumber(nvp);
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

        qCInfo(KSTARS_INDI) << "Setting location from device:" << deviceName << "Longitude:" << lng.toDMSString() << "Latitude:" <<
                            lat.toDMSString();

        KStars::Instance()->data()->setLocation(*geo);
    }
    else if (!strcmp(nvp->name, "WATCHDOG_HEARTBEAT"))
    {
        if (watchDogTimer == nullptr)
        {
            watchDogTimer = new QTimer(this);
            connect(watchDogTimer, &QTimer::timeout, this, &GenericDevice::resetWatchdog);
        }

        if (connected && nvp->np[0].value > 0)
        {
            // Reset timer 5 seconds before it is due
            // To account for any networking delays
            double nextMS = qMax(100.0, (nvp->np[0].value - 5) * 1000);
            watchDogTimer->start(nextMS);
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

    //    strncpy(BLOBFilename, filename.toLatin1(), MAXINDIFILENAME);
    //    bp->aux2 = BLOBFilename;

    // Text Streaming
    if (dataType == DATA_ASCII)
    {
        // First time, create a data file to hold the stream.

        auto it = std::find_if(streamFileMetadata.begin(), streamFileMetadata.end(), [bp](const StreamFileMetadata & data)
        {
            return (bp->bvp->device == data.device && bp->bvp->name == data.property && bp->name == data.element);
        });

        QFile *streamDatafile = nullptr;

        // New stream data file
        if (it == streamFileMetadata.end())
        {
            StreamFileMetadata metadata;
            metadata.device = bp->bvp->device;
            metadata.property = bp->bvp->name;
            metadata.element = bp->name;

            // Create new file instance
            // Since it's a child of this class, don't worry about deallocating it
            streamDatafile = new QFile(this);
            metadata.file = streamDatafile;

            streamFileMetadata.append(metadata);
        }
        else
            streamDatafile = (*it).file;

        // Try to get

        QDataStream out(streamDatafile);
        for (nr = 0; nr < bp->size; nr += n)
            n = out.writeRawData(static_cast<char *>(bp->blob) + nr, bp->size - nr);

        out.writeRawData((const char *)"\n", 1);
        streamDatafile->flush();

    }
    else
    {
        QFile fits_temp_file(filename);
        if (!fits_temp_file.open(QIODevice::WriteOnly))
        {
            qCCritical(KSTARS_INDI) << "GenericDevice Error: Unable to open " << fits_temp_file.fileName();
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
    auto svp = baseDevice->getSwitch("CONFIG_PROCESS");

    if (!svp)
        return false;

    const char *strConfig = nullptr;

    switch (tConfig)
    {
        case LOAD_LAST_CONFIG:
            strConfig = "CONFIG_LOAD";
            break;

        case SAVE_CONFIG:
            strConfig = "CONFIG_SAVE";
            break;

        case LOAD_DEFAULT_CONFIG:
            strConfig = "CONFIG_DEFAULT";
            break;

        case PURGE_CONFIG:
            strConfig = "CONFIG_PURGE";
            break;
    }

    svp->reset();
    if (strConfig)
    {
        auto sp = svp->findWidgetByName(strConfig);
        if (!sp)
            return false;
        sp->setState(ISS_ON);
    }

    clientManager->sendNewSwitch(svp);

    return true;
}

void GenericDevice::createDeviceInit()
{
    if (Options::showINDIMessages())
        KStars::Instance()->statusBar()->showMessage(i18n("%1 is online.", m_Name), 0);

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
    auto timeUTC = baseDevice->getText("TIME_UTC");

    if (timeUTC)
    {
        auto timeEle = timeUTC->findWidgetByName("UTC");
        if (timeEle)
            timeEle->setText(isoTS.toLatin1().constData());

        auto offsetEle = timeUTC->findWidgetByName("OFFSET");
        if (offsetEle)
            offsetEle->setText(offset.toLatin1().constData());

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

    auto nvp = baseDevice->getNumber("GEOGRAPHIC_COORD");

    if (!nvp)
        return;

    auto np = nvp->findWidgetByName("LONG");

    if (!np)
        return;

    np->setValue(longNP);

    np = nvp->findWidgetByName("LAT");
    if (!np)
        return;

    np->setValue(geo->lat()->Degrees());

    np = nvp->findWidgetByName("ELEV");
    if (!np)
        return;

    np->setValue(geo->elevation());

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
            clientManager->connectDevice(m_Name.toLatin1().constData());
            break;

        case INDI_DISCONNECT:
            clientManager->disconnectDevice(m_Name.toLatin1().constData());
            break;

        case INDI_SET_PORT:
        {
            if (ptr == nullptr)
                return false;

            auto tvp = baseDevice->getText("DEVICE_PORT");

            if (!tvp)
                return false;

            auto tp = tvp->findWidgetByName("PORT");

            tp->setText((static_cast<QString *>(ptr))->toLatin1().constData());

            clientManager->sendNewText(tvp);
        }
        break;

        // We do it here because it could be either CCD or FILTER interfaces, so no need to duplicate code
        case INDI_SET_FILTER:
        {
            if (ptr == nullptr)
                return false;

            auto nvp = baseDevice->getNumber("FILTER_SLOT");

            if (!nvp)
                return false;

            int requestedFilter = *(static_cast<int *>(ptr));

            if (requestedFilter == nvp->np[0].value)
                break;

            nvp->at(0)->setValue(requestedFilter);

            clientManager->sendNewNumber(nvp);
        }
        break;

        // We do it here because it could be either CCD or FILTER interfaces, so no need to duplicate code
        case INDI_SET_FILTER_NAMES:
        {
            if (ptr == nullptr)
                return false;

            auto tvp = baseDevice->getText("FILTER_NAME");

            if (!tvp)
                return false;

            QStringList *requestedFilters = static_cast<QStringList*>(ptr);

            if (requestedFilters->count() != tvp->count())
                return false;

            for (uint8_t i = 0; i < tvp->ntp; i++)
                tvp->at(i)->setText(requestedFilters->at(i).toLatin1().constData());
            clientManager->sendNewText(tvp);
        }
        break;

        case INDI_CONFIRM_FILTER:
        {
            auto svp = baseDevice->getSwitch("CONFIRM_FILTER_SET");
            if (!svp)
                return false;

            svp->at(0)->setState(ISS_ON);
            clientManager->sendNewSwitch(svp);
        }
        break;

        // We do it here because it could be either FOCUSER or ROTATOR interfaces, so no need to duplicate code
        case INDI_SET_ROTATOR_ANGLE:
        {
            if (ptr == nullptr)
                return false;

            auto nvp = baseDevice->getNumber("ABS_ROTATOR_ANGLE");

            if (!nvp)
                return false;

            double requestedAngle = *(static_cast<double *>(ptr));

            if (requestedAngle == nvp->at(0)->getValue())
                break;

            nvp->at(0)->setValue(requestedAngle);

            clientManager->sendNewNumber(nvp);
        }
        break;

        // We do it here because it could be either FOCUSER or ROTATOR interfaces, so no need to duplicate code
        case INDI_SET_ROTATOR_TICKS:
        {
            if (ptr == nullptr)
                return false;

            auto nvp = baseDevice->getNumber("ABS_ROTATOR_POSITION");

            if (!nvp)
                return false;

            int32_t requestedTicks = *(static_cast<int32_t *>(ptr));

            if (requestedTicks == nvp->at(0)->getValue())
                break;

            nvp->at(0)->setValue(requestedTicks);

            clientManager->sendNewNumber(nvp);
        }
        break;

    }

    return true;
}

bool GenericDevice::setProperty(QObject *setPropCommand)
{
    GDSetCommand *indiCommand = static_cast<GDSetCommand *>(setPropCommand);

    //qDebug() << "We are trying to set value for property " << indiCommand->indiProperty << " and element" << indiCommand->indiElement << " and value " << indiCommand->elementValue;

    auto pp = baseDevice->getProperty(indiCommand->indiProperty.toLatin1().constData());

    if (!pp)
        return false;

    switch (indiCommand->propType)
    {
        case INDI_SWITCH:
        {
            auto svp = pp->getSwitch();

            if (!svp)
                return false;

            auto sp = svp->findWidgetByName(indiCommand->indiElement.toLatin1().constData());

            if (!sp)
                return false;

            if (svp->getRule() == ISR_1OFMANY || svp->getRule() == ISR_ATMOST1)
                svp->reset();

            sp->setState(indiCommand->elementValue.toInt() == 0 ? ISS_OFF : ISS_ON);

            //qDebug() << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off");
            clientManager->sendNewSwitch(svp);

            return true;
        }

        case INDI_NUMBER:
        {
            auto nvp = pp->getNumber();

            if (!nvp)
                return false;

            auto np = nvp->findWidgetByName(indiCommand->indiElement.toLatin1().constData());

            if (!np)
                return false;

            double value = indiCommand->elementValue.toDouble();

            if (value == np->getValue())
                return true;

            np->setValue(value);

            //qDebug() << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off");
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
    auto nvp = baseDevice->getNumber(propName.toLatin1());

    if (!nvp)
        return false;

    auto np = nvp->findWidgetByName(elementName.toLatin1());

    if (!np)
        return false;

    *min  = np->getMin();
    *max  = np->getMax();
    *step = np->getStep();

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

INDI::Property GenericDevice::getProperty(const QString &propName)
{
    return baseDevice->getProperty(propName.toLatin1().constData());
}

bool GenericDevice::setJSONProperty(const QString &propName, const QJsonArray &propElements)
{
    for (auto &oneProp : * (baseDevice->getProperties()))
    {
        if (propName == QString(oneProp->getName()))
        {
            switch (oneProp->getType())
            {
                case INDI_SWITCH:
                {
                    auto svp = oneProp->getSwitch();
                    if (svp->getRule() == ISR_1OFMANY || svp->getRule() == ISR_ATMOST1)
                        svp->reset();

                    for (auto oneElement : propElements)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        auto sp = svp->findWidgetByName(oneElementObject["name"].toString().toLatin1().constData());
                        if (sp)
                        {
                            sp->setState(static_cast<ISState>(oneElementObject["state"].toInt()));
                        }
                    }

                    clientManager->sendNewSwitch(svp);
                    return true;
                }

                case INDI_NUMBER:
                {
                    auto nvp = oneProp->getNumber();
                    for (const auto &oneElement : propElements)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        auto np = nvp->findWidgetByName(oneElementObject["name"].toString().toLatin1().constData());
                        if (np)
                        {
                            double newValue = oneElementObject["value"].toDouble(std::numeric_limits<double>::quiet_NaN());
                            if (std::isnan(newValue))
                            {
                                f_scansexa(oneElementObject["value"].toString().toLatin1().constData(), &newValue);
                            }
                            np->setValue(newValue);
                        }
                    }

                    clientManager->sendNewNumber(nvp);
                    return true;
                }

                case INDI_TEXT:
                {
                    auto tvp = oneProp->getText();
                    for (const auto &oneElement : propElements)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        auto tp = tvp->findWidgetByName(oneElementObject["name"].toString().toLatin1().constData());
                        if (tp)
                            tp->setText(oneElementObject["text"].toString().toLatin1().constData());
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

bool GenericDevice::getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact)
{
    for (auto &oneProp : * (baseDevice->getProperties()))
    {
        if (propName == QString(oneProp->getName()))
        {
            switch (oneProp->getType())
            {
                case INDI_SWITCH:
                    propertyToJson(oneProp->getSwitch(), propObject, compact);
                    return true;

                case INDI_NUMBER:
                    propertyToJson(oneProp->getNumber(), propObject, compact);
                    return true;

                case INDI_TEXT:
                    propertyToJson(oneProp->getText(), propObject, compact);
                    return true;

                case INDI_LIGHT:
                    propertyToJson(oneProp->getLight(), propObject, compact);
                    return true;

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

bool GenericDevice::getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject)
{
    auto blobProperty = baseDevice->getProperty(propName.toLatin1().constData());
    if (blobProperty == nullptr)
        return false;

    auto oneBLOB = blobProperty->getBLOB()->findWidgetByName(elementName.toLatin1().constData());
    if (!oneBLOB)
        return false;

    // Now convert to base64 and send back.
    QByteArray data = QByteArray::fromRawData(static_cast<const char *>(oneBLOB->getBlob()), oneBLOB->getBlobLen());

    QString encoded = data.toBase64(QByteArray::Base64UrlEncoding);
    blobObject.insert("property", propName);
    blobObject.insert("element", elementName);
    blobObject.insert("size", encoded.size());
    blobObject.insert("data", encoded);

    return true;
}

void GenericDevice::resetWatchdog()
{
    auto nvp = baseDevice->getNumber("WATCHDOG_HEARTBEAT");

    if (nvp)
        // Send heartbeat to driver
        clientManager->sendNewNumber(nvp);
}

DeviceDecorator::DeviceDecorator(GDInterface * iPtr)
{
    interfacePtr = iPtr;
    baseDevice    = interfacePtr->getBaseDevice();
    clientManager = interfacePtr->getDriverInfo()->getClientManager();
    m_isProxy = iPtr->getType() != KSTARS_UNKNOWN;

    connect(iPtr, &GDInterface::Connected, this, &DeviceDecorator::Connected);
    connect(iPtr, &GDInterface::Disconnected, this, &DeviceDecorator::Disconnected);
    connect(iPtr, &GDInterface::propertyDefined, this, &DeviceDecorator::propertyDefined);
    connect(iPtr, &GDInterface::propertyDeleted, this, &DeviceDecorator::propertyDeleted);
    connect(iPtr, &GDInterface::messageUpdated, this, &DeviceDecorator::messageUpdated);
    connect(iPtr, &GDInterface::switchUpdated, this, &DeviceDecorator::switchUpdated);
    connect(iPtr, &GDInterface::numberUpdated, this, &DeviceDecorator::numberUpdated);
    connect(iPtr, &GDInterface::textUpdated, this, &DeviceDecorator::textUpdated);
    connect(iPtr, &GDInterface::BLOBUpdated, this, &DeviceDecorator::BLOBUpdated);
    connect(iPtr, &GDInterface::lightUpdated, this, &DeviceDecorator::lightUpdated);
}

DeviceDecorator::~DeviceDecorator()
{
    if (m_isProxy == false)
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

void DeviceDecorator::registerProperty(INDI::Property prop)
{
    interfacePtr->registerProperty(prop);
}

void DeviceDecorator::removeProperty(const QString &name)
{
    interfacePtr->removeProperty(name);
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

const QString &DeviceDecorator::getDeviceName() const
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

Properties DeviceDecorator::getProperties()
{
    return interfacePtr->getProperties();
}

INDI::Property DeviceDecorator::getProperty(const QString &propName)
{
    return interfacePtr->getProperty(propName);
}

bool DeviceDecorator::getJSONProperty(const QString &propName, QJsonObject &propObject, bool compact)
{
    return interfacePtr->getJSONProperty(propName, propObject, compact);
}

bool DeviceDecorator::getJSONBLOB(const QString &propName, const QString &elementName, QJsonObject &blobObject)
{
    return interfacePtr->getJSONBLOB(propName, elementName, blobObject);
}

bool DeviceDecorator::setJSONProperty(const QString &propName, const QJsonArray &propElements)
{
    return interfacePtr->setJSONProperty(propName, propElements);
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
    m_Name = baseDevice->getDeviceName();
}

const QString &ST4::getDeviceName()
{
    return m_Name;
}

void ST4::setDECSwap(bool enable)
{
    swapDEC = enable;
}

bool ST4::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    bool raOK  = doPulse(ra_dir, ra_msecs);
    bool decOK = doPulse(dec_dir, dec_msecs);
    return (raOK && decOK);
}

bool ST4::doPulse(GuideDirection dir, int msecs)
{
    auto raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INDI::PropertyView<INumber> *npulse   = nullptr;
    INDI::WidgetView<INumber> *dirPulse   = nullptr;

    if (!raPulse || !decPulse)
        return false;

    if (dir == RA_INC_DIR || dir == RA_DEC_DIR)
    {
        raPulse->at(0)->setValue(0);
        raPulse->at(1)->setValue(0);
    }
    else
    {
        decPulse->at(0)->setValue(0);
        decPulse->at(1)->setValue(0);
    }

    switch (dir)
    {
        case RA_INC_DIR:
            npulse = raPulse;
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_W");
            break;

        case RA_DEC_DIR:
            npulse = raPulse;
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_E");
            break;

        case DEC_INC_DIR:
            npulse = decPulse;
            dirPulse = npulse->findWidgetByName(swapDEC ? "TIMED_GUIDE_S" : "TIMED_GUIDE_N");
            break;

        case DEC_DEC_DIR:
            npulse = decPulse;
            dirPulse = npulse->findWidgetByName(swapDEC ? "TIMED_GUIDE_N" : "TIMED_GUIDE_S");
            break;

        default:
            return false;
    }

    if (!dirPulse)
        return false;

    dirPulse->setValue(msecs);

    clientManager->sendNewNumber(npulse);

    //qDebug() << "Sending pulse for " << npulse->getName() << " in direction " << dirPulse->getName() << " for " << msecs << " ms ";

    return true;
}

void propertyToJson(ISwitchVectorProperty *svp, QJsonObject &propObject, bool compact)
{
    QJsonArray switches;
    for (int i = 0; i < svp->nsp; i++)
    {
        QJsonObject oneSwitch = {{"name", svp->sp[i].name}, {"state", svp->sp[i].s}};
        if (!compact)
            oneSwitch.insert("label", svp->sp[i].label);
        switches.append(oneSwitch);
    }

    propObject = {{"device", svp->device}, {"name", svp->name}, {"state", svp->s}, {"switches", switches}};
    if (!compact)
    {
        propObject.insert("label", svp->label);
        propObject.insert("group", svp->group);
        propObject.insert("perm", svp->p);
        propObject.insert("rule", svp->r);
    }
}

void propertyToJson(INumberVectorProperty *nvp, QJsonObject &propObject, bool compact)
{
    QJsonArray numbers;
    for (int i = 0; i < nvp->nnp; i++)
    {
        QJsonObject oneNumber = {{"name", nvp->np[i].name}, {"value", nvp->np[i].value}};
        if (!compact)
        {
            oneNumber.insert("label", nvp->np[i].label);
            oneNumber.insert("min", nvp->np[i].min);
            oneNumber.insert("max", nvp->np[i].max);
            oneNumber.insert("step", nvp->np[i].step);
            oneNumber.insert("format", nvp->np[i].format);
        }
        numbers.append(oneNumber);
    }

    propObject = {{"device", nvp->device}, {"name", nvp->name}, {"state", nvp->s}, {"numbers", numbers}};
    if (!compact)
    {
        propObject.insert("label", nvp->label);
        propObject.insert("group", nvp->group);
        propObject.insert("perm", nvp->p);
    }
}

void propertyToJson(ITextVectorProperty *tvp, QJsonObject &propObject, bool compact)
{
    QJsonArray Texts;
    for (int i = 0; i < tvp->ntp; i++)
    {
        QJsonObject oneText = {{"name", tvp->tp[i].name}, {"text", tvp->tp[i].text}};
        if (!compact)
        {
            oneText.insert("label", tvp->tp[i].label);
        }
        Texts.append(oneText);
    }

    propObject = {{"device", tvp->device}, {"name", tvp->name}, {"state", tvp->s}, {"texts", Texts}};
    if (!compact)
    {
        propObject.insert("label", tvp->label);
        propObject.insert("group", tvp->group);
        propObject.insert("perm", tvp->p);
    }
}

void propertyToJson(ILightVectorProperty *lvp, QJsonObject &propObject, bool compact)
{
    QJsonArray Lights;
    for (int i = 0; i < lvp->nlp; i++)
    {
        QJsonObject oneLight = {{"name", lvp->lp[i].name}, {"state", lvp->lp[i].s}};
        if (!compact)
        {
            oneLight.insert("label", lvp->lp[i].label);
        }
        Lights.append(oneLight);
    }

    propObject = {{"device", lvp->device}, {"name", lvp->name}, {"state", lvp->s}, {"lights", Lights}};
    if (!compact)
    {
        propObject.insert("label", lvp->label);
        propObject.insert("group", lvp->group);
    }
}

void propertyToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    switch (prop->getType())
    {
        case INDI_SWITCH:
            propertyToJson(prop->getSwitch(), propObject, compact);
            break;
        case INDI_TEXT:
            propertyToJson(prop->getText(), propObject, compact);
            break;
        case INDI_NUMBER:
            propertyToJson(prop->getNumber(), propObject, compact);
            break;
        case INDI_LIGHT:
            propertyToJson(prop->getLight(), propObject, compact);
            break;
        default:
            break;
    }
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
