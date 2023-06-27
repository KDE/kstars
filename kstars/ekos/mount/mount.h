/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef MOUNT_H
#define MOUNT_H

#include <QQmlContext>
#include "ui_mount.h"

#include "indi/indistd.h"
#include "indi/indifocuser.h"
#include "indi/indimount.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/align/polaralignmentassistant.h"

class QQuickView;
class QQuickItem;

namespace Ekos
{
/**
 *@class Mount
 *@short Supports controlling INDI telescope devices including setting/retrieving mount properties, slewing, motion and speed controls, in addition to enforcing altitude limits and parking/unparking.
 *@author Jasem Mutlaq
 *@version 1.5
 */

class OpticalTrainManager;

class Mount : public QWidget, public Ui::Mount
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Mount")
        Q_PROPERTY(QString opticalTrain READ opticalTrain WRITE setOpticalTrain)
        Q_PROPERTY(ISD::Mount::Status status READ status NOTIFY newStatus)
        Q_PROPERTY(ISD::ParkStatus parkStatus READ parkStatus NOTIFY newParkStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QList<double> altitudeLimits READ altitudeLimits WRITE setAltitudeLimits)
        Q_PROPERTY(bool altitudeLimitsEnabled READ altitudeLimitsEnabled WRITE setAltitudeLimitsEnabled)
        Q_PROPERTY(double hourAngleLimit READ hourAngleLimit WRITE setHourAngleLimit)
        Q_PROPERTY(bool hourAngleLimitEnabled READ hourAngleLimitEnabled WRITE setHourAngleLimitEnabled)
        Q_PROPERTY(bool autoParkEnabled READ autoParkEnabled WRITE setAutoParkEnabled)
        Q_PROPERTY(QList<double> equatorialCoords READ equatorialCoords)
        Q_PROPERTY(QList<double> horizontalCoords READ horizontalCoords)
        Q_PROPERTY(double hourAngle READ hourAngle)
        Q_PROPERTY(int slewRate READ slewRate WRITE setSlewRate)
        Q_PROPERTY(int slewStatus READ slewStatus)
        Q_PROPERTY(bool canPark READ canPark)
        Q_PROPERTY(ISD::Mount::PierSide pierSide READ pierSide NOTIFY pierSideChanged)

    public:
        Mount();
        ~Mount() override;

        //typedef enum { PARKING_IDLE, PARKING_OK, UNPARKING_OK, PARKING_BUSY, UNPARKING_BUSY, PARKING_ERROR } ParkingStatus;

        /**
             * @brief addMount Add a new Mount device
             * @param device pointer to mount device
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setMount(ISD::Mount *device);

        /**
             * @brief addGPS Add a new GPS device
             * @param device pointer to gps device
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool addGPS(ISD::GPS *device);

        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);

        void setupOpticalTrainManager();
        void refreshOpticalTrain();
        QString opticalTrain() const
        {
            return opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrainCombo->setCurrentText(value);
        }

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

        ISD::Mount::Status status() const
        {
            return m_Status;
        }
        ISD::Mount *activeMount() const
        {
            return m_Mount;
        }
        QString statusString(bool translated = true) const
        {
            if (m_Mount)
                return m_Mount->statusString(m_Status, translated);
            else
                return "NA";
        }
        ISD::Mount::PierSide pierSide() const
        {
            if (m_Mount)
                return m_Mount->pierSide();
            else
                return ISD::Mount::PIER_UNKNOWN;
        }
        ISD::ParkStatus parkStatus() const
        {
            return m_ParkStatus;
        }

        /**
         * @brief getMeridianFlipState
         * @return
         */
        QSharedPointer<MeridianFlipState> getMeridianFlipState() const
        {
            return mf_state;
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

        /** DBUS interface function.
             * Get the hour angle of that time the mount has slewed to the current position.
             * This is used to manage the meridian flip for mounts which do not report pier side.
             * only one attempt to flip is done.
             */
        Q_SCRIPTABLE double initialHA()
        {
            return mf_state->initialPositionHA();
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

        Q_INVOKABLE void setUpDownReversed(bool enabled);
        Q_INVOKABLE void setLeftRightReversed(bool enabled);

        QString meridianFlipStatusDescription()
        {
            return meridianFlipStatusWidget->getStatus();
        }

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);

    public slots:

        /**
             * @brief syncTelescopeInfo Update telescope information to reflect any property changes
             */
        void syncTelescopeInfo();
        /**
             * @brief updateProperty Update properties under watch in the mount module
             * @param prop INDI property
             */
        void updateProperty(INDI::Property prop);

        /**
             * @brief updateLog Update mount module log to include any messages arriving for the telescope driver
             * @param messageID ID of the new message
             */
        void updateLog(int messageID);

        /**
             * @brief updateTelescopeCoords is triggered by the ISD::Mount::newCoord() event and updates the displayed
             * coordinates of the mount and to ensure mount is within altitude limits if the altitude limits are enabled.
             * The frequency of this update depends on the REFRESH parameter of the INDI mount device.
             * @param position latest coordinates the mount reports it is pointing to
             * @param pierSide pierSide
             * @param ha hour angle of the latest coordinates
             */
        void updateTelescopeCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

        /**
             * @brief move Issues motion command to the mount to move in a particular direction based the request NS and WE values
             * @param command Either ISD::Mount::MOTION_START (0) or ISD::Mount::MOTION_STOP (1)
             * @param NS is either -1 for no direction, or ISD::Mount::MOTION_NORTH (0), or ISD::Mount::MOTION_SOUTH (1)
             * @param WE is either -1 for no direction, or ISD::Mount::MOTION_WEST (0), or ISD::Mount::MOTION_EAST (1)
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
             * @brief saveLimits Saves altitude limit to the user options and updates the INDI telescope driver limits
             */
        void saveLimits();

        /**
             * @brief Enable or disable altitude limits
             * @param enable True to enable, false to disable.
             */
        void setAltitudeLimits(bool enable);

        /**
             * @brief resumeAltLimits calls enableAltitudeLimits(true). This function is mostly used to enable altitude limit after a meridian flip is complete.
             */
        void resumeAltLimits();

        /**
             * @brief suspendAltLimits calls enableAltitudeLimits(false). This function is mostly used to disable altitude limit once a meridial flip process is started.
             */
        void suspendAltLimits();

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

        void toggleMountToolBox();

        /**
         * @brief set meridian flip activation and hours
         * @param activate true iff the meridian flip should be executed
         * @param degrees angle past the meridian when the flip should be delayed
         */
        void setMeridianFlipValues(bool activate, double degrees);

        /**
         * @brief React upon status changes of the polar alignment - mainly to
         *        avoid meridian flips happening during polar alignment.
         */
        void paaStageChanged(int stage);

        /**
         * @brief registerNewModule Register an Ekos module as it arrives via DBus
         * and create the appropriate DBus interface to communicate with it.
         * @param name of module
         */
        void registerNewModule(const QString &name);

        /**
         * @brief gotoTarget Slew to target coordinates.
         * @param target Target
         * @return True if slew successful, false otherwise.
         */
        bool gotoTarget(const SkyPoint &target);

        /**
         * @brief syncAxisReversed Update Mount Control GUI on the reverse motion toggled state.
         * @param axis RA (left/right) or DE (up/down)
         * @param reversed True if reversed, false otherwise.
         */
        void syncAxisReversed(INDI_EQ_AXIS axis, bool reversed);

        /**
         * @brief stopTimers Need to stop update timers when profile is disconnected
         * but due to timing and race conditions, the timers can trigger an invalid access
         * to INDI device.
         */
        void stopTimers();

    private slots:
        void startParkTimer();
        void stopParkTimer();
        void startAutoPark();

    signals:
        void newLog(const QString &text);
        /**
         * @brief Update event with the current telescope position
         * @param position mount position. Independent from the mount type,
         * the EQ coordinates(both JNow and J2000) as well as the alt/az values are filled.
         * @param pierside for GEMs report the pier side the scope is currently (PierSide::PIER_WEST means
         * the mount is on the western side of the pier pointing east of the meridian).
         * @param ha current hour angle
         */
        void newCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);
        /**
         * @brief The mount has finished the slew to a new target.
         * @param currentCoords exact position where the mount is positioned
         */
        void newTarget(SkyPoint &currentCoord);

        /**
         * @brief The mount has finished the slew to a new target.
         * @param Name Name of object, if any, the mount is positioned at.
         */
        void newTargetName(const QString &name);
        /**
         * @brief Change in the mount status.
         */
        void newStatus(ISD::Mount::Status status);
        void newParkStatus(ISD::ParkStatus status);
        void pierSideChanged(ISD::Mount::PierSide side);
        void slewRateChanged(int index);
        void ready();
        void newMeridianFlipText(const QString &text);

        void settingsUpdated(const QVariantMap &settings);

        void trainChanged();

    private:
        ////////////////////////////////////////////////////////////////////
        /// Settings
        ////////////////////////////////////////////////////////////////////

        /**
         * @brief Connect GUI elements to sync settings once updated.
         */
        void connectSettings();
        /**
         * @brief Stop updating settings when GUI elements are updated.
         */
        void disconnectSettings();
        /**
         * @brief loadSettings Load setting from Options and set them accordingly.
         */
        void loadGlobalSettings();

        /**
         * @brief syncSettings When checkboxes, comboboxes, or spin boxes are updated, save their values in the
         * global and per-train settings.
         */
        void syncSettings();

        /**
         * @brief syncControl Sync setting to widget. The value depends on the widget type.
         * @param settings Map of all settings
         * @param key name of widget to sync
         * @param widget pointer of widget to set
         * @return True if sync successful, false otherwise
         */
        bool syncControl(const QVariantMap &settings, const QString &key, QWidget * widget);

        void syncGPS();
        void setScopeStatus(ISD::Mount::Status status);
        /* Meridian flip state handling */
        QSharedPointer<MeridianFlipState> mf_state;
        void setupParkUI();

        bool hasCaptureInterface { false };

        ISD::Mount *m_Mount {nullptr};
        ISD::GPS *m_GPS {nullptr};
        QList<ISD::GPS*> m_GPSes;

        QStringList m_LogText;
        SkyPoint telescopeCoord;
        SkyPoint *targetPosition {nullptr};
        QString lastNotificationMessage;

        // Auto Park
        QTimer autoParkTimer;

        // Limits
        int m_AbortDispatch {-1};
        bool m_AltitudeLimitEnabled {false};
        double m_LastAltitude {0};
        bool m_HourAngleLimitEnabled {false};
        double m_LastHourAngle {0};

        // GPS
        bool GPSInitialized = {false};

        ISD::Mount::Status m_Status = ISD::Mount::MOUNT_IDLE;
        ISD::ParkStatus m_ParkStatus = ISD::PARK_UNKNOWN;

        // Settings
        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;

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
