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


    void setTelescope(ISD::GDInterface *newTelescope);

    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

public slots:

    /* Telescope Info */
    void syncTelescopeInfo();
    void updateNumber(INumberVectorProperty *nvp);
    void updateSwitch(ISwitchVectorProperty *svp);
    void updateTelescopeCoords();
    void move();
    void stop();
    void save();


signals:
    void newLog();


private:

    ISD::Telescope *currentTelescope;
    QStringList logText;
    SkyPoint telescopeCoord;
    QString lastNotificationMessage;

};

}

#endif  // Mount
