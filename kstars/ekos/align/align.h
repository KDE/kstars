/*  Ekos Polar Alignment Tool
    Copyright (C) 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include "ui_align.h"
#include "ui_mountmodel.h"
#include "ekos/ekos.h"
#include "indi/indiccd.h"
#include "indi/indistd.h"
#include "indi/inditelescope.h"
#include "indi/indidome.h"
#include "ekos/auxiliary/filtermanager.h"

#include <QTime>
#include <QTimer>

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
#include <QtDBus/qtdbusglobal.h>
#else
#include <QtDBus/qdbusmacros.h>
#endif

#include <memory>

class QProgressIndicator;

class AlignView;
class FOV;
class StarObject;
class ProfileInfo;

namespace Ekos
{
class AstrometryParser;
class OnlineAstrometryParser;
class OfflineAstrometryParser;
class RemoteAstrometryParser;
class OpsAstrometry;
class OpsAlign;
class OpsAstrometryCfg;
class OpsAstrometryIndexFiles;

/**
 *@class Align
 *@short Align class handles plate-solving and polar alignment measurement and correction using astrometry.net
 * The align class can capture images from the CCD and use either online or offline astrometry.net engine to solve the plate constants and find the center RA/DEC coordinates. The user selects the action
 * to perform when the solver completes successfully.
 * Align module provide Polar Align Helper tool which enables easy-to-follow polar alignment procedure given wide FOVs (> 1.5 degrees)
 * For small FOVs, the Legacy polar alignment measurement should be used.
 * LEGACY: Measurement of polar alignment errors is performed by capturing two images on selected points in the sky and measuring the declination drift to calculate
 * the error in the mount's azimutn and altitude displacement from optimal. Correction is carried by asking the user to re-center a star by adjusting the telescope's azimuth and/or altitude knobs.
 *@author Jasem Mutlaq
 *@version 1.3
 */
class Align : public QWidget, public Ui::Align
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")

  public:
    explicit Align(ProfileInfo *activeProfile);
    virtual ~Align();

    typedef enum {
        AZ_INIT,
        AZ_FIRST_TARGET,
        AZ_SYNCING,
        AZ_SLEWING,
        AZ_SECOND_TARGET,
        AZ_CORRECTING,
        AZ_FINISHED
    } AZStage;
    typedef enum {
        ALT_INIT,
        ALT_FIRST_TARGET,
        ALT_SYNCING,
        ALT_SLEWING,
        ALT_SECOND_TARGET,
        ALT_CORRECTING,
        ALT_FINISHED
    } ALTStage;
    typedef enum { GOTO_SYNC, GOTO_SLEW, GOTO_NOTHING } GotoMode;
    typedef enum { SOLVER_ONLINE, SOLVER_OFFLINE, SOLVER_REMOTE } SolverType;
    typedef enum {
        PAH_IDLE,
        PAH_FIRST_CAPTURE,
        PAH_FIND_CP,
        PAH_FIRST_ROTATE,
        PAH_SECOND_CAPTURE,
        PAH_SECOND_ROTATE,
        PAH_THIRD_CAPTURE,
        PAH_STAR_SELECT,
        PAH_PRE_REFRESH,
        PAH_REFRESH,
        PAH_ERROR
    } PAHStage;
    typedef enum { NORTH_HEMISPHERE, SOUTH_HEMISPHERE } HemisphereType;

    // Image Scales
    const QStringList ImageScales = { "dw", "aw", "app" };

    enum CircleSolution
    {
        NO_CIRCLE_SOLUTION,
        ONE_CIRCLE_SOLUTION,
        TWO_CIRCLE_SOLUTION,
        INFINITE_CIRCLE_SOLUTION
    };

    /** @defgroup AlignDBusInterface Ekos DBus Interface - Align Module
         * Ekos::Align interface provides advanced scripting capabilities to solve images using online or offline astrometry.net
        */

    /*@{*/

    /** DBUS interface function.
         * Select CCD
         * @param device CCD device name
         * @return Returns true if device if found and selected, false otherwise.
         */
    Q_SCRIPTABLE bool setCCD(const QString &device);

    /** DBUS interface function.
         * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
         * @param device The filter device name
         * @return Returns true if filter device is found and set, false otherwise.
         */
    Q_SCRIPTABLE bool setFilter(QString device, int filterSlot);

    /** DBUS interface function.
         * Start the plate-solving process given the passed image file.
         * @param filename Name of image file to solve. FITS and JPG/JPG/TIFF formats are accepted.
         * @param isGenerated Set to true if filename is generated from a CCD capture operation. If the file is loaded from any storage or network media, pass false.
         * @return Returns true if device if found and selected, false otherwise.
         */
    Q_SCRIPTABLE Q_NOREPLY void startSolving(const QString &filename, bool isGenerated = true);

    /** DBUS interface function.
         * Select Solver Action after successfully solving an image.
         * @param mode 0 for Sync, 1 for Slew To Target, 2 for Nothing (just display solution results)
         */
    Q_SCRIPTABLE Q_NOREPLY void setSolverAction(int mode);

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
    Q_SCRIPTABLE Q_NOREPLY void setSolverArguments(const QString &value);

    /** DBUS interface function.
         * Get existing solver options.
         * @return String containing all arguments.
         */
    Q_SCRIPTABLE QString getSolverArguments();

    /** DBUS interface function.
         * Sets the telescope type (PRIMARY or GUIDE) that should be used for FOV calculations. This value is loaded form driver settings by default.
         * @param index 0 for PRIMARY telescope, 1 for GUIDE telescope
         */
    Q_SCRIPTABLE Q_NOREPLY void setFOVTelescopeType(int index);

    int getFOVTelescopeType() {return FOVScopeCombo->currentIndex();}

    /** @}*/

    /**
         * @brief Add CCD to the list of available CCD.
         * @param newCCD pointer to CCD device.
         */
    void addCCD(ISD::GDInterface *newCCD);

    /**
         * @brief addFilter Add filter to the list of available filters.
         * @param newFilter pointer to filter device.
         */
    void addFilter(ISD::GDInterface *newFilter);

    /**
         * @brief Set the current telescope
         * @param newTelescope pointer to telescope device.
         */
    void setTelescope(ISD::GDInterface *newTelescope);

    /**
         * @brief Set the current dome
         * @param ewDome pointer to telescope device.
         */
    void setDome(ISD::GDInterface *newDome);

    void setRotator(ISD::GDInterface *newRotator);

    /* @brief Set telescope and guide scope info. All measurements is in millimeters.
    * @param primaryFocalLength Primary Telescope Focal Length. Set to 0 to skip setting this value.
    * @param primaryAperture Primary Telescope Aperture. Set to 0 to skip setting this value.
    * @param guideFocalLength Guide Telescope Focal Length. Set to 0 to skip setting this value.
    * @param guideAperture Guide Telescope Aperture. Set to 0 to skip setting this value.
    */
    void setTelescopeInfo(double primaryFocalLength, double primaryAperture, double guideFocalLength, double guideAperture);

    /**
         * @brief setAstrometryDevice
         * @param newAstrometry
         */
    void setAstrometryDevice(ISD::GDInterface *newAstrometry);

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

    /**
         * @brief Return FOV object used to represent the current CCD/Telescope combination.
         */
    FOV *getSensorFOV() { return sensorFOV.get(); }

    /**
         * @brief getFOVScale Returns calculated FOV values
         * @param fov_w FOV width in arcmins
         * @param fov_h FOV height in arcmins
         * @param fov_scale FOV scale in arcsec per pixel
         */
    void getFOVScale(double &fov_w, double &fov_h, double &fov_scale);

    /**
     * @brief getCalculatedFOVScale Get calculated FOV scales from the current CCD+Telescope combination.
     * @param fov_w return calculated fov width in arcminutes
     * @param fov_h return calculated fov height in arcminutes
     * @param fov_scale return calculated fov pixcale in arcsecs per pixel.
     * @note This is NOT the same as effective FOV which is the measured FOV from astrometry. It is the
     * theoritical FOV from calculated values.
     */
    void getCalculatedFOVScale(double &fov_w, double &fov_h, double &fov_scale);

    void setFilterManager(const QSharedPointer<FilterManager> &manager);

    // Ekos Live Client helper functions
    QStringList getActiveSolvers() const;
    int getActiveSolverIndex() const;
    void setCaptureSettings(const QJsonObject &settings);

    /**
         * @brief generateOptions Generate astrometry.net option given the supplied map
         * @param optionsMap List of key=value pairs for all astrometry.net options
         * @return String List of valid astrometry.net options
         */
    static QStringList generateOptions(const QVariantMap &optionsMap);
    static void generateFOVBounds(double fov_h, double fov_v, QString &fov_low, QString &fov_high);

  public slots:

    /**
         * @brief Process updated device properties
         * @param nvp pointer to updated property.
         */
    void processNumber(INumberVectorProperty *nvp);

    /**
         * @brief Process updated device properties
         * @param svp pointer to updated property.
         */
    void processSwitch(ISwitchVectorProperty *svp);

    /**
         * @brief Check CCD and make sure information is updated and FOV is re-calculated.
         * @param CCDNum By default, we check the already selected CCD in the dropdown menu. If CCDNum is specified, the check is made against this specific CCD in the dropdown menu. CCDNum is the index of the CCD in the dropdown menu.
         */
    void checkCCD(int CCDNum = -1);

    /**
         * @brief Check Filter and make sure information is updated accordingly.
         * @param filterNum By default, we check the already selected filter in the dropdown menu. If filterNum is specified, the check is made against this specific filter in the dropdown menu.
         *  filterNum is the index of the filter in the dropdown menu.
         */
    void checkFilter(int filterNum = -1);

    /**
         * @brief checkCCDExposureProgress Track the progress of CCD exposure
         * @param targeChip Target chip under exposure
         * @param remaining how many seconds remaining
         * @param state status of exposure
         */
    void checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state);
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

    void setFocusStatus(Ekos::FocusState state);

    // Log
    void appendLogText(const QString &);

    // Capture
    void setCaptureComplete();

    // Update Capture Module status
    void setCaptureStatus(Ekos::CaptureState newState);
    // Update Mount module status
    void setMountStatus(ISD::Telescope::TelescopeStatus newState);

    // PAH Ekos Live
    QString getPAHStage() const { return PAHStages[pahStage];}
    bool isPAHEnabled() const { return isPAHReady; }
    QString getPAHMessage() const;

    void startPAHProcess();
    void stopPAHProcess();
    void setPAHCorrectionOffsetPercentage(double dx, double dy);
    void setPAHMountDirection(int index) { PAHDirectionCombo->setCurrentIndex(index);}
    void setPAHMountRotation(int value) {PAHRotationSpin->setValue(value);}
    void setPAHRefreshDuration(double value) { PAHExposure->setValue(value);}
    void startPAHRefreshProcess();
    void setPAHRefreshComplete();
    void setPAHCorrectionSelectionComplete();
    void zoomAlignView();

    // Align Settings
    QJsonObject getSettings() const;
    void setSettings(const QJsonObject &settings);

    // PAH Settings. PAH should be in separate class
    QJsonObject getPAHSettings() const;
    void setPAHSettings(const QJsonObject &settings);

  private slots:

    /* Polar Alignment */
    void measureAltError();
    void measureAzError();
    void correctAzError();
    void correctAltError();

    void setDefaultCCD(QString ccd);

    void saveSettleTime();

    // Solver timeout
    void checkAlignmentTimeout();

    void updateTelescopeType(int index);

    // External View
    void showFITSViewer();
    void toggleAlignWidgetFullScreen();

    // Polar Alignment Helper slots

    void rotatePAH();
    void setPAHCorrectionOffset(int x, int y);
    void setWCSToggled(bool result);

    //Solutions Display slots
    void buildTarget();
    void handlePointTooltip(QMouseEvent *event);
    void handleVerticalPlotSizeChange();
    void handleHorizontalPlotSizeChange();
    void selectSolutionTableRow(int row, int column);
    void slotClearAllSolutionPoints();
    void slotRemoveSolutionPoint();
    void slotMountModel();

    //Mount Model Slots

    void slotWizardAlignmentPoints();
    void slotStarSelected(const QString selectedStar);
    void slotLoadAlignmentPoints();
    void slotSaveAsAlignmentPoints();
    void slotSaveAlignmentPoints();
    void slotClearAllAlignPoints();
    void slotRemoveAlignPoint();
    void slotAddAlignPoint();
    void slotFindAlignObject();
    void resetAlignmentProcedure();
    void startStopAlignmentProcedure();
    void startAlignmentPoint();
    void finishAlignmentPoint(bool solverSucceeded);
    void moveAlignPoint(int logicalIndex, int oldVisualIndex, int newVisualIndex);
    void exportSolutionPoints();
    void alignTypeChanged(const QString alignType);
    void togglePreviewAlignPoints();
    void slotSortAlignmentPoints();
    void slotAutoScaleGraph();

    // Settings
    void syncSettings();

  protected slots:
    /**
         * @brief After a solver process is completed successfully, sync, slew to target, or do nothing as set by the user.
         */
    void executeGOTO();

    /**
     * @brief refreshAlignOptions is called when settings are updated in OpsAlign.
     */
    void refreshAlignOptions();

  signals:
    void newLog();
    void newStatus(Ekos::AlignState state);
    // This is sent when we load an image in the view
    void newImage(FITSView *view);
    // This is sent when the pixmap is updated within the view
    void newFrame(FITSView *view);
    void polarResultUpdated(QLineF correctionVector, QString polarError);
    void newCorrectionVector(QLineF correctionVector);
    void newSolverResults(double orientation, double ra, double dec, double pixscale);
    void newSolution(const QJsonObject &solution);

    // Polar Assistant Tool
    void newPAHStage(PAHStage stage);
    void newPAHMessage(const QString &message);    
    void newFOVTelescopeType(int index);
    void PAHEnabled(bool);

    // Settings
    void settingsUpdated(const QJsonObject &settings);

  private:
    /**
        * @brief Calculate Field of View of CCD+Telescope combination that we need to pass to astrometry.net solver.
        */
    void calculateFOV();    

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

    /**
         * @brief setWCSEnabled enables/disables World Coordinate System settings in the CCD driver.
         * @param enable true to enable WCS, false to disable.
         */
    void setWCSEnabled(bool enable);

    /**
         * @brief calculatePAHError Calculate polar alignment error in the Polar Alignment Helper (PAH) method
         */
    void calculatePAHError();

    /**
         * @brief processPAHStage After solver is complete, handle PAH Stage processing
         */
    void processPAHStage(double orientation, double ra, double dec, double pixscale);

    CircleSolution findCircleSolutions(const QPointF &p1, const QPointF p2, double angle,
                                       QPair<QPointF, QPointF> &circleSolutions);

    double distance(const QPointF &p1, const QPointF &p2);
    bool findRACircle(QVector3D &RACircle);
    bool isPerpendicular(const QPointF &p1, const QPointF &p2, const QPointF &p3);
    bool calcCircle(const QPointF &p1, const QPointF &p2, const QPointF &p3, QVector3D &RACircle);

    void resizeEvent(QResizeEvent *event);

    bool alignmentPointsAreBad();
    bool loadAlignmentPoints(const QString &fileURL);
    bool saveAlignmentPoints(const QString &path);

    void generateAlignStarList();
    bool isVisible(const SkyObject *so);
    double getAltitude(const SkyObject *so);
    const SkyObject *getWizardAlignObject(double ra, double de);
    void calculateAngleForRALine(double &raIncrement, double &initRA, double initDEC, double lat, double raPoints,
                                 double minAlt);
    void calculateAZPointsForDEC(dms dec, dms alt, dms &AZEast, dms &AZWest);
    void updatePreviewAlignPoints();
    int findNextAlignmentPointAfter(int currentSpot);
    int findClosestAlignmentPointToTelescope();
    void swapAlignPoints(int firstPt, int secondPt);

    // Effective FOV

    /**
     * @brief getEffectiveFOV Search database for effective FOV that matches the current profile and settings
     * @return Variant Map containing effect FOV data or empty variant map if none found
     */
    QVariantMap getEffectiveFOV();
    void saveNewEffectiveFOV(double newFOVW, double newFOVH);
    QList<QVariantMap> effectiveFOVs;
    void syncFOV();

    /// Which chip should we invoke in the current CCD?
    bool useGuideHead { false };
    /// Can the mount sync its coordinates to those set by Ekos?
    bool canSync { false };
    // LoadSlew mode is when we load an image and solve it, no capture is done.
    //bool loadSlewMode;
    /// If load and slew is solved successfully, coordinates obtained, slewed to target, and then captured, solved, and re-slewed to target again.
    IPState loadSlewState { IPS_IDLE };
    // Target Position Angle of solver Load&Slew image to be used for rotator if necessary
    double loadSlewTargetPA { std::numeric_limits<double>::quiet_NaN() };
    double currentRotatorPA { -1 };
    /// Solver iterations count
    uint8_t solverIterations { 0 };

    // FOV
    double ccd_hor_pixel { -1 };
    double ccd_ver_pixel { -1 };
    double focal_length { -1 };
    double aperture { -1 };
    double fov_x { 0 };
    double fov_y { 0 };
    double fov_pixscale { 0 };
    int ccd_width { 0 };
    int ccd_height { 0 };

    // Keep track of solver results
    double sOrientation { -1 };
    double sRA { -1 };
    double sDEC { -1 };

    /// Solver alignment coordinates
    SkyPoint alignCoord;
    /// Target coordinates we need to slew to
    SkyPoint targetCoord;
    /// Actual current telescope coordinates
    SkyPoint telescopeCoord;
    /// Coord from Load & Slew
    SkyPoint loadSlewCoord;
    /// Difference between solution and target coordinate
    double targetDiff { 1e6 };

    /// Progress icon if the solver is running
    std::unique_ptr<QProgressIndicator> pi;

    /// Keep track of how long the solver is running
    QTime solverTimer;

    // Polar Alignment
    AZStage azStage;
    ALTStage altStage;
    double azDeviation { 0 };
    double altDeviation { 0 };
    double decDeviation { 0 };
    static const double RAMotion;
    static const double SIDRATE;

    /// Have we slewed?
    bool isSlewDirty { false };    

    // Online and Offline parsers
    AstrometryParser* parser { nullptr };
    std::unique_ptr<OnlineAstrometryParser> onlineParser;
    std::unique_ptr<OfflineAstrometryParser> offlineParser;

    std::unique_ptr<RemoteAstrometryParser> remoteParser;
    ISD::GDInterface *remoteParserDevice { nullptr };

    // Pointers to our devices
    ISD::Telescope *currentTelescope { nullptr };
    ISD::Dome *currentDome { nullptr };
    ISD::CCD *currentCCD { nullptr };
    ISD::GDInterface *currentRotator { nullptr };
    QList<ISD::CCD *> CCDs;

    /// Optional device filter
    ISD::GDInterface *currentFilter { nullptr };
    /// They're generic GDInterface because they could be either ISD::CCD or ISD::Filter
    QList<ISD::GDInterface *> Filters;
    int currentFilterPosition { -1 };
    /// True if we need to change filter position and wait for result before continuing capture
    bool filterPositionPending { false };

    /// Keep track of solver FOV to be plotted in the skymap after each successful solve operation
    std::unique_ptr<FOV> solverFOV;

    std::unique_ptr<FOV> sensorFOV;

    /// WCS
    bool m_wcsSynced { false };

    /// Log
    QStringList logText;

    /// Capture retries
    int retries { 0 };

    // State
    AlignState state { ALIGN_IDLE };
    FocusState focusState { FOCUS_IDLE };

    // Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
    ISD::CCD::UploadMode rememberUploadMode { ISD::CCD::UPLOAD_CLIENT };

    GotoMode currentGotoMode;

    QString dirPath;

    // Timer
    QTimer alignTimer;

    // BLOB Type
    ISD::CCD::BlobType blobType;
    QString blobFileName;

    // Align Frame
    AlignView *alignView { nullptr };

    // FITS Viewer in case user want to display in it instead of internal view
    QPointer<FITSViewer> fv;

    // Polar Alignment Helper
    PAHStage pahStage { PAH_IDLE };
    bool isPAHReady { false };

    // keep track of autoWSC
    bool rememberAutoWCS { false };
    bool rememberSolverWCS { false };

    // Sky centers
    typedef struct
    {
        SkyPoint skyCenter;
        QPointF pixelCenter;
        double pixelScale { 0 };
        double orientation { 0 };
    } PAHImageInfo;

    QVector<PAHImageInfo *> pahImageInfos;

    // User desired offset when selecting a bright star in the image
    QPointF celestialPolePoint, correctionOffset;
    // Correction vector line between mount RA Axis and celestial pole
    QLineF correctionVector;

    // CCDs using Guide Scope for parameters
    //QStringList guideScopeCCDs;

    // Which hemisphere are we located on?
    HemisphereType hemisphere;

    // Differential Slewing
    bool differentialSlewingActivated { false };

    // Astrometry Options
    OpsAstrometry *opsAstrometry { nullptr };
    OpsAlign *opsAlign { nullptr };
    OpsAstrometryCfg *opsAstrometryCfg { nullptr };
    OpsAstrometryIndexFiles *opsAstrometryIndexFiles { nullptr };
    QCPCurve *centralTarget { nullptr };
    QCPCurve *yellowTarget { nullptr };
    QCPCurve *redTarget { nullptr };
    QCPCurve *concentricRings { nullptr };
    QDialog mountModelDialog;
    Ui_mountModel mountModel;
    int currentAlignmentPoint { 0 };
    bool mountModelRunning { false };
    bool mountModelReset { false };
    bool targetAccuracyNotMet { false };
    bool previewShowing { false };
    QUrl alignURL;
    QUrl alignURLPath;
    QVector<const StarObject *> alignStars;

    ISD::CCD::TelescopeType rememberTelescopeType = { ISD::CCD::TELESCOPE_UNKNOWN };

    double primaryFL = -1, primaryAperture = -1, guideFL = -1, guideAperture = -1;
    bool domeReady = true;

    // CCD Exposure Looping
    bool rememberCCDExposureLooping = { false };

    // Filter Manager
    QSharedPointer<FilterManager> filterManager;

    // Active Profile
    ProfileInfo *m_ActiveProfile { nullptr };

    // PAH Stage Map
    static const QMap<PAHStage, QString> PAHStages;
};
}
