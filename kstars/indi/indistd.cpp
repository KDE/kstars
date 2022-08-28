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

#include "indimount.h"
#include "indicamera.h"
#include "indiguider.h"
#include "indifocuser.h"
#include "indifilterwheel.h"
#include "indidome.h"
#include "indigps.h"
#include "indiweather.h"
#include "indiadaptiveoptics.h"
#include "indidustcap.h"
#include "indilightbox.h"
#include "indidetector.h"
#include "indirotator.h"
#include "indispectrograph.h"
#include "indicorrelator.h"
#include "indiauxiliary.h"

#include "genericdeviceadaptor.h"
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

uint8_t GenericDevice::m_ID = 1;
GenericDevice::GenericDevice(DeviceInfo &idv, ClientManager *cm, QObject *parent) : GDInterface(parent)
{
    // Register DBus
    new GenericDeviceAdaptor(this);
    QDBusConnection::sessionBus().registerObject(QString("/KStars/INDI/GenericDevice/%1").arg(getID()), this);

    m_DeviceInfo    = &idv;
    m_DriverInfo    = idv.getDriverInfo();
    m_BaseDevice    = idv.getBaseDevice();
    m_ClientManager = cm;

    Q_ASSERT_X(m_BaseDevice, __FUNCTION__, "Base device is invalid.");
    Q_ASSERT_X(m_ClientManager, __FUNCTION__, "Client manager is invalid.");

    m_Name        = m_BaseDevice->getDeviceName();

    registerDBusType();

    // JM 2020-09-05: In case KStars time change, update driver time if applicable.
    connect(KStarsData::Instance()->clock(), &SimClock::timeChanged, this, [this]()
    {
        if (Options::useTimeUpdate() && Options::useKStarsSource())
        {
            if (m_BaseDevice != nullptr && isConnected())
            {
                auto tvp = m_BaseDevice->getText("TIME_UTC");
                if (tvp && tvp->getPermission() != IP_RO)
                    updateTime();
            }
        }
    });

    m_ReadyTimer = new QTimer(this);
    m_ReadyTimer->setInterval(250);
    m_ReadyTimer->setSingleShot(true);
    connect(m_ReadyTimer, &QTimer::timeout, this, &GenericDevice::ready);
    connect(this, &GenericDevice::ready, this, [this]()
    {
        generateDevices();
        m_ReadyTimer->disconnect(this);
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

    m_ReadyTimer->start();

    const QString name = prop->getName();

    // In case driver already started
    if (name == "CONNECTION")
    {
        auto svp = prop->getSwitch();

        // Still connecting/disconnecting...
        if (!svp || svp->getState() == IPS_BUSY)
            return;

        auto conSP = svp->findWidgetByName("CONNECT");

        if (!conSP)
            return;

        if (m_Connected == false && svp->getState() == IPS_OK && conSP->getState() == ISS_ON)
        {
            m_Connected = true;
            emit Connected();
            createDeviceInit();
        }
        else if (m_Connected && conSP->getState() == ISS_OFF)
        {
            m_Connected = false;
            emit Disconnected();
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
                m_DriverInterface = static_cast<uint32_t>(atoi(tp->getText()));
                emit interfaceDefined();
            }

            tp = tvp->findWidgetByName("DRIVER_VERSION");
            if (tp)
            {
                m_DriverVersion = QString(tp->getText());
            }
        }
    }
    else if (name == "SYSTEM_PORTS")
    {
        // Check if our current port is set to one of the system ports. This indicates that the port
        // is not mapped yet to a permenant designation
        auto svp = prop->getSwitch();
        auto port = m_BaseDevice->getText("DEVICE_PORT");
        if (svp && port)
        {
            for (const auto &it : *svp)
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

            if (m_Connected && nvp->at(0)->getValue() > 0)
            {
                // Send immediately a heart beat
                m_ClientManager->sendNewNumber(nvp);
            }
        }
    }

    emit propertyDefined(prop);
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

        if (m_Connected == false && svp->s == IPS_OK && conSP->s == ISS_ON)
        {
            m_Connected = true;
            emit Connected();
            createDeviceInit();

            if (watchDogTimer != nullptr)
            {
                auto nvp = m_BaseDevice->getNumber("WATCHDOG_HEARTBEAT");
                if (nvp && nvp->at(0)->getValue() > 0)
                {
                    // Send immediately
                    m_ClientManager->sendNewNumber(nvp);
                }
            }
        }
        else if (m_Connected && conSP->s == ISS_OFF)
        {
            m_Connected = false;
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

        if (m_Connected && nvp->np[0].value > 0)
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
    // If DRIVER_INFO is updated after being defined, make sure to re-generate concrete devices accordingly.
    if (!strcmp(tvp->name, "DRIVER_INFO"))
    {
        auto tp = IUFindText(tvp, "DRIVER_INTERFACE");
        if (tp)
        {
            m_DriverInterface = static_cast<uint32_t>(atoi(tp->text));
            emit interfaceDefined();

            // If devices were already created but we receieved an update to DRIVER_INTERFACE
            // then we need to re-generate the concrete devices to account for the change.
            // TODO maybe be smart about it and only regenerate interfaces that were not previously defined?
            if (m_ConcreteDevices.isEmpty() == false)
                generateDevices();
        }

        tp = IUFindText(tvp, "DRIVER_VERSION");
        if (tp)
        {
            m_DriverVersion = QString(tp->text);
        }

    }
    // Update KStars time once we receive update from INDI, if the source is set to DEVICE
    else if (!strcmp(tvp->name, "TIME_UTC") && tvp->s == IPS_OK &&
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

bool GenericDevice::processBLOB(IBLOB *bp)
{
    // Ignore write-only BLOBs since we only receive it for state-change
    if (bp->bvp->p == IP_WO)
        return false;

    // If any concrete device processed the blob then we return
    for (auto oneConcreteDevice : m_ConcreteDevices)
    {
        if (oneConcreteDevice->processBLOB(bp))
            return true;
    }

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
            return false;
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
    return true;
}

bool GenericDevice::setConfig(INDIConfig tConfig)
{
    auto svp = m_BaseDevice->getSwitch("CONFIG_PROCESS");

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

    m_ClientManager->sendNewSwitch(svp);

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
    auto timeUTC = m_BaseDevice->getText("TIME_UTC");

    if (timeUTC)
    {
        auto timeEle = timeUTC->findWidgetByName("UTC");
        if (timeEle)
            timeEle->setText(isoTS.toLatin1().constData());

        auto offsetEle = timeUTC->findWidgetByName("OFFSET");
        if (offsetEle)
            offsetEle->setText(offset.toLatin1().constData());

        if (timeEle && offsetEle)
            m_ClientManager->sendNewText(timeUTC);
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

    auto nvp = m_BaseDevice->getNumber("GEOGRAPHIC_COORD");

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

    m_ClientManager->sendNewNumber(nvp);
}

void GenericDevice::Connect()
{
    m_ClientManager->connectDevice(m_Name.toLatin1().constData());
}

void GenericDevice::Disconnect()
{
    m_ClientManager->disconnectDevice(m_Name.toLatin1().constData());
}

bool GenericDevice::setProperty(QObject *setPropCommand)
{
    GDSetCommand *indiCommand = static_cast<GDSetCommand *>(setPropCommand);

    //qDebug() << Q_FUNC_INFO << "We are trying to set value for property " << indiCommand->indiProperty << " and element" << indiCommand->indiElement << " and value " << indiCommand->elementValue;

    auto pp = m_BaseDevice->getProperty(indiCommand->indiProperty.toLatin1().constData());

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

            //qDebug() << Q_FUNC_INFO << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off");
            m_ClientManager->sendNewSwitch(svp);

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

            //qDebug() << Q_FUNC_INFO << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off");
            m_ClientManager->sendNewNumber(nvp);
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
    auto nvp = m_BaseDevice->getNumber(propName.toLatin1());

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
    return m_BaseDevice->getPropertyState(propName.toLatin1().constData());
}

IPerm GenericDevice::getPermission(const QString &propName)
{
    return m_BaseDevice->getPropertyPermission(propName.toLatin1().constData());
}

INDI::Property GenericDevice::getProperty(const QString &propName)
{
    return m_BaseDevice->getProperty(propName.toLatin1().constData());
}

bool GenericDevice::setJSONProperty(const QString &propName, const QJsonArray &propElements)
{
    for (auto &oneProp : * (m_BaseDevice->getProperties()))
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

                    m_ClientManager->sendNewSwitch(svp);
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

                    m_ClientManager->sendNewNumber(nvp);
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

                    m_ClientManager->sendNewText(tvp);
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
    for (auto &oneProp : * (m_BaseDevice->getProperties()))
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
    auto blobProperty = m_BaseDevice->getProperty(propName.toLatin1().constData());
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
    auto nvp = m_BaseDevice->getNumber("WATCHDOG_HEARTBEAT");

    if (nvp)
        // Send heartbeat to driver
        m_ClientManager->sendNewNumber(nvp);
}

bool GenericDevice::findConcreteDevice(uint32_t interface, QSharedPointer<ConcreteDevice> &device)
{
    if (m_ConcreteDevices.contains(interface))
    {
        device = m_ConcreteDevices[interface];
        return true;
    }
    return false;
}

ISD::Mount *GenericDevice::getMount()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::TELESCOPE_INTERFACE))
        return dynamic_cast<ISD::Mount*>(m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].get());
    return nullptr;
}

ISD::Camera *GenericDevice::getCamera()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::CCD_INTERFACE))
        return dynamic_cast<ISD::Camera*>(m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].get());
    return nullptr;
}

ISD::Guider *GenericDevice::getGuider()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::GUIDER_INTERFACE))
        return dynamic_cast<ISD::Guider*>(m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].get());
    return nullptr;
}

ISD::Focuser *GenericDevice::getFocuser()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::FOCUSER_INTERFACE))
        return dynamic_cast<ISD::Focuser*>(m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].get());
    return nullptr;
}

ISD::FilterWheel *GenericDevice::getFilterWheel()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::FILTER_INTERFACE))
        return dynamic_cast<ISD::FilterWheel*>(m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].get());
    return nullptr;
}

ISD::Dome *GenericDevice::getDome()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DOME_INTERFACE))
        return dynamic_cast<ISD::Dome*>(m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].get());
    return nullptr;
}

ISD::GPS *GenericDevice::getGPS()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::GPS_INTERFACE))
        return dynamic_cast<ISD::GPS*>(m_ConcreteDevices[INDI::BaseDevice::GPS_INTERFACE].get());
    return nullptr;
}

ISD::Weather *GenericDevice::getWeather()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::WEATHER_INTERFACE))
        return dynamic_cast<ISD::Weather*>(m_ConcreteDevices[INDI::BaseDevice::WEATHER_INTERFACE].get());
    return nullptr;
}

ISD::AdaptiveOptics *GenericDevice::getAdaptiveOptics()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::AO_INTERFACE))
        return dynamic_cast<ISD::AdaptiveOptics*>(m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].get());
    return nullptr;
}

ISD::DustCap *GenericDevice::getDustCap()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DUSTCAP_INTERFACE))
        return dynamic_cast<ISD::DustCap*>(m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].get());
    return nullptr;
}

ISD::LightBox *GenericDevice::getLightBox()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::LIGHTBOX_INTERFACE))
        return dynamic_cast<ISD::LightBox*>(m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].get());
    return nullptr;
}

ISD::Detector *GenericDevice::getDetector()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DETECTOR_INTERFACE))
        return dynamic_cast<ISD::Detector*>(m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].get());
    return nullptr;
}

ISD::Rotator *GenericDevice::getRotator()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::ROTATOR_INTERFACE))
        return dynamic_cast<ISD::Rotator*>(m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].get());
    return nullptr;
}

ISD::Spectrograph *GenericDevice::getSpectrograph()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::SPECTROGRAPH_INTERFACE))
        return dynamic_cast<ISD::Spectrograph*>(m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].get());
    return nullptr;
}

ISD::Correlator *GenericDevice::getCorrelator()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::CORRELATOR_INTERFACE))
        return dynamic_cast<ISD::Correlator*>(m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].get());
    return nullptr;
}

ISD::Auxiliary *GenericDevice::getAuxiliary()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::AUX_INTERFACE))
        return dynamic_cast<ISD::Auxiliary*>(m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].get());
    return nullptr;
}

void GenericDevice::generateDevices()
{
    // Mount
    if (m_DriverInterface & INDI::BaseDevice::TELESCOPE_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].isNull())
    {
        auto mount = new ISD::Mount(this);
        m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].reset(mount);
        mount->registeProperties();
        if (m_Connected)
        {
            mount->processProperties();
            emit newMount(mount);
        }
        else
        {
            connect(mount, &ISD::ConcreteDevice::ready, this, [this, mount]()
            {
                emit newMount(mount);
            });
        }
    }

    // Camera
    if (m_DriverInterface & INDI::BaseDevice::CCD_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].isNull())
    {
        auto camera = new ISD::Camera(this);
        m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].reset(camera);
        camera->registeProperties();
        if (m_Connected)
        {
            camera->processProperties();
            emit newCamera(camera);
        }
        else
        {
            connect(camera, &ISD::ConcreteDevice::ready, this, [this, camera]()
            {
                emit newCamera(camera);
            });
        }
    }

    // Guider
    if (m_DriverInterface & INDI::BaseDevice::GUIDER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].isNull())
    {
        auto guider = new ISD::Guider(this);
        m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].reset(guider);
        guider->registeProperties();
        if (m_Connected)
        {
            guider->processProperties();
            emit newGuider(guider);
        }
        else
        {
            connect(guider, &ISD::ConcreteDevice::ready, this, [this, guider]()
            {
                emit newGuider(guider);
            });
        }
    }

    // Focuser
    if (m_DriverInterface & INDI::BaseDevice::FOCUSER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].isNull())
    {
        auto focuser = new ISD::Focuser(this);
        m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].reset(focuser);
        focuser->registeProperties();
        if (m_Connected)
        {
            focuser->processProperties();
            emit newFocuser(focuser);
        }
        else
        {
            connect(focuser, &ISD::ConcreteDevice::ready, this, [this, focuser]()
            {
                emit newFocuser(focuser);
            });
        }
    }

    // Filter Wheel
    if (m_DriverInterface & INDI::BaseDevice::FILTER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].isNull())
    {
        auto filterWheel = new ISD::FilterWheel(this);
        m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].reset(filterWheel);
        filterWheel->registeProperties();
        if (m_Connected)
        {
            filterWheel->processProperties();
            emit newFilterWheel(filterWheel);
        }
        else
        {
            connect(filterWheel, &ISD::ConcreteDevice::ready, this, [this, filterWheel]()
            {
                emit newFilterWheel(filterWheel);
            });
        }
    }

    // Dome
    if (m_DriverInterface & INDI::BaseDevice::DOME_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].isNull())
    {
        auto dome = new ISD::Dome(this);
        m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].reset(dome);
        dome->registeProperties();
        if (m_Connected)
        {
            dome->processProperties();
            emit newDome(dome);
        }
        else
        {
            connect(dome, &ISD::ConcreteDevice::ready, this, [this, dome]()
            {
                emit newDome(dome);
            });
        }
    }

    // GPS
    if (m_DriverInterface & INDI::BaseDevice::GPS_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::GPS_INTERFACE].isNull())
    {
        auto gps = new ISD::GPS(this);
        m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].reset(gps);
        gps->registeProperties();
        if (m_Connected)
        {
            gps->processProperties();
            emit newGPS(gps);
        }
        else
        {
            connect(gps, &ISD::ConcreteDevice::ready, this, [this, gps]()
            {
                emit newGPS(gps);
            });
        }
    }

    // Weather
    if (m_DriverInterface & INDI::BaseDevice::WEATHER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::WEATHER_INTERFACE].isNull())
    {
        auto weather = new ISD::Weather(this);
        m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].reset(weather);
        weather->registeProperties();
        if (m_Connected)
        {
            weather->processProperties();
            emit newWeather(weather);
        }
        else
        {
            connect(weather, &ISD::ConcreteDevice::ready, this, [this, weather]()
            {
                emit newWeather(weather);
            });
        }
    }

    // Adaptive Optics
    if (m_DriverInterface & INDI::BaseDevice::AO_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].isNull())
    {
        auto ao = new ISD::AdaptiveOptics(this);
        m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].reset(ao);
        ao->registeProperties();
        if (m_Connected)
        {
            ao->processProperties();
            emit newAdaptiveOptics(ao);
        }
        else
        {
            connect(ao, &ISD::ConcreteDevice::ready, this, [this, ao]()
            {
                emit newAdaptiveOptics(ao);
            });
        }
    }

    // Dust Cap
    if (m_DriverInterface & INDI::BaseDevice::DUSTCAP_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].isNull())
    {
        auto dustCap = new ISD::DustCap(this);
        m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].reset(dustCap);
        dustCap->registeProperties();
        if (m_Connected)
        {
            dustCap->processProperties();
            emit newDustCap(dustCap);
        }
        else
        {
            connect(dustCap, &ISD::ConcreteDevice::ready, this, [this, dustCap]()
            {
                emit newDustCap(dustCap);
            });
        }
    }

    // Light box
    if (m_DriverInterface & INDI::BaseDevice::LIGHTBOX_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].isNull())
    {
        auto lightBox = new ISD::LightBox(this);
        m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].reset(lightBox);
        lightBox->registeProperties();
        if (m_Connected)
        {
            lightBox->processProperties();
            emit newLightBox(lightBox);
        }
        else
        {
            connect(lightBox, &ISD::ConcreteDevice::ready, this, [this, lightBox]()
            {
                emit newLightBox(lightBox);
            });
        }
    }

    // Rotator
    if (m_DriverInterface & INDI::BaseDevice::ROTATOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].isNull())
    {
        auto rotator = new ISD::Rotator(this);
        m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].reset(rotator);
        rotator->registeProperties();
        if (m_Connected)
        {
            rotator->processProperties();
            emit newRotator(rotator);
        }
        else
        {
            connect(rotator, &ISD::ConcreteDevice::ready, this, [this, rotator]()
            {
                emit newRotator(rotator);
            });
        }
    }

    // Detector
    if (m_DriverInterface & INDI::BaseDevice::DETECTOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].isNull())
    {
        auto detector = new ISD::Detector(this);
        m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].reset(detector);
        detector->registeProperties();
        if (m_Connected)
        {
            detector->processProperties();
            emit newDetector(detector);
        }
        else
        {
            connect(detector, &ISD::ConcreteDevice::ready, this, [this, detector]()
            {
                emit newDetector(detector);
            });
        }
    }

    // Spectrograph
    if (m_DriverInterface & INDI::BaseDevice::SPECTROGRAPH_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].isNull())
    {
        auto spectrograph = new ISD::Spectrograph(this);
        m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].reset(spectrograph);
        spectrograph->registeProperties();
        if (m_Connected)
        {
            spectrograph->processProperties();
            emit newSpectrograph(spectrograph);
        }
        else
        {
            connect(spectrograph, &ISD::ConcreteDevice::ready, this, [this, spectrograph]()
            {
                emit newSpectrograph(spectrograph);
            });
        }
    }

    // Correlator
    if (m_DriverInterface & INDI::BaseDevice::CORRELATOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].isNull())
    {
        auto correlator = new ISD::Correlator(this);
        m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].reset(correlator);
        correlator->registeProperties();
        if (m_Connected)
        {
            correlator->processProperties();
            emit newCorrelator(correlator);
        }
        else
        {
            connect(correlator, &ISD::ConcreteDevice::ready, this, [this, correlator]()
            {
                emit newCorrelator(correlator);
            });
        }
    }

    // Auxiliary
    if (m_DriverInterface & INDI::BaseDevice::AUX_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].isNull())
    {
        auto aux = new ISD::Auxiliary(this);
        m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].reset(aux);
        aux->registeProperties();
        if (m_Connected)
        {
            aux->processProperties();
            emit newAuxiliary(aux);
        }
        else
        {
            connect(aux, &ISD::ConcreteDevice::ready, this, [this, aux]()
            {
                emit newAuxiliary(aux);
            });
        }
    }
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
