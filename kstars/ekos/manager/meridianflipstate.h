/*  Ekos state machine for the meridian flip
    SPDX-FileCopyrightText: Wolfgang Reissenberger <sterne-jaeger@openfuture.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QObject>
#include "skypoint.h"

#include <ekos_mount_debug.h>
#include "ekos/ekos.h"

#include "indi/indistd.h"
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

class Mount;

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
            MOUNT_FLIP_ERROR      // errors in the flip process should end up here
        } MeridianFlipMountState;

        // overall meridian flip stage
        typedef enum
        {
            MF_NONE,       /* no meridian flip planned */
            MF_REQUESTED,  /* meridian flip necessary, confirmation requested to start the flip */
            MF_READY,      /* confirmations received, the meridian flip may start               */
            MF_INITIATED,  /* meridian flip started                                             */
            MF_FLIPPING,   /* slew started to the target position                               */
            MF_COMPLETED,  /* meridian flip completed, re-calibration required                  */
            MF_ALIGNING,   /* alignment running after a successful flip                         */
            MF_GUIDING     /* guiding started after a successful flip                           */
        } MFStage;

        // mount position
        typedef struct MountPosition
        {
            SkyPoint position;
            ISD::Mount::PierSide pierSide;
            dms ha;
            bool valid = false;
        } MountPosition;

        // flag if alignment should be executed after the meridian flip
        bool resumeAlignmentAfterFlip()
        {
            return m_resumeAlignmentAfterFlip;
        }
        void setResumeAlignmentAfterFlip(bool resume);

        // flag if guiding should be resetarted after the meridian flip
        bool resumeGuidingAfterFlip()
        {
            return m_resumeGuidingAfterFlip;
        }
        void setResumeGuidingAfterFlip(bool resume);

        /**
         * @brief Translate the state to a string value.
         */
        static QString MFStageString(MFStage stage);

        // Is the meridian flip enabled?
        bool isEnabled() const
        {
            return m_enabled;
        }
        void setEnabled(bool value);
        // offset past the meridian
        double getOffset() const
        {
            return m_offset;
        }
        void setOffset(double newOffset)
        {
            m_offset = newOffset;
        }

        /**
         * @brief connectMount Establish the connection to the mount
         */
        void connectMount(Mount *mount);

        MeridianFlipState::MFStage getMeridianFlipStage() const
        {
            return meridianFlipStage;
        };
        const QString &getMeridianStatusText()
        {
            return m_lastStatusText;
        }

        /**
         * @brief Update the meridian flip stage
         */
        void updateMeridianFlipStage(const MFStage &stage);

        /**
         * @brief Stop a meridian flip if necessary.
         * @return true if a meridian flip was running
         */
        bool resetMeridianFlip();

        /**
         * @brief Execute actions after the flipping slew has completed.
         */
        void processFlipCompleted();

        /**
         * @brief Check if a meridian flip has already been started
         * @return true iff the scope has started the meridian flip
         */
        inline bool checkMeridianFlipRunning()
        {
            return meridianFlipStage == MF_INITIATED || meridianFlipStage == MF_FLIPPING;
        }

        /**
         * @brief Check if a meridian flip is ready to start, running or some post flip actions are not comleted.
         */
        inline bool checkMeridianFlipActive()
        {
            return meridianFlipStage != MF_NONE && meridianFlipStage != MF_REQUESTED;
        }

        /**
         * @brief Update the current target position
         * @param pos new target (may be null if no target is set)
         */
        void setTargetPosition(SkyPoint *pos);

        /**
         * @brief Make current target position invalid
         */
        void clearTargetPosition()
        {
            targetPosition.valid = false;
        }

        /**
         * @brief Get the hour angle of that time the mount has slewed to the current position.
         *        his is used to manage the meridian flip for mounts which do not report pier side.
         *        (-12.0 < HA <= 12.0)
         */
        double initialPositionHA() const;

        /**
         * @brief access to the meridian flip mount state
         */
        MeridianFlipMountState getMeridianFlipMountState() const
        {
            return meridianFlipMountState;
        }

        double getFlipDelayHrs() const
        {
            return flipDelayHrs;
        }
        void setFlipDelayHrs(double value)
        {
            flipDelayHrs = value;
        }

        /**
         * @brief Change the meridian flip mount state
         */
        void updateMFMountState(MeridianFlipState::MeridianFlipMountState status);

        /**
         * @brief return the string for the status
        */
        static QString meridianFlipStatusString(MeridianFlipMountState status);

        // Access to the current mount device
        void setMountConnected(bool connected)
        {
            m_hasMount = connected;
        }

        // Access to the capture device
        void setHasCaptureInterface(bool present)
        {
            m_hasCaptureInterface = present;
        }

    public slots:
        /**
         * @brief Slot for receiving an update of the capture status
         */
        void setCaptureState(CaptureState state)
        {
            m_CaptureState = state;
        }
        /**
         * @brief Slot for receiving an update to the mount status
         */
        void setMountStatus(ISD::Mount::Status status);
        /**
         * @brief Slot for receiving an update to the mount park status
         */
        void setMountParkStatus(ISD::ParkStatus status);
        /**
         * @brief Slot for receiving a new telescope position
         * @param position new position
         * @param pierSide current pier side
         * @param ha current hour angle
         */
        void updateTelescopeCoord(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

    signals:
        // mount meridian flip status update event
        void newMountMFStatus(MeridianFlipMountState status);
        // Communicate a new meridian flip mount status message
        void newMeridianFlipMountStatusText(const QString &text);
        // slew the telescope to a target
        void slewTelescope(SkyPoint &target);
        // new log text for the module log window
        void newLog(const QString &text);

    private:
        // flag if meridian flip is enabled
        bool m_enabled = false;
        // offset post meridian (in degrees)
        double m_offset;
        // flag if alignment should be executed after the meridian flip
        bool m_resumeAlignmentAfterFlip { false };
        // flag if guiding should be resetarted after the meridian flip
        bool m_resumeGuidingAfterFlip { false };

        // the mount device
        bool m_hasMount { false };
        // capture interface
        bool m_hasCaptureInterface { false };
        // the current capture status
        CaptureState m_CaptureState { CAPTURE_IDLE };
        // the current mount status
        ISD::Mount::Status m_MountStatus = ISD::Mount::MOUNT_IDLE;
        // the previous mount status
        ISD::Mount::Status m_PrevMountStatus = ISD::Mount::MOUNT_IDLE;
        // mount park status
        ISD::ParkStatus m_MountParkStatus = ISD::PARK_UNKNOWN;
        // current overall meridian flip state
        MFStage meridianFlipStage { MF_NONE };

        // current mount meridian flip state
        MeridianFlipMountState meridianFlipMountState { MOUNT_FLIP_NONE };
        // last message published to avoid double entries
        QString m_lastStatusText = "";

        SkyPoint initialMountCoords;

        // current position of the mount
        MountPosition currentPosition;
        // current target position (might be different from current position!)
        MountPosition targetPosition;

        static void updatePosition(MountPosition &pos, const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha, const bool isValid);

        // A meridian flip requires a slew of 180 degrees in the hour angle axis so will take a certain
        // amount of time. Within this time, a slewing state change will be ignored for pier side change checks.
        QDateTime minMeridianFlipEndTime;

        double flipDelayHrs = 0.0;      // delays the next flip attempt if it fails

        /**
         * @brief Internal method for changing the mount meridian flip state. From extermal, use {@see updateMFMountState()}
         */
        void setMeridianFlipMountState(MeridianFlipMountState newMeridianFlipMountState);

        /**
         * @brief Check if a meridian flip if necessary.
         * @param lst local sideral time
         */
        bool checkMeridianFlip(dms lst);

        /**
         * @brief Start a meridian flip if necessary.
         * @return true if a meridian flip was started
         */
        void startMeridianFlip();

        /**
         * @brief Calculate the minimal end time from now plus {@see minMeridianFlipEndTime}
         */
        void updateMinMeridianFlipEndTime();

        /**
         * @brief React upon a meridian flip status change of the mount.
         */
        void publishMFMountStatus(MeridianFlipMountState status);

        /**
         * @brief publish a new meridian flip mount status text
         */
        void publishMFMountStatusText(QString text);

        /**
         * @brief Add log message
         */
        void appendLogText(QString message);

};
} // namespace
