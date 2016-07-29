/*  Ekos Mount Module
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef MOUNT_H
#define MOUNT_H

#include <QtDBus/QtDBus>
#include "ui_mount.h"

#include "indi/indistd.h"
#include "indi/indifocuser.h"
#include "indi/inditelescope.h"

namespace Ekos
{

/**
 *@class Mount
 *@short Supports controlling INDI telescope devices including setting/retrieving mount properties, slewing, motion and speed controls, in addition to enforcing altitude limits and parking/unparking.
 *@author Jasem Mutlaq
 *@version 1.1
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
    Q_SCRIPTABLE bool slew(double RA, double DEC);

    /** DBUS interface function.
     * Get equatorial coords (JNow). An array of doubles is returned. First element is RA in hours. Second elements is DEC in degrees.
     */
    Q_SCRIPTABLE QList<double> getEquatorialCoords();

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
    Q_SCRIPTABLE bool abort();

    /** DBUS interface function.
     * Get the mount slew status ("Idle","Complete", "Busy", "Error")
     */
    Q_SCRIPTABLE IPState getSlewStatus();

    /** DBUS interface function.
     * Get telescope and guide scope info. An array of doubles is returned in order.
     * Primary Telescope Focal Length (mm), Primary Telescope Aperture (mm), Guide Telescope Focal Length (mm), Guide Telescope Aperture (mm)
     */
    Q_SCRIPTABLE QList<double> getTelescopeInfo();

    /** DBUS interface function.
     * Set telescope and guide scope info. All measurements is in millimeters.
     * @param primaryFocalLength Primary Telescope Focal Length
     * @param primaryAperture Primary Telescope Aperture
     * @param guideFocalLength Guide Telescope Focal Length
     * @param guideAperture Guide Telescope Aperture
     */
    Q_SCRIPTABLE Q_NOREPLY void setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture);

    /** DBUS interface function.
     * Can mount park?
     */
    Q_SCRIPTABLE bool canPark();

    /** DBUS interface function.
     * Park mount
     */
    Q_SCRIPTABLE bool park();

    /** DBUS interface function.
     * Unpark mount
     */
    Q_SCRIPTABLE bool unpark();

    /** DBUS interface function.
     * Return parking status of the mount.
     */
    Q_SCRIPTABLE ParkingStatus getParkingStatus();

    /** @}*/

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
     * @brief move Issues command to the mount to move in a particular direction based on the pressed direcitonal buttons in the GUI
     */
    void move();

    /**
     * @brief stop Aborts telescope motion
     */
    void stop();

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

signals:
    void newLog();
    void newCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt);
    void newStatus(ISD::Telescope::TelescopeStatus status);

private:

    ISD::Telescope *currentTelescope;
    QStringList logText;
    SkyPoint telescopeCoord;
    QString lastNotificationMessage;
    double lastAlt;
    int abortDispatch;
    bool altLimitEnabled;

};

}

#endif  // Mount
