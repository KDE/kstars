/*  INDI CCD
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDITELESCOPE_H
#define INDITELESCOPE_H

#include "skypoint.h"
#include <QObject>
#include "basedevice.h"
#include "kstarslite/skypointlite.h"
#include "kstarslite/skyobjectlite.h"

class ClientManagerLite;

/**
 * @class Telescope
 * device handle controlling telescope. It can slew and sync to a specific sky point and supports all standard propreties with INDI
 * telescope device.
 *
 * @author Jasem Mutlaq
 */
class TelescopeLite : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool slewDecreasable READ isSlewDecreasable WRITE setSlewDecreasable NOTIFY slewDecreasableChanged)
    Q_PROPERTY(bool slewIncreasable READ isSlewIncreasable WRITE setSlewIncreasable NOTIFY slewIncreasableChanged)
    Q_PROPERTY(QString slewRateLabel READ getSlewRateLabel WRITE setSlewRateLabel NOTIFY slewRateLabelChanged)
    Q_PROPERTY(QString deviceName READ getDeviceName WRITE setDeviceName NOTIFY deviceNameChanged)
public:
    TelescopeLite(INDI::BaseDevice *device);
    TelescopeLite() { }
    ~TelescopeLite();

    enum TelescopeMotionNS { MOTION_NORTH, MOTION_SOUTH };
    enum TelescopeMotionWE { MOTION_WEST, MOTION_EAST };
    enum TelescopeMotionCommand { MOTION_START, MOTION_STOP };

    Q_ENUMS(TelescopeMotionNS)
    Q_ENUMS(TelescopeMotionWE)
    Q_ENUMS(TelescopeMotionCommand)

    void registerProperty(INDI::Property *prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);

    INDI::BaseDevice *getDevice() { return baseDevice; }
    //deviceName
    QString getDeviceName() { return m_deviceName; }
    void setDeviceName(QString deviceName);

    bool isSlewDecreasable() { return m_slewDecreasable; }
    bool isSlewIncreasable() { return m_slewIncreasable; }

    QString getSlewRateLabel() { return m_slewRateLabel; }

    void setSlewDecreasable(bool slewDecreasable);
    void setSlewIncreasable(bool slewIncreasable);

    void setSlewRateLabel(QString slewRateLabel);

    Q_INVOKABLE bool decreaseSlewRate();
    Q_INVOKABLE bool increaseSlewRate();

    // Common Commands
    Q_INVOKABLE bool slew(SkyPointLite *ScopeTarget) { return slew(ScopeTarget->getPoint()); }
    Q_INVOKABLE bool slew(SkyPoint *ScopeTarget);
    bool slew(double ra, double dec);
    Q_INVOKABLE bool sync(SkyPointLite *ScopeTarget) { return sync(ScopeTarget->getPoint()); }
    Q_INVOKABLE bool sync(SkyPoint *ScopeTarget);
    bool sync(double ra, double dec);
    Q_INVOKABLE bool moveNS(TelescopeMotionNS dir, TelescopeMotionCommand cmd);
    Q_INVOKABLE bool moveWE(TelescopeMotionWE dir, TelescopeMotionCommand cmd);
    bool canGuide();
    bool canSync();
    bool canPark();
    bool isSlewing();
    bool isParked();
    bool isInMotion();
    /*bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool doPulse(GuideDirection dir, int msecs );*/
    bool getEqCoords(double *ra, double *dec);
    void setAltLimits(double minAltitude, double maxAltitude);
    Q_INVOKABLE bool isConnected() { return baseDevice->isConnected(); }

protected:
    bool sendCoords(SkyPoint *ScopeTarget);

public slots:
    Q_INVOKABLE bool abort();
    bool park();
    bool unPark();
    bool setSlewRate(int index);
    void updateSlewRate(QString deviceName, QString propName);

signals:
    void slewDecreasableChanged(bool);
    void slewIncreasableChanged(bool);
    void slewRateLabelChanged(QString);
    void deviceNameChanged(QString);
    void newTelescopeLiteCreated(TelescopeLite *);

private:
    SkyPoint currentCoord;
    double minAlt,maxAlt;
    bool IsParked;
    ClientManagerLite *clientManager;
    INDI::BaseDevice *baseDevice;
    int slewRateIndex;

    QString m_deviceName;

    bool m_slewDecreasable;
    bool m_slewIncreasable;
    QString m_slewRateLabel;
};

#endif // INDITELESCOPE_H
