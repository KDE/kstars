/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_focus.h"
#include "focusfourierpower.h"
#include "ekos/ekos.h"
#include "parameters.h"
#include "ekos/auxiliary/filtermanager.h"

#include "indi/indicamera.h"
#include "indi/indifocuser.h"
#include "indi/indistd.h"
#include "indi/indiweather.h"
#include "indi/indimount.h"

#include "opsfocussettings.h"
#include "opsfocusprocess.h"
#include "opsfocusmechanics.h"
#include "ui_cfz.h"
#include "focusutils.h"

class FocusProfilePlot;
class FITSData;
class FITSView;
class FITSViewer;

namespace Ekos
{

class DarkProcessor;
class FocusAlgorithmInterface;
class FocusFWHM;
#if defined(HAVE_OPENCV)
class FocusBlurriness;
#endif
class PolynomialFit;
class AdaptiveFocus;
class FocusAdvisor;
class StellarSolverProfileEditor;

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

        // AdaptiveFocus and FocusAdvisor are friend classes so they can access methods in Focus
        friend class AdaptiveFocus;
        friend class FocusAdvisor;
        friend class FocusModule;

    public:
        Focus(int id = 0);
        ~Focus();

        typedef enum { FOCUS_NONE, FOCUS_IN, FOCUS_OUT } Direction;
        typedef enum { FOCUS_MANUAL, FOCUS_AUTO } Type;
        typedef enum { FOCUS_ITERATIVE, FOCUS_POLYNOMIAL, FOCUS_LINEAR, FOCUS_LINEAR1PASS } Algorithm;
        typedef enum { FOCUS_CFZ_CLASSIC, FOCUS_CFZ_WAVEFRONT, FOCUS_CFZ_GOLD } CFZAlgorithm;
        typedef enum { FOCUS_STAR_HFR, FOCUS_STAR_HFR_ADJ, FOCUS_STAR_FWHM, FOCUS_STAR_NUM_STARS, FOCUS_STAR_FOURIER_POWER, FOCUS_STAR_STDDEV, FOCUS_STAR_SOBEL, FOCUS_STAR_LAPLASSIAN, FOCUS_STAR_CANNY } StarMeasure;
        typedef enum { FOCUS_STAR_GAUSSIAN, FOCUS_STAR_MOFFAT } StarPSF;
        typedef enum { FOCUS_UNITS_PIXEL, FOCUS_UNITS_ARCSEC } StarUnits;
        typedef enum { FOCUS_WALK_CLASSIC, FOCUS_WALK_FIXED_STEPS, FOCUS_WALK_CFZ_SHUFFLE } FocusWalk;
        typedef enum { FOCUS_MASK_NONE, FOCUS_MASK_RING, FOCUS_MASK_MOSAIC } ImageMaskType;

        /** @defgroup FocusDBusInterface Ekos DBus Interface - Focus Module
             * Ekos::Focus interface provides advanced scripting capabilities to perform manual and automatic focusing operations.
            */

        /*@{*/

        /** DBUS interface function.
             * select the CCD device from the available CCD drivers.
             * @param device The CCD device name
             * @return Returns true if CCD device is found and set, false otherwise.
             */
        Q_SCRIPTABLE QString camera();

        /** DBUS interface function.
             * select the focuser device from the available focuser drivers. The focuser device can be the same as the CCD driver if the focuser functionality was embedded within the driver.
             * @param device The focuser device name
             * @return Returns true if focuser device is found and set, false otherwise.
             */
        Q_SCRIPTABLE QString focuser();

        /** DBUS interface function.
             * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
             * @param device The filter device name
             * @return Returns true if filter device is found and set, false otherwise.
             */
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
            return (m_FocusType == FOCUS_AUTO);
        }

        /** DBUS interface function.
         * @return Returns True if current focuser uses the full field for focusing
         */
        Q_SCRIPTABLE bool useFullField()
        {
            return m_OpsFocusSettings->focusUseFullField->isChecked();
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
            return focusExposure->value();
        }

        /** DBUS interface function.
             * Set CCD binning
             * @param binX horizontal binning
             * @param binY vertical binning
             */
        Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);

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
            * Return state of Focuser module (Ekos::FocusState)
            */

        Q_SCRIPTABLE Ekos::FocusState status()
        {
            return m_state;
        }

        /** @}*/

        /**
             * @brief Add CCD to the list of available CCD.
             * @param newCCD pointer to CCD device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setCamera(ISD::Camera *device);

        /**
             * @brief addFocuser Add focuser to the list of available focusers.
             * @param newFocuser pointer to focuser device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setFocuser(ISD::Focuser *device);

        /**
             * @brief reconnectFocuser Add focuser to the list of available focusers.
             * @param focuser name of the focuser.
             */
        void reconnectFocuser(const QString &focuser);

        /**
             * @brief addFilter Add filter to the list of available filters.
             * @param newFilter pointer to filter device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setFilterWheel(ISD::FilterWheel *device);

        /**
         * @brief setImageMask Select the currently active image mask filtering
         *        the stars relevant for focusing
         */
        void selectImageMask();

        /**
         * @brief updateTemperatureSources Update list of available temperature sources.
         * @param temperatureSources Devices with temperature reporting capability
         * @return True if updated successfully
         */
        void updateTemperatureSources(const QList<QSharedPointer<ISD::GenericDevice>> &temperatureSources);

        /**
         * @brief removeDevice Remove device from Focus module
         * @param deviceRemoved pointer to device
         */
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &deviceRemoved);

        const QSharedPointer<FilterManager> &filterManager() const
        {
            return m_FilterManager;
        }
        void setupFilterManager();
        void connectFilterManager();

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);

        const QString &opsDialogName() const
        {
            return m_opsDialogName;
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
             * @param settleTime if > 0 wait for the given time in seconds before starting to capture
             */
        Q_SCRIPTABLE Q_NOREPLY void capture(double settleTime = 0.0);

        /** DBUS interface function.
             * Focus inward
             * @param ms If set, focus inward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             * @param speedFactor option to multiply the given ms value for faster motion
             */
        Q_SCRIPTABLE bool focusIn(int ms = -1, int speedFactor = 1);

        /** DBUS interface function.
             * Focus outward
             * @param ms If set, focus outward for ms ticks (Absolute Focuser), or ms milliseconds (Relative Focuser). If not set, it will use the value specified in the options.
             */
        Q_SCRIPTABLE bool focusOut(int ms = -1, int speedFactor = 1);

        /**
             * @brief checkFocus Given the minimum required HFR, check focus and calculate HFR. If current HFR exceeds required HFR, start autofocus process, otherwise do nothing.
             * @param requiredHFR Minimum HFR to trigger autofocus process.
             * @param speedFactor option to multiply the given ms value for faster motion
             */
        Q_SCRIPTABLE Q_NOREPLY void checkFocus(double requiredHFR);

        /** @}*/

        /**
             * @brief Run the autofocus process for the currently selected filter
             * @param The reason Autofocus has been called.
             */
        void runAutoFocus(const AutofocusReason autofocusReason, const QString &reasonInfo);

        /**
             * @brief startFraming Begins continuous capture of the CCD and calculates HFR every frame.
             */
        void startFraming();

        /**
         * @brief Move the focuser to the initial focus position.
         */
        void resetFocuser();

        /**
             * @brief checkStopFocus Perform checks before stopping the autofocus operation. Some checks are necessary for in-sequence focusing.
             * @param abort true iff focusing should be aborted, false if it should only be stopped and marked as failed
             */
        void checkStopFocus(bool abort);

        /**
         * @brief React when a meridian flip has been started
         */
        void meridianFlipStarted();

        /**
             * @brief Check CCD and make sure information is updated accordingly. This simply calls syncCameraInfo for the current CCD.
             * @param CCDNum By default, we check the already selected CCD in the dropdown menu. If CCDNum is specified, the check is made against this specific CCD in the dropdown menu.
             *  CCDNum is the index of the CCD in the dropdown menu.
             */
        void checkCamera();

        /**
             * @brief syncCameraInfo Read current CCD information and update settings accordingly.
             */
        void syncCameraInfo();

        /**
         * @brief Update camera controls like Gain, ISO, Offset...etc
         */
        void syncCCDControls();

        /**
             * @brief Check Focuser and make sure information is updated accordingly.
             * @param FocuserNum By default, we check the already selected focuser in the dropdown menu. If FocuserNum is specified, the check is made against this specific focuser in the dropdown menu.
             *  FocuserNum is the index of the focuser in the dropdown menu.
             */
        void checkFocuser();

        /**
             * @brief Check Filter and make sure information is updated accordingly.
             * @param filterNum By default, we check the already selected filter in the dropdown menu. If filterNum is specified, the check is made against this specific filter in the dropdown menu.
             *  filterNum is the index of the filter in the dropdown menu.
             */
        void checkFilter();

        /**
             * @brief Check temperature source and make sure information is updated accordingly.
             * @param name Name of temperature source, if empty then use current source.
             */
        void checkTemperatureSource(const QString &name = QString());

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
         * @brief selectFocusStarFraction Select the focus star based by fraction of the overall size.
         * It calls focusStarSelected after multiplying the fractions (0.0 to 1.0) with the focus view width and height.
         * @param x final x = x * focusview_width
         * @param y final y = y * focusview_height
         */
        void selectFocusStarFraction(double x, double y);

        /**
             * @brief newFITS A new FITS blob is received by the CCD driver.
             * @param bp pointer to blob data
             */
        void processData(const QSharedPointer<FITSData> &data);

        /**
             * @brief updateProperty Read focus number properties of interest as they arrive from the focuser driver and process them accordingly.
             * @param prop INDI Property
             */
        void updateProperty(INDI::Property prop);

        /**
             * @brief processTemperatureSource Updates focus temperature source.
             * @param nvp pointer to updated focuser number property.
             */
        void processTemperatureSource(INDI::Property prop);

        /**
             * @brief setFocusStatus Upon completion of the focusing process, set its status (fail or pass) and reset focus process to clean state.
             * @param status If true, the focus process finished successfully. Otherwise, it failed.
             */
        //void setAutoFocusResult(bool status);
        // Logging
        void appendLogText(const QString &logtext);
        void appendFocusLogText(const QString &text);

        // Adjust focuser offset, relative or absolute
        void adjustFocusOffset(int value, bool useAbsoluteOffset);

        // Update Mount module status
        void setMountStatus(ISD::Mount::Status newState);

        // Update Altitude From Mount
        void setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

        /**
         * @brief toggleVideo Turn on and off video streaming if supported by the camera.
         * @param enabled Set to true to start video streaming, false to stop it if active.
         */
        void toggleVideo(bool enabled);

        /**
         * @brief setWeatherData Updates weather data that could be used to extract focus temperature from observatory
         * in case focus native temperature is not available.
         */
        //void setWeatherData(const std::vector<ISD::Weather::WeatherData> &data);

        /**
         * @brief loadOptionsProfiles Load StellarSolver Profile
         */
        void loadStellarSolverProfiles();

        /**
         * @brief getStellarSolverProfiles
         * @return list of StellarSolver profile names
         */
        QStringList getStellarSolverProfiles();

        QString opticalTrain() const
        {
            return opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrainCombo->setCurrentText(value);
        }

        /**
         * @brief adaptiveFocus moves the focuser between subframes to stay at focus
         */
        void adaptiveFocus();

    protected:
        void addPlotPosition(int pos, double hfr, bool plot = true);

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

        void processCaptureErrorDefault();
        void processCaptureError(ISD::Camera::ErrorType type);

        void setCaptureComplete();

        void showFITSViewer();

        void toggleFocusingWidgetFullScreen();

        void setVideoStreamEnabled(bool enabled);

        void starDetectionFinished();
        void setCurrentMeasure();
        void startAbIns();
        void manualStart();

    signals:
        void newLog(const QString &text);
        void newFocusLog(const QString &text);
        void newStatus(Ekos::FocusState state, const QString &trainname, const bool update = true);
        void newHFR(double hfr, int position, bool inAutofocus, const QString &trainname);
        void newFocusTemperatureDelta(double delta, double absTemperature, const QString &trainname);
        void inSequenceAF(bool requested, const QString &trainname);

        void absolutePositionChanged(int value);
        void focusPositionAdjusted();
        void focusAdaptiveComplete(bool success, const QString &trainname);
        void focusAFOptimise();

        void trainChanged();
        void focuserChanged(int id, bool isValid);

        void suspendGuiding();
        void resumeGuiding();
        void newImage(const QSharedPointer<FITSView> &view);
        void newStarPixmap(QPixmap &);
        void settingsUpdated(const QVariantMap &settings);

        // Signals for Analyze.
        void autofocusStarting(double temperature, const QString &filter, AutofocusReason reason, const QString &reasonInfo);
        void autofocusComplete(double temperature, const QString &filter, const QString &points, const bool useWeights,
                               const QString &curve = "", const QString &title = "");
        void autofocusAborted(const QString &filter, const QString &points, const bool useWeights,
                              const AutofocusFailReason failCode, const QString &failCodeInfo);

        // Focus Advisor
        void newFocusAdvisorStage(int stage);
        void newFocusAdvisorMessage(QString name);

        /**
         * @brief Signal Analyze that an Adaptive Focus iteration is complete
         * @param Active filter
         * @param temperature
         * @param tempTicks is the number of ticks movement due to temperature change
         * @param altitude
         * @param altTicks is the number of ticks movement due to altitude change
         * @param prevPosError is the position error at the previous adaptive focus iteration
         * @param thisPosError is the position error for the current adaptive focus iteration
         * @param totalTicks is the total tick movement for this adaptive focus iteration
         * @param position is the current focuser position
         * @param focuserMoved indicates whether totalTicks > minimum focuser movement
         */
        void adaptiveFocusComplete(const QString &filter, double temperature, double tempTicks, double altitude, double altTicks,
                                   int prevPosError, int thisPosError, int totalTicks, int position, bool focuserMoved);

        // HFR V curve plot events
        /**
         * @brief initialize the HFR V plot
         * @param showPosition show focuser position (true) or count focus iterations (false)
         * @param yAxisLabel is the label to display
         * @param starUnits the units multiplier to display the pixel data
         * @param minimum whether the curve shape is a minimum or maximum
         * @param useWeights whether or not to display weights on the graph
         * @param showPosition show focuser position (true) or show focusing iteration number (false)
         */
        void initHFRPlot(QString str, double starUnits, bool minimum, bool useWeights, bool showPosition);

        /**
          * @brief new HFR plot position with sigma
          * @param pos focuser position with associated error (sigma)
          * @param hfr measured star HFR value
          * @param sigma is the standard deviation of star HFRs
          * @param pulseDuration Pulse duration in ms for relative focusers that only support timers,
          *        or the number of ticks in a relative or absolute focuser
          * */
        void newHFRPlotPosition(double pos, double hfr, double sigma, bool outlier, int pulseDuration, bool plot = true);

        /**
         * @brief draw the approximating polynomial into the HFR V-graph
         * @param poly pointer to the polynomial approximation
         * @param isVShape has the solution a V shape?
         * @param activate make the graph visible?
         */
        void drawPolynomial(PolynomialFit *poly, bool isVShape, bool activate, bool plot = true);

        /**
         * @brief draw the curve into the HFR V-graph
         * @param poly pointer to the polynomial approximation
         * @param isVShape has the solution a V shape?
         * @param activate make the graph visible?
         */
        void drawCurve(CurveFitting *curve, bool isVShape, bool activate, bool plot = true);

        /**
         * @brief Focus solution with minimal HFR found
         * @param solutionPosition focuser position
         * @param solutionValue HFR value
         */
        void minimumFound(double solutionPosition, double solutionValue, bool plot = true);

        /**
         * @brief Draw Critical Focus Zone on graph
         * @param solutionPosition focuser position
         * @param solutionValue HFR value
         * @param m_cfzSteps the size of the CFZ
         * @param plt - whether to plot the CFZ
         */
        void drawCFZ(double minPosition, double minValue, int m_cfzSteps, bool plt);

        /**
         * @brief redraw the entire HFR plot
         * @param poly pointer to the polynomial approximation
         * @param solutionPosition solution focuser position
         * @param solutionValue solution HFR value
         */
        void redrawHFRPlot(PolynomialFit *poly, double solutionPosition, double solutionValue);

        /**
         * @brief draw a title on the focus plot
         * @param title the title
         */
        void setTitle(const QString &title, bool plot = true);

        /**
         * @brief final updates after focus run comopletes on the focus plot
         * @param title
         * @param plot
         */
        void finalUpdates(const QString &title, bool plot = true);

        /**
         * @brief focuserTimedout responding to requests
         * @param focuser
         */
        void focuserTimedout(const QString &focuser);

    private:

        QList<SSolver::Parameters> m_StellarSolverProfiles;
        QString savedOptionsProfiles;
        StellarSolverProfileEditor *optionsProfileEditor { nullptr };

        // Connections
        void initConnections();

        // Settings

        /**
         * @brief Connect GUI elements to sync settings once updated.
         */
        void connectSyncSettings();
        /**
         * @brief Stop updating settings when GUI elements are updated.
         */
        void disconnectSyncSettings();
        /**
         * @brief loadSettings Load setting from Options and set them accordingly.
         */
        void loadGlobalSettings();

        /**
         * @brief checkMosaicMaskLimits Check if the maximum values configured
         * for the aberration style mosaic tile sizes fit into the CCD frame size.
         */
        void checkMosaicMaskLimits();

        /**
         * @brief syncSettings When checkboxes, comboboxes, or spin boxes are updated, save their values in the
         * global and per-train settings.
         */
        void syncSettings();

        /**
         * @brief syncControl Sync setting to widget. The value depends on the widget type.
         * @param settings Map of all settings
         * @param key name of widget to sync
         * @param widget pointer of widget to set
         * @return True if sync successful, false otherwise
         */
        bool syncControl(const QVariantMap &settings, const QString &key, QWidget * widget);

        /**
         * @brief settleSettings Run this function after timeout from debounce timer to update database
         * and emit settingsChanged signal. This is required so we don't overload output.
         */
        void settleSettings();

        /**
         * @brief prepareGUI Perform once only GUI prep processing
         */
        void prepareGUI();

        /**
         * @brief setUseWeights sets the useWeights checkbox
         */
        void setUseWeights();

        /**
         * @brief setDenoise sets the focusDenoise checkbox
         */
        void setDenoise();

        /**
         * @brief setDonutBuster sets the donutBuster checkbox
         */
        void setDonutBuster();

        /**
         * @brief setScanForStartPos sets the scanForStartPos checkbox
         */
        void setScanForStartPos();

        /**
         * @brief addMissingStellarSolverProfiles
         * @param profile to add
         * @param profilePath file pathname
         */
        void addMissingStellarSolverProfile(const QString profilesPath, const QString profile);

        /**
         * @brief Perform the processing to abort an in-flight focus procedure
         */
        void processAbort();

        // HFR Plot
        void initPlots();

        // Positions
        void getAbsFocusPosition();
        bool autoFocusChecks();
        void autoFocusAbs();
        void autoFocusLinear();
        void autoFocusRel();

        // events
        void handleFocusButtonEvent();

        // Linear does plotting differently from the rest.
        void plotLinearFocus();

        // Linear final updates to the curve
        void plotLinearFinalUpdates();

        // Launch the Aberation Inspector popup
        void startAberrationInspector();

        // Get the curve fitting goal based on how the algorithm is progressing
        CurveFitting::FittingGoal getGoal(int numSteps);

        /** @brief Helper function determining whether the focuser behaves like a position
         *         based one (vs. a timer based)
         */
        bool isPositionBased()
        {
            return (canAbsMove || canRelMove || (m_FocusAlgorithm == FOCUS_LINEAR) || (m_FocusAlgorithm == FOCUS_LINEAR1PASS));
        }
        void resetButtons();

        /**
         * @brief updateButtonColors Change button colors due Shift and Ctrl modifiers
         */
        void updateButtonColors(QPushButton *button, bool shift, bool ctrl);

        /**
         * @brief returns whether the Aberration Inspector can be used or not
         * @return can / cant be started
         */
        bool canAbInsStart();
        void stop(FocusState completionState = FOCUS_ABORTED);

        void initView();
        void initHelperObjects();

        /** @brief Sets the plot vectors for Analyze after Autofocus. Used by Linear and Linear1Pass
         */
        void updatePlotPosition();

        /** @brief Build the data string to send to Analyze
         */
        QString getAnalyzeData();

        /**
         * @brief prepareCapture Set common settings for capture for focus module
         * @param targetChip target Chip
         */
        void prepareCapture(ISD::CameraChip *targetChip);

        // HFR / FWHM
        void setHFRComplete();

        // Sets the star algorithm and enables/disables various UI inputs.
        void setFocusDetection(StarAlgorithm starAlgorithm);

        // Sets the algorithm and enables/disables various UI inputs.
        void setFocusAlgorithm(Algorithm algorithm);

        // Set the Auto Star & Box widgets
        void setAutoStarAndBox();

        void setCurveFit(CurveFitting::CurveFit curvefit);

        void setStarMeasure(StarMeasure starMeasure);
        void setStarPSF(StarPSF starPSF);
        void setStarUnits(StarUnits starUnits);
        void setWalk(FocusWalk focusWalk);
        double calculateStarWeight(const bool useWeights, const std::vector<double> values);
        bool boxOverlap(const QPair<int, int> b1Start, const QPair<int, int> b1End, const QPair<int, int> b2Start,
                        const QPair<int, int> b2End);
        double getStarUnits(const StarMeasure starMeasure, const StarUnits starUnits);
        // Calculate the CFZ of the current focus camera
        double calcCameraCFZ();

        // Calculate the CFZ from the screen parameters
        void calcCFZ();

        // Static data for filter's midpoint wavelength changed so update CFZ
        void wavelengthChanged();

        // Reset the CFZ parameters from the current Optical Train
        void resetCFZToOT();

        // Move the focuser in (negative) or out (positive amount).
        bool changeFocus(int amount, bool updateDir = true);

        // Start up capture, or occasionally move focuser again, after current focus-move accomplished.
        void autoFocusProcessPositionChange(IPState state);

        // For the Linear algorithm, which always scans in (from higher position to lower position)
        // if we notice the new position is higher than the current position (that is, it is the start
        // of a new scan), we adjust the new position to be several steps further out than requested
        // and set focuserAdditionalMovement to the extra motion, so that after this motion completes
        // we will then scan back in (back to the originally requested position). This "overscan dance" is done
        // to reduce backlash on such movement changes and so that we've always focused in before capture.
        int adjustLinearPosition(int position, int newPosition, int overscan, bool updateDir);

        // Are we using a Star Measure that assumes a star field that requires star detection
        bool isStarMeasureStarBased();

        // Process the image to get star FWHMs
        void getFWHM(const QList<Edge *> &stars, double *FWHM, double *weight);

        // Process the image to get the Fourier Transform Power
        // If tile = -1 use the whole image; if mosaicTile is specified use just that
        void getFourierPower(double *fourierPower, double *weight, const int mosaicTile = -1);

        // Process the image to get the blurryness factor
        // If tile = -1 use the whole image; if mosaicTile is specified use just that
        void getBlurriness(const StarMeasure starMeasure, const bool denoise, double *blurriness, double *weight,
                           const QRect &roi, const int mosaicTile = -1);

        /**
         * @brief syncTrackingBoxPosition Sync the tracking box to the current selected star center
         */
        void syncTrackingBoxPosition();

        /** @internal Search for stars using the method currently configured, and return the consolidated HFR.
         * @param image_data is the FITS frame to work with.
         * @return the HFR of the star or field of stars in the frame, depending on the consolidation method, or -1 if it cannot be estimated.
         */
        void analyzeSources();

        /** @internal Add a new star measure (HFR, FWHM, etc) for the current focuser position.
         * @param newMeasure is the new measure (e.g. HFR, FWHM, etc) to consider for the current focuser position.
         * @return true if a new sample is required, else false.
         */
        bool appendMeasure(double newMeasure);

        /**
         * @brief completeAutofocusProcedure finishes off autofocus and emits a message for other modules.
         */
        void completeFocusProcedure(FocusState completionState, AutofocusFailReason failCode, QString failCodeInfo = "",
                                    bool plot = true);

        /**
         * @brief activities to be executed after the configured settling time
         * @param completionState state the focuser completed with
         * @param autoFocusUsed is autofocus running?
         * @param buildOffsetsUsed is autofocus running as a result of build offsets
         * @param failCode is the reason for the Autofocus failure
         * @param failCodeInfo contains extra info about failCode
         */
        void settle(const FocusState completionState, const bool autoFocusUsed,
                    const bool buildOffsetsUsed, const AutofocusFailReason failCode, const QString failCodeInfo);

        void setLastFocusTemperature();
        void setLastFocusAlt();
        bool findTemperatureElement(const QSharedPointer<ISD::GenericDevice> &device);

        /**
         * @brief reset Adaptive Focus parameters
         * @param Adaptive Focus enabled
         */
        void resetAdaptiveFocus(bool enabled);

        void setupOpticalTrainManager();
        void refreshOpticalTrain();

        /**
         * @brief set member valiables for the scope attached to the current Optical Train
         * @param Optical Train scope parameters
         * @param Optical Train reducer
         */
        void setScopeDetails(const QJsonObject &scope, const double reducer);

        /**
         * @brief handleFocusMotionTimeout When focuser is command to go to a target position, we expect to receive a notification
         * that it arrived at the desired destination. If not, we command it again.
         */
        void handleFocusMotionTimeout();

        /**
         * @brief returns axis label based on measure selected
         * @param starMeasure the star measure beuing used
         */
        QString getyAxisLabel(StarMeasure starMeasure);

        /**
         * @brief disable input widgets at the start of an AF run
         * @param the widget to disable
         * @param whether to disable at the widget level or disable all the children
         */
        void AFDisable(QWidget * widget, const bool children);

        /**
         * @brief returns whether the Gain input field is enabled outside of autofocus and
         * whether logically is should be enabled during AF even though all input widgets are disabled
         */
        bool isFocusGainEnabled();

        /**
         * @brief returns whether the ISO input field is enabled outside of autofocus and
         * whether logically is should be enabled during AF even though all input widgets are disabled
         */
        bool isFocusISOEnabled();

        /**
         * @brief returns whether the SubFrame input field is enabled outside of autofocus and
         * whether logically is should be enabled during AF even though all input widgets are disabled
         */
        bool isFocusSubFrameEnabled();

        /**
         * @brief Save the focus frame for later dubugging
         */
        void saveFocusFrame();

        /**
         * @brief Initialise donut processing
         */
        void initDonutProcessing();

        /**
         * @brief Setup Linear Focuser
         * @param initialPosition of the focuser
         */
        void setupLinearFocuser(int initialPosition);

        /**
         * @brief Initialise the Scan Start Position algorithm
         * @param force Scan Start Pos
         * @param startPosition
         * @return whether Scan for Start Position was initiated
         */
        bool initScanStartPos(const bool force, const int initialPosition);

        /**
         * @brief Process the scan for the Autofocus starting position
         */
        void scanStartPos();

        /**
         * @brief Reset donut processing
         */
        void resetDonutProcessing();

        /**
         * @brief Adjust Autofocus capture exposure based on user settings when using Donut Buster
         */
        void donutTimeDilation();

        /**
         * @brief Filter out duplicate Autofocus requests
         * @param autofocusReason reason for running this Autofocus
         * @return true means do not run AF (previous run only just completed)
         */
        bool checkAFOptimisation(const AutofocusReason autofocusReason);

        /// Focuser device needed for focus operation
        ISD::Focuser *m_Focuser { nullptr };
        int m_focuserId;
        /// CCD device needed for focus operation
        ISD::Camera *m_Camera { nullptr };
        /// Optional device filter
        ISD::FilterWheel *m_FilterWheel { nullptr };
        /// Optional temperature source element
        INumber *currentTemperatureSourceElement {nullptr};

        /// Current filter position
        int currentFilterPosition { -1 };
        int fallbackFilterPosition { -1 };
        /// True if we need to change filter position and wait for result before continuing capture
        bool filterPositionPending { false };
        bool fallbackFilterPending { false };

        /// They're generic GDInterface because they could be either ISD::Camera or ISD::FilterWheel or ISD::Weather
        QList<QSharedPointer<ISD::GenericDevice>> m_TemperatureSources;

        /// Last Focus direction. Used by Iterative and Polynomial. NOTE: this does not take account of overscan
        /// so, e.g. an outward move will always by FOCUS_OUT even though overscan will move back in
        Direction m_LastFocusDirection { FOCUS_NONE };
        /// Keep track of the last requested steps
        uint32_t m_LastFocusSteps {0};
        /// What type of focusing are we doing right now?
        Type m_FocusType { FOCUS_MANUAL };
        /// Focus HFR & Centeroid algorithms
        StarAlgorithm m_FocusDetection { ALGORITHM_SEP };
        /// Focus Process Algorithm
        Algorithm m_FocusAlgorithm { FOCUS_LINEAR1PASS };
        /// Curve fit
        CurveFitting::CurveFit m_CurveFit { CurveFitting::FOCUS_HYPERBOLA };
        /// Star measure to use
        StarMeasure m_StarMeasure { FOCUS_STAR_HFR };
        /// PSF to use
        StarPSF m_StarPSF { FOCUS_STAR_GAUSSIAN };
        /// Units to use when displaying HFR or FWHM
        StarUnits m_StarUnits { FOCUS_UNITS_PIXEL };
        /// Units to use when displaying HFR or FWHM
        FocusWalk m_FocusWalk { FOCUS_WALK_FIXED_STEPS };
        /// Are we minimising or maximising?
        CurveFitting::OptimisationDirection m_OptDir { CurveFitting::OPTIMISATION_MINIMISE };
        /// The type of statistics to use
        Mathematics::RobustStatistics::ScaleCalculation m_ScaleCalc { Mathematics::RobustStatistics::SCALE_SESTIMATOR };

        /******************************************
         * "Measure" variables, HFR, FWHM, numStars
         ******************************************/

        /// Current HFR value just fetched from FITS file
        double currentHFR { INVALID_STAR_MEASURE };
        double currentFWHM { INVALID_STAR_MEASURE };
        double currentNumStars { INVALID_STAR_MEASURE };
        double currentFourierPower { INVALID_STAR_MEASURE };
        double currentBlurriness { INVALID_STAR_MEASURE };
        double currentMeasure { INVALID_STAR_MEASURE };
        double currentWeight { 0 };
        /// Last HFR value recorded
        double lastHFR { 0 };
        /// If (currentHFR > deltaHFR) we start the autofocus process.
        double minimumRequiredHFR { INVALID_STAR_MEASURE };
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
        int currentPosition { 0 };
        /// Motion state of the absolute focuser
        IPState currentPositionState {IPS_IDLE};
        /// flag if position or state has changed (to avoid too much logging)
        bool logPositionAndState {true};
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
        /// Current image mask
        ImageMaskType m_currentImageMask = FOCUS_MASK_NONE;

        /****************************
         * Misc. variables
         ****************************/

        /// Are we tring to abort Autofocus?
        bool m_abortInProgress { false };
        /// Are we in the process of capturing an image?
        bool m_captureInProgress { false };
        /// Are we in the process of star detection?
        bool m_starDetectInProgress { false };
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
        bool inScanStartPos { false };
        //bool inSequenceFocus { false };
        /// Keep track of request to retry or abort an AutoFocus run after focus position has been reset
        /// RESTART_NONE = normal operation, no restart
        /// RESTART_NOW = restart the autofocus routine
        /// RESTART_ABORT = when autofocus has been tried MAXIMUM_RESET_ITERATIONS times, abort the routine
        typedef enum { RESTART_NONE = 0, RESTART_NOW, RESTART_ABORT } FocusRestartState;
        FocusRestartState m_RestartState { RESTART_NONE };
        /// Did we reverse direction?
        bool reverseDir { false };
        /// Did the user or the auto selection process finish selecting our focus star?
        bool starSelected { false };
        /// Adjust the focus position to a target value
        bool inAdjustFocus { false };
        /// Build offsets is a special case of the Autofocus run
        bool inBuildOffsets { false };
        /// AF Optimise is a special case of the Autofocus run
        bool inAFOptimise { false };
        // Target frame dimensions
        //int fx,fy,fw,fh;
        /// If HFR=-1 which means no stars detected, we need to decide how many times should the re-capture process take place before we give up or reverse direction.
        int noStarCount { 0 };
        /// Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
        ISD::Camera::UploadMode rememberUploadMode { ISD::Camera::UPLOAD_CLIENT };
        /// Star measure (e.g. HFR, FWHM, etc) values for captured frames before averages
        QVector<double> starMeasureFrames;
        // Camera Fast Exposure
        bool m_RememberCameraFastExposure = { false };
        // Future Watch
        QFutureWatcher<bool> m_StarFinderWatcher;
        // R2 as a measure of how well the curve fits the datapoints. Passed to the V-curve graph for display
        double R2 = 0;
        // Counter to retry the auto focus run if the R2Limit has not been reached
        int m_R2Retries = 0;
        // Counter to retry starting focus operation (autofocus, adjust focus, etc) if the focuser is still active
        int m_StartRetries = 0;
        // Reason code for the Autofocus run - passed to Analyze
        AutofocusReason m_AutofocusReason = AutofocusReason::FOCUS_NONE;
        // Extra information about m_AutofocusReason
        QString m_AutofocusReasonInfo;
        // Autofocus run number - to help with debugging logs
        int m_AFRun = 0;
        // Rerun flag indicating a rerun due to AF failing
        bool m_AFRerun = false;

        ITextVectorProperty *filterName { nullptr };
        INumberVectorProperty *filterSlot { nullptr };

        // Holds the superset of text values in combo-boxes that can have restricted options
        QStringList m_StarMeasureText;
        QStringList m_CurveFitText;
        QStringList m_FocusWalkText;

        // Holds the enabled state of widgets that is used to active functionality in focus
        // during Autofocus when the input interface is disabled
        bool m_FocusGainAFEnabled { false };
        bool m_FocusISOAFEnabled { false };
        bool m_FocusSubFrameAFEnabled { false };

        /****************************
         * Plot variables
         ****************************/

        /// Plot minimum positions
        double minPos { 1e6 };
        /// Plot maximum positions
        double maxPos { 0 };
        /// V curve plot points
        QVector<double> plot_position, plot_value, plot_weight;
        QVector<bool> plot_outlier;
        bool isVShapeSolution = false;

        /// State
        FocusState m_state { Ekos::FOCUS_IDLE };
        FocusState m_pendingState { Ekos::FOCUS_IDLE };
        FocusState state() const
        {
            return m_state;
        }
        void setState(FocusState newState, const bool update = true);

        bool isBusy()
        {
            return m_state == FOCUS_WAITING || m_state == FOCUS_PROGRESS || m_state == FOCUS_FRAMING
                   || m_state == FOCUS_CHANGING_FILTER;
        }

        /// CCD Chip frame settings
        QMap<ISD::CameraChip *, QVariantMap> frameSettings;

        /// Selected star coordinates
        QVector3D starCenter;

        // Remember last star center coordinates in case of timeout in manual select mode
        QVector3D rememberStarCenter;

        /// Focus Frame
        QSharedPointer<FITSView> m_FocusView;

        /// Star Select Timer
        QTimer waitStarSelectTimer;

        /// FITS Viewer in case user want to display in it instead of internal view
        QSharedPointer<FITSViewer> fv;

        /// Track star position and HFR to know if we're detecting bogus stars due to detection algorithm false positive results
        QVector<QVector3D> starsHFR;

        /// Relative Profile
        FocusProfilePlot *profilePlot { nullptr };
        QDialog *profileDialog { nullptr };

        /// Polynomial fitting.
        std::unique_ptr<PolynomialFit> polynomialFit;

        // Curve fitting for focuser movement.
        std::unique_ptr<CurveFitting> curveFitting;

        // Curve fitting for stars.
        std::unique_ptr<CurveFitting> starFitting;

        // FWHM processing.
        std::unique_ptr<FocusFWHM> focusFWHM;

        // Fourier Transform power processing.
        std::unique_ptr<FocusFourierPower> focusFourierPower;

#if defined(HAVE_OPENCV)
        // Blurriness processing.
        std::unique_ptr<FocusBlurriness> focusBlurriness;
#endif

        // Adaptive Focus processing.
        std::unique_ptr<AdaptiveFocus> adaptFocus;

        // Focus Advisor processing.
        std::unique_ptr<FocusAdvisor> focusAdvisor;

        // Capture timers
        QTimer captureTimer;
        QTimer captureTimeout;
        uint8_t captureTimeoutCounter { 0 };
        uint8_t m_MissingCameraCounter { 0 };
        uint8_t captureFailureCounter { 0 };

        // Gain Control
        double GainSpinSpecialValue { INVALID_VALUE };

        // Focus motion timer.
        QTimer m_FocusMotionTimer;
        uint8_t m_FocusMotionTimerCounter { 0 };

        // Focuser reconnect counter
        uint8_t m_FocuserReconnectCounter { 0 };

        // Set m_DebugFocuser = true to simulate a focuser failure
        bool m_DebugFocuser { false };
        uint16_t m_DebugFocuserCounter { 0 };

        // Guide Suspend
        bool m_GuidingSuspended { false };

        // Data
        QSharedPointer<FITSData> m_ImageData;

        // Linear focuser.
        std::unique_ptr<FocusAlgorithmInterface> linearFocuser;
        int focuserAdditionalMovement { 0 };
        bool focuserAdditionalMovementUpdateDir { true };
        int linearRequestedPosition { 0 };

        bool hasDeviation { false };

        //double observatoryTemperature { INVALID_VALUE };
        double m_LastSourceAutofocusTemperature { INVALID_VALUE };
        QSharedPointer<ISD::GenericDevice> m_LastSourceDeviceAutofocusTemperature;
        //TemperatureSource lastFocusTemperatureSource { NO_TEMPERATURE };
        double m_LastSourceAutofocusAlt { INVALID_VALUE };

        // Mount altitude value for logging
        double mountAlt { INVALID_VALUE };

        static constexpr uint8_t MAXIMUM_FLUCTUATIONS {10};

        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;

        // Dark Processor
        QPointer<DarkProcessor> m_DarkProcessor;

        QSharedPointer<FilterManager> m_FilterManager;

        // Maintain a list of disabled widgets when Autofocus is running that can be restored at the end of the run
        QVector <QWidget *> disabledWidgets;

        // Scope parameters of the active optical train
        double m_Aperture = 0.0f;
        double m_FocalLength = 0.0f;
        double m_FocalRatio = 0.0f;
        double m_Reducer = 0.0f;
        double m_CcdPixelSizeX = 0.0f;
        int m_CcdWidth = 0;
        int m_CcdHeight = 0;
        QString m_ScopeType;

        QString m_opsDialogName;

        // Settings popup
        //std::unique_ptr<Ui::Settings> m_SettingsUI;
        //QPointer<QDialog> m_SettingsDialog;
        OpsFocusSettings *m_OpsFocusSettings { nullptr };

        // Process popup
        //std::unique_ptr<Ui::Process> m_ProcessUI;
        //QPointer<QDialog> m_ProcessDialog;
        OpsFocusProcess *m_OpsFocusProcess { nullptr };

        // Mechanics popup
        //std::unique_ptr<Ui::Mechanics> m_MechanicsUI;
        //QPointer<QDialog> m_MechanicsDialog;
        OpsFocusMechanics *m_OpsFocusMechanics { nullptr };

        // CFZ popup
        std::unique_ptr<Ui::focusCFZDialog> m_CFZUI;
        QPointer<QDialog> m_CFZDialog;

        // CFZ
        double m_cfzSteps = 0.0f;

        // Aberration Inspector
        void calculateAbInsData();
        bool m_abInsOn = false;
        int m_abInsRun = 0;
        QVector<int> m_abInsPosition;
        QVector<QVector<double>> m_abInsMeasure;
        QVector<QVector<double>> m_abInsWeight;
        QVector<QVector<int>> m_abInsNumStars;
        QVector<QPoint> m_abInsTileCenterOffset;

        QTimer m_DebounceTimer;

        // Donut Buster
        double m_donutOrigExposure = 0.0;
        QVector<int> m_scanPosition;
        QVector<double> m_scanMeasure;
        QString m_AFfilter = NULL_FILTER;
};

}
