/*  Ekos Polar Alignment Tool
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#ifndef ALIGN_H
#define ALIGN_H

#include <QProcess>
#include <QTime>
#include <QtDBus/QtDBus>

#include <config-kstars.h>

#include "capture.h"

#include "ui_align.h"

#include "indi/inditelescope.h"
#include "indi/indistd.h"

namespace Ekos
{

class AstrometryParser;
class OnlineAstrometryParser;
class OfflineAstrometryParser;

class Align : public QWidget, public Ui::Align
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")

public:
    Align();
    ~Align();

    typedef enum { AZ_INIT, AZ_FIRST_TARGET, AZ_SYNCING, AZ_SLEWING, AZ_SECOND_TARGET, AZ_CORRECTING, AZ_FINISHED } AZStage;
    typedef enum { ALT_INIT, ALT_FIRST_TARGET, ALT_SYNCING, ALT_SLEWING, ALT_SECOND_TARGET, ALT_CORRECTING, ALT_FINISHED } ALTStage;

    /**DBUS interface function.
     * Select CCD
     * @param device CCD device name
     * @return True if device if found and selected, false otherwise.
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /**DBUS interface function.
     * Start the plate-solving process given the passed image file.
     * @param filename Name of image file to solve. FITS and JPG/JPG/TIFF formats are accepted.
     * @param isGenerated Set to true if filename is generated from a CCD capture operation. If the file is loaded from any storage or network media, pass false.
     * @return True if device if found and selected, false otherwise.
     */
    Q_SCRIPTABLE Q_NOREPLY void startSovling(const QString &filename, bool isGenerated=true);

    /**DBUS interface function.
     * Select Goto Mode of Solver. The solver mode is the action the solver performs upon successful solving.
     * @param mode 0 for Sync, 1 for SlewToTarget, 2 for Nothing
     */
    Q_SCRIPTABLE Q_NOREPLY void setGOTOMode(int mode);

    /**DBUS interface function.
     * Returns the solver's solution results
     * @return array of doubles. First item is RA in degrees. Second item is DEC in degrees.
     */
    Q_SCRIPTABLE QList<double> getSolutionResult();

    /**DBUS interface function.
     * Returns true if the solver process completed or aborted, false otherwise.
     */
    Q_SCRIPTABLE bool isSolverComplete() { return m_isSolverComplete; }

    /**DBUS interface function.
     * Returns true if the solver process completed successfully, false otherwise.
     */
    Q_SCRIPTABLE bool isSolverSuccessful() { return m_isSolverSuccessful; }

    /**DBUS interface function.
     * Returns true if the solver load and slew operation completed, false otherwise.
     */
    Q_SCRIPTABLE bool isLoadAndSlewComplete() { return (loadSlewMode == false); }

    /**DBUS interface function.
     * Sets the exposure of the selected CCD device
     * @param value Exposure value in seconds
     */
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);

    /**DBUS interface function.
     * Sets the binning of the selected CCD device
     * @param binX horizontal binning
     * @param binY vertical binning
     */
    Q_SCRIPTABLE Q_NOREPLY void setBinning(int binX, int binY);

    /**DBUS interface function.
     * Sets the arguments that gets passed to the astrometry.net offline solver.
     * @param value space-separated arguments.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverArguments(const QString & value);

    /**DBUS interface function.
     * Sets the solver search area options
     * @param ra center RA for search pattern, in hours.
     * @param dec center DEC for search pattern, in degrees.
     * @param radius radius of search pattern, in degrees.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverSearchOptions(double ra, double dec, double radius);

    /**DBUS interface function.
     * Sets the solver's option
     * @param updateCoords if true, the telescope coordinates are automatically incorporated into the search pattern whenever the telescope completes slewing.
     * @param previewImage if true, the captured image is viewed in the FITSViewer tool before getting passed to the solver.
     * @param verbose if true, extended information will be displayed in the logger window.
     * @param useOAGT if true, use the Off-Axis Guide Telescope focal length and aperture for FOV calculations. Otherwise, the main telescope's focal length and aperture are used for the calculation.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverOptions(bool updateCoords, bool previewImage, bool verbose, bool useOAGT);


    void addCCD(ISD::GDInterface *newCCD, bool isPrimaryCCD);
    void setTelescope(ISD::GDInterface *newTelescope);
    void addGuideHead();    
    void syncCCDInfo();
    void generateArgs();    
    void appendLogText(const QString &);
    void clearLog();
    bool parserOK();
    bool isVerbose();

    QString getLogText() { return logText.join("\n"); }

public slots:


    void updateScopeCoords(INumberVectorProperty *coord);
    void checkCCD(int CCDNum=-1);
    void newFITS(IBLOB *bp);


    /**DBUS interface function.
     * Aborts the solving operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void stopSolving();

    /**DBUS interface function.
     * Select the solver type (online or offline)
     * @param useOnline if true, select the online solver, otherwise the offline solver is selected.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverType(bool useOnline);

    /**DBUS interface function.
     * Capture and solve an image using the astrometry.net engine
     * @return True if the procedure started successful, false otherwise.
     */
    Q_SCRIPTABLE bool captureAndSolve();
    void solverFinished(double orientation, double ra, double dec);
    void solverFailed();

    /* CCD & Telescope Info */
    void syncTelescopeInfo();

    /* Solver Options */
    void checkLineEdits();
    void copyCoordsToBoxes();
    void clearCoordBoxes();

    /* Polar Alignment */
    void checkPolarAlignment();
    void measureAltError();
    void measureAzError();
    void correctAzError();
    void correctAltError();

    /**DBUS interface function.
     * Loads an image (FITS or JPG/TIFF) and solve its coordinates, then it slews to the solved coordinates and an image is captured and solved to ensure
     * the telescope is pointing to the same coordinates of the image.
     * @param fileURL URL to the image to solve
     */
     Q_SCRIPTABLE Q_NOREPLY void loadAndSlew(QUrl fileURL = QUrl());

signals:
        void newLog();

private:
    void calculateFOV();
    void executeMode();
    void executeGOTO();
    void executePolarAlign();
    void Sync();
    void SlewToTarget();
    void calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC);
    void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

    ISD::Telescope *currentTelescope;
    ISD::CCD *currentCCD;
    QList<ISD::CCD *> CCDs;

    Ekos::Capture *captureP;

    bool useGuideHead;
    bool canSync;
    bool loadSlewMode;
    bool m_isSolverComplete;
    bool m_isSolverSuccessful;
    QStringList logText;

    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, fov_x, fov_y;
    int ccd_width, ccd_height;
    double sOrientation, sRA, sDEC;

    SkyPoint alignCoord;
    SkyPoint targetCoord;
    SkyPoint telescopeCoord;

    QProgressIndicator *pi;


    QTime solverTimer;
    AZStage azStage;
    ALTStage altStage;
    double azDeviation, altDeviation;
    double decDeviation;

    static const double RAMotion;
    static const float SIDRATE;

    AstrometryParser *parser;
    OnlineAstrometryParser *onlineParser;
    OfflineAstrometryParser *offlineParser;


};

}

#endif  // ALIGN_H
