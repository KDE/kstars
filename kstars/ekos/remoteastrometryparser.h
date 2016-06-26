/*  Astrometry.net Parser - Remote
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef REMOTEASTROMETRYPARSER_H
#define REMOTEASTROMETRYPARSER_H

#include "indi/indiccd.h"
#include "astrometryparser.h"

namespace Ekos
{

class Align;

/**
 * @class  RemoteAstrometryParser
 * RemoteAstrometryParser invokes the remote astrometry.net solver in the remote CCD driver to solve the captured image.
 * The offline astrometry.net plus index files must be installed and configured on the remote host running the INDI CCD driver.
 *
 * @authro Jasem Mutlaq
 */

class RemoteAstrometryParser: public AstrometryParser
{
        Q_OBJECT

public:
    RemoteAstrometryParser();
    virtual ~RemoteAstrometryParser();

    virtual void setAlign(Align *_align) { align = _align; }
    virtual bool init();
    virtual void verifyIndexFiles(double fov_x, double fov_y);
    virtual bool startSovler(const QString &filename, const QStringList &args, bool generated=true);
    virtual bool stopSolver();

    void setCCD(ISD::CCD *ccd);

public slots:
    void checkCCDStatus(ISwitchVectorProperty * svp);
    void checkCCDResults(INumberVectorProperty * nvp);

private:
    ISD::CCD *currentCCD;
    bool solverRunning;
    bool captureRunning;
    Align *align;
    QTime solverTimer;

};

}

#endif // REMOTEASTROMETRYPARSER_H
