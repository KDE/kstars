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

struct HFRPoint
{
    int pos;
    double HFR;
};

class Focus : public QWidget, public Ui::Focus
{

    Q_OBJECT

public:
    Focus();
    ~Focus();

    void addFocuser(ISD::GDInterface *newFocuser);
    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD);
    void addFilter(ISD::GDInterface *newFilter);
    void focuserDisconnected();

    typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusDirection;
    typedef enum { FOCUS_MANUAL, FOCUS_AUTO, FOCUS_LOOP } FocusType;

    void resetFrame();
    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

public slots:

    /* Focus */
    void startFocus();
    void checkStopFocus();
    void stopFocus();
    void capture();
    void startLooping();

    void checkCCD(int CCDNum=-1);
    void checkFocuser(int FocuserNum=-1);
    void checkFilter(int filterNum=-1);

    void filterLockToggled(bool);
    void updateFilterPos(int index);
    void clearDataPoints();

    void FocusIn(int ms=-1);
    void FocusOut(int ms=-1);

    void focusStarSelected(int x, int y);
    void toggleAutofocus(bool enable);

    void newFITS(IBLOB *bp);
    void processFocusProperties(INumberVectorProperty *nvp);
    void subframeUpdated(bool enable);
    void checkFocus(double delta);
    void setInSequenceFocus(bool);

    void resetFocusFrame();
    void filterChangeWarning(int index);

signals:
        void newLog();
        void autoFocusFinished(bool);
        void suspendGuiding(bool);

private:

    void drawHFRPlot();
    void getAbsFocusPosition();
    void autoFocusAbs();
    void autoFocusRel();
    void resetButtons();


    /* Focus */
    ISD::Focuser *currentFocuser;
    ISD::CCD *currentCCD;
    ISD::GDInterface *currentFilter;

    // They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;

    QList<ISD::CCD *> CCDs;
    QList<ISD::Focuser*> Focusers;

    Ekos::Capture *captureP;

    FocusDirection lastFocusDirection;
    FocusType focusType;

    double HFR;
    int pulseDuration;
    bool canAbsMove;
    bool captureInProgress;
    int absIterations;
    int lastLockFilterPos;

    bool inAutoFocus, inFocusLoop, inSequenceFocus;

    double absCurrentPos;
    double pulseStep;
    double absMotionMax, absMotionMin;
    double deltaHFR;
    double currentHFR;
    double maxHFR;
    int HFRInc;
    int HFRDec;
    int minPos, maxPos;
    bool reverseDir;
    bool starSelected;
    int fx,fy,fw,fh;
    int noStarCount;

    QStringList logText;
    QList<HFRPoint *> HFRAbsolutePoints;
    QList<HFRPoint *> HFRIterativePoints;

    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;
};

}

#endif  // Focus_H
