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
 *@short Supports Control INDI telescopes and displays information about them.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class Mount : public QWidget, public Ui::Mount
{

    Q_OBJECT
    //TODO
    //Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Focus")

public:
    Mount();
    ~Mount();

    /**
     * @brief setTelescope Sets the mount module telescope interface
     * @param newTelescope pointer to telescope interface object
     */
    void setTelescope(ISD::GDInterface *newTelescope);

    // Log functions
    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

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
