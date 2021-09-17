/*
    SPDX-FileCopyrightText: 2013 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2013-2021 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2018-2020 Robert Lancaster <rlancaste@gmail.com>
    SPDX-FileCopyrightText: 2019-2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_align.h"
#include "ekos/ekos.h"
#include "indi/indiccd.h"
#include "indi/indistd.h"
#include "indi/inditelescope.h"
#include "indi/indidome.h"
#include "ksuserdb.h"
#include "ekos/auxiliary/filtermanager.h"

#include <QTime>
#include <QTimer>
#include <QElapsedTimer>
#include <KConfigDialog>

#if QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
#include <QtDBus/qtdbusglobal.h>
#else
#include <QtDBus/qdbusmacros.h>
#endif

#include <stellarsolver.h>
#include <memory>

class QProgressIndicator;

class AlignView;
class FOV;
class StarObject;
class ProfileInfo;

namespace Ekos
{
class AstrometryParser;
class RemoteAstrometryParser;
class OpsAstrometry;
class OpsAlign;
class StellarSolverProfileEditor;
class OpsPrograms;
class OpsASTAP;
class OpsAstrometryIndexFiles;
class MountModel;
class PolarAlignmentAssistant;
class ManualRotator;

/**
 *@class Align
 *@short Align class handles plate-solving and polar alignment measurement and correction using astrometry.net
 * The align class employs StellarSolver library for local solvers and supports remote INDI-based solver.
 * StellarSolver supports internal and external solvers (Astrometry.net, ASTAP, Online Astrometry).
 * If an image is solved successfully, the image central J2000 RA & DE coordinates along with pixel scale, rotation, and partiy are
 * reported back.
 * Index files management is supported with ability to download astrometry.net files. The user may select and edit different solver
 * profiles that provide settings to control both extraction and solving profiles in detail. Manual and automatic field rotation
 * is supported in order to align the solved images to a particular orientation in the sky. The manual rotation assistant is an interactive
 * tool that helps the user to arrive at the desired framing.
 * Align module provide Polar Align Helper tool which enables easy-to-follow polar alignment procedure given wide FOVs (> 1.5 degrees)
 * Legacy polar aligment is deprecated.
 *@author Jasem Mutlaq
 *@version 1.5
 */
class Align : public QWidget, public Ui::Align
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")
        Q_PROPERTY(Ekos::AlignState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString camera READ camera WRITE setCamera)
        Q_PROPERTY(QString filterWheel READ filterWheel WRITE setFilterWheel)
        Q_PROPERTY(QString filter READ filter WRITE setFilter)
        Q_PROPERTY(double exposure READ exposure WRITE setExposure)
        Q_PROPERTY(QList<double> fov READ fov)
        Q_PROPERTY(QList<double> cameraInfo READ cameraInfo)
        Q_PROPERTY(QList<double> telescopeInfo READ telescopeInfo)
        //Q_PROPERTY(QString solverArguments READ solverArguments WRITE setSolverArguments)

    public:
        explicit Align(ProfileInfo *activeProfile);
        virtual ~Align() override;

        typedef enum { GOTO_SYNC, GOTO_SLEW, GOTO_NOTHING } GotoMode;
        typedef enum { SOLVER_LOCAL, SOLVER_REMOTE } SolverMode;
        typedef enum
        {
            ALIGN_RESULT_SUCCESS,
            ALIGN_RESULT_WARNING,
            ALIGN_RESULT_FAILED
        } AlignResult;

        typedef enum
        {
            BLIND_IDLE,
            BLIND_ENGAGNED,
            BLIND_USED
        } BlindState;

        /** @defgroup AlignDBusInterface Ekos DBus Interface - Align Module
             * Ekos::Align interface provides advanced scripting capabilities to solve images using online or offline astrometry.net
            */

        /*@{*/

        /** DBUS interface function.
             * Select CCD
             * @param device CCD device name
             * @return Returns true if device if found and selected, false otherwise.
             */
        Q_SCRIPTABLE bool setCamera(const QString &device);
        Q_SCRIPTABLE QString camera();

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
             * Start the plate-solving process given the passed image file.
             * @param filename Name of image file to solve. FITS and JPG/JPG/TIFF formats are accepted.
             * @param isGenerated Set to true if filename is generated from a CCD capture operation. If the file is loaded from any storage or network media, pass false.
             * @return Returns true if device if found and selected, false otherwise.
             */
        Q_SCRIPTABLE Q_NOREPLY void startSolving();

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
        Q_SCRIPTABLE Ekos::AlignState status()
        {
            return state;
        }

        /** DBUS interface function.
             * @return Returns State of load slew procedure. Idle if not started. Busy if in progress. Ok if complete. Alert if procedure failed.
             */
        Q_SCRIPTABLE int getLoadAndSlewStatus()
        {
            return m_SolveFromFile;
        }

        /** DBUS interface function.
             * Sets the exposure of the selected CCD device.
             * @param value Exposure value in seconds
             */
        Q_SCRIPTABLE Q_NOREPLY void setExposure(double value);
        Q_SCRIPTABLE double exposure()
        {
            return exposureIN->value();
        }

        /** DBUS interface function.
             * Sets the telescope type (PRIMARY or GUIDE) that should be used for FOV calculations. This value is loaded form driver settings by default.
             * @param index 0 for PRIMARY telescope, 1 for GUIDE telescope
             */
        Q_SCRIPTABLE Q_NOREPLY void setFOVTelescopeType(int index);
        int FOVTelescopeType()
        {
            return FOVScopeCombo->currentIndex();
        }

        /** DBUS interface function.
         * Get currently active camera info in this order:
         * width, height, pixel_size_x, pixel_size_y
         */
        Q_SCRIPTABLE QList<double> cameraInfo();

        /** DBUS interface function.
         * Get current active telescope info in this order:
         * focal length, aperture
         */
        Q_SCRIPTABLE QList<double> telescopeInfo();

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
             * @param newDome pointer to telescope device.
             */
        void setDome(ISD::GDInterface *newDome);

        void setRotator(ISD::GDInterface *newRotator);

        void removeDevice(ISD::GDInterface *device);

        /**
             * @brief Set telescope and guide scope info. All measurements is in millimeters.
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
             * @brief Generate arguments we pass to the remote solver.
             */
        QStringList generateRemoteArgs(const QSharedPointer<FITSData> &imageData);

        /**
             * @brief Does our parser exist in the system?
             */
        bool isParserOK();

        // Log
        QStringList logText()
        {
            return m_LogText;
        }
        QString getLogText()
        {
            return m_LogText.join("\n");
        }
        void clearLog();

        /**
             * @brief getFOVScale Returns calculated FOV values
             * @param fov_w FOV width in arcmins
             * @param fov_h FOV height in arcmins
             * @param fov_scale FOV scale in arcsec per pixel
             */
        void getFOVScale(double &fov_w, double &fov_h, double &fov_scale);
        QList<double> fov();

        /**
         * @brief getCalculatedFOVScale Get calculated FOV scales from the current CCD+Telescope combination.
         * @param fov_w return calculated fov width in arcminutes
         * @param fov_h return calculated fov height in arcminutes
         * @param fov_scale return calculated fov pixcale in arcsecs per pixel.
         * @note This is NOT the same as effective FOV which is the measured FOV from astrometry. It is the
         * theoretical FOV from calculated values.
         */
        void getCalculatedFOVScale(double &fov_w, double &fov_h, double &fov_scale);

        void setFilterManager(const QSharedPointer<FilterManager> &manager);

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
         * @brief getStellarSolverProfiles
         * @return list of StellarSolver profile names
         */
        QStringList getStellarSolverProfiles();

        GotoMode currentGOTOMode() const
        {
            return m_CurrentGotoMode;
        }

        /**
             * @brief generateOptions Generate astrometry.net option given the supplied map
             * @param optionsMap List of key=value pairs for all astrometry.net options
             * @return String List of valid astrometry.net options
             */
        static QStringList generateRemoteOptions(const QVariantMap &optionsMap);
        static void generateFOVBounds(double fov_h, QString &fov_low, QString &fov_high, double tolerance = 0.05);

        // access to the mount model UI, required for testing
        MountModel * mountModel() const
        {
            return m_MountModel;
        }

        PolarAlignmentAssistant *polarAlignmentAssistant() const
        {
            return m_PolarAlignmentAssistant;
        }

        bool wcsSynced() const
        {
            return m_wcsSynced;
        }

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
             * @param targetChip Target chip under exposure
             * @param remaining how many seconds remaining
             * @param state status of exposure
             */
        void checkCCDExposureProgress(ISD::CCDChip *targetChip, double remaining, IPState state);
        /**
             * @brief Process new FITS received from CCD.
             * @param bp pointer to blob property
             */
        void processData(const QSharedPointer<FITSData> &data);

        /** DBUS interface function.
             * Loads an image (FITS, RAW, or JPG/PNG) and solve its coordinates, then it slews to the solved coordinates and an image is captured and solved to ensure
             * the telescope is pointing to the same coordinates of the image.
             * @param image buffer to image data.
             * @param extension image extension (e.g. cr2, jpg, fits,..etc).
             */
        bool loadAndSlew(const QByteArray &image, const QString &extension);

        /** \addtogroup AlignDBusInterface
             *  @{
             */

        /**
         * @brief Stop aligning
         * @param mode stop mode (abort or suspend)
         */
        void stop(Ekos::AlignState mode);

        /** DBUS interface function.
             * Aborts the solving operation, handle outside of the align module.
             */
        Q_SCRIPTABLE Q_NOREPLY void abort()
        {
            stop(ALIGN_ABORTED);
        }

        /**
         * @brief Suspend aligning, recovery handled by the align module itself.
         */
        void suspend()
        {
            stop(ALIGN_SUSPENDED);
        }

        /** DBUS interface function.
             * Select the solver mode
             * @param type Set solver type. 0 LOCAL, 1 REMOTE (requires remote astrometry driver to be activated)
             */
        Q_SCRIPTABLE Q_NOREPLY void setSolverMode(int mode);

        /** DBUS interface function.
             * Capture and solve an image using the astrometry.net engine
             * @return Returns true if the procedure started successful, false otherwise.
             */
        Q_SCRIPTABLE bool captureAndSolve();

        /** DBUS interface function.
             * Loads an image (FITS, RAW, or JPG/PNG) and solve its coordinates, then it slews to the solved coordinates and an image is captured and solved to ensure
             * the telescope is pointing to the same coordinates of the image.
             * @param fileURL URL to the image to solve
             */
        Q_SCRIPTABLE bool loadAndSlew(QString fileURL = QString());

        /** DBUS interface function.
             * Sets the target coordinates that the solver compares the solution coordinates to.
             * By default, the target coordinates are those of the current mount when the capture and
             * solve operation is started. In case of SYNC, only the error between the the solution and target
             * coordinates is calculated. When Slew to Target is selected, the mount would be slewed afterwards to
             * this target coordinate.
             * @param ra J2000 Right Ascension in hours.
             * @param de J2000 Declination in degrees.
             */
        Q_SCRIPTABLE Q_NOREPLY void setTargetCoords(double ra, double de);

        Q_SCRIPTABLE Q_NOREPLY void setTargetRotation(double rotation);

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
             * @param eastToTheRight When the image is rotated, so that North is up, East would be to the right.
             */
        void solverFinished(double orientation, double ra, double dec, double pixscale, bool eastToTheRight);

        void solverComplete();

        /**
         * @brief syncTargetToScope set Target Coordinates as the current mount coordinates.
         */
        void syncTargetToMount();

        /**
             * @brief Process solver failure.
             */
        void solverFailed();

        /**
             * @brief We received new telescope info, process them and update FOV.
             */
        bool syncTelescopeInfo();

        void setFocusStatus(Ekos::FocusState state);

        // Log
        void appendLogText(const QString &);

        // Capture
        void setCaptureComplete();

        // Update Capture Module status
        void setCaptureStatus(Ekos::CaptureState newState);
        // Update Mount module status
        void setMountStatus(ISD::Telescope::Status newState);
        //        void setMountCoords(const QString &ra, const QString &dec, const QString &az,
        //                            const QString &alt, int pierSide, const QString &ha);

        // Align Settings
        QJsonObject getSettings() const;
        void setSettings(const QJsonObject &settings);

        void zoomAlignView();
        void setAlignZoom(double scale);

    private slots:

        void setDefaultCCD(QString ccd);

        void saveSettleTime();

        // Solver timeout
        void checkAlignmentTimeout();
        void setAlignTableResult(AlignResult result);

        void updateTelescopeType(int index);

        // External View
        void showFITSViewer();
        void toggleAlignWidgetFullScreen();

        /**
         * @brief prepareCapture Set common settings for capture for align module
         * @param targetChip target Chip
         */
        void prepareCapture(ISD::CCDChip *targetChip);

        //Solutions Display slots
        void buildTarget();
        void handlePointTooltip(QMouseEvent *event);
        void handleVerticalPlotSizeChange();
        void handleHorizontalPlotSizeChange();
        void selectSolutionTableRow(int row, int column);
        void slotClearAllSolutionPoints();
        void slotRemoveSolutionPoint();
        void slotAutoScaleGraph();

        void slotMountModel();

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

        void processPAHStage(int stage);

    signals:
        void newLog(const QString &text);
        void newStatus(Ekos::AlignState state);
        void newSolution(const QVariantMap &solution);

        // This is sent when we load an image in the view
        void newImage(FITSView *view);
        // This is sent when the pixmap is updated within the view
        void newFrame(FITSView *view);
        // Send new solver results
        void newSolverResults(double orientation, double ra, double dec, double pixscale);

        // Settings
        void settingsUpdated(const QJsonObject &settings);

    private:
        /**
         * @brief Retrieve the align status indicator
         */
        QProgressIndicator *getProgressStatus();

        /**
         * @brief Stop the progress animation in the solution table
         */
        void stopProgressAnimation();

        void exportSolutionPoints();

        /**
            * @brief Calculate Field of View of CCD+Telescope combination that we need to pass to astrometry.net solver.
            */
        void calculateFOV();

        /**
         * @brief calculateEffectiveFocalLength Calculate Focal Length purely form astrometric data.
         */
        void calculateEffectiveFocalLength(double newFOVW);

        /**
         * @brief calculateAlignTargetDiff Find the difference between aligned vs. target coordinates and update
         * the GUI accordingly.
         */
        void calculateAlignTargetDiff();

        /**
             * @brief Get formatted RA & DEC coordinates compatible with astrometry.net format.
             * @param ra Right ascension
             * @param dec Declination
             * @param ra_str will contain the formatted RA string
             * @param dec_str will contain the formatted DEC string
             */
        void getFormattedCoords(double ra, double dec, QString &ra_str, QString &dec_str);

        uint8_t getSolverDownsample(uint16_t binnedW);

        /**
             * @brief setWCSEnabled enables/disables World Coordinate System settings in the CCD driver.
             * @param enable true to enable WCS, false to disable.
             */
        void setWCSEnabled(bool enable);

        void resizeEvent(QResizeEvent *event) override;

        KPageWidgetItem *m_IndexFilesPage;
        QString savedOptionsProfiles;

        /**
         * @brief React when a mount motion has been detected
         */
        void handleMountMotion();

        /**
         * @brief Continue aligning according to the current mount status
         */
        void handleMountStatus();

        /**
         * @brief initPolarAlignmentAssistant Initialize Polar Alignment Asssistant Tool
         */
        void initPolarAlignmentAssistant();

        /**
         * @brief initManualRotator Initialize Manual Rotator Tool
         */
        void initManualRotator();

        bool matchPAHStage(uint32_t stage);


        // Effective FOV

        /**
         * @brief getEffectiveFOV Search database for effective FOV that matches the current profile and settings
         * @return Variant Map containing effect FOV data or empty variant map if none found
         */
        QVariantMap getEffectiveFOV();
        void saveNewEffectiveFOV(double newFOVW, double newFOVH);
        QList<QVariantMap> effectiveFOVs;
        void syncFOV();

        // We are using calculated FOV now until a more accurate effective FOV is found.
        bool m_EffectiveFOVPending { false };
        /// Which chip should we invoke in the current CCD?
        bool useGuideHead { false };
        /// Can the mount sync its coordinates to those set by Ekos?
        bool canSync { false };
        // m_SolveFromFile is true we load an image and solve it, no capture is done.
        bool m_SolveFromFile { false };
        // Target Position Angle of solver Load&Slew image to be used for rotator if necessary
        double loadSlewTargetPA { std::numeric_limits<double>::quiet_NaN() };
        double currentRotatorPA { -1 };
        /// Solver iterations count
        uint8_t solverIterations { 0 };
        /// Was solving with scale off used?
        BlindState useBlindScale {BLIND_IDLE};
        /// Was solving with position off used?
        BlindState useBlindPosition {BLIND_IDLE};

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
        double sOrientation { INVALID_VALUE };
        double sRA { INVALID_VALUE };
        double sDEC { INVALID_VALUE };

        /// Solver alignment coordinates
        SkyPoint alignCoord;
        /// Target coordinates we need to slew to
        SkyPoint targetCoord;
        /// Actual current telescope coordinates
        SkyPoint telescopeCoord;
        /// Coord from Load & Slew
        SkyPoint loadSlewCoord;
        /// Difference between solution and target coordinate
        double m_TargetDiffTotal { 1e6 };
        double m_TargetDiffRA { 1e6 };
        double m_TargetDiffDE { 1e6 };

        /// Progress icon if the solver is running
        std::unique_ptr<QProgressIndicator> pi;

        /// Keep track of how long the solver is running
        QElapsedTimer solverTimer;

        // StellarSolver Profiles
        std::unique_ptr<StellarSolver> m_StellarSolver;
        QList<SSolver::Parameters> m_StellarSolverProfiles;

        /// Have we slewed?
        bool m_wasSlewStarted { false };
        // Above flag only stays false for 10s after slew start.
        QElapsedTimer slewStartTimer;
        bool didSlewStart();
        // Only wait this many milliseconds for slew to start.
        // Otherwise assume it has begun.
        static constexpr int MAX_WAIT_FOR_SLEW_START_MSEC = 10000;

        // Online and Offline parsers
        AstrometryParser* parser { nullptr };
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
        std::shared_ptr<FOV> solverFOV;
        std::shared_ptr<FOV> sensorFOV;

        /// WCS
        bool m_wcsSynced { false };

        /// Log
        QStringList m_LogText;

        /// Issue counters
        uint8_t m_CaptureTimeoutCounter { 0 };
        uint8_t m_CaptureErrorCounter { 0 };
        uint8_t m_SlewErrorCounter { 0 };

        QTimer m_CaptureTimer;

        // State
        AlignState state { ALIGN_IDLE };
        FocusState m_FocusState { FOCUS_IDLE };
        CaptureState m_CaptureState { CAPTURE_IDLE };

        // Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
        ISD::CCD::UploadMode rememberUploadMode { ISD::CCD::UPLOAD_CLIENT };

        GotoMode m_CurrentGotoMode;

        QString dirPath;

        // Timer
        QTimer m_AlignTimer;

        // Align Frame
        AlignView *alignView { nullptr };

        // FITS Viewer in case user want to display in it instead of internal view
        QPointer<FITSViewer> fv;

        QUrl alignURL;
        QUrl alignURLPath;

        // keep track of autoWSC
        bool rememberAutoWCS { false };
        bool rememberSolverWCS { false };
        //bool rememberMeridianFlip { false };

        // Differential Slewing
        bool differentialSlewingActivated { false };
        bool targetAccuracyNotMet { false };

        // Astrometry Options
        OpsAstrometry *opsAstrometry { nullptr };
        OpsAlign *opsAlign { nullptr };
        OpsPrograms *opsPrograms { nullptr };
        OpsAstrometryIndexFiles *opsAstrometryIndexFiles { nullptr };
        OpsASTAP *opsASTAP { nullptr };
        StellarSolverProfileEditor *optionsProfileEditor { nullptr };

        // Drawing
        QCPCurve *centralTarget { nullptr };
        QCPCurve *yellowTarget { nullptr };
        QCPCurve *redTarget { nullptr };
        QCPCurve *concentricRings { nullptr };

        // Telescope Settings
        ISD::CCD::TelescopeType rememberTelescopeType = { ISD::CCD::TELESCOPE_UNKNOWN };
        double primaryFL = -1, primaryAperture = -1, guideFL = -1, guideAperture = -1;
        double primaryEffectiveFL = -1, guideEffectiveFL = -1;
        bool m_isRateSynced = false;
        bool domeReady = true;

        // CCD Exposure Looping
        bool rememberCCDExposureLooping = { false };

        // Controls
        double GainSpinSpecialValue {INVALID_VALUE};
        double TargetCustomGainValue {-1};

        // Filter Manager
        QSharedPointer<FilterManager> filterManager;

        // Data
        QSharedPointer<FITSData> m_ImageData;

        // Active Profile
        ProfileInfo *m_ActiveProfile { nullptr };

        // Threshold to notify settle time is 3 seconds
        static constexpr uint16_t DELAY_THRESHOLD_NOTIFY { 3000 };

        // Mount Model
        // N.B. We do not need to use "smart pointer" here as the object memroy
        // is taken care of by the Qt framework.
        MountModel *m_MountModel {nullptr};
        PolarAlignmentAssistant *m_PolarAlignmentAssistant {nullptr};
        ManualRotator *m_ManualRotator {nullptr};

};
}
