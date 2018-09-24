/*  Ekos guide class interface
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ekos/ekos.h"

#include <QObject>
#include <QVector3D>

#include <cstdint>

class QString;

namespace Ekos
{
/**
 * @class GuideInterface
 * @short Interface skeleton for implementation of different guiding applications and/or routines
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
class GuideInterface : public QObject
{
    Q_OBJECT

  public:
    GuideInterface() = default;
    virtual ~GuideInterface() override = default;

    virtual bool Connect()     = 0;
    virtual bool Disconnect()  = 0;
    virtual bool isConnected() = 0;

    virtual bool calibrate()           = 0;
    virtual bool guide()               = 0;
    virtual bool suspend()             = 0;
    virtual bool resume()              = 0;
    virtual bool abort()               = 0;
    virtual bool dither(double pixels) = 0;
    virtual bool clearCalibration()    = 0;
    virtual bool reacquire() { return false; }

    virtual bool setGuiderParams(double ccdPixelSizeX, double ccdPixelSizeY, double mountAperture,
                                 double mountFocalLength);
    virtual bool getGuiderParams(double *ccdPixelSizeX, double *ccdPixelSizeY, double *mountAperture,
                                 double *mountFocalLength);

    virtual bool setFrameParams(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t binX, uint16_t binY);
    virtual bool getFrameParams(uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h, uint16_t *binX, uint16_t *binY);

    virtual void setStarPosition(QVector3D& starCenter);

  signals:
    void newLog(const QString &);
    void newStatus(Ekos::GuideState);
    void newAxisDelta(double delta_ra, double delta_dec);
    void newAxisSigma(double sigma_ra, double sigma_dec);
    void newAxisPulse(double pulse_ra, double pulse_dec);
    void newStarPosition(const QVector3D &newCenter, bool updateNow);
    void newStarPixmap(QPixmap &);

    void frameCaptureRequested();

  protected:
    Ekos::GuideState state { GUIDE_IDLE };
    double ccdPixelSizeX { 0 };
    double ccdPixelSizeY { 0 };
    double mountAperture { 0 };
    double mountFocalLength { 0 };
    uint16_t subX { 0 };
    uint16_t subY { 0 };
    uint16_t subW { 0 };
    uint16_t subH { 0 };
    uint16_t subBinX { 1 };
    uint16_t subBinY { 1 };
};
}
