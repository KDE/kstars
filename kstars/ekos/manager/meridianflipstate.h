/*  Ekos state machine for the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include "skypoint.h"

#include <ekos_mount_debug.h>

#include "indi/indimount.h"

/**
 * @brief A meridian flip is executed by issueing a scope motion to the target.
 *
 * Essentially, the meridian flip relies upon that the mount hardware selects the
 * pier side of the scope on the appropriate side depending whether the scope points east
 * or west of the meridian.
 *
 * While tracking a certain object a pier side change is necessary as soon as the position
 * crosses the meridian. As soon as this is detected, the meridian flip works as follows:
 * - A meridian flip request is sent to Capture so that a currently running capture can
 *   be completed before the flip starts.
 * - Capture completes the currently running capture, stops capturing and sends a confirmation
 *   so that the flip may start.
 * - Now a slew command is issued to the current target (i.e. the last position where a slew has
 *   been executed). This will force the mount hardware to reconsider the pier side and slews
 *   to the same position but changing the pier side.
 * - As soon as the slew has been completed alignment is executed, guiding restarted and Capture
 *   informed that capturing may continue.
 */

namespace Ekos
{
class MeridianFlipState : public QObject
{
    Q_OBJECT
public:
    explicit MeridianFlipState(QObject *parent = nullptr);


    // Meridian flip state of the mount
    typedef enum
    {
        MOUNT_FLIP_NONE,      // this is the default state, comparing the hour angle with the next flip position
                              // it moves to MOUNT_FLIP_PLANNED when a flip is needed
        MOUNT_FLIP_PLANNED,   // a meridian flip is ready to be started due to the target position and the
        // configured offsets and signals to the Capture class that a flip is required
        MOUNT_FLIP_WAITING,   // step after FLUP_PLANNED waiting until Capture completes a running exposure
        MOUNT_FLIP_ACCEPTED,  // Capture is idle or has completed the exposure and will wait until the flip
                              // is completed.
        MOUNT_FLIP_RUNNING,   // this signals that a flip slew is in progress, when the slew is completed
                              // the state is set to MOUNT_FLIP_COMPLETED
        MOUNT_FLIP_COMPLETED, // this checks that the flip was completed successfully or not and after tidying up
                              // moves to MOUNT_FLIP_NONE to wait for the next flip requirement.
                              // Capture sees this and resumes.
        MOUNT_FLIP_ERROR,     // errors in the flip process should end up here
        MOUNT_FLIP_INACTIVE   // do not execute a meridian flip since it will disturb other activities like
        // a running polar alignment
    } MeridianFlipMountState;

    // overall meridian flip stage
    typedef enum {
        MF_NONE,       /* no meridian flip planned */
        MF_REQUESTED,  /* meridian flip necessary, confirmation requested to start the flip */
        MF_READY,      /* confirmations received, the meridian flip may start               */
        MF_INITIATED,  /* meridian flip started                                             */
        MF_FLIPPING,   /* slew started to the target position                               */
        MF_SLEWING,    /* slew started to the target position (where is the difference???)  */
        MF_COMPLETED,  /* meridian flip completed, re-calibration required                  */
        MF_ALIGNING,   /* alignment running after a successful flip                         */
        MF_GUIDING     /* guiding started after a successful flip                           */
    } MFStage;


    /**
     * @brief Translate the state to a string value.
     */
    static QString MFStageString(MFStage stage);


    SkyPoint initialMountCoords;
    // flag if alignment should be executed after the meridian flip
    bool resumeAlignmentAfterFlip { false };
    // flag if guiding should be resetarted after the meridian flip
    bool resumeGuidingAfterFlip { false };

    MeridianFlipState::MFStage getMeridianFlipStage() const { return meridianFlipStage; };

    void setMeridianFlipStage(const MFStage &value);

    /**
     * @brief Check if a meridian flip has already been started
     * @return true iff the scope has started the meridian flip
     */
    inline bool checkMeridianFlipRunning()
    {
        return meridianFlipStage == MF_INITIATED || meridianFlipStage == MF_FLIPPING || meridianFlipStage == MF_SLEWING;
    }

    /**
     * @brief Check if a meridian flip is ready to start, running or some post flip actions are not comleted.
     */
    inline bool checkMeridianFlipActive()
    {
        return meridianFlipStage != MF_NONE && meridianFlipStage != MF_REQUESTED;
    }


    /**
     * @brief Hour angle of that time the mount has slewed to the current position.
     */
    double initialPositionHA() const {return m_initialPositionHA;};
    /**
     * @brief Derive the initial position hour angle from the given RA value
     */
    void setInitialPositionHA(double RA);

    /**
     * @brief access to the meridian flip mount state
     */
    MeridianFlipMountState getMeridianFlipMountState() const { return meridianFlipMountState; }

    double getFlipDelayHrs() const { return flipDelayHrs; }
    void setFlipDelayHrs(double value) { flipDelayHrs = value; }

    QDateTime getMinMeridianFlipEndTime() const { return minMeridianFlipEndTime; }
    /**
     * @brief Update the minimal meridian flip end time - current UTC plus {@see minMeridianFlipDurationSecs}
     */
    void updateMinMeridianFlipEndTime();

    /**
     * @brief Change the meridian flip mount state
     */
    void updateMFMountState(MeridianFlipState::MeridianFlipMountState status);

    /**
     * @brief React upon a meridian flip status change of the mount.
     */
    void publishMFMountStatus(MeridianFlipMountState status);

    /**
     * @brief publish a new meridian flip mount status text
     */
    void publishMFMountStatusText(QString text);
    /**
     * @brief return the string for the status
    */
    static QString meridianFlipStatusString(MeridianFlipMountState status);

    // Access to the current mount device
    ISD::Mount *getMount() const { return m_Mount; }
    void setMount(ISD::Mount *newMount) { m_Mount = newMount; }


signals:
    /**
     * @brief mount meridian flip status update event
     */
    void newMountMFStatus(MeridianFlipMountState status);

    /**
     * @brief Communicate a new meridian flip mount status message
     */
    void newMeridianFlipMountStatusText(const QString &text);



private:
    // the mount device
    ISD::Mount *m_Mount {nullptr};
    // current overall meridian flip state
    MFStage meridianFlipStage { MF_NONE };

    // current mount meridian flip state
    MeridianFlipMountState meridianFlipMountState { MOUNT_FLIP_NONE };
    // last message published to avoid double entries
    QString m_lastStatusText = "";
    /**
     * Get the hour angle of that time the mount has slewed to the current position.
     * This is used to manage the meridian flip for mounts which do not report pier side.
     * only one attempt to flip is done.
     */
    double m_initialPositionHA;

    // A meridian flip requires a slew of 180 degrees in the hour angle axis so will take at least
    // the time for that, currently set to 20 seconds
    // not reliable for pointing state change detection but reported if the pier side is unknown
    QDateTime minMeridianFlipEndTime;
    int minMeridianFlipDurationSecs = 20;

    double flipDelayHrs = 0.0;      // delays the next flip attempt if it fails

    /**
     * @brief Internal method for changing the mount meridian flip state. From extermal, use {@see updateMFMountState()}
     */
    void setMeridianFlipMountState(MeridianFlipMountState newMeridianFlipMountState);

};
} // namespace
