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
#include "indi/indicamera.h"
#include "indi/indistd.h"
#include "indi/indimount.h"
#include "skypoint.h"

#include <QTime>
#include <QTimer>
#include <QElapsedTimer>
#include <KConfigDialog>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QtDBus/qtdbusglobal.h>
#elif QT_VERSION >= QT_VERSION_CHECK(5, 8, 0)
#include <qtdbusglobal.h>
#else
#include <qdbusmacros.h>
#endif

#include <stellarsolver.h>
#include <memory>

class QProgressIndicator;

class AlignView;
class FITSViewer;
class FOV;
class StarObject;
class ProfileInfo;
class RotatorSettings;

namespace Ekos
{
class AstrometryParser;
class DarkProcessor;
class FilterManager;
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
 *@version 2.0
 */
class Align : public QWidget, public Ui::Align
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Align")
        Q_PROPERTY(Ekos::AlignState status READ status NOTIFY newStatus)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)
        Q_PROPERTY(QString opticalTrain READ opticalTrain WRITE setOpticalTrain)
        Q_PROPERTY(QString camera READ camera)
        Q_PROPERTY(QString filterWheel READ filterWheel)
        Q_PROPERTY(QString filter READ filter WRITE setFilter)
        Q_PROPERTY(double exposure READ exposure WRITE setExposure)
        Q_PROPERTY(double targetPositionAngle MEMBER m_TargetPositionAngle)
        Q_PROPERTY(QList<double> fov READ fov)
        Q_PROPERTY(QList<double> cameraInfo READ cameraInfo)
        Q_PROPERTY(QList<double> telescopeInfo READ telescopeInfo)
        //Q_PROPERTY(QString solverArguments READ solverArguments WRITE setSolverArguments)

    public:
        explicit Align(const QSharedPointer<ProfileInfo> &activeProfile);
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
        Q_SCRIPTABLE QString camera();

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
            return alignExposure->value();
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
             * @brief Add Camera to the list of available Cameras.
             * @param device pointer to camera device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setCamera(ISD::Camera *device);

        /**
             * @brief addFilterWheel Add new filter wheel filter device.
             * @param device pointer to filter device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setFilterWheel(ISD::FilterWheel *device);

        /**
             * @brief Add new mount
             * @param device pointer to mount device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setMount(ISD::Mount *device);

        /**
             * @brief Add new Dome
             * @param device pointer to dome device.
             * @return True if added successfully, false if duplicate or failed to add.
             */
        bool setDome(ISD::Dome *device);

        /**
             * @brief Add new Rotator
             * @param device pointer to rotator device.
             */
        void setRotator(ISD::Rotator *device);

        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);

        /**
             * @brief setAstrometryDevice
             * @param newAstrometry
             */
        void setAstrometryDevice(const QSharedPointer<ISD::GenericDevice> &device);

        /**
             * @brief CCD information is updated, sync them.
             */
        void syncCameraInfo();

        /**
         * @brief syncCCDControls Update camera controls like gain, offset, ISO..etc.
         */
        void syncCameraControls();

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
        Q_SCRIPTABLE void clearLog();

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

        void setupFilterManager();
        void setupPlot();
        void setupSolutionTable();
        void setupOptions();

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
             * @param prop INDI Property
             */
        void updateProperty(INDI::Property prop);


        /**
             * @brief Check CCD and make sure information is updated and FOV is re-calculated.
             * @param CCDNum By default, we check the already selected CCD in the dropdown menu. If CCDNum is specified, the check is made against this specific CCD in the dropdown menu. CCDNum is the index of the CCD in the dropdown menu.
             */
        void checkCamera();

        /**
             * @brief Check Filter and make sure information is updated accordingly.
             * @param filterNum By default, we check the already selected filter in the dropdown menu. If filterNum is specified, the check is made against this specific filter in the dropdown menu.
             *  filterNum is the index of the filter in the dropdown menu.
             */
        void checkFilter();

        /**
             * @brief checkCameraExposureProgress Track the progress of CCD exposure
             * @param targetChip Target chip under exposure
             * @param remaining how many seconds remaining
             * @param state status of exposure
             */
        void checkCameraExposureProgress(ISD::CameraChip *targetChip, double remaining, IPState state);
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
             * @return Returns true if the procedure started successful, false otherwise (return true, not false, when retrying!)
             */
        Q_SCRIPTABLE bool captureAndSolve(bool initialCall = true);

        /** DBUS interface function.
             * Loads an image (FITS, RAW, or JPG/PNG) and solve its coordinates, then it slews to the solved coordinates and an image is captured and solved to ensure
             * the telescope is pointing to the same coordinates of the image.
             * @param fileURL URL to the image to solve
             */
        Q_SCRIPTABLE bool loadAndSlew(QString fileURL = QString());

        /** DBUS interface function.
             * Sets the target coordinates that the solver compares the solution coordinates to.
             * By default, the target coordinates are those of the current mount when the capture and
             * solve operation is started. In case of SYNC, only the error between the solution and target
             * coordinates is calculated. When Slew to Target is selected, the mount would be slewed afterwards to
             * this target coordinate.
             * @param ra0 J2000 Right Ascension in hours.
             * @param de0 J2000 Declination in degrees.
             */
        Q_SCRIPTABLE Q_NOREPLY void setTargetCoords(double ra0, double de0);

        /**
         * @brief getTargetCoords QList of target coordinates.
         * @return First value is J2000 RA in hours. Second value is J2000 DE in degrees.
         */
        Q_SCRIPTABLE QList<double> getTargetCoords();


        /**
          * @brief Set the alignment target where the mount is expected to point at.
          * @param targetCoord exact coordinates of the target position.
          */
        void setTarget(const SkyPoint &targetCoord);

        /**
         * @brief Set the coordinates that the mount reports as its position
         * @param position current mount position
         */
        void setTelescopeCoordinates(const SkyPoint &position)
        {
            m_TelescopeCoord = position;
        }

        Q_SCRIPTABLE Q_NOREPLY void setTargetPositionAngle(double value);

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
        void setMountStatus(ISD::Mount::Status newState);

        void zoomAlignView();
        void setAlignZoom(double scale);

        // Manual Rotator Dialog
        void toggleManualRotator(bool toggled);

        /**
         * @brief checkIfRotationRequired Check whether we need to perform an ALIGN_ROTATING action, whether manual or automatic.
         * @return True if rotation is required as per the settings, false is not required.
         */
        bool checkIfRotationRequired();

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);

        /**
         * @brief settleSettings Run this function after timeout from debounce timer to update database
         * and emit settingsChanged signal. This is required so we don't overload output.
         */
        void settleSettings();

        // Trains
        QString opticalTrain() const
        {
            return opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrainCombo->setCurrentText(value);
        }

        Ekos::OpsAlign *getAlignOptionsModule()
        {
            return opsAlign;
        }

    private slots:
        // Solver timeout
        void checkAlignmentTimeout();
        void setAlignTableResult(AlignResult result);

        // External View
        void showFITSViewer();
        void toggleAlignWidgetFullScreen();

        /**
         * @brief prepareCapture Set common settings for capture for align module
         * @param targetChip target Chip
         */
        void prepareCapture(ISD::CameraChip *targetChip);

        //Solutions Display slots
        void buildTarget();
        void handlePointTooltip(QMouseEvent *event);
        void handleVerticalPlotSizeChange();
        void handleHorizontalPlotSizeChange();
        void selectSolutionTableRow(int row, int column);
        void slotClearAllSolutionPoints();
        void slotRemoveSolutionPoint();
        void slotAutoScaleGraph();

        // Model
        void slotMountModel();

        // Capture Timeout
        void processCaptureTimeout();

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
        void newPAAStage(int stage);
        void newSolution(const QVariantMap &solution);

        // This is sent when we load an image in the view
        void newImage(const QSharedPointer<FITSView> &view);
        // This is sent when the pixmap is updated within the view
        void newFrame(const QSharedPointer<FITSView> &view);
        // Send new solver results
        void newSolverResults(double orientation, double ra, double dec, double pixscale);

        // Train changed
        void trainChanged();

        // Settings
        void settingsUpdated(const QVariantMap &settings);

        // Manual Rotator
        void manualRotatorChanged(double currentPA, double targetPA, double pixscale);

        // Astrometry index files progress
        void newDownloadProgress(QString info);

    private:

        void setupOpticalTrainManager();
        void refreshOpticalTrain();

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

        void setState (AlignState value);
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
        void setupPolarAlignmentAssistant();

        void setupRotatorControl();

        /**
         * @brief initManualRotator Initialize Manual Rotator Tool
         */
        void setupManualRotator();

        /**
         * @brief initDarkProcessor Initialize Dark Processor
         */
        void setuptDarkProcessor();

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
        double m_TargetPositionAngle { std::numeric_limits<double>::quiet_NaN() };
        // Target Pierside of solver Load&Slew image to be used
        ISD::Mount::PierSide m_TargetPierside = ISD::Mount::PIER_UNKNOWN;
        double currentRotatorPA { -1 };
        /// Solver iterations count
        uint8_t solverIterations { 0 };
        /// Was solving with scale off used?
        BlindState useBlindScale {BLIND_IDLE};
        /// Was solving with position off used?
        BlindState useBlindPosition {BLIND_IDLE};

        // FOV
        double m_CameraPixelWidth { -1 };
        double m_CameraPixelHeight { -1 };
        uint16_t m_CameraWidth { 0 };
        uint16_t m_CameraHeight { 0 };

        double m_FocalLength {-1};
        double m_Aperture {-1};
        double m_FocalRatio {-1};
        double m_Reducer = {-1};

        double m_FOVWidth { 0 };
        double m_FOVHeight { 0 };
        double m_FOVPixelScale { 0 };

        // Keep raw rotator angle
        double sRawAngle { INVALID_VALUE };
        // Keep track of solver results
        double sOrientation { INVALID_VALUE };
        double sRA { INVALID_VALUE };
        double sDEC { INVALID_VALUE };

        /// Solver alignment coordinates
        SkyPoint m_AlignCoord;
        /// Target coordinates the mount will slew to
        SkyPoint m_TargetCoord;
        /// Final coordinates we want to reach in case of differential align
        SkyPoint m_DestinationCoord;
        /// Current telescope coordinates
        SkyPoint m_TelescopeCoord;
        /// Difference between solution and target coordinate
        double m_TargetDiffTotal { 1e6 };
        double m_TargetDiffRA { 1e6 };
        double m_TargetDiffDE { 1e6 };

        /// Progress icon if the solver is running
        std::unique_ptr<QProgressIndicator> pi;

        /// Keep track of how long the solver is running
        QElapsedTimer solverTimer;

        // The StellarSolver
        std::unique_ptr<StellarSolver> m_StellarSolver;
        // StellarSolver Profiles
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
        QSharedPointer<ISD::GenericDevice> m_RemoteParserDevice;

        // Pointers to our devices
        ISD::Mount *m_Mount { nullptr };
        ISD::Dome *m_Dome { nullptr };
        ISD::Camera *m_Camera { nullptr };
        ISD::Rotator *m_Rotator { nullptr };
        ISD::FilterWheel *m_FilterWheel { nullptr };

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
        bool m_resetCaptureTimeoutCounter = false;

        QTimer m_CaptureTimer;

        // State
        AlignState state { ALIGN_IDLE };
        FocusState m_FocusState { FOCUS_IDLE };
        CaptureState m_CaptureState { CAPTURE_IDLE };

        // Track which upload mode the CCD is set to. If set to UPLOAD_LOCAL, then we need to switch it to UPLOAD_CLIENT in order to do focusing, and then switch it back to UPLOAD_LOCAL
        ISD::Camera::UploadMode rememberUploadMode { ISD::Camera::UPLOAD_CLIENT };

        GotoMode m_CurrentGotoMode;

        QString dirPath;

        // Timer
        QTimer m_AlignTimer;
        QTimer m_DebounceTimer;

        // Align Frame
        QSharedPointer<AlignView> m_AlignView;

        // FITS Viewer in case user want to display in it instead of internal view
        QSharedPointer<FITSViewer> fv;

        QUrl alignURL;
        QUrl alignURLPath;

        // keep track of autoWSC
        bool rememberAutoWCS { false };
        bool rememberSolverWCS { false };

        // move rotator
        bool RotatorGOTO { false };

        // Align slew
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
        double m_EffectiveFocalLength = -1;
        bool m_isRateSynced = false;

        // CCD Exposure Looping
        bool m_RememberCameraFastExposure = { false };

        // Controls
        double alignGainSpecialValue {INVALID_VALUE};
        double TargetCustomGainValue {-1};

        // Data
        QSharedPointer<FITSData> m_ImageData;

        // Active Profile
        QSharedPointer<ProfileInfo> m_ActiveProfile;

        // Threshold to notify settle time is 3 seconds
        static constexpr uint16_t DELAY_THRESHOLD_NOTIFY { 3000 };

        // Mount Model
        // N.B. We do not need to use "smart pointer" here as the object memroy
        // is taken care of by the Qt framework.
        MountModel *m_MountModel {nullptr};
        PolarAlignmentAssistant *m_PolarAlignmentAssistant {nullptr};
        ManualRotator *m_ManualRotator {nullptr};

        // Dark Processor
        QPointer<DarkProcessor> m_DarkProcessor;

        // Filter Manager
        QSharedPointer<FilterManager> m_FilterManager;

        // Rotator Control
        QSharedPointer<RotatorSettings> m_RotatorControlPanel;
        int m_RotatorTimeFrame = 0;
        bool m_estimateRotatorTimeFrame = false;

        // Settings
        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;

        bool m_UsedScale = false;
        bool m_UsedPosition = false;
        double m_ScaleUsed = 0;
        double m_RAUsed = 0;
        double m_DECUsed = 0;
};
}
