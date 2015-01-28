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

/**
 *@class Focus
 *@short Supports manual focusing and auto focusing using relative and absolute INDI focusers.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class Focus : public QWidget, public Ui::Focus
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Focus")

public:
    Focus();
    ~Focus();

    typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusDirection;
    typedef enum { FOCUS_MANUAL, FOCUS_AUTO, FOCUS_LOOP } FocusType;

    /** DBUS interface function.
     * select the CCD device from the available CCD drivers.
     * @param device The CCD device name
     * @return Returns true if CCD device is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /** DBUS interface function.
     * select the focuser device from the available focuser drivers. The focuser device can be the same as the CCD driver if the focuser functionality was embedded within the driver.
     * @param device The focuser device name
     * @return Returns true if focuser device is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setFocuser(QString device);

    /** DBUS interface function.
     * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
     * @param device The filter device name
     * @return Returns true if filter device is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);

    /** DBUS interface function.
     * @return Returns true if autofocus operation is complete. False otherwise.
     */
    Q_SCRIPTABLE bool isAutoFocusComplete() { return (inAutoFocus == false);}

    /** DBUS interface function.
     * @return Returns true if autofocus operation is successful. False otherwise.
     */
    Q_SCRIPTABLE bool isAutoFocusSuccessful() { return m_autoFocusSuccesful;}

    /** DBUS interface function.
     * @return Returns Half-Flux-Radius in pixels.
     */
    Q_SCRIPTABLE double getHFR() { return currentHFR; }

    /** DBUS interface function.
     * Set Focus mode (Manual or Auto)
     * @param mode 0 for manual, any other value for auto.
     */
    Q_SCRIPTABLE Q_NOREPLY void setFocusMode(int mode);

    /** DBUS interface function.
     * Set CCD exposure value
     * @param value exposure value in seconds.
     */
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);

    /** DBUS interface function.
     * Set CCD binning
     * @param binX horizontal binning
     * @param binY vertical binning
     */
    Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);

    /** DBUS interface function.
     * Set image filter to apply to the image after capture.
     * @param value Image filter (Auto Stretch, High Contrast, Equalize, High Pass)
     */
    Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString & value);

    /** DBUS interface function.
     * Set Auto Focus options. The options must be set before starting the autofocus operation. If no options are set, the options loaded from the user configuration are used.
     * @param selectAutoStar If true, Ekos will attempt to automatically select the best focus star in the frame. If it fails to select a star, the user will be asked to select a star manually.
     * @param useSubFrame if true, Ekos will capture a subframe around the selected focus star. The subframe size is determined by the boxSize parameter.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusOptions(bool selectAutoStar, bool useSubFrame);

    /** DBUS interface function.
     * Set Autofocus parameters
     * @param boxSize the box size around the focus star in pixels. The boxsize is used to subframe around the focus star.
     * @param stepSize the initial step size to be commanded to the focuser. If the focuser is absolute, the step size is in ticks. For relative focusers, the focuser will be commanded to focus inward for stepSize milliseconds initially.
     * @param maxTravel the maximum steps permitted before the autofocus operation aborts.
     * @param tolerance Measure of how accurate the autofocus algorithm is. If the difference between the current HFR and minimum measured HFR is less than %tolerance after the focuser traversed both ends of the V-curve, then the focusing operation
     * is deemed successful. Otherwise, the focusing operation will continue.
     */
    Q_SCRIPTABLE Q_NOREPLY void setAutoFocusParameters(int boxSize, int stepSize, int maxTravel, double tolerance);

    /** DBUS interface function.
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
    /** DBUS interface function.
     * Start the autofocus operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void startFocus();

    /** DBUS interface function.
     * Abort the autofocus operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void stopFocus();

    /** DBUS interface function.
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

    /** DBUS interface function.
     * Focus inward
     * @param ms If set, focus inward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
     */
    Q_SCRIPTABLE Q_NOREPLY void FocusIn(int ms=-1);

    /** DBUS interface function.
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
    void updateFocusStatus(bool status);
    void resetFocusFrame();
    void filterChangeWarning(int index);

signals:
        void newLog();
        void autoFocusFinished(bool,double);
        void suspendGuiding(bool);

private:
    void drawHFRPlot();
    void getAbsFocusPosition();
    void autoFocusAbs();
    void autoFocusRel();
    void resetButtons();

    // Devices needed for Focus operation
    ISD::Focuser *currentFocuser;
    ISD::CCD *currentCCD;
    // Optional device
    ISD::GDInterface *currentFilter;

    // List of Focusers
    QList<ISD::Focuser*> Focusers;
    // List of CCDs
    QList<ISD::CCD *> CCDs;
    // They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;

    // As the name implies
    FocusDirection lastFocusDirection;
    // What type of focusing are we doing right now?
    FocusType focusType;

    /*********************
    * HFR Club variables
    *********************/

    // Current HFR value just fetched from FITS file
    double currentHFR;
    // Last HFR value recorded
    double lastHFR;
    // If (currentHFR > deltaHFR) we start the autofocus process.
    double deltaHFR;
    // Maximum HFR recorded
    double maxHFR;
    // Is HFR increasing? We're going away from the sweet spot! If HFRInc=1, we re-capture just to make sure HFR calculations are correct, if HFRInc > 1, we switch directions
    int HFRInc;
    // If HFR decreasing? Well, good job. Once HFR start decreasing, we can start calculating HFR slope and estimating our next move.
    int HFRDec;

    /****************************
    * Absolute position focusers
    ****************************/
    // Absolute focus position
    double currentPosition;
    // What was our position before we started the focus process?
    int initialFocuserAbsPosition;
    // Pulse duration in ms for relative focusers that only support timers, or the number of ticks in a relative or absolute focuser
    int pulseDuration;
    // Does the focuser support absolute motion?
    bool canAbsMove;
    // Range of motion for our lovely absolute focuser
    double absMotionMax, absMotionMin;
    // How many iterations have we completed now in our absolute autofocus algorithm? We can't go forever
    int absIterations;

    /****************************
    * Misc. variables
    ****************************/

    // Are we in the process of capturing an image?
    bool captureInProgress;
    // Was the frame modified by us? Better keep track since we need to return it to its previous state once we are done with the focus operation.
    bool frameModified;
    // If the autofocus process fails, let's not ruin the capture session probably taking place in the next tab. Instead, we should restart it and try again, but we keep count until we hit MAXIMUM_RESET_ITERATIONS
    // and then we truely give up.
    int resetFocusIteration;
    // Which filter must we use once the autofocus process kicks in?
    int lockFilterPosition;
    // Keep track of what we're doing right now
    bool inAutoFocus, inFocusLoop, inSequenceFocus, m_autoFocusSuccesful, resetFocus;   
    // Did we reverse direction?
    bool reverseDir;
    // Did the user or the auto selection process finish selecting our focus star?
    bool starSelected;
    // Target frame dimensions
    int fx,fy,fw,fh;
    // Origianl frame dimensions
    int orig_x, orig_y, orig_w, orig_h;
    // If HFR=-1 which means no stars detected, we need to decide how many times should the re-capture process take place before we give up or reverse direction.
    int noStarCount;

    QStringList logText;
    ITextVectorProperty *filterName;
    INumberVectorProperty *filterSlot;

    /****************************
    * Plot variables
    ****************************/

    // Plot minimum and maximum positions
    int minPos, maxPos;
    // List of V curve plot points
    QList<HFRPoint *> HFRAbsolutePoints;
    // List of iterative curve points
    QList<HFRPoint *> HFRIterativePoints;
};

}

#endif  // Focus_H
