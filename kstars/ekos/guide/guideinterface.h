/*  Ekos guide class interface
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef GUIDEINTERFACE_H
#define GUIDEINTERFACE_H

#include <QObject>
#include "ekos/ekos.h"

namespace Ekos
{

/**
 *@class GuideInterface
 *@short Interface skeleton for implementation of different guiding applications and/or routines
 *@author Jasem Mutlaq
 *@version 1.0
 */
class GuideInterface : public QObject
{

    Q_OBJECT

public:
    GuideInterface();
    virtual ~GuideInterface() {}

    virtual bool Connect() = 0;
    virtual bool Disconnect() = 0;

    virtual bool calibrate() = 0;
    virtual bool guide() = 0;
    virtual bool suspend() = 0;
    virtual bool resume() = 0;
    virtual bool stop() = 0;
    virtual bool dither(double pixels) = 0;

    virtual void setGuiderParams(double ccdpixel_x, double ccdpixel_y, double aperture, double focal);

signals:
    void newLog(const QString &);
    void newStatus(Ekos::GuideState);
    void newAxisDelta(double delta_ra, double delta_dec);
    void newStarPosition(QVector3D, bool);

private:
    Ekos::GuideState state;

};

}

#endif  // GUIDEINTERFACE_H
