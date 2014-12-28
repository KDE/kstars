/*  Ekos Focus tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef FOCUS_H
#define FOCUS_H

#include <QtDBus/QtDBus>

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
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Focus")

public:
    Focus();
    ~Focus();

    typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusDirection;
    typedef enum { FOCUS_MANUAL, FOCUS_AUTO, FOCUS_LOOP } FocusType;

    Q_SCRIPTABLE bool setCCD(QString device);
    Q_SCRIPTABLE bool setFocuser(QString device);
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);
    Q_SCRIPTABLE bool isAutoFocusComplete() { return (inAutoFocus == false);}
    Q_SCRIPTABLE bool isAutoFocusSuccessful() { return m_autoFocusSuccesful;}
    Q_SCRIPTABLE double getHFR() { return currentHFR; }
    Q_SCRIPTABLE Q_NOREPLY void setFocusMode(int mode);
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
    Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);
    Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString & value);
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusOptions(bool selectAutoStar, bool useSubFrame);
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance);

    Q_SCRIPTABLE Q_NOREPLY void resetFrame();

    void addFocuser(ISD::GDInterface *newFocuser);
    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD);
    void addFilter(ISD::GDInterface *newFilter);
    void focuserDisconnected();  

    void appendLogText(const QString &);
    void clearLog();
    QString getLogText() { return logText.join("\n"); }

public slots:

    /* Focus */
    Q_SCRIPTABLE Q_NOREPLY void startFocus();
    Q_SCRIPTABLE Q_NOREPLY void stopFocus();
    Q_SCRIPTABLE Q_NOREPLY void capture();
    void startLooping();
    void checkStopFocus();
    void checkCCD(int CCDNum=-1);
    void checkFocuser(int FocuserNum=-1);
    void checkFilter(int filterNum=-1);

    void filterLockToggled(bool);
    void updateFilterPos(int index);
    void clearDataPoints();

    Q_SCRIPTABLE Q_NOREPLY void FocusIn(int ms=-1);
    Q_SCRIPTABLE Q_NOREPLY void FocusOut(int ms=-1);

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
    bool frameModified;
    int absIterations;
    int lastLockFilterPos;

    bool inAutoFocus, inFocusLoop, inSequenceFocus, m_autoFocusSuccesful;

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
    int orig_x, orig_y, orig_w, orig_h;
    int noStarCount;

    QStringList logText;
    QList<HFRPoint *> HFRAbsolutePoints;
    QList<HFRPoint *> HFRIterativePoints;

    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;
};

}

#endif  // Focus_H
