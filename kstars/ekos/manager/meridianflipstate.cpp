/*  Ekos state machine for the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "meridianflipstate.h"
#include "ekos/mount/mount.h"
#include "Options.h"

#include "kstarsdata.h"
#include "indicom.h"

#include <ekos_capture_debug.h>

namespace Ekos
{
MeridianFlipState::MeridianFlipState(QObject *parent) : QObject(parent)
{
}

void MeridianFlipState::setResumeAlignmentAfterFlip(bool resume)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "Setting resume alignment after flip to" << (resume ? "true" : "false");
    m_resumeAlignmentAfterFlip = resume;
}

void MeridianFlipState::setResumeGuidingAfterFlip(bool resume)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "Setting resume guiding after flip to" << (resume ? "true" : "false");
    m_resumeGuidingAfterFlip = resume;
}

QString MeridianFlipState::MFStageString(MFStage stage)
{
    switch(stage)
    {
        case MF_NONE:
            return "MF_NONE";
        case MF_REQUESTED:
            return "MF_REQUESTED";
        case MF_READY:
            return "MF_READY";
        case MF_INITIATED:
            return "MF_INITIATED";
        case MF_FLIPPING:
            return "MF_FLIPPING";
        case MF_COMPLETED:
            return "MF_COMPLETED";
        case MF_ALIGNING:
            return "MF_ALIGNING";
        case MF_GUIDING:
            return "MF_GUIDING";
    }
    return "MFStage unknown.";
}

void MeridianFlipState::setEnabled(bool value)
{
    m_enabled = value;
    // reset meridian flip if disabled
    if (m_enabled == false)
        updateMFMountState(MOUNT_FLIP_NONE);
}

void MeridianFlipState::connectMount(Mount *mount)
{
    connect(mount, &Mount::newCoords, this, &MeridianFlipState::updateTelescopeCoord, Qt::UniqueConnection);
    connect(mount, &Mount::newStatus, this, &MeridianFlipState::setMountStatus, Qt::UniqueConnection);
}

void MeridianFlipState::updateMeridianFlipStage(const MFStage &stage)
{
    qCDebug(KSTARS_EKOS_CAPTURE) << "updateMeridianFlipStage: " << MeridianFlipState::MFStageString(stage);

    if (meridianFlipStage != stage)
    {
        switch (stage)
        {
            case MeridianFlipState::MF_NONE:
                meridianFlipStage = stage;
                break;

            case MeridianFlipState::MF_READY:
                if (getMeridianFlipStage() == MeridianFlipState::MF_REQUESTED)
                {
                    // we keep the stage on requested until the mount starts the meridian flip
                    updateMFMountState(MeridianFlipState::MOUNT_FLIP_ACCEPTED);
                }
                else if (m_CaptureState == CAPTURE_PAUSED)
                {
                    // paused after meridian flip requested
                    meridianFlipStage = stage;
                    updateMFMountState(MeridianFlipState::MOUNT_FLIP_ACCEPTED);
                }
                else if (!(checkMeridianFlipRunning()
                           || getMeridianFlipStage() == MeridianFlipState::MF_COMPLETED))
                {
                    // if neither a MF has been requested (checked above) or is in a post
                    // MF calibration phase, no MF needs to take place.
                    // Hence we set to the stage to NONE
                    meridianFlipStage = MeridianFlipState::MF_NONE;
                    break;
                }
                // in any other case, ignore it
                break;

            case MeridianFlipState::MF_INITIATED:
                meridianFlipStage = MeridianFlipState::MF_INITIATED;
                break;

            case MeridianFlipState::MF_REQUESTED:
                if (m_CaptureState == CAPTURE_PAUSED)
                    // paused before meridian flip requested
                    updateMFMountState(MeridianFlipState::MOUNT_FLIP_ACCEPTED);
                else
                    updateMFMountState(MeridianFlipState::MOUNT_FLIP_WAITING);
                meridianFlipStage = stage;
                break;

            case MeridianFlipState::MF_COMPLETED:
                meridianFlipStage = MeridianFlipState::MF_COMPLETED;
                break;

            default:
                meridianFlipStage = stage;
                break;
        }
    }
}



bool MeridianFlipState::checkMeridianFlip(dms lst)
{
    // checks if a flip is possible
    if (m_hasMount == false)
    {
        publishMFMountStatusText(i18n("Meridian flip inactive (no scope connected)"));
        updateMFMountState(MOUNT_FLIP_NONE);
        return false;
    }

    if (isEnabled() == false)
    {
        publishMFMountStatusText(i18n("Meridian flip inactive (flip not requested)"));
        return false;
    }

    // Will never get called when parked!
    if (m_MountParkStatus == ISD::PARK_PARKED)
    {
        publishMFMountStatusText(i18n("Meridian flip inactive (parked)"));
        return false;
    }

    if (targetPosition.valid == false || isEnabled() == false)
    {
        publishMFMountStatusText(i18n("Meridian flip inactive (no target set)"));
        return false;
    }

    // get the time after the meridian that the flip is called for (Degrees --> Hours)
    double offset = rangeHA(m_offset / 15.0);

    double hrsToFlip = 0;       // time to go to the next flip - hours  -ve means a flip is required

    double ha =  currentPosition.ha.HoursHa();     // -12 to 0 to +12

    // calculate time to next flip attempt.  This uses the current hour angle, the pier side if available
    // and the meridian flip offset to get the time to the flip
    //
    // *** should it use the target position so it will continue to track the target even if the mount is not tracking?
    //
    // Note: the PierSide code relies on the mount reporting the pier side correctly
    // It is possible that a mount can flip before the meridian and this has caused problems so hrsToFlip is calculated
    // assuming the mount can flip up to three hours early.

    static ISD::Mount::PierSide initialPierSide;    // used when the flip has completed to determine if the flip was successful

    // adjust ha according to the pier side.
    switch (currentPosition.pierSide)
    {
        case ISD::Mount::PierSide::PIER_WEST:
            // this is the normal case, tracking from East to West, flip is near Ha 0.
            break;
        case ISD::Mount::PierSide::PIER_EAST:
            // this is the below the pole case, tracking West to East, flip is near Ha 12.
            // shift ha by 12h
            ha = rangeHA(ha + 12);
            break;
        default:
            // This is the case where the PierSide is not available, make one attempt only
            setFlipDelayHrs(0);
            // we can only attempt a flip if the mount started before the meridian, assumed in the unflipped state
            if (initialPositionHA() >= 0)
            {
                publishMFMountStatusText(i18n("Meridian flip inactive (slew after meridian)"));
                if (getMeridianFlipMountState() == MOUNT_FLIP_NONE)
                    return false;
            }
            break;
    }
    // get the time to the next flip, allowing for the pier side and
    // the possibility of an early flip
    // adjust ha so an early flip is allowed for
    if (ha >= 9.0)
        ha -= 24.0;
    hrsToFlip = offset + getFlipDelayHrs() - ha;

    int hh = static_cast<int> (hrsToFlip);
    int mm = static_cast<int> ((hrsToFlip - hh) * 60);
    int ss = static_cast<int> ((hrsToFlip - hh - mm / 60.0) * 3600);
    QString message = i18n("Meridian flip in %1", QTime(hh, mm, ss).toString(Qt::TextDate));

    // handle the meridian flip state machine
    switch (getMeridianFlipMountState())
    {
        case MOUNT_FLIP_NONE:
            publishMFMountStatusText(message);

            if (hrsToFlip <= 0)
            {
                // signal that a flip can be done
                qCInfo(KSTARS_EKOS_MOUNT) << "Meridian flip planned with LST=" <<
                                          lst.toHMSString() <<
                                          " scope RA=" << currentPosition.position.ra().toHMSString() <<
                                          " ha=" << ha <<
                                          ", meridian diff=" << offset <<
                                          ", hrstoFlip=" << hrsToFlip <<
                                          ", flipDelayHrs=" << getFlipDelayHrs() <<
                                          ", " << ISD::Mount::pierSideStateString(currentPosition.pierSide);

                initialPierSide = currentPosition.pierSide;
                updateMFMountState(MOUNT_FLIP_PLANNED);
            }
            break;

        case MOUNT_FLIP_PLANNED:
            // handle the case where there is no Capture module
            if (m_hasCaptureInterface == false)
            {
                qCDebug(KSTARS_EKOS_MOUNT) << "no capture interface, starting flip slew.";
                updateMFMountState(MOUNT_FLIP_ACCEPTED);
                return true;
            }
            return false;

        case MOUNT_FLIP_ACCEPTED:
            // set by the Capture module when it's ready
            return true;

        case MOUNT_FLIP_RUNNING:
            if (m_MountStatus == ISD::Mount::MOUNT_TRACKING)
            {
                if (minMeridianFlipEndTime <= KStarsData::Instance()->clock()->utc())
                {
                    // meridian flip slew completed, did it work?
                    // check tracking only when the minimal flip duration has passed
                    bool flipFailed = false;

                    // pointing state change check only for mounts that report pier side
                    if (currentPosition.pierSide == ISD::Mount::PIER_UNKNOWN)
                    {
                        appendLogText(i18n("Assuming meridian flip completed, but pier side unknown."));
                        // signal that capture can resume
                        updateMFMountState(MOUNT_FLIP_COMPLETED);
                        return false;
                    }
                    else if (currentPosition.pierSide == initialPierSide)
                    {
                        flipFailed = true;
                        qCWarning(KSTARS_EKOS_MOUNT) << "Meridian flip failed, pier side not changed";
                    }

                    if (flipFailed)
                    {
                        if (getFlipDelayHrs() <= 1.0)
                        {
                            // Set next flip attempt to be 4 minutes in the future.
                            // These depend on the assignment to flipDelayHrs above.
                            constexpr double delayHours = 4.0 / 60.0;
                            if (currentPosition.pierSide == ISD::Mount::PierSide::PIER_EAST)
                                setFlipDelayHrs(rangeHA(ha + 12 + delayHours) - offset);
                            else
                                setFlipDelayHrs(ha + delayHours - offset);

                            // check to stop an infinite loop, 1.0 hrs for now but should use the Ha limit
                            appendLogText(i18n("meridian flip failed, retrying in 4 minutes"));
                        }
                        else
                        {
                            appendLogText(i18n("No successful Meridian Flip done, delay too long"));
                        }
                        updateMFMountState(MOUNT_FLIP_COMPLETED);   // this will resume imaging and try again after the extra delay
                    }
                    else
                    {
                        setFlipDelayHrs(0);
                        appendLogText(i18n("Meridian flip completed OK."));
                        // signal that capture can resume
                        updateMFMountState(MOUNT_FLIP_COMPLETED);
                    }
                }
                else
                    qCInfo(KSTARS_EKOS_MOUNT) << "Tracking state during meridian flip reached too early, ignored.";
            }
            break;

        case MOUNT_FLIP_COMPLETED:
            updateMFMountState(MOUNT_FLIP_NONE);
            break;

        default:
            break;
    }
    return false;
}

void MeridianFlipState::startMeridianFlip()
{
    if (/*initialHA() > 0 || */ targetPosition.valid == false)
    {
        // no meridian flip necessary
        qCDebug(KSTARS_EKOS_MOUNT) << "No meridian flip: no target defined";
        return;
    }

    if (m_MountStatus != ISD::Mount::MOUNT_TRACKING)
    {
        // this should never happen
        if (m_hasMount == false)
            qCWarning(KSTARS_EKOS_MOUNT()) << "No mount connected!";

        // no meridian flip necessary
        qCInfo(KSTARS_EKOS_MOUNT) << "No meridian flip: mount not tracking, current state:" <<
                                  ISD::Mount::mountStates[m_MountStatus].untranslatedText();
        return;
    }

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    double HA = rangeHA(lst.Hours() - targetPosition.position.ra().Hours());

    // execute meridian flip
    qCInfo(KSTARS_EKOS_MOUNT) << "Meridian flip: slewing to RA=" <<
                              targetPosition.position.ra().toHMSString() <<
                              "DEC=" << targetPosition.position.dec().toDMSString() <<
                              " Hour Angle " << dms(HA).toHMSString();

    updateMinMeridianFlipEndTime();
    updateMFMountState(MeridianFlipState::MOUNT_FLIP_RUNNING);

    // start the re-slew to the current target expecting that the mount firmware changes the pier side
    emit slewTelescope(targetPosition.position);
}

bool MeridianFlipState::resetMeridianFlip()
{

    // reset the meridian flip status if the slew is not the meridian flip itself
    if (meridianFlipMountState != MOUNT_FLIP_RUNNING)
    {
        updateMFMountState(MOUNT_FLIP_NONE);
        setFlipDelayHrs(0);
        qCDebug(KSTARS_EKOS_MOUNT) << "flipDelayHrs set to zero in slew, m_MFStatus=" <<
                                   meridianFlipStatusString(meridianFlipMountState);
        // meridian flip not running, no need for post MF handling
        return false;
    }
    // don't interrupt a meridian flip directly
    return true;
}

void MeridianFlipState::processFlipCompleted()
{
    appendLogText(i18n("Telescope completed the meridian flip."));
    if (m_CaptureState == CAPTURE_IDLE || m_CaptureState == CAPTURE_ABORTED || m_CaptureState == CAPTURE_COMPLETE)
    {
        // reset the meridian flip stage and jump directly MF_NONE, since no
        // restart of guiding etc. necessary
        updateMeridianFlipStage(MeridianFlipState::MF_NONE);
        return;
    }

}


void MeridianFlipState::setMeridianFlipMountState(MeridianFlipMountState newMeridianFlipMountState)
{
    qCDebug (KSTARS_EKOS_MOUNT) << "Setting meridian flip status to " << meridianFlipStatusString(newMeridianFlipMountState);
    publishMFMountStatus(newMeridianFlipMountState);
    meridianFlipMountState = newMeridianFlipMountState;
}

void MeridianFlipState::appendLogText(QString message)
{
    qCInfo(KSTARS_EKOS_MOUNT) << message;
    emit newLog(message);
}

void MeridianFlipState::updateMinMeridianFlipEndTime()
{
    minMeridianFlipEndTime = KStarsData::Instance()->clock()->utc().addSecs(Options::minFlipDuration());
}

void MeridianFlipState::updateMFMountState(MeridianFlipMountState status)
{
    if (getMeridianFlipMountState() != status)
    {
        if (status == MOUNT_FLIP_ACCEPTED)
        {
            // ignore accept signal if none was requested
            if (meridianFlipStage != MF_REQUESTED)
                return;
        }
        // in all other cases, handle it straight forward
        setMeridianFlipMountState(status);
        emit newMountMFStatus(status);
    }
}

void MeridianFlipState::publishMFMountStatus(MeridianFlipMountState status)
{
    // avoid double entries
    if (status == meridianFlipMountState)
        return;

    switch (status)
    {
        case MOUNT_FLIP_NONE:
            publishMFMountStatusText(i18n("Status: inactive"));
            break;

        case MOUNT_FLIP_PLANNED:
            publishMFMountStatusText(i18n("Meridian flip planned..."));
            break;

        case MOUNT_FLIP_WAITING:
            publishMFMountStatusText(i18n("Meridian flip waiting..."));
            break;

        case MOUNT_FLIP_ACCEPTED:
            publishMFMountStatusText(i18n("Meridian flip ready to start..."));
            break;

        case MOUNT_FLIP_RUNNING:
            publishMFMountStatusText(i18n("Meridian flip running..."));
            break;

        case MOUNT_FLIP_COMPLETED:
            publishMFMountStatusText(i18n("Meridian flip completed."));
            break;

        default:
            break;
    }

}

void MeridianFlipState::publishMFMountStatusText(QString text)
{
    // avoid double entries
    if (text != m_lastStatusText)
    {
        emit newMeridianFlipMountStatusText(text);
        m_lastStatusText = text;
    }
}

QString MeridianFlipState::meridianFlipStatusString(MeridianFlipMountState status)
{
    switch (status)
    {
        case MOUNT_FLIP_NONE:
            return "MOUNT_FLIP_NONE";
        case MOUNT_FLIP_PLANNED:
            return "MOUNT_FLIP_PLANNED";
        case MOUNT_FLIP_WAITING:
            return "MOUNT_FLIP_WAITING";
        case MOUNT_FLIP_ACCEPTED:
            return "MOUNT_FLIP_ACCEPTED";
        case MOUNT_FLIP_RUNNING:
            return "MOUNT_FLIP_RUNNING";
        case MOUNT_FLIP_COMPLETED:
            return "MOUNT_FLIP_COMPLETED";
        case MOUNT_FLIP_ERROR:
            return "MOUNT_FLIP_ERROR";
    }
    return "not possible";
}





void MeridianFlipState::setMountStatus(ISD::Mount::Status status)
{
    qCDebug(KSTARS_EKOS_MOUNT) << "New mount state for MF:" << ISD::Mount::mountStates[status].untranslatedText();
    m_PrevMountStatus = m_MountStatus;
    m_MountStatus = status;
}

void MeridianFlipState::setMountParkStatus(ISD::ParkStatus status)
{
    // clear the meridian flip when parking
    if (status == ISD::PARK_PARKING || status == ISD::PARK_PARKED)
        updateMFMountState(MOUNT_FLIP_NONE);

    m_MountParkStatus = status;
}


void MeridianFlipState::updatePosition(MountPosition &pos, const SkyPoint &position, ISD::Mount::PierSide pierSide,
                                       const dms &ha, const bool isValid)
{
    pos.position = position;
    pos.pierSide = pierSide;
    pos.ha       = ha;
    pos.valid    = isValid;
}

void MeridianFlipState::updateTelescopeCoord(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha)
{
    updatePosition(currentPosition, position, pierSide, ha, true);

    // If we just finished a slew, let's update initialHA and the current target's position,
    // but only if the meridian flip is enabled
    if (m_MountStatus == ISD::Mount::MOUNT_TRACKING && m_PrevMountStatus == ISD::Mount::MOUNT_SLEWING
            && isEnabled())
    {
        if (meridianFlipMountState == MOUNT_FLIP_NONE)
        {
            setFlipDelayHrs(0);
        }
        // set the target position
        updatePosition(targetPosition, currentPosition.position, currentPosition.pierSide, currentPosition.ha, true);
        qCDebug(KSTARS_EKOS_MOUNT) << "Slew finished, MFStatus " <<
                                   meridianFlipStatusString(meridianFlipMountState);
        // ensure that this is executed only once
        m_PrevMountStatus = m_MountStatus;
    }

    QChar sgn(ha.Hours() <= 12.0 ? '+' : '-');

    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());

    // don't check the meridian flip while in motion
    bool inMotion = (m_MountStatus == ISD::Mount::MOUNT_SLEWING || m_MountStatus == ISD::Mount::MOUNT_MOVING
                     || m_MountStatus == ISD::Mount::MOUNT_PARKING);
    if ((inMotion == false) && checkMeridianFlip(lst))
        startMeridianFlip();
    else
    {
        const QString message(i18n("Meridian flip inactive (parked)"));
        if (m_MountParkStatus == ISD::PARK_PARKED /* && meridianFlipStatusWidget->getStatus() != message */)
        {
            publishMFMountStatusText(message);
        }
    }
}

void MeridianFlipState::setTargetPosition(SkyPoint *pos)
{
    if (pos != nullptr)
    {
        dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
        dms ha = dms(lst.Degrees() - pos->ra().Degrees());

        qCDebug(KSTARS_EKOS_MOUNT) << "Setting target RA=" << pos->ra().toHMSString() << "DEC=" << pos->dec().toDMSString();
        updatePosition(targetPosition, *pos, ISD::Mount::PIER_UNKNOWN, ha, true);
    }
    else
    {
        clearTargetPosition();
    }
}

double MeridianFlipState::initialPositionHA() const
{
    double HA = targetPosition.ha.HoursHa();
    return HA;
}


} // namespace

