/*  INDI Focuser
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef INDIFOCUSER_H
#define INDIFOCUSER_H

#include "indistd.h"

namespace ISD
{
/**
 * @class Focuser
 * Focuser class handles control of INDI focuser devices. Both relative and absolute focusers can be controlled.
 *
 * @author Jasem Mutlaq
 */

class Focuser : public DeviceDecorator
{
    Q_OBJECT

  public:
    enum FocusDirection
    {
        FOCUS_INWARD,
        FOCUS_OUTWARD
    };

    Focuser(GDInterface *iPtr) : DeviceDecorator(iPtr) { dType = KSTARS_FOCUSER; }

    void processSwitch(ISwitchVectorProperty *svp);
    void processText(ITextVectorProperty *tvp);
    void processNumber(INumberVectorProperty *nvp);
    void processLight(ILightVectorProperty *lvp);

    DeviceFamily getType() { return dType; }

    bool focusIn();
    bool focusOut();
    bool moveByTimer(int msecs);
    bool moveAbs(int steps);
    bool moveRel(int steps);

    bool canAbsMove();
    bool canRelMove();
    bool canTimerMove();

    bool getFocusDirection(FocusDirection *dir);
};
}

#endif // INDIFOCUSER_H
