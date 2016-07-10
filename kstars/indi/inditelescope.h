/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDITELESCOPE_H
#define INDITELESCOPE_H

#include "indistd.h"

namespace ISD
{

/**
 * @class Telescope
 * device handle controlling telescope. It can slew and sync to a specific sky point and supports all standard propreties with INDI
 * telescope device.
 *
 * @author Jasem Mutlaq
 */
class Telescope : public DeviceDecorator
{
    Q_OBJECT

public:
    Telescope(GDInterface *iPtr);
    ~Telescope();

    typedef enum { MOTION_NORTH, MOTION_SOUTH } TelescopeMotionNS;
    typedef enum { MOTION_WEST, MOTION_EAST } TelescopeMotionWE;
    typedef enum { MOTION_START, MOTION_STOP } TelescopeMotionCommand;
    typedef enum { MOUNT_IDLE, MOUNT_SLEWING, MOUNT_TRACKING, MOUNT_PARKING, MOUNT_PARKED, MOUNT_ERROR } TelescopeStatus;

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);

    DeviceFamily getType() { return dType;}

    // Common Commands
    bool Slew(SkyPoint *ScopeTarget);
    bool Slew(double ra, double dec);
    bool Sync(SkyPoint *ScopeTarget);
    bool Sync(double ra, double dec);    
    bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd);
    bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd);
    bool canGuide();
    bool canSync();
    bool canPark();
    bool isSlewing();
    bool isParked();
    bool isInMotion();
    TelescopeStatus getStatus();
    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool doPulse(GuideDirection dir, int msecs );
    bool getEqCoords(double *ra, double *dec);
    void setAltLimits(double minAltitude, double maxAltitude);

    static const QString getStatusString(TelescopeStatus status);

protected:
    bool sendCoords(SkyPoint *ScopeTarget);

public slots:
    virtual bool runCommand(int command, void *ptr=NULL);
    bool Abort();
    bool Park();
    bool UnPark();
    bool setSlewRate(int index);

private:
    SkyPoint currentCoord;
    double minAlt,maxAlt;
    bool IsParked;

};

}

#endif // INDITELESCOPE_H
