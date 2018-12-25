/*  INDI Telescope
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QDBusArgument>
#include <QTimer>

#include "indistd.h"
#include "skypoint.h"

class SkyObject;

namespace ISD
{
/**
 * @class Telescope
 * device handle controlling telescope. It can slew and sync to a specific sky point and supports all standard properties with INDI
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
    typedef enum { PIER_UNKNOWN=-1, PIER_WEST = 0 , PIER_EAST = 0 } PierSide;
    typedef enum {
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
    bool StopNS();
    bool MoveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd);
    bool StopWE();
    bool isSlewing();
    bool isInMotion();
    QString getManualMotionString() const;

    // Guiding
    bool canGuide();
    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
    bool doPulse(GuideDirection dir, int msecs);

    // Parking
    bool canPark();
    bool isParked() { return m_ParkStatus == PARK_PARKED; }
    bool canCustomPark() { return m_hasCustomParking; }
    bool sendParkingOptionCommand(ParkOptionCommand command);

    // Status
    ParkStatus parkStatus() { return m_ParkStatus; }
    Status status();
    const QString getStatusString(Status status);

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

    // Pier side
    PierSide pierSide() const { return m_PierSide; }

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
    void newParkStatus(ISD::ParkStatus status);
    void slewRateChanged(int rate);
    void pierSideChanged(PierSide side);
    void ready();

  private:
    SkyPoint currentCoord;
    double minAlt=0, maxAlt=90;
    ParkStatus m_ParkStatus = PARK_UNKNOWN;
    IPState EqCoordPreviousState;
    QTimer centerLockTimer;
    QTimer readyTimer;
    SkyObject *currentObject = nullptr;
    bool inManualMotion      = false;
    bool inCustomParking     = false;
    IPState NSPreviousState  = IPS_IDLE;
    IPState WEPreviousState  = IPS_IDLE;
    PierSide m_PierSide = PIER_UNKNOWN;

    QMap<TrackModes, uint8_t> TrackMap;
    TrackModes currentTrackMode { TRACK_SIDEREAL };

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

Q_DECLARE_METATYPE(ISD::Telescope::Status)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Telescope::Status& source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Telescope::Status &dest);

Q_DECLARE_METATYPE(ISD::Telescope::PierSide)
QDBusArgument &operator<<(QDBusArgument &argument, const ISD::Telescope::PierSide& source);
const QDBusArgument &operator>>(const QDBusArgument &argument, ISD::Telescope::PierSide &dest);
