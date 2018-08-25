/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
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
 *@version 1.3
 */
class Mount : public QWidget, public Ui::Mount
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Mount")

  public:
    Mount();
    ~Mount();

    typedef enum { PARKING_IDLE, PARKING_OK, UNPARKING_OK, PARKING_BUSY, UNPARKING_BUSY, PARKING_ERROR } ParkingStatus;

    /**
         * @brief setTelescope Sets the mount module telescope interface
         * @param newTelescope pointer to telescope interface object
         */
    void setTelescope(ISD::GDInterface *newTelescope);

    void setGPS(ISD::GDInterface *newGPS);

    // Log functions
    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

    /** @defgroup MountDBusInterface Ekos Mount DBus Interface
         * Mount interface provides advanced scripting capabilities to control INDI mounts.
        */

    /*@{*/

    /** DBUS interface function.
         * Returns the mount altitude limits.
         * @return Returns array of doubles. First item is minimum altititde in degrees. Second item is maximum altitude limit in degrees.
         */
    Q_SCRIPTABLE QList<double> getAltitudeLimits();

    /** DBUS interface function.
         * Sets the mount altitude limits, and whether they are enabled or disabled.
         */
    Q_SCRIPTABLE Q_NOREPLY void setAltitudeLimits(double minAltitude, double maxAltitude, bool enabled);

    /** DBUS interface function.
         * Returns whether the mount limits are enabled or disabled.
         * @return True if enabled, false otherwise.
         */
    Q_SCRIPTABLE bool isLimitsEnabled();

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
    Q_INVOKABLE Q_SCRIPTABLE QList<double> getEquatorialCoords();

    /** DBUS interface function.
         * Get Horizontal coords. An array of doubles is returned. First element is Azimuth in degrees. Second elements is Altitude in degrees.
         */
    Q_SCRIPTABLE QList<double> getHorizontalCoords();

    /** DBUS interface function.
         * Get mount hour angle in hours (-12 to +12).
         */
    Q_SCRIPTABLE double getHourAngle();

    /** DBUS interface function.
         * Aborts the mount motion
         * @return true if the command is sent successfully, false otherwise.
         */
    Q_INVOKABLE Q_SCRIPTABLE bool abort();

    /** DBUS interface function.
         * Get the mount slew status ("Idle","Complete", "Busy", "Error")
         */
    Q_INVOKABLE Q_SCRIPTABLE IPState getSlewStatus();


    /** DBUS interface function.
         * Get the mount slew rate index 0 to N-1, or -1 if slew rates are not supported.
         */
    Q_INVOKABLE Q_SCRIPTABLE int getSlewRate();

    /** DBUS interface function.
         * Get telescope and guide scope info. An array of doubles is returned in order.
         * Primary Telescope Focal Length (mm), Primary Telescope Aperture (mm), Guide Telescope Focal Length (mm), Guide Telescope Aperture (mm)
         */
    Q_INVOKABLE Q_SCRIPTABLE QList<double> getTelescopeInfo();

    /** DBUS interface function.
         * Set telescope and guide scope info and save them in INDI mount driver. All measurements is in millimeters.
         * @param primaryFocalLength Primary Telescope Focal Length. Set to 0 to skip setting this value.
         * @param primaryAperture Primary Telescope Aperture. Set to 0 to skip setting this value.
         * @param guideFocalLength Guide Telescope Focal Length. Set to 0 to skip setting this value.
         * @param guideAperture Guide Telescope Aperture. Set to 0 to skip setting this value.
         */
    Q_SCRIPTABLE Q_NOREPLY void setTelescopeInfo(double primaryFocalLength, double primaryAperture,
                                                 double guideFocalLength, double guideAperture);

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
    Q_INVOKABLE Q_SCRIPTABLE ParkingStatus getParkingStatus();

    Q_INVOKABLE bool setSlewRate(int index);    

    Q_INVOKABLE void setTrackEnabled(bool enabled);

    Q_INVOKABLE void setJ2000Enabled(bool enabled);

    /** @}*/

    Q_INVOKABLE void findTarget();

    // Center mount in Sky Map
    Q_INVOKABLE void centerMount();

    // Get list of scopes
    QJsonArray getScopes() const;

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

    bool setScopeConfig(int index);

    void toggleMountToolBox();

private slots:

    void startParkTimer();
    void stopParkTimer();
    void startAutoPark();

  signals:
    void newLog();
    void newCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt);
    void newTarget(const QString &name);    
    void newStatus(ISD::Telescope::TelescopeStatus status);
    void slewRateChanged(int index);

  private:
    void syncGPS();

    ISD::Telescope *currentTelescope = nullptr;
    ISD::GDInterface *currentGPS = nullptr;
    QStringList logText;
    SkyPoint telescopeCoord;
    QString lastNotificationMessage;
    QTimer updateTimer;
    QTimer autoParkTimer;
    double lastAlt;
    int abortDispatch;
    bool altLimitEnabled;
    bool GPSInitialized = {false};
    ISD::Telescope::TelescopeStatus lastStatus = ISD::Telescope::MOUNT_IDLE;

    QQuickView *m_BaseView = nullptr;
    QQuickItem *m_BaseObj  = nullptr;
    QQmlContext *m_Ctxt    = nullptr;

    QQuickItem *m_SpeedSlider = nullptr, *m_SpeedLabel = nullptr, *m_raValue = nullptr, *m_deValue = nullptr,
               *m_azValue = nullptr, *m_altValue = nullptr, *m_haValue = nullptr, *m_zaValue = nullptr,
               *m_targetText = nullptr, *m_targetRAText = nullptr, *m_targetDEText = nullptr, *m_Park = nullptr,
               *m_Unpark = nullptr, *m_statusText = nullptr, *m_J2000Check = nullptr, *m_JNowCheck=nullptr;
};
}

#endif // Mount
