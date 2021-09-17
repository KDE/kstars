/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "basedevice.h"
#include "skypoint.h"
#include "kstarslite/skypointlite.h"
#include "kstarslite/skyobjectlite.h"

#include <QObject>

class ClientManagerLite;

/**
 * @class TelescopeLite
 *
 * device handle controlling telescope. It can slew and sync to a specific sky point and supports all standard properties with INDI
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
    explicit TelescopeLite(INDI::BaseDevice *device);
    TelescopeLite() { }
    ~TelescopeLite();

    enum TelescopeMotionNS
    {
        MOTION_NORTH,
        MOTION_SOUTH
    };
    enum TelescopeMotionWE
    {
        MOTION_WEST,
        MOTION_EAST
    };
    enum TelescopeMotionCommand
    {
        MOTION_START,
        MOTION_STOP
    };

    Q_ENUMS(TelescopeMotionNS)
    Q_ENUMS(TelescopeMotionWE)
    Q_ENUMS(TelescopeMotionCommand)

    void registerProperty(INDI::Property prop);
    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);

    INDI::BaseDevice *getDevice() { return baseDevice; }
    //deviceName
    QString getDeviceName() { return m_deviceName; }
    void setDeviceName(const QString &deviceName);

    bool isSlewDecreasable() { return m_slewDecreasable; }
    bool isSlewIncreasable() { return m_slewIncreasable; }

    QString getSlewRateLabel() { return m_slewRateLabel; }

    void setSlewDecreasable(bool slewDecreasable);
    void setSlewIncreasable(bool slewIncreasable);

    void setSlewRateLabel(const QString &slewRateLabel);

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
    Q_INVOKABLE QStringList getSlewRateLabels() { return m_slewRateLabels; };

  protected:
    bool sendCoords(SkyPoint *ScopeTarget);

  public slots:
    Q_INVOKABLE bool abort();
    bool park();
    bool unPark();
    bool setSlewRate(int index);
    void updateSlewRate(const QString &deviceName, const QString &propName);

  signals:
    void slewDecreasableChanged(bool);
    void slewIncreasableChanged(bool);
    void slewRateLabelChanged(QString);
    void deviceNameChanged(QString);
    void newTelescopeLiteCreated(TelescopeLite *);
    void slewRateUpdate(int index, int count);

  private:
    SkyPoint currentCoord;
    double minAlt { 0 };
    double maxAlt { 0 };
    bool IsParked { false };
    ClientManagerLite *clientManager { nullptr };
    INDI::BaseDevice *baseDevice { nullptr };
    int slewRateIndex { 0 };
    QString m_deviceName;
    bool m_slewDecreasable { false };
    bool m_slewIncreasable { false };
    QString m_slewRateLabel;
    QStringList m_slewRateLabels;
};
