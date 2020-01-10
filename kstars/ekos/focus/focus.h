/*  Ekos Focus tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_focus.h"
#include "ekos/ekos.h"
#include "ekos/auxiliary/filtermanager.h"
#include "fitsviewer/fitsviewer.h"
#include "indi/indiccd.h"
#include "indi/indifocuser.h"
#include "indi/indistd.h"
#include "indi/inditelescope.h"

#include <QtDBus/QtDBus>

namespace Ekos
{

class FocusAlgorithmInterface;
class PolynomialFit;

/**
 * @class Focus
 * @short Supports manual focusing and auto focusing using relative and absolute INDI focusers.
 *
 * @author Jasem Mutlaq
 * @version 1.5
 */
class Focus : public QWidget, public Ui::Focus
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Focus")
        Q_PROPERTY(Ekos::FocusState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString camera READ camera WRITE setCamera)
        Q_PROPERTY(QString focuser READ focuser WRITE setFocuser)
        Q_PROPERTY(QString filterWheel READ filterWheel WRITE setFilterWheel)
        Q_PROPERTY(QString filter READ filter WRITE setFilter)
        Q_PROPERTY(double HFR READ getHFR NOTIFY newHFR)
        Q_PROPERTY(double exposure READ exposure WRITE setExposure)

    public:
        Focus();
        ~Focus();

        typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } FocusDirection;
        typedef enum { FOCUS_MANUAL, FOCUS_AUTO } FocusType;
        typedef enum { FOCUS_ITERATIVE, FOCUS_POLYNOMIAL, FOCUS_LINEAR } FocusAlgorithm;

        /** @defgroup FocusDBusInterface Ekos DBus Interface - Focus Module
             * Ekos::Focus interface provides advanced scripting capabilities to perform manual and automatic focusing operations.
            */

        /*@{*/

        /** DBUS interface function.
             * select the CCD device from the available CCD drivers.
             * @param device The CCD device name
             * @return Returns true if CCD device is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setCamera(const QString &device);
        Q_SCRIPTABLE QString camera();

        /** DBUS interface function.
             * select the focuser device from the available focuser drivers. The focuser device can be the same as the CCD driver if the focuser functionality was embedded within the driver.
             * @param device The focuser device name
             * @return Returns true if focuser device is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setFocuser(const QString &device);
        Q_SCRIPTABLE QString focuser();

        /** DBUS interface function.
             * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
             * @param device The filter device name
             * @return Returns true if filter device is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setFilterWheel(const QString &device);
        Q_SCRIPTABLE QString filterWheel();

        /** DBUS interface function.
             * select the filter from the available filters.
             * @param filter The filter name
             * @return Returns true if filter is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setFilter(const QString &filter);
        Q_SCRIPTABLE QString filter();

        /** DBUS interface function.
             * @return Returns True if current focuser supports auto-focusing
             */
        Q_SCRIPTABLE bool canAutoFocus()
        {
            return (focusType == FOCUS_AUTO);
        }

        /** DBUS interface function.
             * @return Returns Half-Flux-Radius in pixels.
             */
        Q_SCRIPTABLE double getHFR()
        {
            return currentHFR;
        }

        /** DBUS interface function.
             * Set CCD exposure value
             * @param value exposure value in seconds.
             */
        Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
        Q_SCRIPTABLE double exposure()
        {
            return exposureIN->value();
        }

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
        Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString &value);

        /** DBUS interface function.
             * Set Auto Focus options. The options must be set before starting the autofocus operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable If true, Ekos will attempt to automatically select the best focus star in the frame. If it fails to select a star, the user will be asked to select a star manually.
             */
        Q_SCRIPTABLE Q_NOREPLY void setAutoStarEnabled(bool enable);

        /** DBUS interface function.
             * Set Auto Focus options. The options must be set before starting the autofocus operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, Ekos will capture a subframe around the selected focus star. The subframe size is determined by the boxSize parameter.
             */
        Q_SCRIPTABLE Q_NOREPLY void setAutoSubFrameEnabled(bool enable);

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
            * resetFrame Resets the CCD frame to its full native resolution.
            */
        Q_SCRIPTABLE Q_NOREPLY void resetFrame();

        /** DBUS interface function.
            * Return state of Focuser modue (Ekos::FocusState)
            */

        Q_SCRIPTABLE Ekos::FocusState status()
        {
            return state;
        }

        /** @}*/

        /**
             * @brief Add CCD to the list of available CCD.
             * @param newCCD pointer to CCD device.
             */
        void addCCD(ISD::GDInterface *newCCD);

        /**
             * @brief addFocuser Add focuser to the list of available focusers.
             * @param newFocuser pointer to focuser device.
             */
        void addFocuser(ISD::GDInterface *newFocuser);

        /**
             * @brief addFilter Add filter to the list of available filters.
             * @param newFilter pointer to filter device.
             */
        void addFilter(ISD::GDInterface *newFilter);

        /**
         * @brief removeDevice Remove device from Focus module
         * @param deviceRemoved pointer to device
         */
        void removeDevice(ISD::GDInterface *deviceRemoved);

        void setFilterManager(const QSharedPointer<FilterManager> &manager);

        void clearLog();
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }

    public slots:

        /** \addtogroup FocusDBusInterface
             *  @{
             */

        /* Focus */
        /** DBUS interface function.
             * Start the autofocus operation.
             */
        Q_SCRIPTABLE Q_NOREPLY void start();

        /** DBUS interface function.
             * Abort the autofocus operation.
             */
        Q_SCRIPTABLE Q_NOREPLY void abort();

        /** DBUS interface function.
             * Capture a focus frame.
             */
        Q_SCRIPTABLE Q_NOREPLY void capture();

        /** DBUS interface function.
             * Focus inward
             * @param ms If set, focus inward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             */
        Q_SCRIPTABLE bool focusIn(int ms = -1);

        /** DBUS interface function.
             * Focus outward
             * @param ms If set, focus outward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             */
        Q_SCRIPTABLE bool focusOut(int ms = -1);

        /** @}*/

        /**
             * @brief startFraming Begins continuous capture of the CCD and calculates HFR every frame.
             */
        void startFraming();

        /**
             * @brief checkStopFocus Perform checks before stopping the autofocus operation. Some checks are necessary for in-sequence focusing.
             */
        void checkStopFocus();

        /**
             * @brief Check CCD and make sure information is updated accordingly. This simply calls syncCCDInfo for the current CCD.
             * @param CCDNum By default, we check the already selected CCD in the dropdown menu. If CCDNum is specified, the check is made against this specific CCD in the dropdown menu.
             *  CCDNum is the index of the CCD in the dropdown menu.
             */
        void checkCCD(int CCDNum = -1);

        /**
             * @brief syncCCDInfo Read current CCD information and update settings accordingly.
             */
        void syncCCDInfo();

        /**
             * @brief Check Focuser and make sure information is updated accordingly.
             * @param FocuserNum By default, we check the already selected focuser in the dropdown menu. If FocuserNum is specified, the check is made against this specific focuser in the dropdown menu.
             *  FocuserNum is the index of the focuser in the dropdown menu.
             */
        void checkFocuser(int FocuserNum = -1);

        /**
             * @brief Check Filter and make sure information is updated accordingly.
             * @param filterNum By default, we check the already selected filter in the dropdown menu. If filterNum is specified, the check is made against this specific filter in the dropdown menu.
             *  filterNum is the index of the filter in the dropdown menu.
             */
        void checkFilter(int filterNum = -1);

        /**
             * @brief clearDataPoints Remove all data points from HFR plots
             */
        void clearDataPoints();

        /**
             * @brief focusStarSelected The user selected a focus star, save its coordinates and subframe it if subframing is enabled.
             * @param x X coordinate
             * @param y Y coordinate
             */
        void focusStarSelected(int x, int y);

        /**
             * @brief newFITS A new FITS blob is received by the CCD driver.
             * @param bp pointer to blob data
             */
        void newFITS(IBLOB *bp);

        /**
             * @brief processFocusNumber Read focus number properties of interest as they arrive from the focuser driver and process them accordingly.
             * @param nvp pointer to updated focuser number property.
             */
        void processFocusNumber(INumberVectorProperty *nvp);

        /**
             * @brief checkFocus Given the minimum required HFR, check focus and calculate HFR. If current HFR exceeds required HFR, start autofocus process, otherwise do nothing.
             * @param requiredHFR Minimum HFR to trigger autofocus process.
             */
        void checkFocus(double requiredHFR);

        /**
             * @brief setFocusStatus Upon completion of the focusing process, set its status (fail or pass) and reset focus process to clean state.
             * @param status If true, the focus process finished successfully. Otherwise, it failed.
             */
        void setAutoFocusResult(bool status);

        /**
             * @brief filterChangeWarning Warn the user it is not a good idea to apply image filter in the filter process as they can skew the HFR calculations.
             * @param index Index of image filter selected by the user.
             */
        void filterChangeWarning(int index);

        // Log
        void appendLogText(const QString &);

        // Adjust focuser offset, relative or absolute
        void adjustFocusOffset(int value, bool useAbsoluteOffset);

        // Update Mount module status
        void setMountStatus(ISD::Telescope::Status newState);

        /**
         * @brief toggleVideo Turn on and off video streaming if supported by the camera.
         * @param enabled Set to true to start video streaming, false to stop it if active.
         */
        void toggleVideo(bool enabled);

    private slots:
        /**
             * @brief toggleSubframe Process enabling and disabling subfrag.
             * @param enable If true, subframing is enabled. If false, subframing is disabled. Even if subframing is enabled, it must be supported by the CCD driver.
             */
        void toggleSubframe(bool enable);

        void checkAutoStarTimeout();

        void setAbsoluteFocusTicks();

        void updateBoxSize(int value);

        void processCaptureTimeout();

        void processCaptureFailure();

        void setCaptureComplete();

        void showFITSViewer();

        void toggleFocusingWidgetFullScreen();

        void setVideoStreamEnabled(bool enabled);

        void syncSettings();

        void graphPolynomialFunction();

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::FocusState state);
        void newHFR(double hfr, int position);

        void absolutePositionChanged(int value);
        void focusPositionAdjusted();

        void suspendGuiding();
        void resumeGuiding();
        void newStarPixmap(QPixmap &);
        void newProfilePixmap(QPixmap &);

    private:

        ////////////////////////////////////////////////////////////////////
        /// Connections
        ////////////////////////////////////////////////////////////////////
        void initConnections();

        ////////////////////////////////////////////////////////////////////
        /// Settings
        ////////////////////////////////////////////////////////////////////

        /**
         * @brief initSettings Connect settings to slots to update the value when changed
         */
        void initSettingsConnections();
        /**
         * @brief loadSettings Load setting from Options and set them accordingly.
         */
        void loadSettings();

        ////////////////////////////////////////////////////////////////////
        /// HFR Plot
        ////////////////////////////////////////////////////////////////////
        void initPlots();
        void drawHFRPlot();
        void drawHFRIndeces();
        void drawProfilePlot();

        ////////////////////////////////////////////////////////////////////
        /// Positions
        ////////////////////////////////////////////////////////////////////
        void getAbsFocusPosition();
        void autoFocusAbs();
        void autoFocusRel();
        void resetButtons();
        void stop(bool aborted = false);

        void initView();

        // Move the focuser in (negative) or out (positive amount).
        bool changeFocus(int amount);

        // Start up capture, or occasionally move focuser again, after current focus-move accomplished.
        void autoFocusProcessPositionChange(IPState state);

        // For the Linear algorithm, which always scans in (from higher position to lower position)
        // if we notice the new position is higher than the current position (that is, it is the start
        // of a new scan), we adjust the new position to be several steps further out than requested
        // and set focuserAdditionalMovement to the extra motion, so that after this motion completes
        // we will then scan back in (back to the originally requested position). This "dance" is done
        // to reduce backlash on such movement changes and so that we've always focused in before capture.
        int adjustLinearPosition(int position, int newPosition);

        /**
         * @brief syncTrackingBoxPosition Sync the tracking box to the current selected star center
         */
        void syncTrackingBoxPosition();

        /// Focuser device needed for focus operation
        ISD::Focuser *currentFocuser { nullptr };
        /// CCD device needed for focus operation
        ISD::CCD *currentCCD { nullptr };

        /// Optional device filter
        ISD::GDInterface *currentFilter { nullptr };
        /// Current filter position
        int currentFilterPosition { -1 };
        int fallbackFilterPosition { -1 };
        /// True if we need to change filter position and wait for result before continuing capture
        bool filterPositionPending { false };
        bool fallbackFilterPending { false };

        /// List of Focusers
        QList<ISD::Focuser *> Focusers;
        /// List of CCDs
        QList<ISD::CCD *> CCDs;
        /// They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
        QList<ISD::GDInterface *> Filters;

        /// As the name implies
        FocusDirection lastFocusDirection { FOCUS_NONE };
        /// What type of focusing are we doing right now?
        FocusType focusType { FOCUS_MANUAL };
        /// Focus HFR & Centeroid algorithms
        StarAlgorithm focusDetection { ALGORITHM_GRADIENT };
        /// Focus Process Algorithm
        FocusAlgorithm focusAlgorithm { FOCUS_ITERATIVE };

        /*********************
         * HFR Club variables
         *********************/

        /// Current HFR value just fetched from FITS file
        double currentHFR { 0 };
        /// Last HFR value recorded
        double lastHFR { 0 };
        /// If (currentHFR > deltaHFR) we start the autofocus process.
        double minimumRequiredHFR { -1 };
        /// Maximum HFR recorded
        double maxHFR { 1 };
        /// Is HFR increasing? We're going away from the sweet spot! If HFRInc=1, we re-capture just to make sure HFR calculations are correct, if HFRInc > 1, we switch directions
        int HFRInc { 0 };
        /// If HFR decreasing? Well, good job. Once HFR start decreasing, we can start calculating HFR slope and estimating our next move.
        int HFRDec { 0 };

        /****************************
         * Absolute position focusers
         ****************************/
        /// Absolute focus position
        double currentPosition { 0 };
        /// What was our position before we started the focus process?
        int initialFocuserAbsPosition { -1 };
        /// Pulse duration in ms for relative focusers that only support timers, or the number of ticks in a relative or absolute focuser
        int pulseDuration { 1000 };
        /// Does the focuser support absolute motion?
        bool canAbsMove { false };
        /// Does the focuser support relative motion?
        bool canRelMove { false };
        /// Does the focuser support timer-based motion?
        bool canTimerMove { false };
        /// Maximum range of motion for our lovely absolute focuser
        double absMotionMax { 0 };
        /// Minimum range of motion for our lovely absolute focuser
        double absMotionMin { 0 };
        /// How many iterations have we completed now in our absolute autofocus algorithm? We can't go forever
        int absIterations { 0 };

        /****************************
         * Misc. variables
         ****************************/

        /// Are we in the process of capturing an image?
        bool captureInProgress { false };
        // Was the frame modified by us? Better keep track since we need to return it to its previous state once we are done with the focus operation.
        //bool frameModified;
        /// Was the modified frame subFramed?
        bool subFramed { false };
        /// If the autofocus process fails, let's not ruin the capture session probably taking place in the next tab. Instead, we should restart it and try again, but we keep count until we hit MAXIMUM_RESET_ITERATIONS
        /// and then we truly give up.
        int resetFocusIteration { 0 };
        /// Which filter must we use once the autofocus process kicks in?
        int lockedFilterIndex { -1 };
        /// Keep track of what we're doing right now
        bool inAutoFocus { false };
        bool inFocusLoop { false };
        bool inSequenceFocus { false };
        bool resetFocus { false };
        /// Did we reverse direction?
        bool reverseDir { false };
        /// Did the user or the auto selection process finish selecting our focus star?
        bool starSelected { false };
        /// Adjust the focus position to a target value
        bool adjustFocus { false };
        // Target frame dimensions
        //int fx,fy,fw,fh;
        /// If HFR=-1 which means no stars detected, we need to decide how many times should the re-capture process take place before we give up or reverse direction.
        int noStarCount { 0 };
        /// Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
        ISD::CCD::UploadMode rememberUploadMode { ISD::CCD::UPLOAD_CLIENT };
        /// Previous binning setting
        int activeBin { 0 };
        /// HFR values for captured frames before averages
        QVector<double> HFRFrames;
        // CCD Exposure Looping
        bool rememberCCDExposureLooping = { false };

        QStringList m_LogText;
        ITextVectorProperty *filterName { nullptr };
        INumberVectorProperty *filterSlot { nullptr };

        /****************************
         * Plot variables
         ****************************/

        /// Plot minimum positions
        double minPos { 1e6 };
        /// Plot maximum positions
        double maxPos { 0 };
        /// List of V curve plot points
        /// V-Curve graph
        QCPGraph *v_graph { nullptr };

        // Last gaussian fit values
        QVector<double> lastGausIndexes;
        QVector<double> lastGausFrequencies;
        QCPGraph *currentGaus { nullptr };
        QCPGraph *firstGaus { nullptr };
        QCPGraph *lastGaus { nullptr };

        QVector<double> hfr_position, hfr_value;

        // Pixmaps
        QPixmap profilePixmap;

        /// State
        Ekos::FocusState state { Ekos::FOCUS_IDLE };

        /// FITS Scale
        FITSScale defaultScale;

        /// CCD Chip frame settings
        QMap<ISD::CCDChip *, QVariantMap> frameSettings;

        /// Selected star coordinates
        QVector3D starCenter;

        // Remember last star center coordinates in case of timeout in manual select mode
        QVector3D rememberStarCenter;

        /// Focus Frame
        FITSView *focusView { nullptr };

        /// Star Select Timer
        QTimer waitStarSelectTimer;

        /// FITS Viewer in case user want to display in it instead of internal view
        QPointer<FITSViewer> fv;

        /// Track star position and HFR to know if we're detecting bogus stars due to detection algorithm false positive results
        QVector<QVector3D> starsHFR;

        /// Relative Profile
        QCustomPlot *profilePlot { nullptr };
        QDialog *profileDialog { nullptr };

        /// Polynomial fitting.
        std::unique_ptr<PolynomialFit> polynomialFit;
        int polySolutionFound { 0 };
        QCPGraph *polynomialGraph = nullptr;
        QCPGraph *focusPoint = nullptr;
        bool polynomialGraphIsShown = false;

        // Capture timeout timer
        QTimer captureTimeout;
        uint8_t captureTimeoutCounter { 0 };
        uint8_t captureFailureCounter { 0 };

        // Guide Suspend
        bool m_GuidingSuspended { false };

        // Filter Manager
        QSharedPointer<FilterManager> filterManager;

        // Experimental linear focuser.
        std::unique_ptr<FocusAlgorithmInterface> linearFocuser;
        int focuserAdditionalMovement { 0 };

        bool hasDeviation { false };
};
}
