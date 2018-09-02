/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "indistd.h"
#include "skypoint.h"

class SkyObject;

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
    explicit Telescope(GDInterface *iPtr);
    virtual ~Telescope() override = default;

    typedef enum { MOTION_NORTH, MOTION_SOUTH } TelescopeMotionNS;
    typedef enum { MOTION_WEST, MOTION_EAST } TelescopeMotionWE;
    typedef enum { MOTION_START, MOTION_STOP } TelescopeMotionCommand;
    typedef enum {
        MOUNT_IDLE,
        MOUNT_MOVING,
        MOUNT_SLEWING,
        MOUNT_TRACKING,
        MOUNT_PARKING,
        MOUNT_PARKED,
        MOUNT_ERROR
    } TelescopeStatus;
    typedef enum { PARK_UNKNOWN, PARK_PARKED, PARK_PARKING, PARK_UNPARKING, PARK_UNPARKED } ParkStatus;
    typedef enum { PARK_OPTION_CURRENT, PARK_OPTION_DEFAULT, PARK_OPTION_WRITE_DATA } ParkOptionCommand;

    void registerProperty(INDI::Property *prop) override;
    void processSwitch(ISwitchVectorProperty *svp) override;
    void processText(ITextVectorProperty *tvp) override;
    void processNumber(INumberVectorProperty *nvp) override;

    DeviceFamily getType() override { return dType; }

    // Coordinates
    bool getEqCoords(double *ra, double *dec);
    bool isJ2000() { return m_isJ2000; }

    // Slew
    bool Slew(SkyPoint *ScopeTarget);
    bool Slew(double ra, double dec);

    // Sync
    bool Sync(SkyPoint *ScopeTarget);
    bool Sync(double ra, double dec);
    bool canSync();

    // Tracking
    bool canControlTrack() const { return m_canControlTrack; }
    bool isTracking();

    // Track Mode
    bool hasTrackModes() const { return m_hasTrackModes; }    
    bool getTrackMode(uint8_t &index);

    // Custom Track Rate
    bool hasCustomTrackRate() const { return m_hasTrackModes; }    
    bool getCustomTrackRate(double &raRate, double &deRate);

    // Motion
    bool MoveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd);
    bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd);
    bool isSlewing();
    bool isInMotion();
    QString getManualMotionString() const;

    // Guiding
    bool canGuide();
    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
    bool doPulse(GuideDirection dir, int msecs);

    // Parking
    bool canPark();
    bool isParked() { return parkStatus == PARK_PARKED; }
    bool canCustomPark() { return m_hasCustomParking; }
    bool sendParkingOptionCommand(ParkOptionCommand command);

    // Status
    ParkStatus getParkStatus() { return parkStatus; }
    TelescopeStatus getStatus();
    const QString getStatusString(TelescopeStatus status);

    // Altitude Limits
    void setAltLimits(double minAltitude, double maxAltitude);

    // Alignment Model
    bool setAlignmentModelEnabled(bool enable);
    bool clearAlignmentModel();
    bool hasAlignmentModel() { return m_hasAlignmentModel; }

    // Slew Rates
    bool hasSlewRates() { return m_hasSlewRates; }
    QStringList slewRates() { return m_slewRates; }
    int getSlewRate() const;

  protected:
    bool sendCoords(SkyPoint *ScopeTarget);

  public slots:
    virtual bool runCommand(int command, void *ptr = nullptr) override;
    bool Abort();
    bool Park();
    bool UnPark();
    bool setSlewRate(int index);
    bool setTrackEnabled(bool enable);
    bool setCustomTrackRate(double raRate, double deRate);
    bool setTrackMode(uint8_t index);

  signals:
    void newTarget(const QString &);
    void slewRateChanged(int rate);

  private:
    SkyPoint currentCoord;
    double minAlt, maxAlt;
    ParkStatus parkStatus = PARK_UNKNOWN;
    IPState EqCoordPreviousState;
    QTimer *centerLockTimer  = nullptr;
    SkyObject *currentObject = nullptr;
    bool inManualMotion      = false;
    bool inCustomParking     = false;
    IPState NSPreviousState  = IPS_IDLE;
    IPState WEPreviousState  = IPS_IDLE;

    bool m_hasAlignmentModel = { false };
    bool m_canControlTrack = { false };
    bool m_hasTrackModes { false};
    bool m_hasCustomTrackRate { false};
    bool m_hasCustomParking { false };
    bool m_hasSlewRates { false };
    bool m_isJ2000 { false };
    QStringList m_slewRates;
};
}
