/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indimount.h"

#include "ksmessagebox.h"
#include "driverinfo.h"
#include "kstars.h"
#include "Options.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "ksnotification.h"

#include <KActionCollection>

#include <QAction>
#include <QtDBus/qdbusmetatype.h>

#include <indi_debug.h>

// Qt version calming
#include <qtendl.h>

namespace ISD
{

const QList<KLocalizedString> Mount::mountStates = { ki18n("Idle"),  ki18n("Moving"), ki18n("Slewing"),
                                                     ki18n("Tracking"), ki18n("Parking"), ki18n("Parked"),
                                                     ki18n("Error")
                                                   };

Mount::Mount(GenericDevice *parent) : ConcreteDevice(parent)
{
    // Set it for 5 seconds for now as not to spam the display update
    centerLockTimer.setInterval(5000);
    centerLockTimer.setSingleShot(true);
    connect(&centerLockTimer, &QTimer::timeout, this, [this]()
    {
        //runCommand(INDI_CENTER_LOCK);
        centerLock();
    });

    // Regularly update the coordinates even if no update has been sent from the INDI service
    updateCoordinatesTimer.setInterval(1000);
    updateCoordinatesTimer.setSingleShot(false);
    connect(&updateCoordinatesTimer, &QTimer::timeout, this, [this]()
    {
        if (isConnected())
        {
            currentCoords.EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
            emit newCoords(currentCoords, pierSide(), hourAngle());
        }
    });

    qRegisterMetaType<ISD::Mount::Status>("ISD::Mount::Status");
    qDBusRegisterMetaType<ISD::Mount::Status>();

    qRegisterMetaType<ISD::Mount::PierSide>("ISD::Mount::PierSide");
    qDBusRegisterMetaType<ISD::Mount::PierSide>();

    // Need to delay check for alignment model to upon connection is established since the property is defined BEFORE Telescope class is created.
    // and therefore no registerProperty is called for these properties since they were already registered _before_ the Telescope
    // class was created.
    m_hasAlignmentModel = getProperty("ALIGNMENT_POINTSET_ACTION").isValid() || getProperty("ALIGNLIST").isValid();
}

void Mount::registerProperty(INDI::Property prop)
{
    if (prop.isNameMatch("TELESCOPE_INFO"))
    {
        auto ti = prop.getNumber();

        if (!ti)
            return;

        bool aperture_ok = false, focal_ok = false;
        double temp = 0;

        auto aperture = ti->findWidgetByName("TELESCOPE_APERTURE");
        if (aperture && aperture->getValue() <= 0)
        {
            if (getDriverInfo()->getAuxInfo().contains("TELESCOPE_APERTURE"))
            {
                temp = getDriverInfo()->getAuxInfo().value("TELESCOPE_APERTURE").toDouble(&aperture_ok);
                if (aperture_ok)
                {
                    aperture->setValue(temp);
                    auto g_aperture = ti->findWidgetByName("GUIDER_APERTURE");
                    if (g_aperture && g_aperture->getValue() <= 0)
                        g_aperture->setValue(aperture->getValue());
                }
            }
        }

        auto focal_length = ti->findWidgetByName("TELESCOPE_FOCAL_LENGTH");
        if (focal_length && focal_length->getValue() <= 0)
        {
            if (getDriverInfo()->getAuxInfo().contains("TELESCOPE_FOCAL_LENGTH"))
            {
                temp = getDriverInfo()->getAuxInfo().value("TELESCOPE_FOCAL_LENGTH").toDouble(&focal_ok);
                if (focal_ok)
                {
                    focal_length->setValue(temp);
                    auto g_focal = ti->findWidgetByName("GUIDER_FOCAL_LENGTH");
                    if (g_focal && g_focal->getValue() <= 0)
                        g_focal->setValue(focal_length->getValue());
                }
            }
        }

        if (aperture_ok && focal_ok)
            sendNewProperty(ti);
    }
    else if (prop.isNameMatch("ON_COORD_SET"))
    {
        m_canGoto = IUFindSwitch(prop.getSwitch(), "TRACK") != nullptr;
        m_canSync = IUFindSwitch(prop.getSwitch(), "SYNC") != nullptr;
        m_canFlip = IUFindSwitch(prop.getSwitch(), "FLIP") != nullptr;
    }
    else if (prop.isNameMatch("TELESCOPE_PIER_SIDE"))
    {
        auto svp = prop.getSwitch();
        int currentSide = svp->findOnSwitchIndex();
        if (currentSide != m_PierSide)
        {
            m_PierSide = static_cast<PierSide>(currentSide);
            emit pierSideChanged(m_PierSide);
        }
    }
    else if (prop.isNameMatch("TELESCOPE_PARK"))
        updateParkStatus();
    else if (prop.isNameMatch("TELESCOPE_TRACK_STATE"))
        m_canControlTrack = true;
    else if (prop.isNameMatch("TELESCOPE_TRACK_MODE"))
    {
        m_hasTrackModes = true;
        auto svp = prop.getSwitch();
        for (int i = 0; i < svp->count(); i++)
        {
            if (svp->at(i)->isNameMatch("TRACK_SIDEREAL"))
                TrackMap[TRACK_SIDEREAL] = i;
            else if (svp->at(i)->isNameMatch("TRACK_SOLAR"))
                TrackMap[TRACK_SOLAR] = i;
            else if (svp->at(i)->isNameMatch("TRACK_LUNAR"))
                TrackMap[TRACK_LUNAR] = i;
            else if (svp->at(i)->isNameMatch("TRACK_CUSTOM"))
                TrackMap[TRACK_CUSTOM] = i;
        }
    }
    else if (prop.isNameMatch("TELESCOPE_TRACK_RATE"))
        m_hasCustomTrackRate = true;
    else if (prop.isNameMatch("TELESCOPE_ABORT_MOTION"))
        m_canAbort = true;
    else if (prop.isNameMatch("TELESCOPE_PARK_OPTION"))
        m_hasCustomParking = true;
    else if (prop.isNameMatch("TELESCOPE_SLEW_RATE"))
    {
        m_hasSlewRates = true;
        auto svp = prop.getSwitch();
        if (svp)
        {
            m_slewRates.clear();
            for (const auto &it : *svp)
                m_slewRates << it.getLabel();
        }
    }
    else if (prop.isNameMatch("EQUATORIAL_EOD_COORD"))
    {
        m_isJ2000 = false;
        m_hasEquatorialCoordProperty = true;
    }
    else if (prop.isNameMatch("SAT_TRACKING_STAT"))
    {
        m_canTrackSatellite = true;
    }
    else if (prop.isNameMatch("EQUATORIAL_COORD"))
    {
        m_isJ2000 = true;
        m_hasEquatorialCoordProperty = true;
    }
}

void Mount::updateJ2000Coordinates(SkyPoint *coords)
{
    SkyPoint J2000Coord(coords->ra(), coords->dec());
    J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
    coords->setRA0(J2000Coord.ra());
    coords->setDec0(J2000Coord.dec());
}

void ISD::Mount::updateTarget()
{
    emit newTarget(currentCoords);
    double maxrad = 0.1;
    currentObject = KStarsData::Instance()->skyComposite()->objectNearest(&currentCoords, maxrad);
    if (currentObject)
        emit newTargetName(currentObject->name());
    // If there is no object, we must clear target as it might give wrong
    // indication we are still on it.
    else
        emit newTargetName(QString());
}

void Mount::processNumber(INDI::Property prop)
{
    auto nvp = prop.getNumber();
    if (nvp->isNameMatch("EQUATORIAL_EOD_COORD") || nvp->isNameMatch("EQUATORIAL_COORD"))
    {
        auto RA  = nvp->findWidgetByName("RA");
        auto DEC = nvp->findWidgetByName("DEC");

        if (RA == nullptr || DEC == nullptr)
            return;

        // set both JNow and J2000 coordinates
        if (isJ2000())
        {
            currentCoords.setRA0(RA->value);
            currentCoords.setDec0(DEC->value);
            currentCoords.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        }
        else
        {
            currentCoords.setRA(RA->value);
            currentCoords.setDec(DEC->value);
            // calculate J2000 coordinates
            updateJ2000Coordinates(&currentCoords);
        }

        // calculate horizontal coordinates
        currentCoords.EquatorialToHorizontal(KStars::Instance()->data()->lst(),
                                             KStars::Instance()->data()->geo()->lat());
        // ensure that coordinates are regularly updated
        if (! updateCoordinatesTimer.isActive())
            updateCoordinatesTimer.start();

        // update current status
        auto currentStatus = status(nvp);

        if (nvp->getState() == IPS_BUSY && EqCoordPreviousState != IPS_BUSY)
        {
            if (currentStatus == MOUNT_SLEWING)
                KSNotification::event(QLatin1String("SlewStarted"), i18n("Mount is slewing to target location"), KSNotification::Mount);
        }
        else if (EqCoordPreviousState == IPS_BUSY && nvp->getState() == IPS_OK && slewDefined())
        {
            if (Options::useExternalSkyMap())
            {
                // For external skymaps the only way to determine the target is to take the position where the mount
                // starts to track
                updateTarget();
            }
            else
            {
                // In case that we use KStars as skymap, we intentionally do not communicate the target here, since it
                // has been set at the beginning of the slew AND we cannot be sure that the position the INDI
                // mount reports when starting to track is exactly that one where the slew went to.
                KSNotification::event(QLatin1String("SlewCompleted"), i18n("Mount arrived at target location"), KSNotification::Mount);
            }
        }

        EqCoordPreviousState = nvp->getState();

        KStars::Instance()->map()->update();
    }
    // JM 2022.03.11 Only process HORIZONTAL_COORD if it was the ONLY source of information
    // When a driver both sends EQUATORIAL_COORD and HORIZONTAL_COORD, we should prioritize EQUATORIAL_COORD
    // especially since the conversion from horizontal to equatorial is not as accurate and can result in weird
    // coordinates near the poles.
    else if (nvp->isNameMatch("HORIZONTAL_COORD") && m_hasEquatorialCoordProperty == false)
    {
        auto Az  = nvp->findWidgetByName("AZ");
        auto Alt = nvp->findWidgetByName("ALT");

        if (Az == nullptr || Alt == nullptr)
            return;

        currentCoords.setAz(Az->value);
        currentCoords.setAlt(Alt->value);
        currentCoords.HorizontalToEquatorial(KStars::Instance()->data()->lst(),
                                             KStars::Instance()->data()->geo()->lat());

        // calculate J2000 coordinates
        updateJ2000Coordinates(&currentCoords);

        // ensure that coordinates are regularly updated
        if (! updateCoordinatesTimer.isActive())
            updateCoordinatesTimer.start();

        KStars::Instance()->map()->update();
    }
    else if (nvp->isNameMatch("POLLING_PERIOD"))
    {
        // set the timer how often the coordinates should be published
        auto period = nvp->findWidgetByName("PERIOD_MS");
        if (period != nullptr)
            updateCoordinatesTimer.setInterval(static_cast<int>(period->getValue()));

    }
}

void Mount::processSwitch(INDI::Property prop)
{
    bool manualMotionChanged = false;
    auto svp = prop.getSwitch();

    if (svp->isNameMatch("CONNECTION"))
    {
        auto conSP = svp->findWidgetByName("CONNECT");
        if (conSP)
        {
            // TODO We must allow for multiple mount drivers to be online, not just one
            // For the actions taken, the user should be able to specify which mounts shall receive the commands. It could be one
            // or more. For now, we enable/disable telescope group on the assumption there is only one mount present.
            if (conSP->getState() == ISS_ON)
                KStars::Instance()->slotSetTelescopeEnabled(true);
            else
            {
                KStars::Instance()->slotSetTelescopeEnabled(false);
                centerLockTimer.stop();
            }
        }
    }
    else if (svp->isNameMatch("TELESCOPE_PARK"))
        updateParkStatus();
    else if (svp->isNameMatch("TELESCOPE_ABORT_MOTION"))
    {
        if (svp->s == IPS_OK)
        {
            inCustomParking = false;
            KSNotification::event(QLatin1String("MountAborted"), i18n("Mount motion was aborted"), KSNotification::Mount,
                                  KSNotification::Warn);
        }
    }
    else if (svp->isNameMatch("TELESCOPE_PIER_SIDE"))
    {
        int currentSide = IUFindOnSwitchIndex(svp);
        if (currentSide != m_PierSide)
        {
            m_PierSide = static_cast<PierSide>(currentSide);
            emit pierSideChanged(m_PierSide);
        }
    }
    else if (svp->isNameMatch("TELESCOPE_TRACK_MODE"))
    {
        auto sp = svp->findOnSwitch();
        if (sp)
        {
            if (sp->isNameMatch("TRACK_SIDEREAL"))
                currentTrackMode = TRACK_SIDEREAL;
            else if (sp->isNameMatch("TRACK_SOLAR"))
                currentTrackMode = TRACK_SOLAR;
            else if (sp->isNameMatch("TRACK_LUNAR"))
                currentTrackMode = TRACK_LUNAR;
            else
                currentTrackMode = TRACK_CUSTOM;
        }
    }
    else if (svp->isNameMatch("TELESCOPE_MOTION_NS"))
        manualMotionChanged = true;
    else if (svp->isNameMatch("TELESCOPE_MOTION_WE"))
        manualMotionChanged = true;
    else if (svp->isNameMatch("TELESCOPE_REVERSE_MOTION"))
    {
        emit axisReversed(AXIS_DE, svp->at(0)->getState() == ISS_ON);
        emit axisReversed(AXIS_RA, svp->at(1)->getState() == ISS_ON);
    }

    if (manualMotionChanged)
    {
        auto NSCurrentMotion = getSwitch("TELESCOPE_MOTION_NS")->getState();
        auto WECurrentMotion = getSwitch("TELESCOPE_MOTION_WE")->getState();
        inCustomParking = false;
        inManualMotion = (NSCurrentMotion == IPS_BUSY || WECurrentMotion == IPS_BUSY);
    }
}

void Mount::processText(INDI::Property prop)
{
    auto tvp = prop.getText();
    if (tvp->isNameMatch("SAT_TLE_TEXT"))
    {
        if ((tvp->getState() == IPS_OK) && (m_TLEIsSetForTracking))
        {
            auto trajWindow = getText("SAT_PASS_WINDOW");
            if (!trajWindow)
            {
                qCDebug(KSTARS_INDI) << "Property SAT_PASS_WINDOW not found";
            }
            else
            {
                auto trajStart = trajWindow->findWidgetByName("SAT_PASS_WINDOW_START");
                auto trajEnd = trajWindow->findWidgetByName("SAT_PASS_WINDOW_END");

                if (!trajStart || !trajEnd)
                {
                    qCDebug(KSTARS_INDI) << "Start or end in SAT_PASS_WINDOW not found";
                }
                else
                {
                    trajStart->setText(g_satPassStart.toString(Qt::ISODate).toLocal8Bit().data());
                    trajEnd->setText(g_satPassEnd.toString(Qt::ISODate).toLocal8Bit().data());

                    sendNewProperty(trajWindow);
                    m_windowIsSetForTracking = true;
                }
            }
        }
    }
    else if (tvp->isNameMatch("SAT_PASS_WINDOW"))
    {
        if ((tvp->getState() == IPS_OK) && (m_TLEIsSetForTracking) && (m_windowIsSetForTracking))
        {
            auto trackSwitchV  = getSwitch("SAT_TRACKING_STAT");
            if (!trackSwitchV)
            {
                qCDebug(KSTARS_INDI) << "Property SAT_TRACKING_STAT not found";
            }
            else
            {
                auto trackSwitch = trackSwitchV->findWidgetByName("SAT_TRACK");
                if (trackSwitch)
                {
                    trackSwitchV->reset();
                    trackSwitch->setState(ISS_ON);

                    sendNewProperty(trackSwitchV);
                    m_TLEIsSetForTracking = false;
                    m_windowIsSetForTracking = false;
                }
            }
        }
    }
}

void Mount::updateParkStatus()
{
    auto svp = getSwitch("TELESCOPE_PARK");
    if (!svp)
        return;

    auto sp = svp->findWidgetByName("PARK");
    if (sp)
    {
        if (svp->getState() == IPS_ALERT)
        {
            // First, inform everyone watch this that an error occurred.
            emit newParkStatus(PARK_ERROR);
            // JM 2021-03-08: Reset parking internal state to either PARKED or UNPARKED.
            // Whatever the current switch is set to
            m_ParkStatus = (sp->getState() == ISS_ON) ? PARK_PARKED : PARK_UNPARKED;
            KSNotification::event(QLatin1String("MountParkingFailed"), i18n("Mount parking failed"), KSNotification::Mount,
                                  KSNotification::Alert);
        }
        else if (svp->getState() == IPS_BUSY && sp->s == ISS_ON && m_ParkStatus != PARK_PARKING)
        {
            m_ParkStatus = PARK_PARKING;
            KSNotification::event(QLatin1String("MountParking"), i18n("Mount parking is in progress"), KSNotification::Mount);
            currentObject = nullptr;

            emit newParkStatus(m_ParkStatus);
        }
        else if (svp->getState() == IPS_BUSY && sp->getState() == ISS_OFF && m_ParkStatus != PARK_UNPARKING)
        {
            m_ParkStatus = PARK_UNPARKING;
            KSNotification::event(QLatin1String("MountUnParking"), i18n("Mount unparking is in progress"), KSNotification::Mount);

            emit newParkStatus(m_ParkStatus);
        }
        else if (svp->getState() == IPS_OK && sp->getState() == ISS_ON && m_ParkStatus != PARK_PARKED)
        {
            m_ParkStatus = PARK_PARKED;
            KSNotification::event(QLatin1String("MountParked"), i18n("Mount parked"), KSNotification::Mount);
            currentObject = nullptr;

            emit newParkStatus(m_ParkStatus);

            QAction *parkAction = KStars::Instance()->actionCollection()->action("telescope_park");
            if (parkAction)
                parkAction->setEnabled(false);
            QAction *unParkAction = KStars::Instance()->actionCollection()->action("telescope_unpark");
            if (unParkAction)
                unParkAction->setEnabled(true);

            emit newTarget(currentCoords);
            emit newTargetName(QString());
        }
        else if ( (svp->getState() == IPS_OK || svp->getState() == IPS_IDLE) && sp->getState() == ISS_OFF
                  && m_ParkStatus != PARK_UNPARKED)
        {
            m_ParkStatus = PARK_UNPARKED;
            KSNotification::event(QLatin1String("MountUnparked"), i18n("Mount unparked"), KSNotification::Mount);
            currentObject = nullptr;

            emit newParkStatus(m_ParkStatus);

            QAction *parkAction = KStars::Instance()->actionCollection()->action("telescope_park");
            if (parkAction)
                parkAction->setEnabled(true);
            QAction *unParkAction = KStars::Instance()->actionCollection()->action("telescope_unpark");
            if (unParkAction)
                unParkAction->setEnabled(false);
        }
    }
}
bool Mount::canGuide()
{
    auto raPulse  = getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = getNumber("TELESCOPE_TIMED_GUIDE_NS");

    return raPulse && decPulse;
}

bool Mount::canPark()
{
    auto parkSP = getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    return (parkSW != nullptr);
}

bool Mount::isSlewing()
{
    auto EqProp = getNumber("EQUATORIAL_EOD_COORD");

    if (!EqProp)
        return false;

    return (EqProp->getState() == IPS_BUSY);
}

bool Mount::isInMotion()
{
    return (isSlewing() || inManualMotion);
}

bool Mount::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (canGuide() == false)
        return false;

    bool raOK  = doPulse(ra_dir, ra_msecs);
    bool decOK = doPulse(dec_dir, dec_msecs);

    return raOK && decOK;
}

bool Mount::doPulse(GuideDirection dir, int msecs)
{
    auto raPulse  = getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = getNumber("TELESCOPE_TIMED_GUIDE_NS");
    INDI::PropertyView<INumber> *npulse   = nullptr;
    INDI::WidgetView<INumber>   *dirPulse = nullptr;

    if (!raPulse || !decPulse)
        return false;

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
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_N");
            break;

        case DEC_DEC_DIR:
            npulse = decPulse;
            dirPulse = npulse->findWidgetByName("TIMED_GUIDE_S");
            break;

        default:
            return false;
    }

    if (!dirPulse)
        return false;

    dirPulse->setValue(msecs);

    sendNewProperty(npulse);

    return true;
}


void Mount::setCustomParking(SkyPoint * coords)
{
    bool rc = false;
    if (coords == nullptr)
        rc = sendCoords(KStars::Instance()->map()->clickedPoint());
    else
        rc = sendCoords(coords);

    inCustomParking = rc;
}

void Mount::find()
{
    updateJ2000Coordinates(&currentCoords);
    double maxrad = 1000.0 / Options::zoomFactor();
    SkyObject *currentObject = KStarsData::Instance()->skyComposite()->objectNearest(&currentCoords, maxrad);
    KStars::Instance()->map()->setFocusObject(currentObject);
    KStars::Instance()->map()->setDestination(currentCoords);
}
void Mount::centerLock()
{
    if (Options::isTracking() == false ||
            currentCoords.angularDistanceTo(KStars::Instance()->map()->focus()).Degrees() > 0.5)
    {
        updateJ2000Coordinates(&currentCoords);
        KStars::Instance()->map()->setDestination(currentCoords);
        KStars::Instance()->map()->setFocusPoint(&currentCoords);
        KStars::Instance()->map()->setFocusObject(nullptr);
        Options::setIsTracking(true);
    }
    centerLockTimer.start();
}

void Mount::centerUnlock()
{
    KStars::Instance()->map()->stopTracking();
    centerLockTimer.stop();
}

bool Mount::sendCoords(SkyPoint * ScopeTarget)
{
    double currentRA = 0, currentDEC = 0, currentAlt = 0, currentAz = 0;
    bool useJ2000 {false}, useEquatorialCoordinates {false}, useHorizontalCoordinates {false};

    // Can we use Equatorial Coordinates?
    // Check EOD (JNow) first.
    auto EqProp = getNumber("EQUATORIAL_EOD_COORD");
    // If not found or found but only READ-ONLY permission, check J2000 property.
    if (!EqProp.isValid() || EqProp.getPermission() == IP_RO)
    {
        // Find J2000 Property and its permission
        auto J2000EQNP = getNumber("EQUATORIAL_COORD");
        if (J2000EQNP.isValid() && EqProp.getPermission() != IP_RO)
        {
            EqProp = J2000EQNP;
            useEquatorialCoordinates = true;
            useJ2000 = true;
        }
    }
    else
        useEquatorialCoordinates = true;

    // Can we use Horizontal Coordinates?
    auto HorProp = getNumber("HORIZONTAL_COORD");
    if (HorProp.isValid() && HorProp.getPermission() != IP_RO)
        useHorizontalCoordinates = true;

    //qDebug() << Q_FUNC_INFO << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

    if (useEquatorialCoordinates)
    {
        currentRA  = EqProp.findWidgetByName("RA")->getValue();
        currentDEC = EqProp.findWidgetByName("DEC")->getValue();

        ScopeTarget->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    }

    if (useHorizontalCoordinates)
    {
        currentAz  = HorProp.findWidgetByName("AZ")->getValue();
        currentAlt = HorProp.findWidgetByName("ALT")->getValue();
    }

    /* Could not find either properties! */
    if (useEquatorialCoordinates == false && useHorizontalCoordinates == false)
        return false;

    // Function for sending the coordinates to the INDI mount device
    // via the ClientManager. This helper function translates EKOS objects into INDI commands.
    auto sendToMountDevice = [ = ]()
    {
        // communicate the new target only if a slew will be executed for the given coordinates
        if (slewDefined())
        {
            emit newTarget(*ScopeTarget);
            if (currentObject)
                emit newTargetName(currentObject->name());
            // If there is no object, we must clear target as it might give wrong
            // indication we are still on it.
            else
                emit newTargetName(QString());
        }

        if (useEquatorialCoordinates)
        {
            dms ra, de;

            if (useJ2000)
            {
                // If we have invalid DEC, then convert coords to J2000
                if (ScopeTarget->dec0().Degrees() == 180.0)
                {
                    ScopeTarget->setRA0(ScopeTarget->ra());
                    ScopeTarget->setDec0(ScopeTarget->dec());
                    ScopeTarget->catalogueCoord( KStars::Instance()->data()->ut().djd());
                    ra = ScopeTarget->ra();
                    de = ScopeTarget->dec();
                }
                else
                {
                    ra = ScopeTarget->ra0();
                    de = ScopeTarget->dec0();
                }
            }
            else
            {
                ra = ScopeTarget->ra();
                de = ScopeTarget->dec();
            }

            EqProp.findWidgetByName("RA")->setValue(ra.Hours());
            EqProp.findWidgetByName("DEC")->setValue(de.Degrees());
            sendNewProperty(EqProp);

            qCDebug(KSTARS_INDI) << "ISD:Telescope sending coords RA:" << ra.toHMSString() << "DE:" << de.toDMSString();

            EqProp.findWidgetByName("RA")->setValue(currentRA);
            EqProp.findWidgetByName("DEC")->setValue(currentDEC);
        }
        // Only send Horizontal Coord property if Equatorial is not available.
        else if (useHorizontalCoordinates)
        {
            HorProp.findWidgetByName("AZ")->setValue(ScopeTarget->az().Degrees());
            HorProp.findWidgetByName("ALT")->setValue(ScopeTarget->alt().Degrees());
            sendNewProperty(HorProp);
            HorProp.findWidgetByName("AZ")->setValue(currentAz);
            HorProp.findWidgetByName("ALT")->setValue(currentAlt);
        }

    };

    // Helper function that first checks for the selected target object whether the
    // tracking modes have to be adapted (special cases moon and sun), explicitely warns before
    // slewing to the sun and finally (independant whether there exists a target object
    // for the target coordinates) calls sendToMountDevice
    auto checkObjectAndSend = [ = ]()
    {
        // Search within 0.1 degrees indepdent of zoom level.
        double maxrad = 0.1;
        currentObject = KStarsData::Instance()->skyComposite()->objectNearest(ScopeTarget, maxrad);
        if (currentObject)
        {
            auto checkTrackModes = [ = ]()
            {
                if (m_hasTrackModes)
                {
                    // Tracking Moon
                    if (currentObject->type() == SkyObject::MOON)
                    {
                        if (currentTrackMode != TRACK_LUNAR && TrackMap.contains(TRACK_LUNAR))
                            setTrackMode(TrackMap.value(TRACK_LUNAR));
                    }
                    // Tracking Sun
                    else if (currentObject->name() == i18n("Sun"))
                    {
                        if (currentTrackMode != TRACK_SOLAR && TrackMap.contains(TRACK_SOLAR))
                            setTrackMode(TrackMap.value(TRACK_SOLAR));
                    }
                    // If Last track mode was either set to SOLAR or LUNAR but now we are slewing to a different object
                    // then we automatically fallback to sidereal. If the current track mode is CUSTOM or something else, nothing
                    // changes.
                    else if (currentTrackMode == TRACK_SOLAR || currentTrackMode == TRACK_LUNAR)
                        setTrackMode(TRACK_SIDEREAL);

                }
            };

            // Sun Warning, but don't ask if tracking is already solar.
            if (currentObject->name() == i18n("Sun") && currentTrackMode != TRACK_SOLAR)
            {
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                    checkTrackModes();
                    sendToMountDevice();
                });
                connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [ = ]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                });

                KSMessageBox::Instance()->questionYesNo(
                    i18n("Warning! Looking at the Sun without proper protection can lead to irreversible eye damage!"),
                    i18n("Sun Warning"));
            }
            else
            {
                checkTrackModes();
                sendToMountDevice();
            }
        }
        else
            sendToMountDevice();
    };

    // If altitude limits is enabled, then reject motion immediately.
    double targetAlt = ScopeTarget->altRefracted().Degrees();

    if ((-90 <= minAlt && maxAlt <= 90) && (targetAlt < minAlt || targetAlt > maxAlt) && !altLimitsTrackingOnly)
    {
        KSNotification::event(QLatin1String("IndiServerMessage"),
                              i18n("Requested altitude %1 is outside the specified altitude limit boundary (%2,%3).",
                                   QString::number(targetAlt, 'g', 3), QString::number(minAlt, 'g', 3),
                                   QString::number(maxAlt, 'g', 3)), KSNotification::Mount, KSNotification::Warn);
        qCInfo(KSTARS_INDI) << "Requested altitude " << QString::number(targetAlt, 'g', 3)
                            << " is outside the specified altitude limit boundary ("
                            << QString::number(minAlt, 'g', 3) << "," << QString::number(maxAlt, 'g', 3) << ").";
        return false;
    }

    // If disabled, then check if below horizon and warning the user unless the user previously dismissed it.
    if (Options::confirmBelowHorizon() && targetAlt < 0 && minAlt == -1)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
        {
            if (minAlt < -90 && +90 < maxAlt)
                Options::setConfirmBelowHorizon(false);
            KSMessageBox::Instance()->disconnect(this);
            checkObjectAndSend();
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            if (useEquatorialCoordinates)
            {
                EqProp.findWidgetByName("RA")->setValue(currentRA);
                EqProp.findWidgetByName("DEC")->setValue(currentDEC);
            }
            if (useHorizontalCoordinates)
            {
                HorProp.findWidgetByName("AZ")->setValue(currentAz);
                HorProp.findWidgetByName("ALT")->setValue(currentAlt);
            }
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Requested altitude is below the horizon. Are you sure you want to proceed?"),
                                                i18n("Telescope Motion"), 15, false);
    }
    else
        checkObjectAndSend();

    return true;
}

bool Mount::slewDefined()
{
    auto motionSP = getSwitch("ON_COORD_SET");

    if (!motionSP.isValid())
        return false;
    // A slew will happen if either Track, Slew, or Flip
    // is selected
    auto sp = motionSP.findOnSwitch();
    if(sp != nullptr &&
            (sp->name == std::string("TRACK") ||
             sp->name == std::string("SLEW") ||
             sp->name == std::string("FLIP")))
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool Mount::Slew(double ra, double dec, bool flip)
{
    SkyPoint target;

    if (m_isJ2000)
    {
        target.setRA0(ra);
        target.setDec0(dec);
    }
    else
    {
        target.setRA(ra);
        target.setDec(dec);
    }

    return Slew(&target, flip);
}

bool Mount::Slew(SkyPoint * ScopeTarget, bool flip)
{
    auto motionSP = getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto slewSW = flip ? motionSP->findWidgetByName("FLIP") : motionSP->findWidgetByName("TRACK");

    if (flip && (!slewSW))
        slewSW = motionSP->findWidgetByName("TRACK");

    if (!slewSW)
        slewSW = motionSP->findWidgetByName("SLEW");

    if (!slewSW)
        return false;

    if (slewSW->getState() != ISS_ON)
    {
        motionSP->reset();
        slewSW->setState(ISS_ON);
        sendNewProperty(motionSP);

        qCDebug(KSTARS_INDI) << "ISD:Telescope: " << slewSW->getName();
    }

    return sendCoords(ScopeTarget);
}

bool Mount::Sync(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);

    return Sync(&target);
}

bool Mount::Sync(SkyPoint * ScopeTarget)
{
    auto motionSP = getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto syncSW = motionSP->findWidgetByName("SYNC");

    if (!syncSW)
        return false;

    if (syncSW->getState() != ISS_ON)
    {
        motionSP->reset();
        syncSW->setState(ISS_ON);
        sendNewProperty(motionSP);

        qCDebug(KSTARS_INDI) << "ISD:Telescope: Syncing...";
    }

    return sendCoords(ScopeTarget);
}

bool Mount::abort()
{
    auto motionSP = getSwitch("TELESCOPE_ABORT_MOTION");

    if (!motionSP)
        return false;

    auto abortSW = motionSP->findWidgetByName("ABORT");

    if (!abortSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: Aborted." << Qt::endl;

    abortSW->setState(ISS_ON);
    sendNewProperty(motionSP);

    inCustomParking = false;

    return true;
}

bool Mount::park()
{
    auto parkSP = getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    if (!parkSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: Parking..." << Qt::endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

bool Mount::unpark()
{
    auto parkSP = getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");

    if (!parkSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: UnParking..." << Qt::endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    sendNewProperty(parkSP);

    return true;
}

bool Mount::getEqCoords(double * ra, double * dec)
{
    auto EqProp = getNumber("EQUATORIAL_EOD_COORD");
    if (!EqProp)
    {
        EqProp = getNumber("EQUATORIAL_COORD");
        if (!EqProp)
            return false;
    }

    auto RAEle = EqProp->findWidgetByName("RA");
    if (!RAEle)
        return false;

    auto DecEle = EqProp->findWidgetByName("DEC");
    if (!DecEle)
        return false;

    *ra  = RAEle->getValue();
    *dec = DecEle->getValue();

    return true;
}

bool Mount::MoveNS(VerticalMotion dir, MotionCommand cmd)
{
    auto motionSP = getSwitch("TELESCOPE_MOTION_NS");

    if (!motionSP)
        return false;

    auto motionNorth = motionSP->findWidgetByName("MOTION_NORTH");
    auto motionSouth = motionSP->findWidgetByName("MOTION_SOUTH");

    if (!motionNorth || !motionSouth)
        return false;

    // If same direction, return
    if (dir == MOTION_NORTH && motionNorth->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    if (dir == MOTION_SOUTH && motionSouth->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    motionSP->reset();

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_NORTH)
            motionNorth->setState(ISS_ON);
        else
            motionSouth->setState(ISS_ON);
    }

    sendNewProperty(motionSP);

    return true;
}

bool Mount::StopWE()
{
    auto motionSP = getSwitch("TELESCOPE_MOTION_WE");

    if (!motionSP)
        return false;

    motionSP->reset();

    sendNewProperty(motionSP);

    return true;
}

bool Mount::StopNS()
{
    auto motionSP = getSwitch("TELESCOPE_MOTION_NS");

    if (!motionSP)
        return false;

    motionSP->reset();

    sendNewProperty(motionSP);

    return true;
}

bool Mount::MoveWE(HorizontalMotion dir, MotionCommand cmd)
{
    auto motionSP = getSwitch("TELESCOPE_MOTION_WE");

    if (!motionSP)
        return false;

    auto motionWest = motionSP->findWidgetByName("MOTION_WEST");
    auto motionEast = motionSP->findWidgetByName("MOTION_EAST");

    if (!motionWest || !motionEast)
        return false;

    // If same direction, return
    if (dir == MOTION_WEST && motionWest->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    if (dir == MOTION_EAST && motionEast->getState() == ((cmd == MOTION_START) ? ISS_ON : ISS_OFF))
        return true;

    motionSP->reset();

    if (cmd == MOTION_START)
    {
        if (dir == MOTION_WEST)
            motionWest->setState(ISS_ON);
        else
            motionEast->setState(ISS_ON);
    }

    sendNewProperty(motionSP);

    return true;
}

bool Mount::setSlewRate(int index)
{
    auto slewRateSP = getSwitch("TELESCOPE_SLEW_RATE");

    if (!slewRateSP)
        return false;

    if (index < 0 || index > slewRateSP->count())
        return false;
    else if (slewRateSP->findOnSwitchIndex() == index)
        return true;

    slewRateSP->reset();

    slewRateSP->at(index)->setState(ISS_ON);

    sendNewProperty(slewRateSP);

    emit slewRateChanged(index);

    return true;
}

int Mount::getSlewRate() const
{
    auto slewRateSP = getSwitch("TELESCOPE_SLEW_RATE");

    if (!slewRateSP)
        return -1;

    return slewRateSP->findOnSwitchIndex();
}

void Mount::setAltLimits(double minAltitude, double maxAltitude, bool trackingOnly)
{
    minAlt = minAltitude;
    maxAlt = maxAltitude;
    altLimitsTrackingOnly = trackingOnly;
}

bool Mount::setAlignmentModelEnabled(bool enable)
{
    bool wasExecuted                   = false;

    // For INDI Alignment Subsystem
    auto alignSwitch = getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE");
    if (alignSwitch)
    {
        alignSwitch->at(0)->setState(enable ? ISS_ON : ISS_OFF);
        sendNewProperty(alignSwitch);
        wasExecuted = true;
    }

    // For EQMod Alignment --- Temporary until all drivers switch fully to INDI Alignment Subsystem
    alignSwitch = getSwitch("ALIGNMODE");
    if (alignSwitch)
    {
        alignSwitch->reset();
        // For now, always set alignment mode to NSTAR on enable.
        if (enable)
            alignSwitch->at(2)->setState(ISS_ON);
        // Otherwise, set to NO ALIGN
        else
            alignSwitch->at(0)->setState(ISS_ON);

        sendNewProperty(alignSwitch);
        wasExecuted = true;
    }

    return wasExecuted;
}

bool Mount::setSatelliteTLEandTrack(QString tle, const KStarsDateTime satPassStart, const KStarsDateTime satPassEnd)
{
    auto tleTextVec = getText("SAT_TLE_TEXT");
    if (!tleTextVec)
    {
        qCDebug(KSTARS_INDI) << "Property SAT_TLE_TEXT not found";
        return false;
    }

    auto tleText = tleTextVec->findWidgetByName("TLE");
    if (!tleText)
        return false;

    tleText->setText(tle.toLocal8Bit().data());

    sendNewProperty(tleTextVec);
    m_TLEIsSetForTracking = true;
    g_satPassStart = satPassStart;
    g_satPassEnd = satPassEnd;
    return true;
    // See Telescope::processText for the following steps (setting window and switch)
}


bool Mount::clearParking()
{
    auto parkSwitch  = getSwitch("TELESCOPE_PARK_OPTION");
    if (!parkSwitch)
        return false;

    auto clearParkSW = parkSwitch->findWidgetByName("PARK_PURGE_DATA");
    if (!clearParkSW)
        return false;

    parkSwitch->reset();
    clearParkSW->setState(ISS_ON);

    sendNewProperty(parkSwitch);
    return true;
}

bool Mount::clearAlignmentModel()
{
    bool wasExecuted = false;

    // Note: Should probably use INDI Alignment Subsystem Client API in the future?
    auto clearSwitch  = getSwitch("ALIGNMENT_POINTSET_ACTION");
    auto commitSwitch = getSwitch("ALIGNMENT_POINTSET_COMMIT");
    if (clearSwitch && commitSwitch)
    {
        clearSwitch->reset();
        // ALIGNMENT_POINTSET_ACTION.CLEAR
        clearSwitch->at(4)->setState(ISS_ON);
        sendNewProperty(clearSwitch);
        commitSwitch->at(0)->setState(ISS_ON);
        sendNewProperty(commitSwitch);
        wasExecuted = true;
    }

    // For EQMod Alignment --- Temporary until all drivers switch fully to INDI Alignment Subsystem
    clearSwitch = getSwitch("ALIGNLIST");
    if (clearSwitch)
    {
        // ALIGNLISTCLEAR
        clearSwitch->reset();
        clearSwitch->at(1)->setState(ISS_ON);
        sendNewProperty(clearSwitch);
        wasExecuted = true;
    }

    return wasExecuted;
}

Mount::Status Mount::status()
{
    auto EqProp = getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == nullptr)
    {
        EqProp = getNumber("EQUATORIAL_COORD");
        if (EqProp == nullptr)
            return MOUNT_ERROR;
    }

    return status(EqProp);
}

const QString Mount::statusString(Mount::Status status, bool translated) const
{
    switch (status)
    {
        case ISD::Mount::MOUNT_MOVING:
            return (translated ? mountStates[status].toString() : mountStates[status].untranslatedText() + QString(" %1").arg(
                        getManualMotionString()));
        default:
            return translated ? mountStates[status].toString() : mountStates[status].untranslatedText();
    }
}

QString Mount::getManualMotionString() const
{
    QString NSMotion, WEMotion;

    auto movementSP = getSwitch("TELESCOPE_MOTION_NS");
    if (movementSP)
    {
        if (movementSP->at(MOTION_NORTH)->getState() == ISS_ON)
            NSMotion = 'N';
        else if (movementSP->at(MOTION_SOUTH)->getState() == ISS_ON)
            NSMotion = 'S';
    }

    movementSP = getSwitch("TELESCOPE_MOTION_WE");
    if (movementSP)
    {
        if (movementSP->at(MOTION_WEST)->getState() == ISS_ON)
            WEMotion = 'W';
        else if (movementSP->at(MOTION_EAST)->getState() == ISS_ON)
            WEMotion = 'E';
    }

    return QString("%1%2").arg(NSMotion, WEMotion);
}

bool Mount::setTrackEnabled(bool enable)
{
    auto trackSP = getSwitch("TELESCOPE_TRACK_STATE");
    if (!trackSP)
        return false;

    auto trackON  = trackSP->findWidgetByName("TRACK_ON");
    auto trackOFF = trackSP->findWidgetByName("TRACK_OFF");

    if (!trackON || !trackOFF)
        return false;

    trackON->setState(enable ? ISS_ON : ISS_OFF);
    trackOFF->setState(enable ? ISS_OFF : ISS_ON);

    sendNewProperty(trackSP);

    return true;
}

bool Mount::isTracking()
{
    return (status() == MOUNT_TRACKING);
}

bool Mount::setTrackMode(uint8_t index)
{
    auto trackModeSP = getSwitch("TELESCOPE_TRACK_MODE");
    if (!trackModeSP)
        return false;

    if (index >= trackModeSP->nsp)
        return false;

    trackModeSP->reset();
    trackModeSP->at(index)->setState(ISS_ON);

    sendNewProperty(trackModeSP);

    return true;
}

bool Mount::getTrackMode(uint8_t &index)
{
    auto trackModeSP = getSwitch("TELESCOPE_TRACK_MODE");
    if (!trackModeSP)
        return false;

    index = trackModeSP->findOnSwitchIndex();

    return true;
}

bool Mount::setCustomTrackRate(double raRate, double deRate)
{
    auto trackRateNP = getNumber("TELESCOPE_TRACK_RATE");
    if (!trackRateNP)
        return false;

    auto raRateN = trackRateNP->findWidgetByName("TRACK_RATE_RA");
    auto deRateN = trackRateNP->findWidgetByName("TRACK_RATE_DE");

    if (!raRateN || !deRateN)
        return false;

    raRateN->setValue(raRate);
    deRateN->setValue(deRate);

    sendNewProperty(trackRateNP);

    return true;
}

bool Mount::getCustomTrackRate(double &raRate, double &deRate)
{
    auto trackRateNP = getNumber("TELESCOPE_TRACK_RATE");
    if (!trackRateNP)
        return false;

    auto raRateN = trackRateNP->findWidgetByName("TRACK_RATE_RA");
    auto deRateN = trackRateNP->findWidgetByName("TRACK_RATE_DE");

    if (!raRateN || !deRateN)
        return false;

    raRate = raRateN->getValue();
    deRate = deRateN->getValue();

    return true;

}

bool Mount::sendParkingOptionCommand(ParkOptionCommand command)
{
    auto parkOptionsSP = getSwitch("TELESCOPE_PARK_OPTION");
    if (!parkOptionsSP)
        return false;

    parkOptionsSP->reset();
    parkOptionsSP->at(command)->setState(ISS_ON);
    sendNewProperty(parkOptionsSP);

    return true;
}

Mount::Status Mount::status(INumberVectorProperty * nvp)
{
    Status newMountStatus = MOUNT_ERROR;
    switch (nvp->s)
    {
        case IPS_IDLE:
            if (inManualMotion)
                newMountStatus = MOUNT_MOVING;
            else if (isParked())
                newMountStatus = MOUNT_PARKED;
            else
                newMountStatus = MOUNT_IDLE;
            break;

        case IPS_OK:
            if (inManualMotion)
                newMountStatus = MOUNT_MOVING;
            else if (inCustomParking)
            {
                inCustomParking = false;
                // set CURRENT position as the desired parking position
                sendParkingOptionCommand(PARK_OPTION_CURRENT);
                // Write data to disk
                sendParkingOptionCommand(PARK_OPTION_WRITE_DATA);

                newMountStatus = MOUNT_TRACKING;
            }
            else
                newMountStatus = MOUNT_TRACKING;
            break;

        case IPS_BUSY:
            if (inManualMotion)
                newMountStatus = MOUNT_MOVING;
            else
            {
                auto parkSP = getSwitch("TELESCOPE_PARK");
                if (parkSP && parkSP->getState() == IPS_BUSY)
                    newMountStatus = MOUNT_PARKING;
                else
                    newMountStatus = MOUNT_SLEWING;
            }
            break;

        case IPS_ALERT:
            inCustomParking = false;
            newMountStatus = MOUNT_ERROR;
    }

    if (previousMountStatus != newMountStatus)
        emit newStatus(newMountStatus);

    previousMountStatus = newMountStatus;
    return newMountStatus;
}

const dms Mount::hourAngle() const
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    return dms(lst.Degrees() - currentCoords.ra().Degrees());
}

bool Mount::isReversed(INDI_EQ_AXIS axis)
{
    auto reversed = getSwitch("TELESCOPE_REVERSE_MOTION");
    if (!reversed)
        return false;

    return reversed->at(axis == AXIS_DE ? 0 : 1)->getState() == ISS_ON;
}

bool Mount::setReversedEnabled(INDI_EQ_AXIS axis, bool enabled)
{
    auto reversed = getSwitch("TELESCOPE_REVERSE_MOTION");
    if (!reversed)
        return false;

    reversed->at(axis == AXIS_DE ? 0 : 1)->setState(enabled ? ISS_ON : ISS_OFF);
    sendNewProperty(reversed);
    return true;
}

void Mount::stopTimers()
{
    updateCoordinatesTimer.stop();
    centerLockTimer.stop();
}

}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Mount::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Mount::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Mount::Status>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Mount::PierSide &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Mount::PierSide &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Mount::PierSide>(a);
    return argument;
}

