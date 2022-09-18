/*  Ekos state machine for the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "meridianflipstate.h"

#include "kstarsdata.h"
#include "indicom.h"

namespace Ekos
{
MeridianFlipState::MeridianFlipState(QObject *parent) : QObject(parent)
{
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
        case MF_SLEWING:
            return "MF_SLEWING";
        case MF_COMPLETED:
            return "MF_COMPLETED";
        case MF_ALIGNING:
            return "MF_ALIGNING";
        case MF_GUIDING:
            return "MF_GUIDING";
    }
    return "MFStage unknown.";
}

void MeridianFlipState::setMeridianFlipStage(const MFStage &value)
{
    meridianFlipStage = value;
}

void MeridianFlipState::setInitialPositionHA(double RA)
{
    dms lst = KStarsData::Instance()->geo()->GSTtoLST(KStarsData::Instance()->clock()->utc().gst());
    double HA = lst.Hours() - RA;
    m_initialPositionHA = rangeHA(HA);
}

void MeridianFlipState::setMeridianFlipMountState(MeridianFlipMountState newMeridianFlipMountState)
{
    qCDebug (KSTARS_EKOS_MOUNT) << "Setting meridian flip status to " << meridianFlipStatusString(newMeridianFlipMountState);
    publishMFMountStatus(newMeridianFlipMountState);
    meridianFlipMountState = newMeridianFlipMountState;
}

void MeridianFlipState::updateMinMeridianFlipEndTime()
{
    minMeridianFlipEndTime = KStarsData::Instance()->clock()->utc().addSecs(minMeridianFlipDurationSecs);
}

void MeridianFlipState::updateMFMountState(MeridianFlipMountState status)
{
    if (getMeridianFlipMountState() != status)
    {
        if (status == MOUNT_FLIP_ACCEPTED) {
            // ignore accept signal if none was requested
            if (meridianFlipStage != MF_REQUESTED)
                return;
            else
            {
                // if the mount is not tracking, we go back one step
                if ( m_Mount == nullptr || m_Mount->isTracking() == false)
                {
                    setMeridianFlipMountState(MOUNT_FLIP_PLANNED);
                    emit newMountMFStatus(MOUNT_FLIP_PLANNED);
                }
            }
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
        publishMFMountStatusText(i18n("Meridian ready to start..."));
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
        case MOUNT_FLIP_INACTIVE:
            return "MOUNT_FLIP_INACTIVE";
    }
    return "not possible";
}

} // namespace

