/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_guide.h"
#include "guideinterface.h"
#include "guidestatewidget.h"
#include "ekos/ekos.h"
#include "indi/indiccd.h"
#include "indi/inditelescope.h"

#include <QTime>
#include <QTimer>
#include <QtDBus>

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
        Q_PROPERTY(QString camera READ camera WRITE setCamera)
        Q_PROPERTY(QString st4 READ st4 WRITE setST4)
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
        Q_SCRIPTABLE bool setCamera(const QString &device);
        Q_SCRIPTABLE QString camera();

        /** DBUS interface function.
             * select the ST4 device from the available ST4 drivers.
             * @param device The ST4 device name
             * @return Returns true if ST4 device is found and set, false otherwise.
             */
        Q_SCRIPTABLE bool setST4(const QString &device);
        Q_SCRIPTABLE QString st4();

        /** DBUS interface function.
             * @return Returns List of registered ST4 devices.
             */
        Q_SCRIPTABLE QStringList getST4Devices();

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
            return state;
        }

        /** DBUS interface function.
             * Set CCD exposure value
             * @param value exposure value in seconds.
             */
        Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
        double exposure()
        {
            return exposureIN->value();
        }

        /** DBUS interface function.
             * Set image filter to apply to the image after capture.
             * @param value Image filter (Auto Stretch, High Contrast, Equalize, High Pass)
             */
        Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString &value);

        /** DBUS interface function.
             * Set calibration Use Two Axis option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, calibration will be performed in both RA and DEC axis. Otherwise, only RA axis will be calibrated.
             */
        Q_SCRIPTABLE Q_NOREPLY void setCalibrationTwoAxis(bool enable);

        /** DBUS interface function.
             * Set auto star calibration option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, Ekos will attempt to automatically select the best guide star and proceed with the calibration procedure.
             */
        Q_SCRIPTABLE Q_NOREPLY void setCalibrationAutoStar(bool enable);

        /** DBUS interface function.
             * In case of automatic star selection, calculate the appropriate square size given the selected star width. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, Ekos will attempt to automatically select the best square size for calibration and guiding phases.
             */
        Q_SCRIPTABLE Q_NOREPLY void setCalibrationAutoSquareSize(bool enable);

        /** DBUS interface function.
             * Set calibration dark frame option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
             * @param enable if true, a dark frame will be captured to subtract from the light frame.
             */
        Q_SCRIPTABLE Q_NOREPLY void setDarkFrameEnabled(bool enable);

        /** DBUS interface function.
             * Set calibration parameters.
             * @param pulseDuration Pulse duration in milliseconds to use in the calibration steps.
             */
        Q_SCRIPTABLE Q_NOREPLY void setCalibrationPulseDuration(int pulseDuration);

        /** DBUS interface function.
             * Set guiding box size. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
             * @param index box size index (0 to 4) for box size from 8 to 128 pixels. The box size should be suitable for the size of the guide star selected. The boxSize is also used to select the subframe size around the guide star. Default is 16 pixels
             */
        Q_SCRIPTABLE Q_NOREPLY void setGuideBoxSizeIndex(int index);

        /** DBUS interface function.
             * Set guiding algorithm. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
             * @param index Select the algorithm used to calculate the centroid of the guide star (0 --> Smart, 1 --> Fast, 2 --> Auto, 3 --> No thresh).
             */
        Q_SCRIPTABLE Q_NOREPLY void setGuideAlgorithmIndex(int index);

        /** DBUS interface function.
             * Enable or disables dithering. Set dither range
             * @param enable if true, dithering is enabled and is performed after each exposure is complete. Otherwise, dithering is disabled.
             * @param value dithering range in pixels. Ekos will move the guide star in a random direction for the specified dithering value in pixels.
             */
        Q_SCRIPTABLE Q_NOREPLY void setDitherSettings(bool enable, double value);

        /** @}*/

        void addCamera(ISD::GDInterface *newCCD);
        void configurePHD2Camera();
        void setTelescope(ISD::GDInterface *newTelescope);
        void addST4(ISD::ST4 *setST4);
        void setAO(ISD::ST4 *newAO);
        void removeDevice(ISD::GDInterface *device);

        bool isDithering();

        void addGuideHead(ISD::GDInterface *newCCD);
        void syncTelescopeInfo();
        void syncCCDInfo();

        /**
             * @brief clearLog As the name suggests
             */
        void clearLog();
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
            return boxSizeCombo->currentText().toInt();
        }

        GuideInterface *getGuider()
        {
            return guider;
        }

        QJsonObject getSettings() const;
        void setSettings(const QJsonObject &settings);

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
              * @brief checkCCD Check all CCD parameters and ensure all variables are updated to reflect the selected CCD
              * @param ccdNum CCD index number in the CCD selection combo box
              */
        void checkCCD(int ccdNum = -1);

        /**
             * @brief checkExposureValue This function is called by the INDI framework whenever there is a new exposure value. We use it to know if there is a problem with the exposure
             * @param targetChip Chip for which the exposure is undergoing
             * @param exposure numbers of seconds left in the exposure
             * @param expState State of the exposure property
             */
        void checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState expState);

        /**
             * @brief newFITS is called by the INDI framework whenever there is a new BLOB arriving
             */
        void processData(const QSharedPointer<FITSData> &data);

        /**
             * @brief setST4 Sets a new ST4 device from the combobox index
             * @param index Index of selected ST4 in the combobox
             */
        void setST4(int index);

        /**
             * @brief Set telescope and guide scope info. All measurements is in millimeters.
             * @param primaryFocalLength Primary Telescope Focal Length. Set to 0 to skip setting this value.
             * @param primaryAperture Primary Telescope Aperture. Set to 0 to skip setting this value.
             * @param guideFocalLength Guide Telescope Focal Length. Set to 0 to skip setting this value.
             * @param guideAperture Guide Telescope Aperture. Set to 0 to skip setting this value.
             */
        void setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture);

        // This Function will allow PHD2 to update the exposure values to the recommended ones.
        QString setRecommendedExposureValues(QList<double> values);

        // Append Log entry
        void appendLogText(const QString &);

        // Update Guide module status
        void setStatus(Ekos::GuideState newState);

        // Update Capture Module status
        void setCaptureStatus(Ekos::CaptureState newState);
        // Update Mount module status
        void setMountStatus(ISD::Telescope::Status newState);
        void setMountCoords(const QString &ra, const QString &dec, const QString &az, const QString &alt, int pierSide,
                            const QString &ha);

        // Update Pier Side
        void setPierSide(ISD::Telescope::PierSide newSide);

        // Star Position
        void setStarPosition(const QVector3D &newCenter, bool updateNow);

        // Capture
        void setCaptureComplete();

        // Send pulse to ST4 driver
        bool sendPulse(GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs);
        bool sendPulse(GuideDirection dir, int msecs);

        /**
             * @brief setDECSwap Change ST4 declination pulse direction. +DEC pulses increase DEC if swap is OFF. When on +DEC pulses result in decreasing DEC.
             * @param enable True to enable DEC swap. Off to disable it.
             */
        void setDECSwap(bool enable);

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

    protected slots:
        void updateTelescopeType(int index);
        void updateCCDBin(int index);

        /**
                * @brief processCCDNumber Process number properties arriving from CCD. Currently, binning changes are processed.
                * @param nvp pointer to number property.
                */
        void processCCDNumber(INumberVectorProperty *nvp);

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
        void onEnableDirRA(bool enable);
        void onEnableDirDEC(bool enable);
        void syncSettings();

        void setAxisDelta(double ra, double de);
        void setAxisSigma(double ra, double de);
        void setAxisPulse(double ra, double de);
        void setSNR(double snr);
        void calibrationUpdate(GuideInterface::CalibrationUpdateType type, const QString &message = QString(""), double dx = 0,
                               double dy = 0);

        void processGuideOptions();
        void configSEPMultistarOptions();

        void onControlDirectionChanged(bool enable);
        void updatePHD2Directions();

        void showFITSViewer();

        void processCaptureTimeout();

        void nonGuidedDither();

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::GuideState status);

        void newImage(FITSView *view);
        void newStarPixmap(QPixmap &);

        // Immediate deviations in arcsecs
        void newAxisDelta(double ra, double de);
        // Sigma deviations in arcsecs RMS
        void newAxisSigma(double ra, double de);

        void guideStats(double raError, double decError, int raPulse, int decPulse,
                        double snr, double skyBg, int numStars);

        void guideChipUpdated(ISD::CCDChip *);
        void settingsUpdated(const QJsonObject &settings);
        void driverTimedout(const QString &deviceName);

    private slots:
        void setDefaultST4(const QString &driver);
        void setDefaultCCD(const QString &ccd);

    private:

        void resizeEvent(QResizeEvent *event) override;

        /**
             * @brief updateGuideParams Update the guider and frame parameters due to any changes in the mount and/or ccd frame
             */
        void updateGuideParams();

        /**
             * @brief syncTrackingBoxPosition Sync the tracking box to the current selected star center
             */
        void syncTrackingBoxPosition();

        /**
             * @brief loadSettings Loads and applies all settings from KStars options
             */
        void loadSettings();

        /**
             * @brief saveSettings Saves all current settings into KStars options
             */
        void saveSettings();

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
        void prepareCapture(ISD::CCDChip *targetChip);


        void handleManualDither();

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

        // Driver
        void reconnectDriver(const QString &camera, const QString &via);

        // Operation Stack
        QStack<GuideState> operationStack;

        // Devices
        ISD::CCD *currentCCD { nullptr };
        QString lastPHD2CameraName; //This is for the configure PHD2 camera method.
        ISD::Telescope *currentTelescope { nullptr };
        ISD::ST4 *ST4Driver { nullptr };
        ISD::ST4 *GuideDriver { nullptr };

        // Device Containers
        QList<ISD::ST4 *> ST4List;
        QList<ISD::CCD *> CCDs;

        // Guider process
        GuideInterface *guider { nullptr };
        GuiderType guiderType { GUIDE_INTERNAL };

        // Star
        QVector3D starCenter;

        // Guide Params
        //int guideBinIndex { 0 };
        int guideFilterIndex { 0 };
        double ccdPixelSizeX { -1 };
        double ccdPixelSizeY { -1 };
        double aperture { -1 };
        double focal_length { -1 };
        double guideDeviationRA { 0 };
        double guideDeviationDEC { 0 };
        double pixScaleX { -1 };
        double pixScaleY { -1 };

        // State
        GuideState state { GUIDE_IDLE };
        GuideStateWidget *guideStateWidget { nullptr };

        // Guide timer
        QTime guideTimer;

        // Capture timeout timer
        QTimer captureTimeout;
        uint8_t m_CaptureTimeoutCounter { 0 };
        uint8_t m_DeviceRestartCounter { 0 };

        // Pulse Timer
        QTimer pulseTimer;

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
        GuideView *guideView { nullptr };

        // Calibration done already?
        bool calibrationComplete { false };

        // Was the modified frame subFramed?
        bool subFramed { false };

        // CCD Chip frame settings
        QMap<ISD::CCDChip *, QVariantMap> frameSettings;

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
        QPointer<FITSViewer> fv;
        QSharedPointer<FITSData> m_ImageData;

        double primaryFL = -1, primaryAperture = -1, guideFL = -1, guideAperture = -1;
        ISD::Telescope::Status m_MountStatus { ISD::Telescope::MOUNT_IDLE };

        bool graphOnLatestPt = true;

        //This is for enforcing the PHD2 Star lock when Guide is pressed,
        //autostar is not selected, and the user has chosen a star.
        //This connection storage is so that the connection can be disconnected after enforcement
        QMetaObject::Connection guideConnect;

        QCPItemText *calLabel  { nullptr };

        // The scales of these zoom levels are defined in Guide::zoomX().
        static constexpr int defaultXZoomLevel = 3;
        int driftGraphZoomLevel {defaultXZoomLevel};


        // The accumulated non-guided dither offsets (in milliseconds) in the RA and DEC directions.
        int nonGuidedDitherRaOffsetMsec = 0, nonGuidedDitherDecOffsetMsec = 0;

        // Random generator for non guided dithering
        std::mt19937 nonGuidedPulseGenerator;

        // Flag to check if random generator for non guided dithering is initialized.
        bool isNonGuidedDitherInitialized = false;

        // Reset non guided dithering properties and initialize the random generator seed if not already done.
        // Should be called in Guide::Guide() for initial seed initialization, and then in setCaptureStatus to reset accumulated drift
        // every time a capture task is completed or aborted.
        void resetNonGuidedDither();
};
}
