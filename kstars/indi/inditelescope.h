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
    typedef enum { PARK_UNKNOWN, PARK_PARKED, PARK_PARKING, PARK_UNPARKING, PARK_UNPARKED } ParkStatus;

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);

    DeviceFamily getType() { return dType;}

    // Coordinates
    bool getEqCoords(double *ra, double *dec);

    // Slew
    bool Slew(SkyPoint *ScopeTarget);
    bool Slew(double ra, double dec);

    // Sync
    bool Sync(SkyPoint *ScopeTarget);
    bool Sync(double ra, double dec);
    bool canSync();

    // Motion
    bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd);
    bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd);
    bool isSlewing();
    bool isInMotion();

    // Guiding
    bool canGuide();
    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool doPulse(GuideDirection dir, int msecs );

    // Parking
    bool canPark();
    bool isParked() { return parkStatus == PARK_PARKED; }

    // Status
    ParkStatus getParkStatus() { return parkStatus; }
    TelescopeStatus getStatus();
    static const QString getStatusString(TelescopeStatus status);

    // Altitude Limits
    void setAltLimits(double minAltitude, double maxAltitude);

    // Alignment Model
    bool setAlignmentModelEnabled(bool enable);
    bool clearAlignmentModel();

protected:
    bool sendCoords(SkyPoint *ScopeTarget);

public slots:
    virtual bool runCommand(int command, void *ptr=NULL);
    bool Abort();
    bool Park();
    bool UnPark();
    bool setSlewRate(int index);

signals:
    void newTarget(const QString &);

private:
    SkyPoint currentCoord;
    double minAlt,maxAlt;
    ParkStatus parkStatus = PARK_UNKNOWN;
    IPState EqCoordPreviousState;

};

}

#endif // INDITELESCOPE_H
