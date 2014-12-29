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

    /**DBUS interface function.
     * select the CCD device from the available CCD drivers.
     * @param device The CCD device name
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /**DBUS interface function.
     * select the focuser device from the available focuser drivers. The focuser device can be the same as the CCD driver if the focuser functionality was embedded within the driver.
     * @param device The focuser device name
     */
    Q_SCRIPTABLE bool setFocuser(QString device);

    /**DBUS interface function.
     * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
     * @param device The filter device name
     */
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);

    /**DBUS interface function.
     * @return True if autofocus operation is complete. False otherwise.
     */
    Q_SCRIPTABLE bool isAutoFocusComplete() { return (inAutoFocus == false);}

    /**DBUS interface function.
     * @return True if autofocus operation is successful. False otherwise.
     */
    Q_SCRIPTABLE bool isAutoFocusSuccessful() { return m_autoFocusSuccesful;}

    /**DBUS interface function.
     * @return Half-Flux-Radius in pixels.
     */
    Q_SCRIPTABLE double getHFR() { return currentHFR; }

    /**DBUS interface function.
     * Set Focus mode (Manual or Auto)
     * @param mode 0 for manual, any other value for auto.
     */
    Q_SCRIPTABLE Q_NOREPLY void setFocusMode(int mode);

    /**DBUS interface function.
     * Set CCD exposure value
     * @param value exposure value in seconds.
     */
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);

    /**DBUS interface function.
     * Set CCD binning
     * @param binX horizontal binning
     * @param binY vertical binning
     */
    Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);

    /**DBUS interface function.
     * Set image filter to apply to the image after capture.
     * @param value Image filter (Auto Stretch, High Contrast, Equalize, High Pass)
     */
    Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString & value);

    /**DBUS interface function.
     * Set Auto Focus options. The options must be set before starting the autofocus operation. If no options are set, the options loaded from the user configuration are used.
     * @param selectAutoStar If true, Ekos will attempt to automatically select the best focus star in the frame. If it fails to select a star, the user will be asked to select a star manually.
     * @param useSubFrame if true, Ekos will capture a subframe around the selected focus star. The subframe size is determined by the boxSize parameter.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusOptions(bool selectAutoStar, bool useSubFrame);

    /**DBUS interface function.
     * Set Autofocus parameters
     * @param boxSize the box size around the focus star in pixels. The boxsize is used to subframe around the focus star.
     * @param stepSize the initial step size to be commanded to the focuser. If the focuser is absolute, the step size is in ticks. For relative focusers, the focuser will be commanded to focus inward for stepSize milliseconds initially.
     * @param maxTravel the maximum steps permitted before the autofocus operation aborts.
     * @param tolerance Measure of how accurate the autofocus algorithm is. If the difference between the current HFR and minimum measured HFR is less than %tolerance after the focuser traversed both ends of the V-curve, then the focusing operation
     * is deemed successful. Otherwise, the focusing operation will continue.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance);

    /**DBUS interface function.
     * Resets the CCD frame to its full resolution.
     */
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
    /**DBUS interface function.
     * Start the autofocus operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void startFocus();

    /**DBUS interface function.
     * Abort the autofocus operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void stopFocus();

    /**DBUS interface function.
     * Capture a focus frame.
     */
    Q_SCRIPTABLE Q_NOREPLY void capture();

    void startLooping();
    void checkStopFocus();
    void checkCCD(int CCDNum=-1);
    void checkFocuser(int FocuserNum=-1);
    void checkFilter(int filterNum=-1);

    void filterLockToggled(bool);
    void updateFilterPos(int index);
    void clearDataPoints();

    /**DBUS interface function.
     * Focus inward
     * @param ms If set, focus inward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
     */
    Q_SCRIPTABLE Q_NOREPLY void FocusIn(int ms=-1);

    /**DBUS interface function.
     * Focus outward
     * @param ms If set, focus outward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
     */
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
