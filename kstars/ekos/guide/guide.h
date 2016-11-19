/*  Ekos guide tool
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef guide_H
#define guide_H

#include <QTimer>
#include <QtDBus/QtDBus>

#include "ekos/ekos.h"

#include "indi/indistd.h"
#include "indi/inditelescope.h"
#include "indi/indiccd.h"

#include "guide.h"


#include "fitsviewer/fitscommon.h"

#include "ui_guide.h"

class QTabWidget;
class FITSView;
class ScrollGraph;
class QProgressIndicator;

namespace Ekos
{

class GuideInterface;
class OpsCalibration;
class OpsGuide;
class InternalGuider;
class PHD2;
class LinGuider;

/**
 *@class Guide
 *@short Performs calibration and autoguiding using an ST4 port or directly via the INDI driver. Can be used with the following external guiding applications:
 * PHD2
 * LinGuider
 *@author Jasem Mutlaq
 *@version 1.4
 */
class Guide : public QWidget, public Ui::Guide
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Guide")

public:
    Guide();
    ~Guide();

    enum GuiderStage { CALIBRATION_STAGE, GUIDE_STAGE };
    enum GuiderType { GUIDE_INTERNAL, GUIDE_PHD2, GUIDE_LINGUIDER };

    /** @defgroup GuideDBusInterface Ekos DBus Interface - Capture Module
     * Ekos::Guide interface provides advanced scripting capabilities to calibrate and guide a mount via a CCD camera.
    */

    /*@{*/

    /** DBUS interface function.
     * select the CCD device from the available CCD drivers.
     * @param device The CCD device name
     * @return Returns true if CCD device is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /** DBUS interface function.
     * select the ST4 device from the available ST4 drivers.
     * @param device The ST4 device name
     * @return Returns true if ST4 device is found and set, false otherwise.
     */
    Q_SCRIPTABLE bool setST4(QString device);

    /** DBUS interface function.
     * @return Returns List of registered ST4 devices.
     */
    Q_SCRIPTABLE QStringList getST4Devices();

    /**
     * @brief getStatus Return guide module status
     * @return state of guide module from Ekos::GuideState
     */
    Q_SCRIPTABLE uint getStatus() { return state;}

    /** DBUS interface function.
     * @return Returns guiding deviation from guide star in arcsecs. First elemenet is RA guiding deviation, second element is DEC guiding deviation.
     */
    Q_SCRIPTABLE QList<double> getGuidingDeviation();

    /** DBUS interface function.
     * Set CCD exposure value
     * @value exposure value in seconds.
     */
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);

    /** DBUS interface function.
     * Set image filter to apply to the image after capture.
     * @param value Image filter (Auto Stretch, High Contrast, Equalize, High Pass)
     */
    Q_SCRIPTABLE Q_NOREPLY void setImageFilter(const QString & value);

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
     * Set rapid guiding option. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, it will activate RapidGuide in the CCD driver. When Rapid Guide is used, no frames are sent to Ekos for analysis and the centeroid calculations are done in the CCD driver.
     */
    //Q_SCRIPTABLE Q_NOREPLY void setGuideRapidEnabled(bool enable);

    /** DBUS interface function.
     * Enable or disables dithering. Set dither range
     * @param enable if true, dithering is enabled and is performed after each exposure is complete. Otheriese, dithering is disabled.
     * @param value dithering range in pixels. Ekos will move the guide star in a random direction for the specified dithering value in pixels.
     */
    Q_SCRIPTABLE Q_NOREPLY void setDitherSettings(bool enable, double value);

    /** @}*/

    void addCCD(ISD::GDInterface *newCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addST4(ISD::ST4 *setST4);
    void setAO(ISD::ST4 *newAO);

    bool isDithering();

    void addGuideHead(ISD::GDInterface *newCCD);
    void syncTelescopeInfo();
    void syncCCDInfo();

    /**
     * @brief clearLog As the name suggests
     */
    void clearLog();    

    /**
     * @return Return curent log text of guide module
     */
    QString getLogText() { return logText.join("\n"); }

    /**
     * @brief getStarPosition Return star center as selected by the user or auto-detected by KStars
     * @return QVector3D of starCenter. The 3rd parameter is used to store current bin settings and in unrelated to the star position.
     */
    QVector3D getStarPosition() { return starCenter; }

    // Tracking Box
    int getTrackingBoxSize() { return boxSizeCombo->currentText().toInt(); }

    //void startRapidGuide();
    //void stopRapidGuide();

    GuideInterface * getGuider() { return guider;}

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
     * Start the calibration operation.
     * @return Returns true if calibration started successfully, false otherwise.
     */
     Q_SCRIPTABLE bool calibrate();

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
     * Attempts to automatically select a star from the current guide frame
     * @return Returns true if a star is selected successfully, false otherwise
     */
     Q_SCRIPTABLE bool selectAutoStar();

    /** DBUS interface function.
     * Set guiding options. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, it will select a subframe around the guide star depending on the boxSize size.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSubFrameEnabled(bool enable);

    /** DBUS interface function.
     * @brief startAutoCalibrateGuide Start calibration with auto star selected followed immediately by guiding.
     */
    Q_SCRIPTABLE Q_NOREPLY void startAutoCalibrateGuide();

    /** DBUS interface function.
     * Selects which guiding process to utilize for calibration & guiding.
     * @param type Type of guider process to use. 0 for internal guider, 1 for external PHD2, 2 for external lin_guider. Pass -1 to select default guider in options.
     * @return True if guiding is switched to the new requested type. False otherwise.
     */
    Q_SCRIPTABLE bool setGuiderType(int type);

    /**
      * @brief checkCCD Check all CCD parameters and ensure all variables are updated to reflect the selected CCD
      * @param ccdNum CCD index number in the CCD selection combo box
      */
     void checkCCD(int ccdNum=-1);

     /**
      * @brief checkExposureValue This function is called by the INDI framework whenever there is a new exposure value. We use it to know if there is a problem with the exposure
      * @param targetChip Chip for which the exposure is undergoing
      * @param exposure numbers of seconds left in the exposure
      * @param state State of the exposure property
      */
     void checkExposureValue(ISD::CCDChip *targetChip, double exposure, IPState expState);

     /**
      * @brief newFITS is called by the INDI framework whenever there is a new BLOB arriving
      */
     void newFITS(IBLOB*);

     /**
      * @brief setST4 Sets a new ST4 device from the combobox index
      * @param index Index of selected ST4 in the combobox
      */
     void setST4(int index);     

     /**
      * @brief processRapidStarData is called by INDI framework when we receive new Rapid Guide data
      * @param targetChip target Chip for which rapid guide is enabled
      * @param dx Deviation in X
      * @param dy Deviation in Y
      * @param fit fitting score
      */
     //void processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit);

     // Append Log entry
     void appendLogText(const QString &);

     // Update Guide module status
     void setStatus(Ekos::GuideState newState);

     // Update Capture Module status
     void setCaptureStatus(Ekos::CaptureState newState);
     // Update Mount module status
     void setMountStatus(ISD::Telescope::TelescopeStatus newState);

     // Star Position
     void setStarPosition(const QVector3D &newCenter, bool updateNow);

     // Capture
     void setCaptureComplete();

     // Send pulse to ST4 driver
     bool sendPulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
     bool sendPulse( GuideDirection dir, int msecs );

     /**
      * @brief setDECSwap Change ST4 declination pulse direction. +DEC pulses increase DEC if swap is OFF. When on +DEC pulses result in decreasing DEC.
      * @param enable True to enable DEC swap. Off to disable it.
      */
     void setDECSwap(bool enable);

protected slots:
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

     // Display guide information when hovering over the drift graph
     void driftMouseOverLine(QMouseEvent *event);

     // Reset graph if right clicked
     void driftMouseClicked(QMouseEvent *event);

     //void onXscaleChanged( int i );
     //void onYscaleChanged( int i );
     void onThresholdChanged( int i );
     void onInfoRateChanged( double val );
     void onEnableDirRA( bool enable );
     void onEnableDirDEC( bool enable );
     void onInputParamChanged();

     //void onRapidGuideChanged(bool enable);

     void setAxisDelta(double ra, double de);
     void setAxisSigma(double ra, double de);
     void setAxisPulse(double ra, double de);

     void processGuideOptions();

     void onControlDirectionChanged(bool enable);

     void showFITSViewer();

signals:
    void newLog();
    void newStatus(Ekos::GuideState status);

    void newStarPixmap(QPixmap &);
    void newProfilePixmap(QPixmap &);

    void newAxisDelta(double delta_ra, double delta_dec);
    void guideChipUpdated(ISD::CCDChip*);

private:
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

    // Operation stack
    void buildOperationStack(GuideState operation);
    bool executeOperationStack();
    bool executeOneOperation(GuideState operation);

    bool captureOneFrame();

    void refreshColorScheme();

    // Operation Stack
    QStack<GuideState> operationStack;

    // Devices
    ISD::CCD *currentCCD;
    ISD::Telescope *currentTelescope;
    ISD::ST4* ST4Driver;
    ISD::ST4* AODriver;
    ISD::ST4* GuideDriver;

    // Device Containers
    QList<ISD::ST4*> ST4List;
    QList<ISD::CCD *> CCDs;

    // Guider process
    GuideInterface *guider;
    GuiderType guiderType;

    // Star
    QVector3D starCenter;

    // Guide Params
    double ccdPixelSizeX, ccdPixelSizeY, mountAperture, mountFocalLength, guideDeviationRA, guideDeviationDEC, pixScaleX, pixScaleY;

    // Rapid Guide
    //bool rapidGuideReticleSet;

    // State
    GuideState state;

    // Guide timer
    QTime guideTimer;

    // Pulse Timer
    QTimer pulseTimer;

    // Log
    QStringList logText;

    // Misc
    bool useGuideHead;

    // Progress Activity Indicator
    QProgressIndicator *pi;

    // Options
    OpsCalibration *opsCalibration;
    OpsGuide *opsGuide;

    // Guide Frame
    FITSView *guideView;

    // Auto star operation
    bool autoStarCaptured;

    // Calibration done already?
    bool calibrationComplete = false;

    // Was the modified frame subFramed?
    bool subFramed;       

    // CCD Chip frame settings
    QMap<ISD::CCDChip *, QVariantMap> frameSettings;

    // Profile Pixmap
    QPixmap profilePixmap;

    // Flag to start auto calibration followed immediately by guiding
    bool autoCalibrateGuide;

    // Pointers of guider processes
    QPointer<InternalGuider> internalGuider;
    QPointer<PHD2> phd2Guider;
    QPointer<LinGuider> linGuider;
    QPointer<FITSViewer> fv;
};

}

#endif  // guide_H
