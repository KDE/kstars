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

#include "ui_align.h"

#include "ekos/ekos.h"

#include "indi/inditelescope.h"
#include "indi/indiccd.h"
#include "indi/indistd.h"

class FOV;
class QProgressIndicator;

namespace Ekos
{

class AstrometryParser;
class OnlineAstrometryParser;
class OfflineAstrometryParser;
class RemoteAstrometryParser;

/**
 *@class Align
 *@short Align class handles plate-solving and polar alignment measurement and correction using astrometry.net
 * The align class can capture images from the CCD and use either online or offline astrometry.net engine to solve the plate constants and find the center RA/DEC coordinates. The user selects the action
 * to perform when the solver completes successfully. Measurement of polar alignment errors is performed by capturing two images on selected points in the sky and measuring the declination drift to calculate
 * the error in the mount's azimutn and altitude displacement from optimal. Correction is carried by asking the user to re-center a star by adjusting the telescope's azimuth and/or altitude knobs.
 *@author Jasem Mutlaq
 *@version 1.2
 */
class Align : public QWidget, public Ui::Align
{

    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")

public:
    Align();
    ~Align();

    typedef enum { AZ_INIT, AZ_FIRST_TARGET, AZ_SYNCING, AZ_SLEWING, AZ_SECOND_TARGET, AZ_CORRECTING, AZ_FINISHED } AZStage;
    typedef enum { ALT_INIT, ALT_FIRST_TARGET, ALT_SYNCING, ALT_SLEWING, ALT_SECOND_TARGET, ALT_CORRECTING, ALT_FINISHED } ALTStage;
    typedef enum { GOTO_SYNC, GOTO_SLEW, GOTO_NOTHING } GotoMode;
    typedef enum { SOLVER_ONLINE, SOLVER_OFFLINE, SOLVER_REMOTE} SolverType;

    /** @defgroup AlignDBusInterface Ekos DBus Interface - Align Module
     * Ekos::Align interface provides advanced scripting capabilities to solve images using online or offline astrometry.net
    */

    /*@{*/

    /** DBUS interface function.
     * Select CCD
     * @param device CCD device name
     * @return Returns true if device if found and selected, false otherwise.
     */
    Q_SCRIPTABLE bool setCCD(QString device);

    /** DBUS interface function.
     * Start the plate-solving process given the passed image file.
     * @param filename Name of image file to solve. FITS and JPG/JPG/TIFF formats are accepted.
     * @param isGenerated Set to true if filename is generated from a CCD capture operation. If the file is loaded from any storage or network media, pass false.
     * @return Returns true if device if found and selected, false otherwise.
     */
    Q_SCRIPTABLE Q_NOREPLY void startSolving(const QString &filename, bool isGenerated=true);

    /** DBUS interface function.
     * Select Goto Mode of Solver. The solver mode is the action the solver performs upon successful solving.
     * @param mode 0 for Sync, 1 for SlewToTarget, 2 for Nothing
     */
    Q_SCRIPTABLE Q_NOREPLY void setGOTOMode(int mode);

    /** DBUS interface function.
     * Returns the solver's solution results
     * @return Returns array of doubles. First item is RA in degrees. Second item is DEC in degrees.
     */
    Q_SCRIPTABLE QList<double> getSolutionResult();

    /** DBUS interface function.
     * Returns the solver's current status
     * @return Returns solver status (Ekos::AlignState)
     */
    Q_SCRIPTABLE int getStatus() { return state; }

#if 0
    /** DBUS interface function.
     * @return Returns true if the solver process completed or aborted, false otherwise.
     */
    Q_SCRIPTABLE bool isSolverComplete() { return m_isSolverComplete; }

    /** DBUS interface function.
     * @return Returns true if the solver process completed successfully, false otherwise.
     */
    Q_SCRIPTABLE bool isSolverSuccessful() { return m_isSolverSuccessful; }
#endif

    /** DBUS interface function.
     * @return Returns State of load slew procedure. Idle if not started. Busy if in progress. Ok if complete. Alert if procedure failed.
     */
    Q_SCRIPTABLE int getLoadAndSlewStatus() { return loadSlewState; }

    /** DBUS interface function.
     * Sets the exposure of the selected CCD device.
     * @param value Exposure value in seconds
     */
    Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);

    /** DBUS interface function.
     * Sets the arguments that gets passed to the astrometry.net offline solver.
     * @param value space-separated arguments.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverArguments(const QString & value);

    /** DBUS interface function.
     * Sets the solver search area options
     * @param ra center RA for search pattern, in hours.
     * @param dec center DEC for search pattern, in degrees.
     * @param radius radius of search pattern, in degrees.
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverSearchOptions(double ra, double dec, double radius);

    /** DBUS interface function.
     * Sets the solver's option
     * @param useOAGT if true, use the Off-Axis Guide Telescope focal length and aperture for FOV calculations. Otherwise, the main telescope's focal length and aperture are used for the calculation.
     */
    Q_SCRIPTABLE Q_NOREPLY void setUseOAGT(bool enabled);


    /** @}*/

    /**
     * @brief Add CCD to the list of available CCD.
     * @param newCCD pointer to CCD device.
     */
    void addCCD(ISD::GDInterface *newCCD);

    /**
     * @brief Set the current telescope
     * @newTelescope pointer to telescope device.
     */
    void setTelescope(ISD::GDInterface *newTelescope);

    /**
     * @brief CCD information is updated, sync them.
     */
    void syncCCDInfo();

    /**
     * @brief Generate arguments we pass to the online and offline solvers. Keep user own arguments in place.
     */
    void generateArgs();

    /**
     * @brief Does our parser exist in the system?
     */
    bool isParserOK();

    // Log
    QString getLogText() { return logText.join("\n"); }
    void clearLog();

    /**
     * @brief Return FOV object used to represent the solved image orientation on the sky map.
     */
    FOV *fov();


public slots:

    /**
     * @brief Process updated telescope coordinates.
     * @coord pointer to telescope coordinate property.
     */
    void processTelescopeNumber(INumberVectorProperty *coord);

    /**
     * @brief Check CCD and make sure information is updated and FOV is re-calculated.
     * @param CCDNum By default, we check the already selected CCD in the dropdown menu. If CCDNum is specified, the check is made against this specific CCD in the dropdown menu. CCDNum is the index of the CCD in the dropdown menu.
     */
    void checkCCD(int CCDNum=-1);

    /**
     * @brief checkCCDExposureProgress Track the progress of CCD exposure
     * @param targeChip Target chip under exposure
     * @param remaining how many seconds remaining
     * @param state status of exposure
     */
    void checkCCDExposureProgress(ISD::CCDChip* targetChip, double remaining, IPState state);
    /**
     * @brief Process new FITS received from CCD.
     * @param bp pointer to blob property
     */
    void newFITS(IBLOB *bp);

    /** \addtogroup AlignDBusInterface
     *  @{
     */

    /** DBUS interface function.
     * Aborts the solving operation.
     */
    Q_SCRIPTABLE Q_NOREPLY void abort();

    /** DBUS interface function.
     * Select the solver type
     * @param type Set solver type. 0 online, 1 offline, 2 remote
     */
    Q_SCRIPTABLE Q_NOREPLY void setSolverType(int type);

    /** DBUS interface function.
     * Capture and solve an image using the astrometry.net engine
     * @return Returns true if the procedure started successful, false otherwise.
     */
    Q_SCRIPTABLE bool captureAndSolve();

    /** DBUS interface function.
     * Loads an image (FITS or JPG/TIFF) and solve its coordinates, then it slews to the solved coordinates and an image is captured and solved to ensure
     * the telescope is pointing to the same coordinates of the image.
     * @param fileURL URL to the image to solve
     */
     Q_SCRIPTABLE Q_NOREPLY void loadAndSlew(QString fileURL = QString());

    /** DBUS interface function.
     * Sets the binning of the selected CCD device.
     * @param binIndex Index of binning value. Default values range from 0 (binning 1x1) to 3 (binning 4x4)
     */
    Q_SCRIPTABLE Q_NOREPLY void setBinningIndex(int binIndex);

    /** @}*/

    /**
     * @brief Solver finished successfully, process the data and execute the required actions depending on the mode.
     * @param orientation Orientation of image in degrees (East of North)
     * @param ra Center RA in solved image, degrees.
     * @param dec Center DEC in solved image, degrees.
     * @param pixscale Image scale is arcsec/pixel
     */
    void solverFinished(double orientation, double ra, double dec, double pixscale);

    /**
     * @brief Process solver failure.
     */
    void solverFailed();

    /**
     * @brief We received new telescope info, process them and update FOV.
     */
    void syncTelescopeInfo();

    /**
     * @brief setWCSEnabled enables/disables World Coordinate System settings in the CCD driver.
     * @param enable true to enable WCS, false to disable.
     */
    void setWCSEnabled(bool enable);

    void setLockedFilter(ISD::GDInterface *filter, int lockedPosition);

    void setFocusStatus(Ekos::FocusState state);

    // Log
    void appendLogText(const QString &);

    // Capture
    void setCaptureComplete();

    // Update Capture Module status
    void setCaptureStatus(Ekos::CaptureState newState);

private slots:
    /* Solver Options */
    void checkLineEdits();
    void copyCoordsToBoxes();
    void clearCoordBoxes();

    /* Polar Alignment */
    void measureAltError();
    void measureAzError();
    void correctAzError();
    void correctAltError();

    void processFilterNumber(INumberVectorProperty *nvp);

    void setDefaultCCD(QString ccd);

    void saveSettleTime();

    // Solver timeout
    void checkAlignmentTimeout();

signals:
        void newLog();
        void solverComplete(bool);
        void solverSlewComplete();
        void newStatus(Ekos::AlignState state);
        void newSolutionDeviation(double ra_arcsecs, double de_arcsecs);

private:
    /**
    * @brief Calculate Field of View of CCD+Telescope combination that we need to pass to astrometry.net solver.
    */
    void calculateFOV();

    /**
     * @brief After a solver process is completed successfully, sync, slew to target, or do nothing as set by the user.
     */
    void executeGOTO();

    /**
     * @brief After a solver process is completed successfully, measure Azimuth or Altitude error as requested by the user.
     */
    void executePolarAlign();

    /**
     * @brief Sync the telescope to the solved alignment coordinate.
     */
    void Sync();

    /**
     * @brief Slew the telescope to the solved alignment coordinate.
     */
    void Slew();

    /**
     * @brief Sync the telescope to the solved alignment coordinate, and then slew to the target coordinate.
     */
    void SlewToTarget();

    /**
     * @brief Calculate polar alignment error magnitude and direction.
     * The calculation is performed by first capturing and solving a frame, then slewing 30 arcminutes and solving another frame to find the exact coordinates, then computing the error.
     * @param initRA RA of first frame.
     * @param initDEC DEC of first frame
     * @param finalRA RA of second frame
     * @param finalDEC DEC of second frame
     * @param initAz Azimuth of first frame
     */
    void calculatePolarError(double initRA, double initDEC, double finalRA, double finalDEC, double initAz);

    /**
     * @brief Get formatted RA & DEC coordinates compatible with astrometry.net format.
     * @param ra Right ascension
     * @param dec Declination
     * @param ra_str will contain the formatted RA string
     * @param dec_str will contain the formatted DEC string
     */
    void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

    /**
     * @brief getSolverOptionsFromFITS Generates a set of solver options given the supplied FITS image. The function reads FITS keyword headers and build the argument list accordingly. In case of a missing header keyword, it falls back to
     * the Alignment module existing values.
     * @param filename FITS path
     * @return List of Solver options
     */
    QStringList getSolverOptionsFromFITS(const QString &filename);

    // Which chip should we invoke in the current CCD?
    bool useGuideHead;
    // Can the mount sync its coordinates to those set by Ekos?
    bool canSync;
    // LoadSlew mode is when we load an image and solve it, no capture is done.
    //bool loadSlewMode;
    // If load and slew is solved successfully, coordinates obtained, slewed to target, and then captured, solved, and re-slewed to target again.
    IPState loadSlewState;
    // Solver iterations count
    uint8_t solverIterations;

    // Keep track of solver status
    //bool m_isSolverComplete;
    //bool m_isSolverSuccessful;
    //bool m_slewToTargetSelected;

    // Focus
    //bool isFocusBusy;

    // FOV
    double ccd_hor_pixel, ccd_ver_pixel, focal_length, aperture, fov_x, fov_y;
    int ccd_width, ccd_height;

    // Keep track of solver results
    double sOrientation, sRA, sDEC;

    // Solver alignment coordinates
    SkyPoint alignCoord;
    // Target coordinates we need to slew to
    SkyPoint targetCoord;
    // Actual current telescope coordinates
    SkyPoint telescopeCoord;
    // Coord from Load & Slew
    SkyPoint loadSlewCoord;
    // Difference between solution and target coordinate
    double targetDiff;

    // Progress icon if the solver is running
    QProgressIndicator *pi;

    // Keep track of how long the solver is running
    QTime solverTimer;

    // Polar Alignment
    AZStage azStage;
    ALTStage altStage;
    double azDeviation, altDeviation;
    double decDeviation;
    static const double RAMotion;
    static const float SIDRATE;

    // Online and Offline parsers
    AstrometryParser *parser;
    OnlineAstrometryParser *onlineParser;
    OfflineAstrometryParser *offlineParser;
    RemoteAstrometryParser *remoteParser;

    // Pointers to our devices
    ISD::Telescope *currentTelescope;
    ISD::CCD *currentCCD;
    QList<ISD::CCD *> CCDs;

    // Optional device filter
    ISD::GDInterface *currentFilter;
    int lockedFilterIndex;
    int currentFilterIndex;
    // True if we need to change filter position and wait for result before continuing capture
    bool filterPositionPending;

    // Keep track of solver FOV to be plotted in the skymap after each successful solve operation
    FOV *solverFOV;

    // WCS
    bool m_wcsSynced;

    // Log
    QStringList logText;

    // Capture retries
    int retries;

    // State
    AlignState state;
    FocusState focusState;

    // Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
    ISD::CCD::UploadMode rememberUploadMode;

    GotoMode currentGotoMode;

    QString dirPath;

    // Timer
    QTimer alignTimer;

    // BLOB Type
    ISD::CCD::BlobType blobType;
    QString blobFileName;
};

}

#endif  // ALIGN_H
