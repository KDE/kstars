/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_guide.h"
#include "guideinterface.h"
#include "ekos/ekos.h"
#include "indi/indicamera.h"
#include "indi/indimount.h"

#include <QTime>
#include <QTimer>

#include <random>

class QProgressIndicator;
class QTabWidget;

class FITSView;
class FITSViewer;
class ScrollGraph;
class GuideView;

namespace Ekos
{
class OpsCalibration;
class OpsGuide;
class OpsDither;
class OpsGPG;
class InternalGuider;
class PHD2;
class LinGuider;
class GuideStateWidget;
class ManualPulse;
class DarkProcessor;

/**
 * @class Guide
 * @short Performs calibration and autoguiding using an ST4 port or directly via the INDI driver. Can be used with the following external guiding applications:
 * PHD2
 * LinGuider
 *
 * @author Jasem Mutlaq
 * @version 1.4
 */
class Guide : public QWidget, public Ui::Guide
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Guide")
        Q_PROPERTY(Ekos::GuideState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString opticalTrain READ opticalTrain WRITE setOpticalTrain)
        Q_PROPERTY(QString camera READ camera)
        Q_PROPERTY(QString guider READ guider)
        Q_PROPERTY(double exposure READ exposure WRITE setExposure)
        Q_PROPERTY(QList<double> axisDelta READ axisDelta NOTIFY newAxisDelta)
        Q_PROPERTY(QList<double> axisSigma READ axisSigma NOTIFY newAxisSigma)

    public:
        Guide();
        ~Guide();

        enum GuiderStage
        {
            CALIBRATION_STAGE,
            GUIDE_STAGE
        };
        enum GuiderType
        {
            GUIDE_INTERNAL,
            GUIDE_PHD2,
            GUIDE_LINGUIDER
        };

        /** @defgroup GuideDBusInterface Ekos DBus Interface - Capture Module
             * Ekos::Guide interface provides advanced scripting capabilities to calibrate and guide a mount via a CCD camera.
            */

        /*@{*/

        /** DBUS interface function.
             * select the CCD device from the available CCD drivers.
             * @param device The CCD device name
             * @return Returns true if CCD device is found and set, false otherwise.
             */
        Q_SCRIPTABLE QString camera();

        /** DBUS interface function.
             * select the ST4 device from the available ST4 drivers.
             * @param device The ST4 device name
             * @return Returns true if ST4 device is found and set, false otherwise.
             */
        Q_SCRIPTABLE QString guider();

        /** DBUS interface function.
         * @brief connectGuider Establish connection to guider application. For internal guider, this always returns true.
         * @return True if successfully connected, false otherwise.
         */
        Q_SCRIPTABLE bool connectGuider();

        /** DBUS interface function.
         * @brief disconnectGuider Disconnect from guider application. For internal guider, this always returns true.
         * @return True if successfully disconnected, false otherwise.
         */
        Q_SCRIPTABLE bool disconnectGuider();

        /**
             * @brief getStatus Return guide module status
             * @return state of guide module from Ekos::GuideState
             */
        Q_SCRIPTABLE Ekos::GuideState status()
        {
            return m_State;
        }

        /** DBUS interface function.
             * Set CCD exposure value
             * @param value exposure value in seconds.
             */
        Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
        double exposure()
        {
            return guideExposure->value();
        }

        /** DBUS interface function.
             * Set calibration dark frame option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, a dark frame will be captured to subtract from the light frame.
             */
        Q_SCRIPTABLE Q_NOREPLY void setDarkFrameEnabled(bool enable);        

        /** @}*/

        /**
         * @brief Add new Camera
         * @param device pointer to camera device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setCamera(ISD::Camera *device);


        /**
         * @brief Add new Mount
         * @param device pointer to Mount device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setMount(ISD::Mount *device);

        /**
         * @brief Add new Guider
         * @param device pointer to Guider device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setGuider(ISD::Guider *device);

        /**
         * @brief Add new Adaptive Optics
         * @param device pointer to AO device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setAdaptiveOptics(ISD::AdaptiveOptics *device);

        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);
        void configurePHD2Camera();

        bool isDithering();
        void syncTelescopeInfo();
        void syncCameraInfo();

        /**
             * @brief clearLog As the name suggests
             */
        Q_SCRIPTABLE void clearLog();
        QStringList logText()
        {
            return m_LogText;
        }

        /**
             * @return Return current log text of guide module
             */
        QString getLogText()
        {
            return m_LogText.join("\n");
        }

        /**
             * @brief getStarPosition Return star center as selected by the user or auto-detected by KStars
             * @return QVector3D of starCenter. The 3rd parameter is used to store current bin settings and in unrelated to the star position.
             */
        QVector3D getStarPosition()
        {
            return starCenter;
        }

        // Tracking Box
        int getTrackingBoxSize()
        {
            return guideSquareSize->currentText().toInt();
        }

        GuideInterface *getGuiderInstance()
        {
            return m_GuiderInstance;
        }

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);

    public slots:

        /** DBUS interface function.
             * Start the autoguiding operation.
             * @return Returns true if guiding started successfully, false otherwise.
             */
        Q_SCRIPTABLE bool guide();

        /** DBUS interface function.
             * Stop any active calibration, guiding, or dithering operation
             * @return Returns true if operation is stopped successfully, false otherwise.
             */
        Q_SCRIPTABLE bool abort();

        /** DBUS interface function.
             * Start the calibration operation. Note that this will not start guiding automatically.
             * @return Returns true if calibration started successfully, false otherwise.
             */
        Q_SCRIPTABLE bool calibrate();

        /** DBUS interface function.
             * Clear calibration data. Next time any guide operation is performed, a calibration is first started.
             */
        Q_SCRIPTABLE Q_NOREPLY void clearCalibration();

        /** DBUS interface function.
             * @brief dither Starts dithering process in a random direction restricted by the number of pixels specified in dither options
             * @return True if dither started successfully, false otherwise.
             */
        Q_SCRIPTABLE bool dither();

        /**
         * @brief Reset non guided dithering properties and initialize the random generator seed if not already done.
         *         Should be called in Guide::Guide() for initial seed initialization, and then every time to reset accumulated drift
         *        every time a capture task is completed or aborted.
         */
        void resetNonGuidedDither();

        /** DBUS interface function.
             * @brief suspend Suspend autoguiding
             * @return True if successful, false otherwise.
             */
        Q_SCRIPTABLE bool suspend();

        /** DBUS interface function.
             * @brief resume Resume autoguiding
             * @return True if successful, false otherwise.
             */
        Q_SCRIPTABLE bool resume();

        /** DBUS interface function.
             * Capture a guide frame
             * @return Returns true if capture command is sent successfully to INDI server.
             */
        Q_SCRIPTABLE bool capture();

        /** DBUS interface function.
             * Loop frames specified by the exposure control continuously until stopped.
             */
        Q_SCRIPTABLE Q_NOREPLY void loop();

        /** DBUS interface function.
             * Set guiding options. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, it will select a subframe around the guide star depending on the boxSize size.
             */
        Q_SCRIPTABLE Q_NOREPLY void setSubFrameEnabled(bool enable);

        /** DBUS interface function.
             * Set guiding options. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, it will select a subframe around the guide star depending on the boxSize size.
             */
        Q_SCRIPTABLE Q_NOREPLY void setAutoStarEnabled(bool enable);

        /** DBUS interface function.
             * Selects which guiding process to utilize for calibration & guiding.
             * @param type Type of guider process to use. 0 for internal guider, 1 for external PHD2, 2 for external lin_guider. Pass -1 to select default guider in options.
             * @return True if guiding is switched to the new requested type. False otherwise.
             */
        Q_SCRIPTABLE bool setGuiderType(int type);

        /** DBUS interface function.
         * @brief axisDelta returns the last immediate axis delta deviation in arcseconds. This is the deviation of locked star position when guiding started.
         * @return List of doubles. First member is RA deviation. Second member is DE deviation.
         */
        Q_SCRIPTABLE QList<double> axisDelta();

        /** DBUS interface function.
         * @brief axisSigma return axis sigma deviation in arcseconds RMS. This is the RMS deviation of locked star position when guiding started.
         * @return List of doubles. First member is RA deviation. Second member is DE deviation.
         */
        Q_SCRIPTABLE QList<double> axisSigma();

        /**
              * @brief checkCamera Check all CCD parameters and ensure all variables are updated to reflect the selected CCD
              * @param ccdNum CCD index number in the CCD selection combo box
              */
        void checkCamera();

        /**
             * @brief checkExposureValue This function is called by the INDI framework whenever there is a new exposure value. We use it to know if there is a problem with the exposure
             * @param targetChip Chip for which the exposure is undergoing
             * @param exposure numbers of seconds left in the exposure
             * @param expState State of the exposure property
             */
        void checkExposureValue(ISD::CameraChip *targetChip, double exposure, IPState expState);

        /**
             * @brief newFITS is called by the INDI framework whenever there is a new BLOB arriving
             */
        void processData(const QSharedPointer<FITSData> &data);

        // Aborts the current exposure, if one is ongoing.
        void abortExposure();

        // This Function will allow PHD2 to update the exposure values to the recommended ones.
        QString setRecommendedExposureValues(QList<double> values);

        // Append Log entry
        void appendLogText(const QString &);

        // Update Guide module status
        void setStatus(Ekos::GuideState newState);

        // Update Capture Module status
        void setCaptureStatus(Ekos::CaptureState newState);
        // Update Mount module status
        void setMountStatus(ISD::Mount::Status newState);
        void setMountCoords(const SkyPoint &position, ISD::Mount::PierSide pierSide, const dms &ha);

        // Update Pier Side
        void setPierSide(ISD::Mount::PierSide newSide);

        // Star Position
        void setStarPosition(const QVector3D &newCenter, bool updateNow);

        // Capture
        void setCaptureComplete();

        // Pulse both RA and DEC axes
        bool sendMultiPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs, CaptureAfterPulses followWithCapture);
        // Pulse for one of the mount axes
        bool sendSinglePulse(GuideDirection dir, int msecs, CaptureAfterPulses followWithCapture);

        /**
             * @brief setDECSwap Change ST4 declination pulse direction. +DEC pulses increase DEC if swap is OFF. When on +DEC pulses result in decreasing DEC.
             * @param enable True to enable DEC swap. Off to disable it.
             */
        void setDECSwap(bool enable);


        /**
         * @brief updateSetting Update per-train and global setting
         * @param key Name of setting
         * @param value Value
         * @note per-train and global settings are updated. Changes are saved to database
         * and to disk immediately.
         */
        void updateSetting(const QString &key, const QVariant &value);

        //plot slots
        void handleVerticalPlotSizeChange();
        void handleHorizontalPlotSizeChange();
        void clearGuideGraphs();
        void clearCalibrationGraphs();
        void slotAutoScaleGraphs();
        void buildTarget();
        void guideHistory();
        void setLatestGuidePoint(bool isChecked);

        void updateDirectionsFromPHD2(const QString &mode);

        void guideAfterMeridianFlip();

        // Trains
        QString opticalTrain() const
        {
            return opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrainCombo->setCurrentText(value);
        }

    protected slots:
        void updateCCDBin(int index);

        /**
                * @brief processCCDNumber Process number properties arriving from CCD. Currently, binning changes are processed.
                * @param nvp pointer to number property.
                */
        void updateProperty(INDI::Property prop);

        /**
             * @brief setTrackingStar Gets called when the user select a star in the guide frame
             * @param x X coordinate of star
             * @param y Y coordinate of star
             */
        void setTrackingStar(int x, int y);

        void saveDefaultGuideExposure();

        void updateTrackingBoxSize(int currentIndex);

        //void onXscaleChanged( int i );
        //void onYscaleChanged( int i );
        void onThresholdChanged(int i);
        void onEnableDirRA();
        void onEnableDirDEC();

        void setAxisDelta(double ra, double de);
        void setAxisSigma(double ra, double de);
        void setAxisPulse(double ra, double de);
        void setSNR(double snr);
        void calibrationUpdate(GuideInterface::CalibrationUpdateType type, const QString &message = QString(""), double dx = 0,
                               double dy = 0);

        void guideInfo(const QString &info);

        void processGuideOptions();
        void configSEPMultistarOptions();

        void onControlDirectionChanged();

        void showFITSViewer();

        void processCaptureTimeout();

        void nonGuidedDither();

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::GuideState status);

        void newImage(const QSharedPointer<FITSView> &view);
        void newStarPixmap(QPixmap &);

        void trainChanged();

        // Immediate deviations in arcsecs
        void newAxisDelta(double ra, double de);
        // Sigma deviations in arcsecs RMS
        void newAxisSigma(double ra, double de);

        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        void guideChipUpdated(ISD::CameraChip *);
        void settingsUpdated(const QVariantMap &settings);
        void driverTimedout(const QString &deviceName);

    private:

        void resizeEvent(QResizeEvent *event) override;

        /**
             * @brief updateGuideParams Update the guider and frame parameters due to any changes in the mount and/or ccd frame
             */
        void updateGuideParams();

        /**
         * @brief check if the guiding chip of the camera should be used (if present)
         */
        void checkUseGuideHead();

        /**
             * @brief syncTrackingBoxPosition Sync the tracking box to the current selected star center
             */
        void syncTrackingBoxPosition();   

        /**
             * @brief setBusy Indicate busy status within the module visually
             * @param enable True if module is busy, false otherwise
             */
        void setBusy(bool enable);

        /**
         * @brief setBLOBEnabled Enable or disable BLOB reception from current CCD if using external guider
         * @param enable True to enable BLOB reception, false to disable BLOB reception
         * @param name CCD to enable to disable. If empty (default), then action is applied to all CCDs.
         */
        void setExternalGuiderBLOBEnabled(bool enable);

        /**
         * @brief prepareCapture Set common settings for capture for guide module
         * @param targetChip target Chip
         */
        void prepareCapture(ISD::CameraChip *targetChip);


        void handleManualDither();

        ////////////////////////////////////////////////////////////////////
        /// Settings
        ////////////////////////////////////////////////////////////////////

        /**
         * @brief Connect GUI elements to sync settings once updated.
         */
        void connectSettings();
        /**
         * @brief Stop updating settings when GUI elements are updated.
         */
        void disconnectSettings();
        /**
         * @brief loadSettings Load setting from Options and set them accordingly.
         */
        void loadGlobalSettings();

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

        // Operation stack
        void buildOperationStack(GuideState operation);
        bool executeOperationStack();
        bool executeOneOperation(GuideState operation);

        // Init Functions
        void initPlots();
        void initDriftGraph();
        void initCalibrationPlot();
        void initView();
        void initConnections();

        bool captureOneFrame();

        void setupOpticalTrainManager();
        void refreshOpticalTrain();        

        // Driver
        void reconnectDriver(const QString &camera, QVariantMap settings);

        // Calibration plot of RA- and DEC-axis
        void drawRADECAxis(QCPItemText *Label, QCPItemLine *Arrow, const double dx, const double dy);


        // Operation Stack
        QStack<GuideState> operationStack;

        // Devices
        ISD::Camera *m_Camera { nullptr };
        ISD::Mount *m_Mount { nullptr };
        ISD::Guider *m_Guider { nullptr };
        ISD::AdaptiveOptics *m_AO { nullptr };

        // Guider process
        GuideInterface *m_GuiderInstance { nullptr };

        //This is for the configure PHD2 camera method.
        QString m_LastPHD2CameraName, m_LastPHD2MountName;
        GuiderType guiderType { GUIDE_INTERNAL };

        // Star
        QVector3D starCenter;

        // Guide Params
        int guideBinIndex { 0 };    // Selected or saved binning for guiding
        double ccdPixelSizeX { -1 };
        double ccdPixelSizeY { -1 };

        // Scope info
        double m_Aperture { -1 };
        double m_FocalLength { -1 };
        double m_FocalRatio { -1 };
        double m_Reducer {-1};

        double guideDeviationRA { 0 };
        double guideDeviationDEC { 0 };
        double pixScaleX { -1 };
        double pixScaleY { -1 };

        // State
        GuideState m_State { GUIDE_IDLE };
        GuideStateWidget *guideStateWidget { nullptr };

        // Guide timer
        QElapsedTimer guideTimer;

        // Debounce Timer
        QTimer m_DebounceTimer;

        // Capture timeout timer
        QTimer captureTimeout;
        uint8_t m_CaptureTimeoutCounter { 0 };
        uint8_t m_DeviceRestartCounter { 0 };

        // Pulse Timer
        QTimer m_PulseTimer;

        // Log
        QStringList m_LogText;

        // Misc
        bool useGuideHead { false };

        // Progress Activity Indicator
        QProgressIndicator *pi { nullptr };

        // Options
        OpsCalibration *opsCalibration { nullptr };
        OpsGuide *opsGuide { nullptr };
        OpsDither *opsDither { nullptr };
        OpsGPG *opsGPG { nullptr };

        // Guide Frame
        QSharedPointer<GuideView> m_GuideView;

        // Calibration done already?
        bool calibrationComplete { false };

        // Was the modified frame subFramed?
        bool subFramed { false };

        // Controls
        double guideGainSpecialValue {INVALID_VALUE};
        double TargetCustomGainValue {-1};

        // CCD Chip frame settings
        QMap<ISD::CameraChip *, QVariantMap> frameSettings;

        // Profile Pixmap
        QPixmap profilePixmap;
        // drift plot
        QPixmap driftPlotPixmap;

        // Flag to start auto calibration followed immediately by guiding
        //bool autoCalibrateGuide { false };

        // Pointers of guider processes
        QPointer<InternalGuider> internalGuider;
        QPointer<PHD2> phd2Guider;
        QPointer<LinGuider> linGuider;
        QSharedPointer<FITSViewer> fv;
        QSharedPointer<FITSData> m_ImageData;

        // Dark Processor
        QPointer<DarkProcessor> m_DarkProcessor;

        // Manual Pulse Dialog
        QPointer<ManualPulse> m_ManaulPulse;

        double primaryFL = -1, primaryAperture = -1, guideFL = -1, guideAperture = -1;
        ISD::Mount::Status m_MountStatus { ISD::Mount::MOUNT_IDLE };

        bool graphOnLatestPt = true;

        //This is for enforcing the PHD2 Star lock when Guide is pressed,
        //autostar is not selected, and the user has chosen a star.
        //This connection storage is so that the connection can be disconnected after enforcement
        QMetaObject::Connection guideConnect;

        // Some layer items
        QCPItemText *calLabel { nullptr }; // Title
        QCPItemText *calRALabel { nullptr }; // RA axis ...
        QCPItemLine *calRAArrow { nullptr }; // ... with direction
        QCPItemText *calDECLabel { nullptr }; // DEC axis ...
        QCPItemLine *calDECArrow { nullptr }; // ... with direction
        double calDecArrowStartX { 0 };
        double calDecArrowStartY { 0 };


        // The scales of these zoom levels are defined in Guide::zoomX().
        static constexpr int defaultXZoomLevel = 3;
        int driftGraphZoomLevel {defaultXZoomLevel};


        // The accumulated non-guided dither offsets (in milliseconds) in the RA and DEC directions.
        int nonGuidedDitherRaOffsetMsec = 0, nonGuidedDitherDecOffsetMsec = 0;

        // Random generator for non guided dithering
        std::mt19937 nonGuidedPulseGenerator;

        // Flag to check if random generator for non guided dithering is initialized.
        bool isNonGuidedDitherInitialized = false;

        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;
};
}
