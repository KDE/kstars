/*  Ekos Focus tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef FOCUS_H
#define FOCUS_H

#include "focus.h"
#include "capture.h"

#include "ui_focus.h"


#include "indi/indistd.h"
#include "indi/indifocuser.h"

namespace Ekos
{


class Focus : public QWidget, public Ui::Focus
{

    Q_OBJECT

public:
    Focus();


    void setFocuser(ISD::GDInterface *newFocuser);
    void setCCD(ISD::GDInterface *newCCD);

    typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusType;

public slots:

    /* Focus */
    void startFocus();
    void stopFocus();
    void capture();

    void FocusIn(int ms=-1);
    void FocusOut(int ms=-1);

    void toggleAutofocus(bool enable);

    void updateImageFilter(int index);

    void newFITS(IBLOB *bp);
    void processFocusProperties(INumberVectorProperty *nvp);
private:

    void getAbsFocusPosition();

    /* Focus */
    ISD::Focuser *currentFocuser;
    ISD::CCD *currentCCD;

    Ekos::Capture *captureP;

    FocusType lastFocusDirection;

    double HFR;
    int pulseDuration;
    bool canAbsMove;
    int absIterations;

    double absCurrentPos;
    double pulseStep;
    double absMotionMax, absMotionMin;

    FITSScale filterType;

};

}

#endif  // Focus_H
