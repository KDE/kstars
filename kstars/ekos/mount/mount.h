/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MOUNT_H
#define MOUNT_H

#include <QQmlContext>
#include <QtDBus>
#include "ui_mount.h"

#include "indi/indistd.h"
#include "indi/indifocuser.h"
#include "indi/inditelescope.h"

class QQuickView;
class QQuickItem;

namespace Ekos
{
/**
 *@class Mount
 *@short Supports controlling INDI telescope devices including setting/retrieving mount properties, slewing, motion and speed controls, in addition to enforcing altitude limits and parking/unparking.
 *@author Jasem Mutlaq
 *@version 1.4
 */

class Mount : public QWidget, public Ui::Mount
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Mount")
        Q_PROPERTY(ISD::Telescope::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QList<double> altitudeLimits READ altitudeLimits WRITE setAltitudeLimits)
        Q_PROPERTY(bool altitudeLimitsEnabled READ altitudeLimitsEnabled WRITE setAltitudeLimitsEnabled)
        Q_PROPERTY(double hourAngleLimit READ hourAngleLimit WRITE setHourAngleLimit)
        Q_PROPERTY(bool hourAngleLimitEnabled READ hourAngleLimitEnabled WRITE setHourAngleLimitEnabled)
        Q_PROPERTY(bool autoParkEnabled READ autoParkEnabled WRITE setAutoParkEnabled)
        Q_PROPERTY(QList<double> equatorialCoords READ equatorialCoords)
        Q_PROPERTY(QList<double> horizontalCoords READ horizontalCoords)
        Q_PROPERTY(QList<double> telescopeInfo READ telescopeInfo WRITE setTelescopeInfo)
        Q_PROPERTY(double hourAngle READ hourAngle)
        Q_PROPERTY(int slewRate READ slewRate WRITE setSlewRate)
        Q_PROPERTY(int slewStatus READ slewStatus)
        Q_PROPERTY(bool canPark READ canPark)
        Q_PROPERTY(ISD::Telescope::PierSide pierSide READ pierSide NOTIFY pierSideChanged)

    public:
        Mount();
        ~Mount();

        //typedef enum { PARKING_IDLE, PARKING_OK, UNPARKING_OK, PARKING_BUSY, UNPARKING_BUSY, PARKING_ERROR } ParkingStatus;

        // This enum defines the meridian flip state machine, this is implemented in
        typedef enum
        {
            FLIP_NONE,      // this is the default state, comparing the hour angle with the next flip position
            // it moves to FLIP_PLANNED when a flip is needed.
            FLIP_PLANNED,   // this signals to the Capture class that a flip is required, the Capture class will
            // move to FLIP_ACCEPTED when it has completed everything that needs to be done.
            FLIP_WAITING,   // Capture seems to set this state to signal that the flip will have to wait
            FLIP_ACCEPTED,  // Capture signals to the mount that a flip slew can be started
            FLIP_RUNNING,   // this signals that a flip slew is in progress, when the slew stops the state
            // is set to FLIP_COMPLETED
            FLIP_COMPLETED, // this checks that the flip was completed successfully or not and after tidying up
            // moves to FLIP_NONE to wait for the next flip requirement.
            // Capture sees this and resumes.
            FLIP_ERROR      // errors in the flip process should end up here
        } MeridianFlipStatus;

        /**
             * @brief setTelescope Sets the mount module telescope interface
             * @param newTelescope pointer to telescope interface object
             */
        void setTelescope(ISD::GDInterface *newTelescope);

        void setGPS(ISD::GDInterface *newGPS);

        void removeDevice(ISD::GDInterface *device);

        // Log functions
        void appendLogText(const QString &);
        void clearLog();
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText() const
        {
            return m_LogText.join("\n");
        }

        ISD::Telescope::Status status() const
        {
            return m_Status;
        }
        ISD::Telescope::PierSide pierSide() const
        {
            if (currentTelescope)
                return currentTelescope->pierSide();
            else
                return ISD::Telescope::PIER_UNKNOWN;
        }
        ISD::ParkStatus parkStatus() const
        {
            return m_ParkStatus;
        }

        MeridianFlipStatus meridianFlipStatus() const
        {
            return m_MFStatus;
        }

        /** @defgroup MountDBusInterface Ekos Mount DBus Interface
             * Mount interface provides advanced scripting capabilities to control INDI mounts.
            */

        /*@{*/

        /** DBUS interface function.
             * Returns the mount altitude limits.
             * @return Returns array of doubles. First item is minimum altitude in degrees. Second item is maximum altitude limit in degrees.
             */
        Q_SCRIPTABLE QList<double> altitudeLimits();

        /** DBUS interface function.
             * Sets the mount altitude limits, and whether they are enabled or disabled.
             * @param limits is a list of double values. 2 values are expected: minAltitude & maxAltitude
             */
        Q_SCRIPTABLE Q_NOREPLY void setAltitudeLimits(QList<double> limits);

        /** DBUS interface function.
             * Enable or disable mount altitude limits.
             */
        Q_SCRIPTABLE void setAltitudeLimitsEnabled(bool enable);

        /** DBUS interface function.
             * Returns whether the mount limits are enabled or disabled.
             * @return True if enabled, false otherwise.
             */
        Q_SCRIPTABLE bool altitudeLimitsEnabled();

        /** DBUS interface function.
             * Returns the mount hour angle limit.
             * @return Returns hour angle limit in hours.
             */
        Q_SCRIPTABLE double hourAngleLimit();

        /** DBUS interface function.
             * Sets the mount altitude limits, and whether they are enabled or disabled.
             * @param limits is a list of double values. 2 values are expected: minAltitude & maxAltitude
             */
        Q_SCRIPTABLE Q_NOREPLY void setHourAngleLimit(double limit);

        /** DBUS interface function.
             * Enable or disable mount hour angle limit. Mount cannot slew and/or track past this
             * hour angle distance.
             */
        Q_SCRIPTABLE void setHourAngleLimitEnabled(bool enable);

        /** DBUS interface function.
             * Returns whether the mount limits are enabled or disabled.
             * @return True if enabled, false otherwise.
             */
        Q_SCRIPTABLE bool hourAngleLimitEnabled();

        /**
         * @brief autoParkEnabled Check if auto-park is enabled.
         * @return True if enabled.
         */
        Q_SCRIPTABLE bool autoParkEnabled();

        /**
         * @brief setAutoParkEnabled Toggle Auto Park
         * @param enable True to start, false to stop
         */
        Q_SCRIPTABLE void setAutoParkEnabled(bool enable);

        /**
         * @brief setAutoParkDailyEnabled toggles everyday Auto Park
         * @param enable true to activate, false to deactivate
         */
        Q_SCRIPTABLE void setAutoParkDailyEnabled(bool enabled);

        /**
         * @brief setAutoParkStartup Set time when automatic parking is activated.
         * @param startup Startup time. should not be more than 12 hours away.
         */
        Q_SCRIPTABLE void setAutoParkStartup(QTime startup);

        Q_SCRIPTABLE bool meridianFlipEnabled();
        Q_SCRIPTABLE double meridianFlipValue();

        /** DBUS interface function.
             * Slew the mount to the RA/DEC (JNow).
             * @param RA Right ascention is hours.
             * @param DEC Declination in degrees.
             * @return true if the command is sent successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool slew(double RA, double DEC);

        /**
              @brief Like above but RA and DEC are strings HH:MM:SS and DD:MM:SS
            */
        Q_INVOKABLE bool slew(const QString &RA, const QString &DEC);

        /** DBUS interface function.
             * Slew the mount to the target. Target name must be valid in KStars.
             * @param target name
             * @return true if the command is sent successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool gotoTarget(const QString &target);

        /** DBUS interface function.
             * Sync the mount to the RA/DEC (JNow).
             * @param RA Right ascention is hours.
             * @param DEC Declination in degrees.
             * @return true if the command is sent successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool sync(double RA, double DEC);

        /** DBUS interface function.
             * Sync the mount to the target. Target name must be valid in KStars.
             * @param target name
             * @return true if the command is sent successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool syncTarget(const QString &target);

        /**
              @brief Like above but RA and DEC are strings HH:MM:SS and DD:MM:SS
            */
        Q_INVOKABLE bool sync(const QString &RA, const QString &DEC);

        /** DBUS interface function.
             * Get equatorial coords (JNow). An array of doubles is returned. First element is RA in hours. Second elements is DEC in degrees.
             */
        Q_SCRIPTABLE QList<double> equatorialCoords();

        /** DBUS interface function.
             * Get Horizontal coords. An array of doubles is returned. First element is Azimuth in degrees. Second elements is Altitude in degrees.
             */
        Q_SCRIPTABLE QList<double> horizontalCoords();

        /** DBUS interface function.
             * Get Horizontal coords.
             */
        Q_SCRIPTABLE SkyPoint currentTarget();

        /** DBUS interface function.
             * Get mount hour angle in hours (-12 to +12).
             */
        Q_SCRIPTABLE double hourAngle();

        double initialPositionHA;
        /** DBUS interface function.
             * Get the hour angle of that time the mount has slewed to the current position.
             * This is used to manage the meridian flip for mounts which do not report pier side.
             * only one attempt to flip is done.
             */
        Q_SCRIPTABLE double initialHA()
        {
            return initialPositionHA;
        }

        Q_SCRIPTABLE void setInitialHA(double ha)
        {
            initialPositionHA = ha;
        }

        /** DBUS interface function.
             * Aborts the mount motion
             * @return true if the command is sent successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool abort();

        /** DBUS interface function.
             * Get the mount slew status ("Idle","Complete", "Busy", "Error")
             */
        Q_INVOKABLE Q_SCRIPTABLE IPState slewStatus();


        /** DBUS interface function.
             * Get the mount slew rate index 0 to N-1, or -1 if slew rates are not supported.
             */
        Q_INVOKABLE Q_SCRIPTABLE int slewRate();

        Q_INVOKABLE Q_SCRIPTABLE bool setSlewRate(int index);

        /** DBUS interface function.
             * Get telescope and guide scope info. An array of doubles is returned in order.
             * Primary Telescope Focal Length (mm), Primary Telescope Aperture (mm), Guide Telescope Focal Length (mm), Guide Telescope Aperture (mm)
             */
        Q_INVOKABLE Q_SCRIPTABLE QList<double> telescopeInfo();

        /** DBUS interface function.
             * Set telescope and guide scope info and save them in INDI mount driver. All measurements is in millimeters.
             * @param info An ordered 4-item list as following:
             * primaryFocalLength Primary Telescope Focal Length. Set to 0 to skip setting this value.
             * primaryAperture Primary Telescope Aperture. Set to 0 to skip setting this value.
             * guideFocalLength Guide Telescope Focal Length. Set to 0 to skip setting this value.
             * guideAperture Guide Telescope Aperture. Set to 0 to skip setting this value.
             */
        Q_SCRIPTABLE Q_NOREPLY void setTelescopeInfo(const QList<double> &info);

        /** DBUS interface function.
             * Reset mount model if supported by the mount.
             * @return true if the command is executed successfully, false otherwise.
             */
        Q_INVOKABLE Q_SCRIPTABLE bool resetModel();

        /** DBUS interface function.
             * Can mount park?
             */
        Q_INVOKABLE Q_SCRIPTABLE bool canPark();

        /** DBUS interface function.
             * Park mount
             */
        Q_INVOKABLE Q_SCRIPTABLE bool park();

        /** DBUS interface function.
             * Unpark mount
             */
        Q_INVOKABLE Q_SCRIPTABLE bool unpark();

        /** DBUS interface function.
             * Return parking status of the mount.
             */
        //Q_INVOKABLE Q_SCRIPTABLE ParkingStatus getParkingStatus();

        Q_INVOKABLE void setTrackEnabled(bool enabled);

        Q_INVOKABLE void setJ2000Enabled(bool enabled);

        /** @}*/

        Q_INVOKABLE void findTarget();

        // target coord conversions for displaying
        Q_INVOKABLE bool raDecToAzAlt(QString qsRA, QString qsDec);
        Q_INVOKABLE bool raDecToHaDec(QString qsRA);
        Q_INVOKABLE bool azAltToRaDec(QString qsAz, QString qsAlt);
        Q_INVOKABLE bool azAltToHaDec(QString qsAz, QString qsAlt);
        Q_INVOKABLE bool haDecToRaDec(QString qsHA);
        Q_INVOKABLE bool haDecToAzAlt(QString qsHA, QString qsDec);

        // Center mount in Sky Map
        Q_INVOKABLE void centerMount();

        // Get list of scopes
        //QJsonArray getScopes() const;

        /*
         * @brief Check if a meridian flip if necessary.
         * @param lst current local sideral time
         * @return true if a meridian flip is necessary
         */
        bool checkMeridianFlip(dms lst);

        /*
         * @brief Execute a meridian flip if necessary.
         * @return true if a meridian flip was necessary
         */
        Q_INVOKABLE bool executeMeridianFlip();

        Q_INVOKABLE void setUpDownReversed(bool enabled);
        Q_INVOKABLE void setLeftRightReversed(bool enabled);

        QString meridianFlipStatusDescription()
        {
            return meridianFlipStatusText->text();
        }

        ///
        /// \brief meridianFlipStatusString
        /// \param status
        /// \return return the string for the status
        ///
        static QString meridianFlipStatusString(MeridianFlipStatus status);

    public slots:

        /**
             * @brief syncTelescopeInfo Update telescope information to reflect any property changes
             */
        void syncTelescopeInfo();
        /**
             * @brief updateNumber Update number properties under watch in the mount module
             * @param nvp pointer to number property
             */
        void updateNumber(INumberVectorProperty *nvp);

        /**
             * @brief updateSwitch Update switch properties under watch in the mount module
             * @param svp pointer to switch property
             */
        void updateSwitch(ISwitchVectorProperty *svp);


        /**
             * @brief updateText Update text properties under watch in the mount module
             * @param tvp pointer to text property
             */
        void updateText(ITextVectorProperty *tvp);

        /**
             * @brief updateLog Update mount module log to include any messages arriving for the telescope driver
             * @param messageID ID of the new message
             */
        void updateLog(int messageID);

        /**
             * @brief updateTelescopeCoords runs every UPDATE_DELAY milliseconds to update the displayed coordinates of the mount and to ensure mount is
             * within altitude limits if the altitude limits are enabled.
             */
        void updateTelescopeCoords();

        /**
             * @brief move Issues motion command to the mount to move in a particular direction based the request NS and WE values
             * @param command Either ISD::Telescope::MOTION_START (0) or ISD::Telescope::MOTION_STOP (1)
             * @param NS is either -1 for no direction, or ISD::Telescope::MOTION_NORTH (0), or ISD::Telescope::MOTION_SOUTH (1)
             * @param WE is either -1 for no direction, or ISD::Telescope::MOTION_WEST (0), or ISD::Telescope::MOTION_EAST (1)
             */
        void motionCommand(int command, int NS, int WE);

        /**
         * @brief Send a guide pulse to the telescope.
         * @param ra_dir RA guide direction
         * @param ra_msecs duration of the RA guiding pulse in milliseconds
         * @param dec_dir dec guide direction
         * @param dec_msecs duration of the DEC guiding pulse in milliseconds
         */
        void doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);

        /**
             * @brief save Save telescope focal length and aperture in the INDI telescope driver configuration.
             */
        void save();

        /**
             * @brief saveLimits Saves altitude limit to the user options and updates the INDI telescope driver limits
             */
        void saveLimits();

        /**
             * @brief enableAltitudeLimits Enable or disable altitude limits
             * @param enable True to enable, false to disable.
             */
        void enableAltitudeLimits(bool enable);

        /**
             * @brief enableAltLimits calls enableAltitudeLimits(true). This function is mostly used to enable altitude limit after a meridian flip is complete.
             */
        void enableAltLimits();

        /**
             * @brief disableAltLimits calls enableAltitudeLimits(false). This function is mostly used to disable altitude limit once a meridial flip process is started.
             */
        void disableAltLimits();

        /**
             * @brief enableHourAngleLimits Enable or disable hour angle limits
             * @param enable True to enable, false to disable.
             */
        void enableHourAngleLimits(bool enable);

        /**
             * @brief enableHaLimits calls enableHourAngleLimits(true). This function is mostly used to enable hour angle limit after a meridian flip is complete.
             */
        void enableHaLimits();

        /**
             * @brief disableAltLimits calls enableHourAngleLimits(false). This function is mostly used to disable altitude limit once a meridial flip process is started.
             */
        void disableHaLimits();

        bool setScopeConfig(int index);

        void toggleMountToolBox();

        void meridianFlipStatusChanged(MeridianFlipStatus status);

        /*
         * @brief set meridian flip activation and hours
         * @param activate true iff the meridian flip should be executed
         * @param hours angle past the meridian when the flip should be delayed
         */
        void setMeridianFlipValues(bool activate, double hours);

        /**
         * @brief registerNewModule Register an Ekos module as it arrives via DBus
         * and create the appropriate DBus interface to communicate with it.
         * @param name of module
         */
        void registerNewModule(const QString &name);

    private slots:
        void startParkTimer();
        void stopParkTimer();
        void startAutoPark();

        void meridianFlipSetupChanged();

    signals:
        void newLog(const QString &text);
        void newCoords(const QString &ra, const QString &dec, const QString &az,
                       const QString &alt, int pierSide, const QString &ha);
        void newTarget(const QString &name);
        void newStatus(ISD::Telescope::Status status);
        void newParkStatus(ISD::ParkStatus status);
        void pierSideChanged(ISD::Telescope::PierSide side);
        void slewRateChanged(int index);
        void ready();
        void newMeridianFlipStatus(MeridianFlipStatus status);
        void newMeridianFlipText(const QString &text);

    private:
        void syncGPS();
        MeridianFlipStatus m_MFStatus = FLIP_NONE;
        void setMeridianFlipStatus(MeridianFlipStatus status);
        void meridianFlipStatusChangedInternal(MeridianFlipStatus status);
        QString pierSideStateString();

        // A meridian flip requires a slew of 180 degrees in the hour angle axis so will take at least
        // the time for that, currently set to 20 seconds
        // not reliable for pointing state change detection but reported if the pier side is unknown
        QDateTime minMeridianFlipEndTime;
        int minMeridianFlipDurationSecs = 20;

        double flipDelayHrs = 0.0;      // delays the next flip attempt if it fails

        QPointer<QDBusInterface> captureInterface { nullptr };

        ISD::Telescope *currentTelescope = nullptr;
        ISD::GDInterface *currentGPS = nullptr;
        QStringList m_LogText;
        SkyPoint *currentTargetPosition = nullptr;
        SkyPoint telescopeCoord;
        QString lastNotificationMessage;

        // Auto Park
        QTimer updateTimer;
        QTimer autoParkTimer;

        // Limits
        int m_AbortDispatch {-1};
        bool m_AltitudeLimitEnabled {false};
        double m_LastAltitude {0};
        bool m_HourAngleLimitEnabled {false};
        double m_LastHourAngle {0};

        // GPS
        bool GPSInitialized = {false};

        ISD::Telescope::Status m_Status = ISD::Telescope::MOUNT_IDLE;
        ISD::ParkStatus m_ParkStatus = ISD::PARK_UNKNOWN;

        QQuickView *m_BaseView = nullptr;
        QQuickItem *m_BaseObj  = nullptr;
        QQmlContext *m_Ctxt    = nullptr;

        QQuickItem *m_SpeedSlider = nullptr, *m_SpeedLabel = nullptr,
                    *m_raValue = nullptr, *m_deValue = nullptr, *m_azValue = nullptr,
                     *m_altValue = nullptr, *m_haValue = nullptr, *m_zaValue = nullptr,
                      *m_targetText = nullptr, *m_targetRAText = nullptr,
                       *m_targetDEText = nullptr, *m_Park = nullptr, *m_Unpark = nullptr,
                        *m_statusText = nullptr, *m_J2000Check = nullptr,
                         *m_JNowCheck = nullptr, *m_equatorialCheck = nullptr,
                          *m_horizontalCheck = nullptr, *m_haEquatorialCheck = nullptr,
                           *m_leftRightCheck = nullptr, *m_upDownCheck = nullptr;
};
}


#endif // Mount
