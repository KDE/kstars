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


class Telescope : public DeviceDecorator
{
    Q_OBJECT

public:
    Telescope(GDInterface *iPtr);
    ~Telescope();

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty* tvp);
    void processNumber(INumberVectorProperty *nvp);

    DeviceFamily getType() { return dType;}

    // Common Commands
    bool Slew(SkyPoint *ScopeTarget);
    bool Sync(SkyPoint *ScopeTarget);
    bool Abort();
    bool Park();
    bool canGuide();
    bool doPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool doPulse(GuideDirection dir, int msecs );
    bool getEqCoords(double *ra, double *dec);

protected:
    bool sendCoords(SkyPoint *ScopeTarget);


public slots:
    virtual bool runCommand(int command, void *ptr=NULL);

private:

    SkyPoint currentCoord;

};

}

#endif // INDITELESCOPE_H
