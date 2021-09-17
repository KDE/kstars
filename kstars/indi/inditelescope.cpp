/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "inditelescope.h"

#include "ksmessagebox.h"
#include "clientmanager.h"
#include "driverinfo.h"
#include "indidevice.h"
#include "kstars.h"
#include "Options.h"
#include "skymap.h"
#include "skymapcomposite.h"
#include "ksnotification.h"

#include <KActionCollection>

#include <QAction>

#include <indi_debug.h>

namespace ISD
{
Telescope::Telescope(GDInterface *iPtr) : DeviceDecorator(iPtr)
{
    dType                = KSTARS_TELESCOPE;
    minAlt               = -1;
    maxAlt               = -1;
    EqCoordPreviousState = IPS_IDLE;

    // Set it for 5 seconds for now as not to spam the display update
    centerLockTimer.setInterval(5000);
    centerLockTimer.setSingleShot(true);
    connect(&centerLockTimer, &QTimer::timeout, this, [this]()
    {
        runCommand(INDI_CENTER_LOCK);
    });

    // If after 250ms no new properties are registered then emit ready
    readyTimer.setInterval(250);
    readyTimer.setSingleShot(true);
    connect(&readyTimer, &QTimer::timeout, this, &Telescope::ready);

    qRegisterMetaType<ISD::Telescope::Status>("ISD::Telescope::Status");
    qDBusRegisterMetaType<ISD::Telescope::Status>();

    qRegisterMetaType<ISD::Telescope::PierSide>("ISD::Telescope::PierSide");
    qDBusRegisterMetaType<ISD::Telescope::PierSide>();

    // Need to delay check for alignment model to upon connection is established since the property is defined BEFORE Telescope class is created.
    // and therefore no registerProperty is called for these properties since they were already registered _before_ the Telescope
    // class was created.
    m_hasAlignmentModel = getProperty("ALIGNMENT_POINTSET_ACTION").isValid() || getProperty("ALIGNLIST").isValid();
}

void Telescope::registerProperty(INDI::Property prop)
{
    if (isConnected())
        readyTimer.start();

    if (prop->isNameMatch("TELESCOPE_INFO"))
    {
        auto ti = prop->getNumber();

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
            clientManager->sendNewNumber(ti);
    }
    else if (prop->isNameMatch("ON_COORD_SET"))
    {
        m_canGoto = IUFindSwitch(prop->getSwitch(), "TRACK") != nullptr;
        m_canSync = IUFindSwitch(prop->getSwitch(), "SYNC") != nullptr;
    }
    // Telescope Park
    else if (prop->isNameMatch("TELESCOPE_PARK"))
    {
        auto svp = prop->getSwitch();

        if (svp)
        {
            auto sp = svp->findWidgetByName("PARK");
            if (sp)
            {
                if ((sp->getState() == ISS_ON) && svp->getState() == IPS_OK)
                {
                    m_ParkStatus = PARK_PARKED;
                    emit newParkStatus(m_ParkStatus);

                    QAction *parkAction = KStars::Instance()->actionCollection()->action("telescope_park");
                    if (parkAction)
                        parkAction->setEnabled(false);
                    QAction *unParkAction = KStars::Instance()->actionCollection()->action("telescope_unpark");
                    if (unParkAction)
                        unParkAction->setEnabled(true);
                }
                else if ((sp->getState() == ISS_OFF) && svp->getState() == IPS_OK)
                {
                    m_ParkStatus = PARK_UNPARKED;
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
    }
    else if (prop->isNameMatch("TELESCOPE_PIER_SIDE"))
    {
        auto svp = prop->getSwitch();
        int currentSide = svp->findOnSwitchIndex();
        if (currentSide != m_PierSide)
        {
            m_PierSide = static_cast<PierSide>(currentSide);
            emit pierSideChanged(m_PierSide);
        }
    }
    else if (prop->isNameMatch("TELESCOPE_TRACK_STATE"))
        m_canControlTrack = true;
    else if (prop->isNameMatch("TELESCOPE_TRACK_MODE"))
    {
        m_hasTrackModes = true;
        auto svp = prop->getSwitch();
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
    else if (prop->isNameMatch("TELESCOPE_TRACK_RATE"))
        m_hasCustomTrackRate = true;
    else if (prop->isNameMatch("TELESCOPE_ABORT_MOTION"))
        m_canAbort = true;
    else if (prop->isNameMatch("TELESCOPE_PARK_OPTION"))
        m_hasCustomParking = true;
    else if (prop->isNameMatch("TELESCOPE_SLEW_RATE"))
    {
        m_hasSlewRates = true;
        auto svp = prop->getSwitch();
        if (svp)
        {
            m_slewRates.clear();
            for (const auto &it : *svp)
                m_slewRates << it.getLabel();
        }
    }
    else if (prop->isNameMatch("EQUATORIAL_EOD_COORD"))
    {
        m_isJ2000 = false;
    }
    else if (prop->isNameMatch("SAT_TRACKING_STAT"))
    {
        m_canTrackSatellite = true;
    }
    else if (prop->isNameMatch("EQUATORIAL_COORD"))
    {
        m_isJ2000 = true;
    }

    DeviceDecorator::registerProperty(prop);
}

void Telescope::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "EQUATORIAL_EOD_COORD") || !strcmp(nvp->name, "EQUATORIAL_COORD"))
    {
        INumber *RA  = IUFindNumber(nvp, "RA");
        INumber *DEC = IUFindNumber(nvp, "DEC");

        if (RA == nullptr || DEC == nullptr)
            return;

        currentCoord.setRA(RA->value);
        currentCoord.setDec(DEC->value);

        // If J2000, convert it to JNow
        if (!strcmp(nvp->name, "EQUATORIAL_COORD"))
        {
            currentCoord.setRA0(RA->value);
            currentCoord.setDec0(DEC->value);
            currentCoord.apparentCoord(static_cast<long double>(J2000), KStars::Instance()->data()->ut().djd());
        }

        currentCoord.EquatorialToHorizontal(KStars::Instance()->data()->lst(),
                                            KStars::Instance()->data()->geo()->lat());

        if (nvp->s == IPS_BUSY && EqCoordPreviousState != IPS_BUSY)
        {
            if (status() != MOUNT_PARKING)
                KSNotification::event(QLatin1String("SlewStarted"), i18n("Mount is slewing to target location"));
        }
        else if (EqCoordPreviousState == IPS_BUSY && nvp->s == IPS_OK)
        {
            KSNotification::event(QLatin1String("SlewCompleted"), i18n("Mount arrived at target location"));

            double maxrad = 1000.0 / Options::zoomFactor();

            currentObject = KStarsData::Instance()->skyComposite()->objectNearest(&currentCoord, maxrad);
            if (currentObject != nullptr)
                emit newTarget(currentObject->name());
        }

        EqCoordPreviousState = nvp->s;

        KStars::Instance()->map()->update();
    }
    else if (!strcmp(nvp->name, "HORIZONTAL_COORD"))
    {
        INumber *Az  = IUFindNumber(nvp, "AZ");
        INumber *Alt = IUFindNumber(nvp, "ALT");

        if (Az == nullptr || Alt == nullptr)
            return;

        currentCoord.setAz(Az->value);
        currentCoord.setAlt(Alt->value);
        currentCoord.HorizontalToEquatorial(KStars::Instance()->data()->lst(),
                                            KStars::Instance()->data()->geo()->lat());

        KStars::Instance()->map()->update();
    }

    DeviceDecorator::processNumber(nvp);
}

void Telescope::processSwitch(ISwitchVectorProperty *svp)
{
    bool manualMotionChanged = false;

    if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *conSP = IUFindSwitch(svp, "CONNECT");
        if (conSP)
        {
            // TODO We must allow for multiple mount drivers to be online, not just one
            // For the actions taken, the user should be able to specify which mounts shall receive the commands. It could be one
            // or more. For now, we enable/disable telescope group on the assumption there is only one mount present.
            if (isConnected() == false && conSP->s == ISS_ON)
                KStars::Instance()->slotSetTelescopeEnabled(true);
            else if (isConnected() && conSP->s == ISS_OFF)
            {
                KStars::Instance()->slotSetTelescopeEnabled(false);
                centerLockTimer.stop();
            }
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_PARK"))
    {
        ISwitch *sp = IUFindSwitch(svp, "PARK");
        if (sp)
        {
            if (svp->s == IPS_ALERT)
            {
                // First, inform everyone watch this that an error occurred.
                emit newParkStatus(PARK_ERROR);
                // JM 2021-03-08: Reset parking internal state to either PARKED or UNPARKED.
                // Whatever the current switch is set to
                m_ParkStatus = (sp->s == ISS_ON) ? PARK_PARKED : PARK_UNPARKED;
                KSNotification::event(QLatin1String("MountParkingFailed"), i18n("Mount parking failed"), KSNotification::EVENT_ALERT);
            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_ON && m_ParkStatus != PARK_PARKING)
            {
                m_ParkStatus = PARK_PARKING;
                KSNotification::event(QLatin1String("MountParking"), i18n("Mount parking is in progress"));
                currentObject = nullptr;

                emit newParkStatus(m_ParkStatus);
            }
            else if (svp->s == IPS_BUSY && sp->s == ISS_OFF && m_ParkStatus != PARK_UNPARKING)
            {
                m_ParkStatus = PARK_UNPARKING;
                KSNotification::event(QLatin1String("MountUnParking"), i18n("Mount unparking is in progress"));

                emit newParkStatus(m_ParkStatus);
            }
            else if (svp->s == IPS_OK && sp->s == ISS_ON && m_ParkStatus != PARK_PARKED)
            {
                m_ParkStatus = PARK_PARKED;
                KSNotification::event(QLatin1String("MountParked"), i18n("Mount parked"));
                currentObject = nullptr;

                emit newParkStatus(m_ParkStatus);

                QAction *parkAction = KStars::Instance()->actionCollection()->action("telescope_park");
                if (parkAction)
                    parkAction->setEnabled(false);
                QAction *unParkAction = KStars::Instance()->actionCollection()->action("telescope_unpark");
                if (unParkAction)
                    unParkAction->setEnabled(true);

                emit newTarget(QString());
            }
            else if ( (svp->s == IPS_OK || svp->s == IPS_IDLE) && sp->s == ISS_OFF && m_ParkStatus != PARK_UNPARKED)
            {
                m_ParkStatus = PARK_UNPARKED;
                KSNotification::event(QLatin1String("MountUnparked"), i18n("Mount unparked"));
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
    else if (!strcmp(svp->name, "TELESCOPE_ABORT_MOTION"))
    {
        if (svp->s == IPS_OK)
        {
            inCustomParking = false;
            KSNotification::event(QLatin1String("MountAborted"), i18n("Mount motion was aborted"), KSNotification::EVENT_WARN);
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_PIER_SIDE"))
    {
        int currentSide = IUFindOnSwitchIndex(svp);
        if (currentSide != m_PierSide)
        {
            m_PierSide = static_cast<PierSide>(currentSide);
            emit pierSideChanged(m_PierSide);
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_TRACK_MODE"))
    {
        ISwitch *sp = IUFindOnSwitch(svp);
        if (sp)
        {
            if (!strcmp(sp->name, "TRACK_SIDEREAL"))
                currentTrackMode = TRACK_SIDEREAL;
            else if (!strcmp(sp->name, "TRACK_SOLAR"))
                currentTrackMode = TRACK_SOLAR;
            else if (!strcmp(sp->name, "TRACK_LUNAR"))
                currentTrackMode = TRACK_LUNAR;
            else
                currentTrackMode = TRACK_CUSTOM;
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_MOTION_NS"))
        manualMotionChanged = true;
    else if (!strcmp(svp->name, "TELESCOPE_MOTION_WE"))
        manualMotionChanged = true;

    if (manualMotionChanged)
    {
        IPState NSCurrentMotion, WECurrentMotion;

        NSCurrentMotion = baseDevice->getSwitch("TELESCOPE_MOTION_NS")->s;
        WECurrentMotion = baseDevice->getSwitch("TELESCOPE_MOTION_WE")->s;

        inCustomParking = false;

        if (NSCurrentMotion == IPS_BUSY || WECurrentMotion == IPS_BUSY || NSPreviousState == IPS_BUSY ||
                WEPreviousState == IPS_BUSY)
        {
            if (inManualMotion == false && ((NSCurrentMotion == IPS_BUSY && NSPreviousState != IPS_BUSY) ||
                                            (WECurrentMotion == IPS_BUSY && WEPreviousState != IPS_BUSY)))
            {
                inManualMotion = true;
                //KSNotification::event(QLatin1String("MotionStarted"), i18n("Mount is manually moving"));
            }
            else if (inManualMotion && ((NSCurrentMotion != IPS_BUSY && NSPreviousState == IPS_BUSY) ||
                                        (WECurrentMotion != IPS_BUSY && WEPreviousState == IPS_BUSY)))
            {
                inManualMotion = false;
                //KSNotification::event(QLatin1String("MotionStopped"), i18n("Mount motion stopped"));
            }

            NSPreviousState = NSCurrentMotion;
            WEPreviousState = WECurrentMotion;
        }
    }

    DeviceDecorator::processSwitch(svp);
}

void Telescope::processText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "SAT_TLE_TEXT"))
    {
        if ((tvp->s == IPS_OK) && (m_TLEIsSetForTracking))
        {
            auto trajWindow = baseDevice->getText("SAT_PASS_WINDOW");
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

                    clientManager->sendNewText(trajWindow);
                    m_windowIsSetForTracking = true;
                }
            }
        }
    }
    else if (!strcmp(tvp->name, "SAT_PASS_WINDOW"))
    {
        if ((tvp->s == IPS_OK) && (m_TLEIsSetForTracking) && (m_windowIsSetForTracking))
        {
            auto trackSwitchV  = baseDevice->getSwitch("SAT_TRACKING_STAT");
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

                    clientManager->sendNewSwitch(trackSwitchV);
                    m_TLEIsSetForTracking = false;
                    m_windowIsSetForTracking = false;
                }
            }
        }
    }
    DeviceDecorator::processText(tvp);
}

bool Telescope::canGuide()
{
    auto raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");

    return raPulse && decPulse;
}


bool Telescope::canPark()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    return (parkSW != nullptr);
}

bool Telescope::isSlewing()
{
    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");

    if (!EqProp)
        return false;

    return (EqProp->getState() == IPS_BUSY);
}

bool Telescope::isInMotion()
{
    return (isSlewing() || inManualMotion);
}

bool Telescope::doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs)
{
    if (canGuide() == false)
        return false;

    bool raOK  = doPulse(ra_dir, ra_msecs);
    bool decOK = doPulse(dec_dir, dec_msecs);

    return raOK && decOK;
}

bool Telescope::doPulse(GuideDirection dir, int msecs)
{
    auto raPulse  = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_WE");
    auto decPulse = baseDevice->getNumber("TELESCOPE_TIMED_GUIDE_NS");
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

    clientManager->sendNewNumber(npulse);

    //qDebug() << "Sending pulse for " << npulse->getName() << " in direction " << dirPulse->getName() << " for " << msecs << " ms " << endl;

    return true;
}

bool Telescope::runCommand(int command, void *ptr)
{
    //qDebug() << "Telescope run command is called!!!" << endl;

    switch (command)
    {
        // set pending based on the outcome of send coords
        case INDI_CUSTOM_PARKING:
        {
            bool rc = false;
            if (ptr == nullptr)
                rc = sendCoords(KStars::Instance()->map()->clickedPoint());
            else
                rc = sendCoords(static_cast<SkyPoint *>(ptr));

            inCustomParking = rc;
        }
        break;


        case INDI_SEND_COORDS:
            if (ptr == nullptr)
                sendCoords(KStars::Instance()->map()->clickedPoint());
            else
                sendCoords(static_cast<SkyPoint *>(ptr));

            break;

        case INDI_FIND_TELESCOPE:
        {
            SkyPoint J2000Coord(currentCoord.ra(), currentCoord.dec());
            J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
            currentCoord.setRA0(J2000Coord.ra());
            currentCoord.setDec0(J2000Coord.dec());
            double maxrad = 1000.0 / Options::zoomFactor();
            SkyObject *currentObject = KStarsData::Instance()->skyComposite()->objectNearest(&currentCoord, maxrad);
            KStars::Instance()->map()->setFocusObject(currentObject);
            KStars::Instance()->map()->setDestination(currentCoord);
        }
        break;

        case INDI_CENTER_LOCK:
        {
            //if (currentObject == nullptr || KStars::Instance()->map()->focusObject() != currentObject)
            if (Options::isTracking() == false ||
                    currentCoord.angularDistanceTo(KStars::Instance()->map()->focus()).Degrees() > 0.5)
            {
                SkyPoint J2000Coord(currentCoord.ra(), currentCoord.dec());
                J2000Coord.catalogueCoord(KStars::Instance()->data()->ut().djd());
                currentCoord.setRA0(J2000Coord.ra());
                currentCoord.setDec0(J2000Coord.dec());
                //KStars::Instance()->map()->setClickedPoint(&currentCoord);
                //KStars::Instance()->map()->slotCenter();
                KStars::Instance()->map()->setDestination(currentCoord);
                KStars::Instance()->map()->setFocusPoint(&currentCoord);
                //KStars::Instance()->map()->setFocusObject(currentObject);
                KStars::Instance()->map()->setFocusObject(nullptr);
                Options::setIsTracking(true);
            }
            centerLockTimer.start();
        }
        break;

        case INDI_CENTER_UNLOCK:
            KStars::Instance()->map()->stopTracking();
            centerLockTimer.stop();
            break;

        default:
            return DeviceDecorator::runCommand(command, ptr);
    }

    return true;
}

bool Telescope::sendCoords(SkyPoint *ScopeTarget)
{
    INumber *RAEle                 = nullptr;
    INumber *DecEle                = nullptr;
    INumber *AzEle                 = nullptr;
    INumber *AltEle                = nullptr;
    double currentRA = 0, currentDEC = 0, currentAlt = 0, currentAz = 0, targetAlt = 0;
    bool useJ2000(false);

    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (!EqProp)
    {
        // J2000 Property
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD");
        if (EqProp)
            useJ2000 = true;
    }

    auto HorProp = baseDevice->getNumber("HORIZONTAL_COORD");

    if (EqProp && EqProp->getPermission() == IP_RO)
        EqProp = nullptr;

    if (HorProp && HorProp->getPermission() == IP_RO)
        HorProp = nullptr;

    //qDebug() << "Skymap click - RA: " << scope_target->ra().toHMSString() << " DEC: " << scope_target->dec().toDMSString();

    if (EqProp)
    {
        RAEle = EqProp->findWidgetByName("RA");
        if (!RAEle)
            return false;

        DecEle = EqProp->findWidgetByName("DEC");
        if (!DecEle)
            return false;

        //if (useJ2000)
        //ScopeTarget->apparentCoord( KStars::Instance()->data()->ut().djd(), static_cast<long double>(J2000));

        currentRA  = RAEle->value;
        currentDEC = DecEle->value;

        ScopeTarget->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());
    }

    if (HorProp)
    {
        AzEle = IUFindNumber(HorProp, "AZ");
        if (!AzEle)
            return false;
        AltEle = IUFindNumber(HorProp, "ALT");
        if (!AltEle)
            return false;

        currentAz  = AzEle->value;
        currentAlt = AltEle->value;
    }

    /* Could not find either properties! */
    if (EqProp == nullptr && HorProp == nullptr)
        return false;

    //targetAz = ScopeTarget->az().Degrees();
    targetAlt = ScopeTarget->altRefracted().Degrees();

    if (minAlt != -1 && maxAlt != -1)
    {
        if (targetAlt < minAlt || targetAlt > maxAlt)
        {
            KSNotification::error(i18n("Requested altitude %1 is outside the specified altitude limit boundary (%2,%3).",
                                       QString::number(targetAlt, 'g', 3), QString::number(minAlt, 'g', 3),
                                       QString::number(maxAlt, 'g', 3)), i18n("Telescope Motion"));
            return false;
        }
    }

    auto sendToClient = [ = ]()
    {
        if (EqProp)
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

            RAEle->value  = ra.Hours();
            DecEle->value = de.Degrees();
            clientManager->sendNewNumber(EqProp);

            qCDebug(KSTARS_INDI) << "ISD:Telescope sending coords RA:" << ra.toHMSString() <<
                                 "(" << RAEle->value << ") DE:" << de.toDMSString() <<
                                 "(" << DecEle->value << ")";

            RAEle->value  = currentRA;
            DecEle->value = currentDEC;
        }
        // Only send Horizontal Coord property if Equatorial is not available.
        else if (HorProp)
        {
            AzEle->value  = ScopeTarget->az().Degrees();
            AltEle->value = ScopeTarget->alt().Degrees();
            clientManager->sendNewNumber(HorProp);
            AzEle->value  = currentAz;
            AltEle->value = currentAlt;
        }

    };

    auto checkObject = [ = ]()
    {
        double maxrad = 1000.0 / Options::zoomFactor();
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

                emit newTarget(currentObject->name());

                sendToClient();
            };

            // Sun Warning, but don't ask if tracking is already solar.
            if (currentObject->name() == i18n("Sun") && currentTrackMode != TRACK_SOLAR)
            {
                connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
                {
                    KSMessageBox::Instance()->disconnect(this);
                    checkTrackModes();
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
                checkTrackModes();
        }
        else
            sendToClient();
    };

    if (targetAlt < 0)
    {
        connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            checkObject();
        });
        connect(KSMessageBox::Instance(), &KSMessageBox::rejected, this, [ = ]()
        {
            KSMessageBox::Instance()->disconnect(this);
            if (EqProp)
            {
                RAEle->value  = currentRA;
                DecEle->value = currentDEC;
            }
            if (HorProp)
            {
                AzEle->value  = currentAz;
                AltEle->value = currentAlt;
            }
        });

        KSMessageBox::Instance()->questionYesNo(i18n("Requested altitude is below the horizon. Are you sure you want to proceed?"),
                                                i18n("Telescope Motion"), 15, false);
    }
    else
        checkObject();

    return true;
}

bool Telescope::Slew(double ra, double dec)
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

    return Slew(&target);
}

bool Telescope::Slew(SkyPoint *ScopeTarget)
{
    auto motionSP = baseDevice->getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto slewSW = motionSP->findWidgetByName("TRACK");

    if (!slewSW)
        slewSW = motionSP->findWidgetByName("SLEW");

    if (!slewSW)
        return false;

    if (slewSW->getState() != ISS_ON)
    {
        motionSP->reset();
        slewSW->setState(ISS_ON);
        clientManager->sendNewSwitch(motionSP);

        qCDebug(KSTARS_INDI) << "ISD:Telescope: " << slewSW->getName();
    }

    return sendCoords(ScopeTarget);
}

bool Telescope::Sync(double ra, double dec)
{
    SkyPoint target;

    target.setRA(ra);
    target.setDec(dec);

    return Sync(&target);
}

bool Telescope::Sync(SkyPoint *ScopeTarget)
{
    auto motionSP = baseDevice->getSwitch("ON_COORD_SET");

    if (!motionSP)
        return false;

    auto syncSW = motionSP->findWidgetByName("SYNC");

    if (!syncSW)
        return false;

    if (syncSW->getState() != ISS_ON)
    {
        motionSP->reset();
        syncSW->setState(ISS_ON);
        clientManager->sendNewSwitch(motionSP);

        qCDebug(KSTARS_INDI) << "ISD:Telescope: Syncing...";
    }

    return sendCoords(ScopeTarget);
}

bool Telescope::Abort()
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_ABORT_MOTION");

    if (!motionSP)
        return false;

    auto abortSW = motionSP->findWidgetByName("ABORT");

    if (!abortSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: Aborted." << endl;

    abortSW->setState(ISS_ON);
    clientManager->sendNewSwitch(motionSP);

    inCustomParking = false;

    return true;
}

bool Telescope::Park()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("PARK");

    if (!parkSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: Parking..." << endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Telescope::UnPark()
{
    auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");

    if (!parkSP)
        return false;

    auto parkSW = parkSP->findWidgetByName("UNPARK");

    if (!parkSW)
        return false;

    qCDebug(KSTARS_INDI) << "ISD:Telescope: UnParking..." << endl;

    parkSP->reset();
    parkSW->setState(ISS_ON);
    clientManager->sendNewSwitch(parkSP);

    return true;
}

bool Telescope::getEqCoords(double *ra, double *dec)
{
    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (!EqProp)
    {
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD");
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

bool Telescope::MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd)
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");

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

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::StopWE()
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");

    if (!motionSP)
        return false;

    motionSP->reset();

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::StopNS()
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");

    if (!motionSP)
        return false;

    motionSP->reset();

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd)
{
    auto motionSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");

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

    clientManager->sendNewSwitch(motionSP);

    return true;
}

bool Telescope::setSlewRate(int index)
{
    auto slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");

    if (!slewRateSP)
        return false;

    if (index < 0 || index > slewRateSP->count())
        return false;
    else if (slewRateSP->findOnSwitchIndex() == index)
        return true;

    slewRateSP->reset();

    slewRateSP->at(index)->setState(ISS_ON);

    clientManager->sendNewSwitch(slewRateSP);

    emit slewRateChanged(index);

    return true;
}

int Telescope::getSlewRate() const
{
    auto slewRateSP = baseDevice->getSwitch("TELESCOPE_SLEW_RATE");

    if (!slewRateSP)
        return -1;

    return slewRateSP->findOnSwitchIndex();
}

void Telescope::setAltLimits(double minAltitude, double maxAltitude)
{
    minAlt = minAltitude;
    maxAlt = maxAltitude;
}

bool Telescope::setAlignmentModelEnabled(bool enable)
{
    bool wasExecuted                   = false;

    // For INDI Alignment Subsystem
    auto alignSwitch = baseDevice->getSwitch("ALIGNMENT_SUBSYSTEM_ACTIVE");
    if (alignSwitch)
    {
        alignSwitch->at(0)->setState(enable ? ISS_ON : ISS_OFF);
        clientManager->sendNewSwitch(alignSwitch);
        wasExecuted = true;
    }

    // For EQMod Alignment --- Temporary until all drivers switch fully to INDI Alignment Subsystem
    alignSwitch = baseDevice->getSwitch("ALIGNMODE");
    if (alignSwitch)
    {
        alignSwitch->reset();
        // For now, always set alignment mode to NSTAR on enable.
        if (enable)
            alignSwitch->at(2)->setState(ISS_ON);
        // Otherwise, set to NO ALIGN
        else
            alignSwitch->at(0)->setState(ISS_ON);

        clientManager->sendNewSwitch(alignSwitch);
        wasExecuted = true;
    }

    return wasExecuted;
}

bool Telescope::setSatelliteTLEandTrack(QString tle, const KStarsDateTime satPassStart, const KStarsDateTime satPassEnd)
{
    auto tleTextVec = baseDevice->getText("SAT_TLE_TEXT");
    if (!tleTextVec)
    {
        qCDebug(KSTARS_INDI) << "Property SAT_TLE_TEXT not found";
        return false;
    }

    auto tleText = tleTextVec->findWidgetByName("TLE");
    if (!tleText)
        return false;

    tleText->setText(tle.toLocal8Bit().data());

    clientManager->sendNewText(tleTextVec);
    m_TLEIsSetForTracking = true;
    g_satPassStart = satPassStart;
    g_satPassEnd = satPassEnd;
    return true;
    // See Telescope::processText for the following steps (setting window and switch)
}


bool Telescope::clearParking()
{
    auto parkSwitch  = baseDevice->getSwitch("TELESCOPE_PARK_OPTION");
    if (!parkSwitch)
        return false;

    auto clearParkSW = parkSwitch->findWidgetByName("PARK_PURGE_DATA");
    if (!clearParkSW)
        return false;

    parkSwitch->reset();
    clearParkSW->setState(ISS_ON);

    clientManager->sendNewSwitch(parkSwitch);
    return true;
}

bool Telescope::clearAlignmentModel()
{
    bool wasExecuted = false;

    // Note: Should probably use INDI Alignment Subsystem Client API in the future?
    auto clearSwitch  = baseDevice->getSwitch("ALIGNMENT_POINTSET_ACTION");
    auto commitSwitch = baseDevice->getSwitch("ALIGNMENT_POINTSET_COMMIT");
    if (clearSwitch && commitSwitch)
    {
        clearSwitch->reset();
        // ALIGNMENT_POINTSET_ACTION.CLEAR
        clearSwitch->at(4)->setState(ISS_ON);
        clientManager->sendNewSwitch(clearSwitch);
        commitSwitch->at(0)->setState(ISS_ON);
        clientManager->sendNewSwitch(commitSwitch);
        wasExecuted = true;
    }

    // For EQMod Alignment --- Temporary until all drivers switch fully to INDI Alignment Subsystem
    clearSwitch = baseDevice->getSwitch("ALIGNLIST");
    if (clearSwitch)
    {
        // ALIGNLISTCLEAR
        clearSwitch->reset();
        clearSwitch->at(1)->setState(ISS_ON);
        clientManager->sendNewSwitch(clearSwitch);
        wasExecuted = true;
    }

    return wasExecuted;
}

Telescope::Status Telescope::status()
{
    auto EqProp = baseDevice->getNumber("EQUATORIAL_EOD_COORD");
    if (EqProp == nullptr)
    {
        EqProp = baseDevice->getNumber("EQUATORIAL_COORD");
        if (EqProp == nullptr)
            return MOUNT_ERROR;
    }

    switch (EqProp->s)
    {
        case IPS_IDLE:
            if (inManualMotion)
                return MOUNT_MOVING;
            else if (isParked())
                return MOUNT_PARKED;
            else
                return MOUNT_IDLE;

        case IPS_OK:
            if (inManualMotion)
                return MOUNT_MOVING;
            else if (inCustomParking)
            {
                inCustomParking = false;
                // set CURRENT position as the desired parking position
                sendParkingOptionCommand(PARK_OPTION_CURRENT);
                // Write data to disk
                sendParkingOptionCommand(PARK_OPTION_WRITE_DATA);

                return MOUNT_TRACKING;
            }
            else
                return MOUNT_TRACKING;

        case IPS_BUSY:
        {
            auto parkSP = baseDevice->getSwitch("TELESCOPE_PARK");
            if (parkSP && parkSP->getState() == IPS_BUSY)
                return MOUNT_PARKING;
            else
                return MOUNT_SLEWING;
        }

        case IPS_ALERT:
            inCustomParking = false;
            return MOUNT_ERROR;
    }

    return MOUNT_ERROR;
}

const QString Telescope::getStatusString(Telescope::Status status)
{
    switch (status)
    {
        case ISD::Telescope::MOUNT_IDLE:
            return i18n("Idle");

        case ISD::Telescope::MOUNT_PARKED:
            return i18n("Parked");

        case ISD::Telescope::MOUNT_PARKING:
            return i18n("Parking");

        case ISD::Telescope::MOUNT_SLEWING:
            return i18n("Slewing");

        case ISD::Telescope::MOUNT_MOVING:
            return i18n("Moving %1", getManualMotionString());

        case ISD::Telescope::MOUNT_TRACKING:
            return i18n("Tracking");

        case ISD::Telescope::MOUNT_ERROR:
            return i18n("Error");
    }

    return i18n("Error");
}

QString Telescope::getManualMotionString() const
{
    QString NSMotion, WEMotion;

    auto movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_NS");
    if (movementSP)
    {
        if (movementSP->at(MOTION_NORTH)->getState() == ISS_ON)
            NSMotion = 'N';
        else if (movementSP->at(MOTION_SOUTH)->getState() == ISS_ON)
            NSMotion = 'S';
    }

    movementSP = baseDevice->getSwitch("TELESCOPE_MOTION_WE");
    if (movementSP)
    {
        if (movementSP->at(MOTION_WEST)->getState() == ISS_ON)
            WEMotion = 'W';
        else if (movementSP->at(MOTION_EAST)->getState() == ISS_ON)
            WEMotion = 'E';
    }

    return QString("%1%2").arg(NSMotion, WEMotion);
}

bool Telescope::setTrackEnabled(bool enable)
{
    auto trackSP = baseDevice->getSwitch("TELESCOPE_TRACK_STATE");
    if (!trackSP)
        return false;

    auto trackON  = trackSP->findWidgetByName("TRACK_ON");
    auto trackOFF = trackSP->findWidgetByName("TRACK_OFF");

    if (!trackON || !trackOFF)
        return false;

    trackON->setState(enable ? ISS_ON : ISS_OFF);
    trackOFF->setState(enable ? ISS_OFF : ISS_ON);

    clientManager->sendNewSwitch(trackSP);

    return true;
}

bool Telescope::isTracking()
{
    return (status() == MOUNT_TRACKING);
}

bool Telescope::setTrackMode(uint8_t index)
{
    auto trackModeSP = baseDevice->getSwitch("TELESCOPE_TRACK_MODE");
    if (!trackModeSP)
        return false;

    if (index >= trackModeSP->nsp)
        return false;

    trackModeSP->reset();
    trackModeSP->at(index)->setState(ISS_ON);

    clientManager->sendNewSwitch(trackModeSP);

    return true;
}

bool Telescope::getTrackMode(uint8_t &index)
{
    auto trackModeSP = baseDevice->getSwitch("TELESCOPE_TRACK_MODE");
    if (!trackModeSP)
        return false;

    index = trackModeSP->findOnSwitchIndex();

    return true;
}

bool Telescope::setCustomTrackRate(double raRate, double deRate)
{
    auto trackRateNP = baseDevice->getNumber("TELESCOPE_TRACK_RATE");
    if (!trackRateNP)
        return false;

    auto raRateN = trackRateNP->findWidgetByName("TRACK_RATE_RA");
    auto deRateN = trackRateNP->findWidgetByName("TRACK_RATE_DE");

    if (!raRateN || !deRateN)
        return false;

    raRateN->setValue(raRate);
    deRateN->setValue(deRate);

    clientManager->sendNewNumber(trackRateNP);

    return true;
}

bool Telescope::getCustomTrackRate(double &raRate, double &deRate)
{
    auto trackRateNP = baseDevice->getNumber("TELESCOPE_TRACK_RATE");
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

bool Telescope::sendParkingOptionCommand(ParkOptionCommand command)
{
    auto parkOptionsSP = baseDevice->getSwitch("TELESCOPE_PARK_OPTION");
    if (!parkOptionsSP)
        return false;

    parkOptionsSP->reset();
    parkOptionsSP->at(command)->setState(ISS_ON);
    clientManager->sendNewSwitch(parkOptionsSP);

    return true;
}

const dms Telescope::hourAngle() const
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    return dms(lst.Degrees() - currentCoord.ra().Degrees());
}

}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Telescope::Status &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Telescope::Status &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Telescope::Status>(a);
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Telescope::PierSide &source)
{
    argument.beginStructure();
    argument << static_cast<int>(source);
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Telescope::PierSide &dest)
{
    int a;
    argument.beginStructure();
    argument >> a;
    argument.endStructure();
    dest = static_cast<ISD::Telescope::PierSide>(a);
    return argument;
}

