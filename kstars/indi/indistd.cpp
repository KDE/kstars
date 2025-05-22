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

#include "indilistener.h"
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

    m_Name = m_BaseDevice.getDeviceName();

    setObjectName(m_Name);

    m_DriverInterface = m_BaseDevice.getDriverInterface();
    m_DriverVersion = m_BaseDevice.getDriverVersion();

    registerDBusType();

    // JM 2020-09-05: In case KStars time change, update driver time if applicable.
    connect(KStarsData::Instance()->clock(), &SimClock::timeChanged, this, [this]()
    {
        if (Options::useTimeUpdate() && Options::timeSource() == "KStars")
        {
            if (isConnected())
            {
                auto tvp = m_BaseDevice.getText("TIME_UTC");
                if (tvp && tvp.getPermission() != IP_RO)
                    updateTime();
            }
        }
    });

    m_ReadyTimer = new QTimer(this);
    m_ReadyTimer->setInterval(250);
    m_ReadyTimer->setSingleShot(true);
    connect(m_ReadyTimer, &QTimer::timeout, this, &GenericDevice::handleTimeout, Qt::UniqueConnection);

    m_TimeUpdateTimer = new QTimer(this);
    m_TimeUpdateTimer->setInterval(5000);
    m_TimeUpdateTimer->setSingleShot(true);
    connect(m_TimeUpdateTimer, &QTimer::timeout, this, &GenericDevice::checkTimeUpdate, Qt::UniqueConnection);

    m_LocationUpdateTimer = new QTimer(this);
    m_LocationUpdateTimer->setInterval(5000);
    m_LocationUpdateTimer->setSingleShot(true);
    connect(m_LocationUpdateTimer, &QTimer::timeout, this, &GenericDevice::checkLocationUpdate, Qt::UniqueConnection);

}

GenericDevice::~GenericDevice()
{
    for (auto &metadata : streamFileMetadata)
        metadata.file->close();
}

void GenericDevice::handleTimeout()
{
    generateDevices();
    // N.B. JM 2022.10.15: Do not disconnect timer.
    // It is possible that other properties can come later.
    // Even they do not make it in the 250ms window. Increasing timeout value alone
    // to 1000ms or more would improve the situation but is not sufficient to account for
    // unexpected delays. Therefore, the best solution is to keep the timer active.
    //m_ReadyTimer->disconnect(this);
    m_Ready = true;
    emit ready();
}

void GenericDevice::checkTimeUpdate()
{
    auto tvp = getProperty("TIME_UTC");
    if (tvp)
    {
        auto timeTP = tvp.getText();
        // If time still empty, then force update.
        if (timeTP && timeTP->getPermission() != IP_RO && timeTP->getState() == IPS_IDLE)
            updateTime();
    }

}

void GenericDevice::checkLocationUpdate()
{
    auto nvp = getProperty("GEOGRAPHIC_COORD");
    if (nvp)
    {
        auto locationNP = nvp.getNumber();
        // If time still empty, then force update.
        if (locationNP && locationNP->getPermission() != IP_RO && locationNP->getState() == IPS_IDLE)
            updateLocation();
    }
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
    if (!prop.getRegistered())
        return;

    m_ReadyTimer->start();

    const QString name = prop.getName();

    // In case driver already started
    if (name == "CONNECTION")
    {
        auto svp = prop.getSwitch();

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
        else
            m_Ready = (svp->s == IPS_OK && conSP->s == ISS_ON);
    }
    else if (name == "DRIVER_INFO")
    {
        auto tvp = prop.getText();
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
        auto svp = prop.getSwitch();
        auto port = m_BaseDevice.getText("DEVICE_PORT");
        if (svp && port)
        {
            for (const auto &it : *svp)
            {
                if (it.isNameMatch(port.at(0)->getText()))
                {
                    emit systemPortDetected();
                    break;
                }
            }
        }
    }
    else if (name == "TIME_UTC" && Options::useTimeUpdate())
    {
        const auto &tvp = prop.getText();

        if (tvp)
        {
            if (Options::timeSource() == "KStars" && tvp->getPermission() != IP_RO)
                updateTime();
            else
                m_TimeUpdateTimer->start();
        }
    }
    else if (name == "GEOGRAPHIC_COORD" && Options::useGeographicUpdate())
    {
        if (Options::locationSource() == "KStars" && prop.getPermission() != IP_RO)
            updateLocation();
        else
            m_LocationUpdateTimer->start();
    }
    else if (name == "WATCHDOG_HEARTBEAT")
    {
        if (watchDogTimer == nullptr)
        {
            watchDogTimer = new QTimer(this);
            connect(watchDogTimer, SIGNAL(timeout()), this, SLOT(resetWatchdog()));
        }

        if (m_Connected && prop.getNumber()->at(0)->getValue() > 0)
        {
            // Send immediately a heart beat
            m_ClientManager->sendNewProperty(prop);
        }
    }

    emit propertyDefined(prop);
}

void GenericDevice::updateProperty(INDI::Property prop)
{
    switch (prop.getType())
    {
        case INDI_SWITCH:
            processSwitch(prop);
            break;
        case INDI_NUMBER:
            processNumber(prop);
            break;
        case INDI_TEXT:
            processText(prop);
            break;
        case INDI_LIGHT:
            processLight(prop);
            break;
        case INDI_BLOB:
            processBLOB(prop);
            break;
        default:
            break;
    }
}

void GenericDevice::removeProperty(INDI::Property prop)
{
    emit propertyDeleted(prop);
}

void GenericDevice::processSwitch(INDI::Property prop)
{
    if (prop.isNameMatch("CONNECTION"))
    {
        // Still connecting/disconnecting...
        if (prop.getState() == IPS_BUSY)
            return;

        auto connectionOn = prop.getSwitch()->findWidgetByName("CONNECT");
        if (m_Connected == false && prop.getState() == IPS_OK && connectionOn->getState() == ISS_ON)
        {
            m_Ready = false;
            connect(m_ReadyTimer, &QTimer::timeout, this, &GenericDevice::handleTimeout, Qt::UniqueConnection);

            m_Connected = true;
            emit Connected();
            createDeviceInit();

            if (watchDogTimer != nullptr)
            {
                auto nvp = m_BaseDevice.getNumber("WATCHDOG_HEARTBEAT");
                if (nvp && nvp.at(0)->getValue() > 0)
                {
                    // Send immediately
                    m_ClientManager->sendNewProperty(nvp);
                }
            }

            m_ReadyTimer->start();
        }
        else if (m_Connected && connectionOn->getState() == ISS_OFF)
        {
            disconnect(m_ReadyTimer, &QTimer::timeout, this, &GenericDevice::handleTimeout);
            m_Connected = false;
            m_Ready = false;
            emit Disconnected();
        }
        else
            m_Ready = (prop.getState() == IPS_OK && connectionOn->getState() == ISS_ON);
    }

    emit propertyUpdated(prop);
}

void GenericDevice::processNumber(INDI::Property prop)
{
    QString deviceName = getDeviceName();
    auto nvp = prop.getNumber();

    if (prop.isNameMatch("GEOGRAPHIC_COORD") && prop.getState() == IPS_OK && Options::locationSource() == deviceName)
    {
        // Update KStars Location once we receive update from INDI, if the source is set to DEVICE
        dms lng, lat;
        double elev = 0;

        auto np = nvp->findWidgetByName("LONG");
        if (!np)
            return;

        // INDI Longitude convention is 0 to 360. We need to turn it back into 0 to 180 EAST, 0 to -180 WEST
        if (np->value < 180)
            lng.setD(np->value);
        else
            lng.setD(np->value - 360.0);

        np = nvp->findWidgetByName("LAT");
        if (!np)
            return;

        lat.setD(np->value);

        // Double check we have valid values
        if (lng.Degrees() == 0 && lat.Degrees() == 0)
        {
            qCWarning(KSTARS_INDI) << "Ignoring invalid device coordinates.";
            return;
        }

        np = nvp->findWidgetByName("ELEV");
        if (np)
            elev = np->value;

        // Update all other INDI devices
        for (auto &oneDevice : INDIListener::devices())
        {
            // Skip updating the device itself
            if (oneDevice->getDeviceName() == getDeviceName())
                continue;

            oneDevice->updateLocation(lng.Degrees(), lat.Degrees(), elev);
        }

        auto geo = KStars::Instance()->data()->geo();
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
    else if (nvp->isNameMatch("WATCHDOG_HEARTBEAT"))
    {
        if (watchDogTimer == nullptr)
        {
            watchDogTimer = new QTimer(this);
            connect(watchDogTimer, &QTimer::timeout, this, &GenericDevice::resetWatchdog);
        }

        auto value = nvp->at(0)->getValue();
        if (m_Connected && value > 0)
        {
            // Reset timer 5 seconds before it is due
            // To account for any networking delays
            double nextMS = qMax(100.0, (value - 5) * 1000);
            watchDogTimer->start(nextMS);
        }
        else if (value == 0)
            watchDogTimer->stop();
    }

    emit propertyUpdated(prop);
}

void GenericDevice::processText(INDI::Property prop)
{
    auto tvp = prop.getText();
    // If DRIVER_INFO is updated after being defined, make sure to re-generate concrete devices accordingly.
    if (tvp->isNameMatch("DRIVER_INFO"))
    {
        auto tp = tvp->findWidgetByName("DRIVER_INTERFACE");
        if (tp)
        {
            m_DriverInterface = static_cast<uint32_t>(atoi(tp->getText()));
            emit interfaceDefined();

            // If devices were already created but we receieved an update to DRIVER_INTERFACE
            // then we need to re-generate the concrete devices to account for the change.
            if (m_ConcreteDevices.isEmpty() == false)
            {
                // If we generated ANY concrete device due to interface update, then we emit ready immediately.
                if (generateDevices())
                    emit ready();
            }
        }

        tp = tvp->findWidgetByName("DRIVER_VERSION");
        if (tp)
        {
            m_DriverVersion = QString(tp->text);
        }

    }
    // Update KStars time once we receive update from INDI, if the source is set to DEVICE
    else if (tvp->isNameMatch("TIME_UTC") && tvp->s == IPS_OK && Options::timeSource() == getDeviceName())
    {
        int d, m, y, min, sec, hour;
        float utcOffset;
        QDate indiDate;
        QTime indiTime;

        auto tp = tvp->findWidgetByName("UTC");

        if (!tp)
        {
            qCWarning(KSTARS_INDI) << "UTC property missing from TIME_UTC";
            return;
        }

        sscanf(tp->getText(), "%d%*[^0-9]%d%*[^0-9]%dT%d%*[^0-9]%d%*[^0-9]%d", &y, &m, &d, &hour, &min, &sec);
        indiDate.setDate(y, m, d);
        indiTime.setHMS(hour, min, sec);

        KStarsDateTime indiDateTime(QDateTime(indiDate, indiTime, Qt::UTC));

        tp = tvp->findWidgetByName("OFFSET");

        if (!tp)
        {
            qCWarning(KSTARS_INDI) << "Offset property missing from TIME_UTC";
            return;
        }

        sscanf(tp->getText(), "%f", &utcOffset);

        // Update all other INDI devices
        for (auto &oneDevice : INDIListener::devices())
        {
            // Skip updating the device itself
            if (oneDevice->getDeviceName() == getDeviceName())
                continue;

            oneDevice->updateTime(tvp->tp[0].text, tvp->tp[1].text);
        }

        qCInfo(KSTARS_INDI) << "Setting UTC time from device:" << getDeviceName() << indiDateTime.toString();

        KStars::Instance()->data()->changeDateTime(indiDateTime);
        KStars::Instance()->data()->syncLST();

        auto geo = KStars::Instance()->data()->geo();
        if (geo->tzrule())
            utcOffset -= geo->tzrule()->deltaTZ();

        // TZ0 is the timezone WTIHOUT any DST offsets. Above, we take INDI UTC Offset (with DST already included)
        // and subtract from it the deltaTZ from the current TZ rule.
        geo->setTZ0(utcOffset);
    }

    emit propertyUpdated(prop);
}

void GenericDevice::processLight(INDI::Property prop)
{
    emit propertyUpdated(prop);
}

void GenericDevice::processMessage(int messageID)
{
    emit messageUpdated(messageID);
}

bool GenericDevice::processBLOB(INDI::Property prop)
{
    // Ignore write-only BLOBs since we only receive it for state-change
    if (prop.getPermission() == IP_WO)
        return false;

    auto bvp = prop.getBLOB();
    auto bp = bvp->at(0);

    // If any concrete device processed the blob then we return
    for (auto &oneConcreteDevice : m_ConcreteDevices)
    {
        if (!oneConcreteDevice.isNull() && oneConcreteDevice->processBLOB(prop))
            return true;
    }

    INDIDataTypes dataType;

    if (!strcmp(bp->getFormat(), ".ascii"))
        dataType = DATA_ASCII;
    else
        dataType = DATA_OTHER;

    QString currentDir = Options::fitsDir();
    int nr, n = 0;

    if (currentDir.endsWith('/'))
        currentDir.truncate(sizeof(currentDir) - 1);

    QString filename(currentDir + '/');

    QString ts = QDateTime::currentDateTime().toString("yyyy-MM-ddThh-mm-ss");

    filename += QString("%1_").arg(bp->getLabel()) + ts + QString(bp->getFormat()).trimmed();

    // Text Streaming
    if (dataType == DATA_ASCII)
    {
        // First time, create a data file to hold the stream.

        auto it = std::find_if(streamFileMetadata.begin(), streamFileMetadata.end(), [bvp, bp](const StreamFileMetadata & data)
        {
            return (bvp->getDeviceName() == data.device && bvp->getName() == data.property && bp->getName() == data.element);
        });

        QFile *streamDatafile = nullptr;

        // New stream data file
        if (it == streamFileMetadata.end())
        {
            StreamFileMetadata metadata;
            metadata.device = bvp->getDeviceName();
            metadata.property = bvp->getName();
            metadata.element = bp->getName();

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
        for (nr = 0; nr < bp->getSize(); nr += n)
            n = out.writeRawData(static_cast<char *>(bp->getBlob()) + nr, bp->getSize() - nr);

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

        for (nr = 0; nr < bp->getSize(); nr += n)
            n = out.writeRawData(static_cast<char *>(bp->getBlob()) + nr, bp->getSize() - nr);

        fits_temp_file.flush();
        fits_temp_file.close();

        auto fmt = QString(bp->getFormat()).toLower().remove('.').toUtf8();
        if (QImageReader::supportedImageFormats().contains(fmt))
        {
            QUrl url(filename);
            url.setScheme("file");
            auto iv = new ImageViewer(url, QString(), KStars::Instance());
            if (iv)
                iv->show();
        }
    }

    if (dataType == DATA_OTHER)
        KStars::Instance()->statusBar()->showMessage(i18n("Data file saved to %1", filename), 0);

    emit propertyUpdated(prop);
    return true;
}

bool GenericDevice::wasConfigLoaded() const
{
    auto svp = m_BaseDevice.getSwitch("CONFIG_PROCESS");

    if (!svp)
        return false;

    return svp->getState() == IPS_OK;
}

bool GenericDevice::setConfig(INDIConfig tConfig)
{
    auto svp = m_BaseDevice.getSwitch("CONFIG_PROCESS");

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

    svp.reset();
    if (strConfig)
    {
        auto sp = svp.findWidgetByName(strConfig);
        if (!sp)
            return false;
        sp->setState(ISS_ON);
    }

    m_ClientManager->sendNewProperty(svp);

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
void GenericDevice::updateTime(const QString &iso8601, const QString &utcOffset)
{
    /* Update Date/Time */
    auto timeUTC = m_BaseDevice.getText("TIME_UTC");
    if (!timeUTC)
        return;

    QString offset, isoTS;

    if (iso8601.isEmpty())
    {
        isoTS = KStars::Instance()->data()->ut().toString(Qt::ISODate).remove('Z');
        offset = QString().setNum(KStars::Instance()->data()->geo()->TZ(), 'g', 2);
    }
    else
    {
        isoTS = iso8601;
        offset = utcOffset;
    }

    auto timeEle = timeUTC.findWidgetByName("UTC");
    if (timeEle)
        timeEle->setText(isoTS.toLatin1().constData());

    auto offsetEle = timeUTC.findWidgetByName("OFFSET");
    if (offsetEle)
        offsetEle->setText(offset.toLatin1().constData());

    if (timeEle && offsetEle)
    {
        qCInfo(KSTARS_INDI) << "Updating" << getDeviceName() << "Time UTC:" << isoTS << "Offset:" << offset;
        m_ClientManager->sendNewProperty(timeUTC);
    }
}

/*********************************************************************************/
/* Update the Driver's Geographical Location					 */
/*********************************************************************************/
void GenericDevice::updateLocation(double longitude, double latitude, double elevation)
{
    auto nvp = m_BaseDevice.getNumber("GEOGRAPHIC_COORD");

    if (!nvp)
        return;

    GeoLocation *geo = KStars::Instance()->data()->geo();

    double longitude_degrees, latitude_degrees, elevation_meters;

    if (longitude == -1 && latitude == -1 && elevation == -1)
    {
        longitude_degrees = geo->lng()->Degrees();
        latitude_degrees = geo->lat()->Degrees();
        elevation_meters = geo->elevation();
    }
    else
    {
        longitude_degrees = longitude;
        latitude_degrees = latitude;
        elevation_meters = elevation;
    }

    if (longitude_degrees < 0)
        longitude_degrees = dms(longitude_degrees + 360.0).Degrees();

    auto np = nvp.findWidgetByName("LONG");

    if (!np)
        return;

    np->setValue(longitude_degrees);

    np = nvp.findWidgetByName("LAT");
    if (!np)
        return;

    np->setValue(latitude_degrees);

    np = nvp.findWidgetByName("ELEV");
    if (!np)
        return;

    np->setValue(elevation_meters);

    qCInfo(KSTARS_INDI) << "Updating" << getDeviceName() << "Location Longitude:" << longitude_degrees << "Latitude:" <<
                        latitude_degrees << "Elevation:" << elevation_meters;

    m_ClientManager->sendNewProperty(nvp);
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

    auto prop = m_BaseDevice.getProperty(indiCommand->indiProperty.toLatin1().constData());

    if (!prop)
        return false;

    switch (indiCommand->propType)
    {
        case INDI_SWITCH:
        {
            auto svp = prop.getSwitch();

            if (!svp)
                return false;

            auto sp = svp->findWidgetByName(indiCommand->indiElement.toLatin1().constData());

            if (!sp)
                return false;

            if (svp->getRule() == ISR_1OFMANY || svp->getRule() == ISR_ATMOST1)
                svp->reset();

            sp->setState(indiCommand->elementValue.toInt() == 0 ? ISS_OFF : ISS_ON);

            //qDebug() << Q_FUNC_INFO << "Sending switch " << sp->name << " with status " << ((sp->s == ISS_ON) ? "On" : "Off");
            m_ClientManager->sendNewProperty(svp);

            return true;
        }

        case INDI_NUMBER:
        {
            auto nvp = prop.getNumber();

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
            m_ClientManager->sendNewProperty(nvp);
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
    auto nvp = m_BaseDevice.getNumber(propName.toLatin1());

    if (!nvp)
        return false;

    auto np = nvp.findWidgetByName(elementName.toLatin1());

    if (!np)
        return false;

    *min  = np->getMin();
    *max  = np->getMax();
    *step = np->getStep();

    return true;
}

IPState GenericDevice::getState(const QString &propName)
{
    return m_BaseDevice.getPropertyState(propName.toLatin1().constData());
}

IPerm GenericDevice::getPermission(const QString &propName)
{
    return m_BaseDevice.getPropertyPermission(propName.toLatin1().constData());
}

INDI::Property GenericDevice::getProperty(const QString &propName)
{
    return m_BaseDevice.getProperty(propName.toLatin1().constData());
}

bool GenericDevice::setJSONProperty(const QString &propName, const QJsonArray &propElements)
{
    for (auto &oneProp : * (m_BaseDevice.getProperties()))
    {
        if (propName == QString(oneProp.getName()))
        {
            switch (oneProp.getType())
            {
                case INDI_SWITCH:
                {
                    auto svp = oneProp.getSwitch();
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

                    m_ClientManager->sendNewProperty(svp);
                    return true;
                }

                case INDI_NUMBER:
                {
                    auto nvp = oneProp.getNumber();
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

                    m_ClientManager->sendNewProperty(nvp);
                    return true;
                }

                case INDI_TEXT:
                {
                    auto tvp = oneProp.getText();
                    for (const auto &oneElement : propElements)
                    {
                        QJsonObject oneElementObject = oneElement.toObject();
                        auto tp = tvp->findWidgetByName(oneElementObject["name"].toString().toLatin1().constData());
                        if (tp)
                            tp->setText(oneElementObject["text"].toString().toLatin1().constData());
                    }

                    m_ClientManager->sendNewProperty(tvp);
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
    for (auto oneProp : m_BaseDevice.getProperties())
    {
        if (propName == oneProp.getName())
        {
            switch (oneProp.getType())
            {
                case INDI_SWITCH:
                    switchToJson(oneProp, propObject, compact);
                    return true;

                case INDI_NUMBER:
                    numberToJson(oneProp, propObject, compact);
                    return true;

                case INDI_TEXT:
                    textToJson(oneProp, propObject, compact);
                    return true;

                case INDI_LIGHT:
                    lightToJson(oneProp, propObject, compact);
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
    auto blobProperty = m_BaseDevice.getProperty(propName.toLatin1().constData());
    if (!blobProperty.isValid())
        return false;

    auto oneBLOB = blobProperty.getBLOB()->findWidgetByName(elementName.toLatin1().constData());
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
    auto nvp = m_BaseDevice.getNumber("WATCHDOG_HEARTBEAT");

    if (nvp)
        // Send heartbeat to driver
        m_ClientManager->sendNewProperty(nvp);
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
        return dynamic_cast<ISD::Mount * >(m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].get());
    return nullptr;
}

ISD::Camera *GenericDevice::getCamera()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::CCD_INTERFACE))
        return dynamic_cast<ISD::Camera * >(m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].get());
    return nullptr;
}

ISD::Guider *GenericDevice::getGuider()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::GUIDER_INTERFACE))
        return dynamic_cast<ISD::Guider * >(m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].get());
    return nullptr;
}

ISD::Focuser *GenericDevice::getFocuser()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::FOCUSER_INTERFACE))
        return dynamic_cast<ISD::Focuser * >(m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].get());
    return nullptr;
}

ISD::FilterWheel *GenericDevice::getFilterWheel()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::FILTER_INTERFACE))
        return dynamic_cast<ISD::FilterWheel * >(m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].get());
    return nullptr;
}

ISD::Dome *GenericDevice::getDome()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DOME_INTERFACE))
        return dynamic_cast<ISD::Dome * >(m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].get());
    return nullptr;
}

ISD::GPS *GenericDevice::getGPS()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::GPS_INTERFACE))
        return dynamic_cast<ISD::GPS * >(m_ConcreteDevices[INDI::BaseDevice::GPS_INTERFACE].get());
    return nullptr;
}

ISD::Weather *GenericDevice::getWeather()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::WEATHER_INTERFACE))
        return dynamic_cast<ISD::Weather * >(m_ConcreteDevices[INDI::BaseDevice::WEATHER_INTERFACE].get());
    return nullptr;
}

ISD::AdaptiveOptics *GenericDevice::getAdaptiveOptics()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::AO_INTERFACE))
        return dynamic_cast<ISD::AdaptiveOptics * >(m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].get());
    return nullptr;
}

ISD::DustCap *GenericDevice::getDustCap()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DUSTCAP_INTERFACE))
        return dynamic_cast<ISD::DustCap * >(m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].get());
    return nullptr;
}

ISD::LightBox *GenericDevice::getLightBox()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::LIGHTBOX_INTERFACE))
        return dynamic_cast<ISD::LightBox * >(m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].get());
    return nullptr;
}

ISD::Detector *GenericDevice::getDetector()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::DETECTOR_INTERFACE))
        return dynamic_cast<ISD::Detector * >(m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].get());
    return nullptr;
}

ISD::Rotator *GenericDevice::getRotator()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::ROTATOR_INTERFACE))
        return dynamic_cast<ISD::Rotator * >(m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].get());
    return nullptr;
}

ISD::Spectrograph *GenericDevice::getSpectrograph()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::SPECTROGRAPH_INTERFACE))
        return dynamic_cast<ISD::Spectrograph * >(m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].get());
    return nullptr;
}

ISD::Correlator *GenericDevice::getCorrelator()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::CORRELATOR_INTERFACE))
        return dynamic_cast<ISD::Correlator * >(m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].get());
    return nullptr;
}

ISD::Auxiliary *GenericDevice::getAuxiliary()
{
    if (m_ConcreteDevices.contains(INDI::BaseDevice::AUX_INTERFACE))
        return dynamic_cast<ISD::Auxiliary * >(m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].get());
    return nullptr;
}

bool GenericDevice::generateDevices()
{
    auto generated = false;
    // Mount
    if (m_DriverInterface & INDI::BaseDevice::TELESCOPE_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].isNull())
    {
        auto mount = new ISD::Mount(this);
        mount->setObjectName("Mount:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::TELESCOPE_INTERFACE].reset(mount);
        mount->registeProperties();
        if (m_Connected)
        {
            mount->processProperties();
            mount->setProperty("dispatched", true);
            emit newMount(mount);
        }
        else
        {
            connect(mount, &ISD::ConcreteDevice::ready, this, [this, mount]()
            {
                if (!mount->property("dispatched").isValid())
                {
                    mount->setProperty("dispatched", true);
                    emit newMount(mount);
                }
            });
        }
    }

    // Camera
    if (m_DriverInterface & INDI::BaseDevice::CCD_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].isNull())
    {
        auto camera = new ISD::Camera(this);
        camera->setObjectName("Camera:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::CCD_INTERFACE].reset(camera);
        camera->registeProperties();
        if (m_Connected)
        {
            camera->processProperties();
            camera->setProperty("dispatched", true);
            emit newCamera(camera);
            emit ready();
        }
        else
        {
            connect(camera, &ISD::ConcreteDevice::ready, this, [this, camera]()
            {
                if (!camera->property("dispatched").isValid())
                {
                    camera->setProperty("dispatched", true);
                    emit newCamera(camera);
                }
            });
        }
    }

    // Guider
    if (m_DriverInterface & INDI::BaseDevice::GUIDER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].isNull())
    {
        auto guider = new ISD::Guider(this);
        guider->setObjectName("Guider:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::GUIDER_INTERFACE].reset(guider);
        guider->registeProperties();
        if (m_Connected)
        {
            guider->processProperties();
            guider->setProperty("dispatched", true);
            emit newGuider(guider);
        }
        else
        {
            connect(guider, &ISD::ConcreteDevice::ready, this, [this, guider]()
            {
                if (!guider->property("dispatched").isValid())
                {
                    guider->setProperty("dispatched", true);
                    emit newGuider(guider);
                }
            });
        }
    }

    // Focuser
    if (m_DriverInterface & INDI::BaseDevice::FOCUSER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].isNull())
    {
        auto focuser = new ISD::Focuser(this);
        focuser->setObjectName("Focuser:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::FOCUSER_INTERFACE].reset(focuser);
        focuser->registeProperties();
        if (m_Connected)
        {
            focuser->processProperties();
            focuser->setProperty("dispatched", true);
            emit newFocuser(focuser);
        }
        else
        {
            connect(focuser, &ISD::ConcreteDevice::ready, this, [this, focuser]()
            {
                if (!focuser->property("dispatched").isValid())
                {
                    focuser->setProperty("dispatched", true);
                    emit newFocuser(focuser);
                }
            });
        }
    }

    // Filter Wheel
    if (m_DriverInterface & INDI::BaseDevice::FILTER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].isNull())
    {
        auto filterWheel = new ISD::FilterWheel(this);
        filterWheel->setObjectName("FilterWheel:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::FILTER_INTERFACE].reset(filterWheel);
        filterWheel->registeProperties();
        if (m_Connected)
        {
            filterWheel->processProperties();
            filterWheel->setProperty("dispatched", true);
            emit newFilterWheel(filterWheel);
        }
        else
        {
            connect(filterWheel, &ISD::ConcreteDevice::ready, this, [this, filterWheel]()
            {
                if (!filterWheel->property("dispatched").isValid())
                {
                    filterWheel->setProperty("dispatched", true);
                    emit newFilterWheel(filterWheel);
                }
            });
        }
    }

    // Dome
    if (m_DriverInterface & INDI::BaseDevice::DOME_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].isNull())
    {
        auto dome = new ISD::Dome(this);
        dome->setObjectName("Dome:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::DOME_INTERFACE].reset(dome);
        dome->registeProperties();
        if (m_Connected)
        {
            dome->processProperties();
            dome->setProperty("dispatched", true);
            emit newDome(dome);
        }
        else
        {
            connect(dome, &ISD::ConcreteDevice::ready, this, [this, dome]()
            {
                if (!dome->property("dispatched").isValid())
                {
                    dome->setProperty("dispatched", true);
                    emit newDome(dome);
                }
            });
        }
    }

    // GPS
    if (m_DriverInterface & INDI::BaseDevice::GPS_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::GPS_INTERFACE].isNull())
    {
        auto gps = new ISD::GPS(this);
        gps->setObjectName("GPS:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::GPS_INTERFACE].reset(gps);
        gps->registeProperties();
        if (m_Connected)
        {
            gps->processProperties();
            gps->setProperty("dispatched", true);
            emit newGPS(gps);
        }
        else
        {
            connect(gps, &ISD::ConcreteDevice::ready, this, [this, gps]()
            {
                if (!gps->property("dispatched").isValid())
                {
                    gps->setProperty("dispatched", true);
                    emit newGPS(gps);
                }
            });
        }
    }

    // Weather
    if (m_DriverInterface & INDI::BaseDevice::WEATHER_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::WEATHER_INTERFACE].isNull())
    {
        auto weather = new ISD::Weather(this);
        weather->setObjectName("Weather:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::WEATHER_INTERFACE].reset(weather);
        weather->registeProperties();
        if (m_Connected)
        {
            weather->processProperties();
            weather->setProperty("dispatched", true);
            emit newWeather(weather);
        }
        else
        {
            connect(weather, &ISD::ConcreteDevice::ready, this, [this, weather]()
            {
                if (!weather->property("dispatched").isValid())
                {
                    weather->setProperty("dispatched", true);
                    emit newWeather(weather);
                }
            });
        }
    }

    // Adaptive Optics
    if (m_DriverInterface & INDI::BaseDevice::AO_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].isNull())
    {
        auto ao = new ISD::AdaptiveOptics(this);
        ao->setObjectName("AdaptiveOptics:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::AO_INTERFACE].reset(ao);
        ao->registeProperties();
        if (m_Connected)
        {
            ao->processProperties();
            ao->setProperty("dispatched", true);
            emit newAdaptiveOptics(ao);
        }
        else
        {
            connect(ao, &ISD::ConcreteDevice::ready, this, [this, ao]()
            {
                if (!ao->property("dispatched").isValid())
                {
                    ao->setProperty("dispatched", true);
                    emit newAdaptiveOptics(ao);
                }
            });
        }
    }

    // Dust Cap
    if (m_DriverInterface & INDI::BaseDevice::DUSTCAP_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].isNull())
    {
        auto dustCap = new ISD::DustCap(this);
        dustCap->setObjectName("DustCap:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::DUSTCAP_INTERFACE].reset(dustCap);
        dustCap->registeProperties();
        if (m_Connected)
        {
            dustCap->processProperties();
            dustCap->setProperty("dispatched", true);
            emit newDustCap(dustCap);
        }
        else
        {
            connect(dustCap, &ISD::ConcreteDevice::ready, this, [this, dustCap]()
            {
                if (!dustCap->property("dispatched").isValid())
                {
                    dustCap->setProperty("dispatched", true);
                    emit newDustCap(dustCap);
                }
            });
        }
    }

    // Light box
    if (m_DriverInterface & INDI::BaseDevice::LIGHTBOX_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].isNull())
    {
        auto lightBox = new ISD::LightBox(this);
        lightBox->setObjectName("LightBox:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::LIGHTBOX_INTERFACE].reset(lightBox);
        lightBox->registeProperties();
        if (m_Connected)
        {
            lightBox->processProperties();
            lightBox->setProperty("dispatched", true);
            emit newLightBox(lightBox);
        }
        else
        {
            connect(lightBox, &ISD::ConcreteDevice::ready, this, [this, lightBox]()
            {
                if (!lightBox->property("dispatched").isValid())
                {
                    lightBox->setProperty("dispatched", true);
                    emit newLightBox(lightBox);
                }
            });
        }
    }

    // Rotator
    if (m_DriverInterface & INDI::BaseDevice::ROTATOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].isNull())
    {
        auto rotator = new ISD::Rotator(this);
        rotator->setObjectName("Rotator:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::ROTATOR_INTERFACE].reset(rotator);
        rotator->registeProperties();
        if (m_Connected)
        {
            rotator->processProperties();
            rotator->setProperty("dispatched", true);
            emit newRotator(rotator);
        }
        else
        {
            connect(rotator, &ISD::ConcreteDevice::ready, this, [this, rotator]()
            {
                if (!rotator->property("dispatched").isValid())
                {
                    rotator->setProperty("dispatched", true);
                    emit newRotator(rotator);
                }
            });
        }
    }

    // Detector
    if (m_DriverInterface & INDI::BaseDevice::DETECTOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].isNull())
    {
        auto detector = new ISD::Detector(this);
        detector->setObjectName("Detector:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::DETECTOR_INTERFACE].reset(detector);
        detector->registeProperties();
        if (m_Connected)
        {
            detector->processProperties();
            detector->setProperty("dispatched", true);
            emit newDetector(detector);
        }
        else
        {
            connect(detector, &ISD::ConcreteDevice::ready, this, [this, detector]()
            {
                if (!detector->property("dispatched").isValid())
                {
                    detector->setProperty("dispatched", true);
                    emit newDetector(detector);
                }
            });
        }
    }

    // Spectrograph
    if (m_DriverInterface & INDI::BaseDevice::SPECTROGRAPH_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].isNull())
    {
        auto spectrograph = new ISD::Spectrograph(this);
        spectrograph->setObjectName("Spectrograph:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::SPECTROGRAPH_INTERFACE].reset(spectrograph);
        spectrograph->registeProperties();
        if (m_Connected)
        {
            spectrograph->processProperties();
            spectrograph->setProperty("dispatched", true);
            emit newSpectrograph(spectrograph);
        }
        else
        {
            connect(spectrograph, &ISD::ConcreteDevice::ready, this, [this, spectrograph]()
            {
                if (!spectrograph->property("dispatched").isValid())
                {
                    spectrograph->setProperty("dispatched", true);
                    emit newSpectrograph(spectrograph);
                }
            });
        }
    }

    // Correlator
    if (m_DriverInterface & INDI::BaseDevice::CORRELATOR_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].isNull())
    {
        auto correlator = new ISD::Correlator(this);
        correlator->setObjectName("Correlator:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::CORRELATOR_INTERFACE].reset(correlator);
        correlator->registeProperties();
        if (m_Connected)
        {
            correlator->processProperties();
            correlator->setProperty("dispatched", true);
            emit newCorrelator(correlator);
        }
        else
        {
            connect(correlator, &ISD::ConcreteDevice::ready, this, [this, correlator]()
            {
                if (!correlator->property("dispatched").isValid())
                {
                    correlator->setProperty("dispatched", true);
                    emit newCorrelator(correlator);
                }
            });
        }
    }

    // Auxiliary
    if (m_DriverInterface & INDI::BaseDevice::AUX_INTERFACE &&
            m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].isNull())
    {
        auto aux = new ISD::Auxiliary(this);
        aux->setObjectName("Auxiliary:" + objectName());
        generated = true;
        m_ConcreteDevices[INDI::BaseDevice::AUX_INTERFACE].reset(aux);
        aux->registeProperties();
        if (m_Connected)
        {
            aux->processProperties();
            aux->setProperty("dispatched", true);
            emit newAuxiliary(aux);
        }
        else
        {
            connect(aux, &ISD::ConcreteDevice::ready, this, [this, aux]()
            {
                if (!aux->property("dispatched").isValid())
                {
                    aux->setProperty("dispatched", true);
                    emit newAuxiliary(aux);
                }
            });
        }
    }

    return generated;
}

void GenericDevice::sendNewProperty(INDI::Property prop)
{
    m_ClientManager->sendNewProperty(prop);
}

void switchToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    auto svp = prop.getSwitch();
    QJsonArray switches;
    for (int i = 0; i < svp->count(); i++)
    {
        QJsonObject oneSwitch = {{"name", svp->at(i)->getName()}, {"state", svp->at(i)->getState()}};
        if (!compact)
            oneSwitch.insert("label", svp->at(i)->getLabel());
        switches.append(oneSwitch);
    }

    propObject = {{"device", svp->getDeviceName()}, {"name", svp->getName()}, {"state", svp->getState()}, {"switches", switches}};
    if (!compact)
    {
        propObject.insert("label", svp->getLabel());
        propObject.insert("group", svp->getGroupName());
        propObject.insert("perm", svp->getPermission());
        propObject.insert("rule", svp->getRule());
    }
}

void numberToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    auto nvp = prop.getNumber();
    QJsonArray numbers;
    for (int i = 0; i < nvp->count(); i++)
    {
        QJsonObject oneNumber = {{"name", nvp->at(i)->getName()}, {"value", nvp->at(i)->getValue()}};
        if (!compact)
        {
            oneNumber.insert("label", nvp->at(i)->getLabel());
            oneNumber.insert("min", nvp->at(i)->getMin());
            oneNumber.insert("max", nvp->at(i)->getMax());
            oneNumber.insert("step", nvp->at(i)->getStep());
            oneNumber.insert("format", nvp->at(i)->getFormat());
        }
        numbers.append(oneNumber);
    }

    propObject = {{"device", nvp->getDeviceName()}, {"name", nvp->getName()}, {"state", nvp->getState()}, {"numbers", numbers}};
    if (!compact)
    {
        propObject.insert("label", nvp->getLabel());
        propObject.insert("group", nvp->getGroupName());
        propObject.insert("perm", nvp->getPermission());
    }
}

void textToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    auto tvp = prop.getText();
    QJsonArray Texts;
    for (int i = 0; i < tvp->count(); i++)
    {
        QJsonObject oneText = {{"name", tvp->at(i)->getName()}, {"text", tvp->at(i)->getText()}};
        if (!compact)
        {
            oneText.insert("label", tvp->at(i)->getLabel());
        }
        Texts.append(oneText);
    }

    propObject = {{"device", tvp->getDeviceName()}, {"name", tvp->getName()}, {"state", tvp->getState()}, {"texts", Texts}};
    if (!compact)
    {
        propObject.insert("label", tvp->getLabel());
        propObject.insert("group", tvp->getGroupName());
        propObject.insert("perm", tvp->getPermission());
    }
}

void lightToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    auto lvp = prop.getLight();
    QJsonArray Lights;
    for (int i = 0; i < lvp->count(); i++)
    {
        QJsonObject oneLight = {{"name", lvp->at(i)->getName()}, {"state", lvp->at(i)->getState()}};
        if (!compact)
        {
            oneLight.insert("label", lvp->at(i)->getLabel());
        }
        Lights.append(oneLight);
    }

    propObject = {{"device", lvp->getDeviceName()}, {"name", lvp->getName()}, {"state", lvp->getState()}, {"lights", Lights}};
    if (!compact)
    {
        propObject.insert("label", lvp->getLabel());
        propObject.insert("group", lvp->getGroupName());
    }
}

void propertyToJson(INDI::Property prop, QJsonObject &propObject, bool compact)
{
    switch (prop.getType())
    {
        case INDI_SWITCH:
            switchToJson(prop, propObject, compact);
            break;
        case INDI_TEXT:
            textToJson(prop, propObject, compact);
            break;
        case INDI_NUMBER:
            numberToJson(prop, propObject, compact);
            break;
        case INDI_LIGHT:
            lightToJson(prop, propObject, compact);
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
