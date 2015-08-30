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

#include "guide/common.h"
#include "guide.h"
#include "fitsviewer/fitscommon.h"
#include "indi/indistd.h"
#include "indi/inditelescope.h"
#include "indi/indiccd.h"
#include "ui_guide.h"

class QTabWidget;
class cgmath;
class rcalibration;
class rguider;
class FITSData;

namespace Ekos
{

/**
 *@class Guide
 *@short Performs calibration and autoguiding using an ST4 port or directly via the INDI driver.
 *@author Jasem Mutlaq
 *@version 1.0
 */
class Guide : public QWidget, public Ui::Guide
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Guide")

public:
    Guide();
    ~Guide();

    enum GuiderStage { CALIBRATION_STAGE, GUIDE_STAGE };

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

    /** DBUS interface function.
     * @return Returns true if calibration procedure is complete.
     */
    Q_SCRIPTABLE bool isCalibrationComplete();

    /** DBUS interface function.
     * @return Returns true if calibration procedure is successful.
     */
    Q_SCRIPTABLE bool isCalibrationSuccessful();

    /** DBUS interface function.
     * @return Returns true if autoguiding is in progress.
     */
    Q_SCRIPTABLE bool isGuiding();

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
     * Set auto calibration option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, Ekos will attempt to automatically select the best guide star and proceed with the calibration procedure.
     */
    Q_SCRIPTABLE Q_NOREPLY void setCalibrationAuto(bool enable);

    /** DBUS interface function.
     * Set calibration dark frame option. The options must be set before starting the calibration operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, a dark frame will be captured to subtract from the light frame.
     */
    Q_SCRIPTABLE Q_NOREPLY void setCalibrationDarkFrame(bool enable);

    /** DBUS interface function.
     * Set calibration parameters.
     * @param boxSize box size in pixels around the guide star. The box size should be suitable for the size of the guide star selected.
     * @param pulseDuration Pulse duration in milliseconds to use in the calibration steps.
     */
    Q_SCRIPTABLE Q_NOREPLY void setCalibrationParams(int boxSize, int pulseDuration);

    /** DBUS interface function.
     * Set guiding box size. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param boxSize box size in pixels around the guide star. The box size should be suitable for the size of the guide star selected. The boxSize is also used to select the subframe size around the guide star.
     */
    Q_SCRIPTABLE Q_NOREPLY void setGuideBoxSize(int boxSize);

    /** DBUS interface function.
     * Set guiding algorithm. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param algorithm Select the algorithm used to calculate the centroid of the guide star (Smart, Fast, Auto, No thresh).
     */
    Q_SCRIPTABLE Q_NOREPLY void setGuideAlgorithm(const QString & algorithm);

    /** DBUS interface function.
     * Set guiding options. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, it will select a subframe around the guide star depending on the boxSize size.
     */
    Q_SCRIPTABLE Q_NOREPLY void setGuideSubFrame(bool enable);

    /** DBUS interface function.
     * Set rapid guiding option. The options must be set before starting the guiding operation. If no options are set, the options loaded from the user configuration are used.
     * @param enable if true, it will activate RapidGuide in the CCD driver. When Rapid Guide is used, no frames are sent to Ekos for analysis and the centeroid calculations are done in the CCD driver.
     */
    Q_SCRIPTABLE Q_NOREPLY void setGuideRapid(bool enable);

    /** DBUS interface function.
     * Enable or disables dithering
     * @param enable if true, dithering is enabled and is performed after each exposure is complete. Otheriese, dithering is disabled.
     * @param value dithering range in pixels. Ekos will move the guide star in a random direction for the specified dithering value in pixels.
     */
    Q_SCRIPTABLE Q_NOREPLY void setDither(bool enable, double value);

    /** @}*/

    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryGuider);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addST4(ISD::ST4 *newST4);
    void setAO(ISD::ST4 *newAO);

    void addGuideHead(ISD::GDInterface *ccd);
    void syncTelescopeInfo();
    void syncCCDInfo();

    void appendLogText(const QString &);
    void clearLog();

    void setDECSwap(bool enable);    
    bool sendPulse( GuideDirection ra_dir, int ra_msecs, GuideDirection dec_dir, int dec_msecs );
    bool sendPulse( GuideDirection dir, int msecs );

    QString getLogText() { return logText.join("\n"); }

    double getReticleAngle();

    void startRapidGuide();
    void stopRapidGuide();

public slots:

    /** DBUS interface function.
     * Start the autoguiding operation.
     * @return Returns true if guiding started successfully, false otherwise.
     */
     Q_SCRIPTABLE bool startGuiding();

    /** DBUS interface function.
     * Stop the autoguiding operation.
     * @return Returns true if guiding stopped successfully, false otherwise.
     */
     Q_SCRIPTABLE bool stopGuiding();

    /** DBUS interface function.
     * Start the calibration operation.
     * @return Returns true if calibration started successfully, false otherwise.
     */
     Q_SCRIPTABLE bool startCalibration();

    /** DBUS interface function.
     * Stop the calibration operation.
     * @return Returns true if calibration stopped successfully, false otherwise.
     */
     Q_SCRIPTABLE bool stopCalibration();

    /** DBUS interface function.
     * Capture a guide frame
     * @return Returns true if capture command is sent successfully to INDI server.
     */
     Q_SCRIPTABLE bool capture();

     void checkCCD(int ccdNum=-1);
     void newFITS(IBLOB*);
     void newST4(int index);
     void processRapidStarData(ISD::CCDChip *targetChip, double dx, double dy, double fit);
     void setUseDarkFrame(bool enable) { useDarkFrame = enable;}
     void updateGuideDriver(double delta_ra, double delta_dec);

     // Auto Calibration Guiding (Cablirate first then start guiding immediately)
     void startAutoCalibrateGuiding();
     void checkAutoCalibrateGuiding(bool successful);

        void viewerClosed();

        void dither();
        void setSuspended(bool enable);

signals:
        void newLog();
        void guideReady();
        void newAxisDelta(double delta_ra, double delta_dec);
        void autoGuidingToggled(bool, bool);
        void ditherComplete();
        void ditherFailed();
        void ditherToggled(bool);
        void guideChipUpdated(ISD::CCDChip*);

private:
    void updateGuideParams();
    ISD::CCD *currentCCD;
    ISD::Telescope *currentTelescope;
    ISD::ST4* ST4Driver;
    ISD::ST4* AODriver;
    ISD::ST4* GuideDriver;

    QList<ISD::ST4*> ST4List;
    QList<ISD::CCD *> CCDs;

    QTabWidget *tabWidget;

    GuiderStage guiderStage;

    cgmath *pmath;
    rcalibration *calibration;
    rguider *guider;

    bool useGuideHead;
    bool isSuspended;

    bool useDarkFrame;
    double darkExposure;
    FITSData *darkImage;

    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, guideDeviationRA, guideDeviationDEC;
    bool rapidGuideReticleSet;

};

}

#endif  // guide_H
