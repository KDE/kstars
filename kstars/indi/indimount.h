/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QDBusArgument>
#include <QTimer>

#include "indiconcretedevice.h"
#include "skypoint.h"

class SkyObject;

namespace ISD
{
/**
 * @class Mount
 * device handle controlling Mounts. It can slew and sync to a specific sky point and supports all standard properties with INDI
 * telescope device.
 *
 * @author Jasem Mutlaq
 */
class Mount : public ConcreteDevice
{
        Q_OBJECT

    public:
        explicit Mount(GenericDevice *parent);
        virtual ~Mount() override = default;

        typedef enum { MOTION_NORTH, MOTION_SOUTH } VerticalMotion;
        typedef enum { MOTION_WEST, MOTION_EAST } HorizontalMotion;
        typedef enum { MOTION_START, MOTION_STOP } MotionCommand;
        typedef enum { PIER_UNKNOWN = -1, PIER_WEST = 0, PIER_EAST = 1 } PierSide;
        typedef enum
        {
            MOUNT_IDLE,
            MOUNT_MOVING,
            MOUNT_SLEWING,
            MOUNT_TRACKING,
            MOUNT_PARKING,
            MOUNT_PARKED,
            MOUNT_ERROR
        } Status;
        typedef enum { PARK_OPTION_CURRENT, PARK_OPTION_DEFAULT, PARK_OPTION_WRITE_DATA } ParkOptionCommand;
        typedef enum { TRACK_SIDEREAL, TRACK_SOLAR, TRACK_LUNAR, TRACK_CUSTOM } TrackModes;


        static const QList<const char *> mountStates;

        void registerProperty(INDI::Property prop) override;
        void processSwitch(ISwitchVectorProperty *svp) override;
        void processText(ITextVectorProperty *tvp) override;
        void processNumber(INumberVectorProperty *nvp) override;

        // Coordinates
        bool getEqCoords(double *ra, double *dec);
        bool isJ2000()
        {
            return m_isJ2000;
        }

        // Slew
        bool Slew(SkyPoint *ScopeTarget);
        bool Slew(double ra, double dec);
        bool canGoto()
        {
            return m_canGoto;
        }

        // Sync
        bool Sync(SkyPoint *ScopeTarget);
        bool Sync(double ra, double dec);
        bool canSync()
        {
            return m_canSync;
        }

        // Tracking
        bool canControlTrack() const
        {
            return m_canControlTrack;
        }
        bool isTracking();

        // Track Mode
        bool hasTrackModes() const
        {
            return m_hasTrackModes;
        }
        bool getTrackMode(uint8_t &index);

        // Custom Track Rate
        bool hasCustomTrackRate() const
        {
            return m_hasTrackModes;
        }
        bool getCustomTrackRate(double &raRate, double &deRate);

        // Motion
        bool MoveNS(VerticalMotion dir, MotionCommand cmd);
        bool StopNS();
        bool MoveWE(HorizontalMotion dir, MotionCommand cmd);
        bool StopWE();
        bool isReversed(INDI_EQ_AXIS axis);
        bool setReversedEnabled(INDI_EQ_AXIS axis, bool enabled);
        bool isSlewing();
        bool isInMotion();
        bool canAbort()
        {
            return m_canAbort;
        }
        QString getManualMotionString() const;

        // Guiding
        bool canGuide();
        bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
        bool doPulse(GuideDirection dir, int msecs);

        // Parking
        bool canPark();
        bool isParked()
        {
            return m_ParkStatus == PARK_PARKED;
        }
        bool canCustomPark()
        {
            return m_hasCustomParking;
        }
        bool sendParkingOptionCommand(ParkOptionCommand command);

        // Status
        ParkStatus parkStatus()
        {
            return m_ParkStatus;
        }

        Status status(INumberVectorProperty *nvp);
        Status status();
        const QString statusString(Status status, bool translated = true) const;

        // Altitude Limits
        void setAltLimits(double minAltitude, double maxAltitude);

        // Alignment Model
        bool setAlignmentModelEnabled(bool enable);
        bool clearAlignmentModel();
        bool clearParking();
        bool hasAlignmentModel()
        {
            return m_hasAlignmentModel;
        }

        // Slew Rates
        bool hasSlewRates()
        {
            return m_hasSlewRates;
        }
        QStringList slewRates()
        {
            return m_slewRates;
        }
        int getSlewRate() const;

        // Pier side
        PierSide pierSide() const
        {
            return m_PierSide;
        }

        // Satellite tracking
        bool canTrackSatellite()
        {
            return m_canTrackSatellite;
        }

        /**
         * @short Tracks satellite on provided TLE, initial epoch for trajectory calculation and window in minutes
         *
         * This function needs a Two-Line-Element and a time window in the form of an initial point and a
         * number of minutes on which the trajectory should start. The function was developed wiht the lx200
         * in mind. If the trajectory has already started, the current time and a window of 1min are sufficient.
         *
         * @param tle Two-line-element.
         * @param satPassStart Start time of the trajectory calculation
         * @param satPassEnd End time of the trajectory calculation
         */
        bool setSatelliteTLEandTrack(QString tle, const KStarsDateTime satPassStart, const KStarsDateTime satPassEnd);

        /**
         * @brief Hour angle of the current coordinates
         */
        const dms hourAngle() const;

        const SkyPoint &currentCoordinates() const
        {
            return currentCoords;
        }

        /**
         * @brief stopTimers Stop timers to prevent timing race condition when device is unavailable
         * and timer is still invoked.
         */
        void stopTimers();

        void centerLock();
        void centerUnlock();
        void find();
        void setCustomParking(SkyPoint *coords = nullptr);

    protected:
        /**
         * @brief Send the coordinates to the mount's INDI driver. Due to the INDI implementation, this
         * function is shared for syncing, slewing and other (partly scope specific) functions like the
         * setting parking position. The interpretation of the coordinates depends in the setting of other
         * INDI switches for slewing, synching, tracking etc.
         * @param ScopeTarget target coordinates
         * @return true if sending the coordinates succeeded
         */
        bool sendCoords(SkyPoint *ScopeTarget);

        /**
         * @brief Check whether sending new coordinates will result into a slew
         */
        bool slewDefined();

        /**
         * @brief Helper function to update the J2000 coordinates of a sky point from its JNow coordinates
         * @param coords sky point with correct JNow values in RA and DEC
         */
        void updateJ2000Coordinates(SkyPoint *coords);

        /**
         * @brief updateParkStatus Updating parking status by checking the TELESCOPE_PARK property.
         */
        void updateParkStatus();

    public slots:
        bool abort();
        bool park();
        bool unPark();
        bool setSlewRate(int index);
        bool setTrackEnabled(bool enable);
        bool setCustomTrackRate(double raRate, double deRate);
        bool setTrackMode(uint8_t index);

    signals:
        /**
         * @brief The mount has finished the slew to a new target.
         * @param currentCoords exact position where the mount is positioned
         */
        void newTarget(SkyPoint &currentCoords);

        /**
         * @brief The mount has finished the slew to a new target.
         * @param Name Name of object, if any, the mount is positioned at.
         */
        void newTargetName(const QString &name);
        /**
         * @brief Change in the mount status.
         */
        void newStatus(ISD::Mount::Status status);
        /**
         * @brief Update event with the current telescope position
         * @param position mount position. Independent from the mount type,
         * the EQ coordinates(both JNow and J2000) as well as the alt/az values are filled.
         * @param pierside for GEMs report the pier side the scope is currently (PierSide::PIER_WEST means
         * the mount is on the western side of the pier pointing east of the meridian).
         * @param ha current hour angle
         */
        void newCoords(const SkyPoint &position, const PierSide pierside, const dms &ha);
        void newParkStatus(ISD::ParkStatus status);
        void slewRateChanged(int rate);
        void pierSideChanged(PierSide side);
        void axisReversed(INDI_EQ_AXIS axis, bool reversed);

    private:
        SkyPoint currentCoords;
        double minAlt {0}, maxAlt = 90;
        ParkStatus m_ParkStatus = PARK_UNKNOWN;
        IPState EqCoordPreviousState {IPS_IDLE};
        QTimer centerLockTimer;
        QTimer updateCoordinatesTimer;
        SkyObject *currentObject = nullptr;
        bool inManualMotion      = false;
        bool inCustomParking     = false;
        IPState NSPreviousState  = IPS_IDLE;
        IPState WEPreviousState  = IPS_IDLE;
        PierSide m_PierSide = PIER_UNKNOWN;

        KStarsDateTime g_satPassStart;
        KStarsDateTime g_satPassEnd;

        QMap<TrackModes, uint8_t> TrackMap;
        TrackModes currentTrackMode { TRACK_SIDEREAL };

        bool m_hasAlignmentModel = { false };
        bool m_canControlTrack = { false };
        bool m_canGoto { false};
        bool m_canSync { false};
        bool m_canAbort { false };
        bool m_canTrackSatellite { false };
        bool m_TLEIsSetForTracking { false };
        bool m_windowIsSetForTracking { false };
        bool m_hasTrackModes { false};
        bool m_hasCustomTrackRate { false};
        bool m_hasCustomParking { false };
        bool m_hasSlewRates { false };
        bool m_isJ2000 { false };
        bool m_hasEquatorialCoordProperty { false };
        QStringList m_slewRates;
};
}

Q_DECLARE_METATYPE(ISD::Mount::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Mount::Status &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Mount::Status &dest);

Q_DECLARE_METATYPE(ISD::Mount::PierSide)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Mount::PierSide &source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Mount::PierSide &dest);
