/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_capture.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"
#include "sequencejobstate.h"
// #include "ekos/manager.h"
#include "ekos/manager/meridianflipstate.h"
#include "customproperties.h"
#include "ekos/ekos.h"
#include "ekos/mount/mount.h"
#include "indi/indicamera.h"
#include "indi/indidustcap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/indimount.h"
#include "ekos/auxiliary/darkprocessor.h"
#include "ekos/auxiliary/rotatorutils.h"
#include "dslrinfodialog.h"
#include "ui_limits.h"

#include <QTimer>
#include <QUrl>
#include <QDBusInterface>

#include <memory>

class QProgressIndicator;
class QTableWidgetItem;
class KDirWatch;
class RotatorSettings;

/**
 * @namespace Ekos
 * @short Ekos is an advanced Astrophotography tool for Linux.
 * It is based on a modular extensible framework to perform common astrophotography tasks. This includes highly accurate GOTOs using astrometry solver, ability to measure and correct polar alignment errors ,
 * auto-focus & auto-guide capabilities, and capture of single or stack of images with filter wheel support.\n
 * Features:
 * - Control your telescope, CCD (& DSLRs), filter wheel, focuser, guider, adaptive optics unit, and any INDI-compatible auxiliary device from Ekos.
 * - Extremely accurate GOTOs using astrometry.net solver (both Online and Offline solvers supported).
 * - Load & Slew: Load a FITS image, slew to solved coordinates, and center the mount on the exact image coordinates in order to get the same desired frame.
 * - Measure & Correct Polar Alignment errors using astrometry.net solver.
 * - Auto and manual focus modes using Half-Flux-Radius (HFR) method.
 * - Automated unattended meridian flip. Ekos performs post meridian flip alignment, calibration, and guiding to resume the capture session.
 * - Automatic focus between exposures when a user-configurable HFR limit is exceeded.
 * - Automatic focus between exposures when the temperature has changed a lot since last focus.
 * - Auto guiding with support for automatic dithering between exposures and support for Adaptive Optics devices in addition to traditional guiders.
 * - Powerful sequence queue for batch capture of images with optional prefixes, timestamps, filter wheel selection, and much more!
 * - Export and import sequence queue sets as Ekos Sequence Queue (.esq) files.
 * - Center the telescope anywhere in a captured FITS image or any FITS with World Coordinate System (WCS) header.
 * - Automatic flat field capture, just set the desired ADU and let Ekos does the rest!
 * - Automatic abort and resumption of exposure tasks if guiding errors exceed a user-configurable value.
 * - Support for dome slaving.
 * - Complete integration with KStars Observation Planner and SkyMap
 * - Integrate with all INDI native devices.
 * - Powerful scripting capabilities via \ref EkosDBusInterface "DBus."
 *
 * The primary class is Ekos::Manager. It handles startup and shutdown of local and remote INDI devices, manages and orchesterates the various Ekos modules, and provides advanced DBus
 * interface to enable unattended scripting.
 *
 * @author Jasem Mutlaq
 * @version 1.9
 */
namespace Ekos
{

class SequenceJob;

/**
 *@class Capture
 *@short Captures single or sequence of images from a CCD.
 * The capture class support capturing single or multiple images from a CCD, it provides a powerful sequence queue with filter wheel selection. Any sequence queue can be saved as Ekos Sequence Queue (.esq).
 * All image capture operations are saved as Sequence Jobs that encapsulate all the different options in a capture process. The user may select in sequence autofocusing by setting a maximum HFR limit. When the limit
 * is exceeded, it automatically trigger autofocus operation. The capture process can also be linked with guide module. If guiding deviations exceed a certain threshold, the capture operation aborts until
 * the guiding deviation resume to acceptable levels and the capture operation is resumed.
 *@author Jasem Mutlaq
 *@version 1.4
 */
class Capture : public QWidget, public Ui::Capture
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Capture")
        Q_PROPERTY(Ekos::CaptureState status READ status NOTIFY newStatus)
        Q_PROPERTY(QString targetName READ getTargetName WRITE setTargetName)
        Q_PROPERTY(QString observerName MEMBER m_ObserverName)
        Q_PROPERTY(QString opticalTrain READ opticalTrain WRITE setOpticalTrain)
        Q_PROPERTY(QString camera READ camera)
        Q_PROPERTY(QString filterWheel READ filterWheel)
        Q_PROPERTY(QString filter READ filter WRITE setFilter)
        Q_PROPERTY(bool coolerControl READ hasCoolerControl WRITE setCoolerControl)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

    public:
        typedef enum
        {
            ADU_LEAST_SQUARES,
            ADU_POLYNOMIAL
        } ADUAlgorithm;

        typedef enum
        {
            NOT_PREVIEW,
            LOCAL_PREVIEW,
            REMOTE_PREVIEW
        } FilenamePreviewType;

        Capture();
        ~Capture();

        /** @defgroup CaptureDBusInterface Ekos DBus Interface - Capture Module
             * Ekos::Capture interface provides advanced scripting capabilities to capture image sequences.
             * @{
            */


        /** DBUS interface function.
             * select the CCD device from the available CCD drivers.
             * @param device The CCD device name
             */
        Q_SCRIPTABLE QString camera();

        /** DBUS interface function.
             * select the filter device from the available filter drivers. The filter device can be the same as the CCD driver if the filter functionality was embedded within the driver.
             * @param device The filter device name
             */
        Q_SCRIPTABLE QString filterWheel();

        /** DBUS interface function.
             * select the filter name from the available filters in case a filter device is active.
             * @param filter The filter name
             */
        Q_SCRIPTABLE bool setFilter(const QString &filter);
        Q_SCRIPTABLE QString filter();

        /** DBUS interface function.
             * Aborts any current jobs and remove all sequence queue jobs.
             */
        Q_SCRIPTABLE Q_NOREPLY void clearSequenceQueue();

        /** DBUS interface function.
             * Returns the overall sequence queue status. If there are no jobs pending, it returns "Invalid". If all jobs are idle, it returns "Idle". If all jobs are complete, it returns "Complete". If one or more jobs are aborted
             * it returns "Aborted" unless it was temporarily aborted due to guiding deviations, then it would return "Suspended". If one or more jobs have errors, it returns "Error". If any jobs is under progress, returns "Running".
             */
        Q_SCRIPTABLE QString getSequenceQueueStatus();

        /** DBUS interface function.
             * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
             * @param fileURL full URL of the filename
             * @param ignoreTarget ignore target defined in the sequence queue file (necessary for using the target of the scheduler)
             */
        Q_SCRIPTABLE bool loadSequenceQueue(const QString &fileURL, bool ignoreTarget = false);

        /** DBUS interface function.
             * Saves the Sequence Queue to the Ekos Sequence Queue file.
             * @param fileURL full URL of the filename
             */
        Q_SCRIPTABLE bool saveSequenceQueue(const QString &path);

        /** DBUS interface function.
             * Enables or disables the maximum guiding deviation and sets its value.
             * @param enable If true, enable the guiding deviation check, otherwise, disable it.
             * @param value if enable is true, it sets the maximum guiding deviation in arcsecs. If the value is exceeded, the capture operation is aborted until the value falls below the value threshold.
             */
        Q_SCRIPTABLE Q_NOREPLY void setMaximumGuidingDeviation(bool enable, double value);

        /** DBUS interface function.
             * Enables or disables the in sequence focus and sets Half-Flux-Radius (HFR) limit.
             * @param enable If true, enable the in sequence auto focus check, otherwise, disable it.
             * @param HFR if enable is true, it sets HFR in pixels. After each exposure, the HFR is re-measured and if it exceeds the specified value, an autofocus operation will be commanded.
             */
        Q_SCRIPTABLE Q_NOREPLY void setInSequenceFocus(bool enable, double HFR);

        /** DBUS interface function.
             * Does the CCD has a cooler control (On/Off) ?
             */
        Q_SCRIPTABLE bool hasCoolerControl();

        /** DBUS interface function.
             * Set the CCD cooler ON/OFF
             *
             */
        Q_SCRIPTABLE bool setCoolerControl(bool enable);

        /** DBUS interface function.
             * @return Returns the percentage of completed captures in all active jobs
             */
        Q_SCRIPTABLE double getProgressPercentage();

        /** DBUS interface function.
             * @return Returns the number of jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getJobCount()
        {
            return m_captureModuleState->allJobs().count();
        }

        /** DBUS interface function.
             * @return Returns the number of pending uncompleted jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getPendingJobCount();

        /** DBUS interface function.
             * @return Returns ID of current active job if any, or -1 if there are no active jobs.
             */
        Q_SCRIPTABLE int getActiveJobID();

        /** DBUS interface function.
             * @return Returns time left in seconds until active job is estimated to be complete.
             */
        Q_SCRIPTABLE int getActiveJobRemainingTime();

        /** DBUS interface function.
             * @return Returns overall time left in seconds until all jobs are estimated to be complete
             */
        Q_SCRIPTABLE int getOverallRemainingTime();

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job state (Idle, In Progress, Error, Aborted, Complete)
             */
        Q_SCRIPTABLE QString getJobState(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job filter name.
             */
        Q_SCRIPTABLE QString getJobFilterName(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns The number of images completed capture in the job.
             */
        Q_SCRIPTABLE int getJobImageProgress(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total number of images to capture in the job.
             */
        Q_SCRIPTABLE int getJobImageCount(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the number of seconds left in an exposure operation.
             */
        Q_SCRIPTABLE double getJobExposureProgress(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total requested exposure duration in the job.
             */
        Q_SCRIPTABLE double getJobExposureDuration(int id);

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the frame type (light, dark, ...) of the job.
             */
        Q_SCRIPTABLE CCDFrameType getJobFrameType(int id);

        /** DBUS interface function.
             * Clear in-sequence focus settings. It sets the autofocus HFR to zero so that next autofocus value is remembered for the in-sequence focusing.
             */
        Q_SCRIPTABLE Q_NOREPLY void clearAutoFocusHFR();

        /** DBUS interface function.
             * Jobs will NOT be checked for progress against the file system and will be always assumed as new jobs.
             */
        Q_SCRIPTABLE Q_NOREPLY void ignoreSequenceHistory();

        /** DBUS interface function.
             * Set count of already completed frames. This is required when we have identical external jobs
             * with identical paths, but we need to continue where we left off. For example, if we have 3 identical
             * jobs, each capturing 5 images. Let's suppose 9 images were captured before. If the count for this signature
             * is set to 1, then we continue to capture frame #2 even though the number of completed images is already
             * larger than required count (5). It is mostly used in conjunction with Ekos Scheduler.
             */
        Q_SCRIPTABLE Q_NOREPLY void setCapturedFramesMap(const QString &signature, int count);


        /** DBUS interface function.
         *  List of logging entries for the capture module.
         */
        Q_SCRIPTABLE QStringList logText()
        {
            return m_LogText;
        }

        /** DBUS interface function.
         *  Single text string holding all log lines for the capture module.
         */
        Q_SCRIPTABLE QString getLogText()
        {
            return m_LogText.join("\n");
        }

        /** DBUS interface function.
         *  Status of the capture module
         */
        Q_SCRIPTABLE CaptureState status()
        {
            return m_captureModuleState->getCaptureState();
        }
        /** @} end of group CaptureDBusInterface */


        // ////////////////////////////////////////////////////////////////////
        // Changing the devices used by Capture
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief Add new Camera
         * @param device pointer to camera device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setCamera(ISD::Camera *device);

        // FIXME add support for guide head, not implemented yet
        void addGuideHead(ISD::Camera *device);

        /**
         * @brief Add new Filter Wheel
         * @param device pointer to filter wheel device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setFilterWheel(ISD::FilterWheel *device);

        /**
         * @brief Add new Dome
         * @param device pointer to Dome device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setDome(ISD::Dome *device);

        /**
         * @brief Add new Dust Cap
         * @param device pointer to Dust Cap device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setDustCap(ISD::DustCap *device);

        /**
         * @brief Add new Light Box
         * @param device pointer to Light Box device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setLightBox(ISD::LightBox *device);

        /**
         * @brief Add new Mount
         * @param device pointer to Mount device.
         * @return True if added successfully, false if duplicate or failed to add.
        */
        bool setMount(ISD::Mount *device);

        /**
         * @brief Add new Rotator
         * @param device pointer to rotator device.
        */
        void setRotator(ISD::Rotator *device);

        // Restart driver
        void reconnectDriver(const QString &camera, const QString &filterWheel);

        /**
         * @brief Generic method for removing any connected device.
         */
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device);

        /**
         * @brief registerNewModule Register an Ekos module as it arrives via DBus
         * and create the appropriate DBus interface to communicate with it.
         * @param name of module
         */
        void registerNewModule(const QString &name);

        // ////////////////////////////////////////////////////////////////////
        // Synchronize UI with device parameters
        // ////////////////////////////////////////////////////////////////////

        void syncFrameType(const QString &name);
        void syncTelescopeInfo();
        void syncCameraInfo();
        void syncFilterInfo();

        // ////////////////////////////////////////////////////////////////////
        // Optical Train handling
        // ////////////////////////////////////////////////////////////////////
        void setupOpticalTrainManager();
        void refreshOpticalTrain();

        QString opticalTrain() const
        {
            return opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            opticalTrainCombo->setCurrentText(value);
        }

        // ////////////////////////////////////////////////////////////////////
        // Rotator
        // ////////////////////////////////////////////////////////////////////
        const QSharedPointer<RotatorSettings> &RotatorControl() const
        {
            return m_RotatorControlPanel;
        }

        // ////////////////////////////////////////////////////////////////////
        // Filter Manager and filters
        // ////////////////////////////////////////////////////////////////////
        void setupFilterManager();

        const QSharedPointer<FilterManager> &filterManager() const
        {
            return m_FilterManager;
        }

        /**
         * @brief checkFilter Refreshes the filter wheel information in the capture module.
         */
        void checkFilter();

        /**
         * @brief shortcut for updating the current filter information for the state machine
         */
        void updateCurrentFilterPosition();

        // ////////////////////////////////////////////////////////////////////
        // Read and write access for EkosLive
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief getSequence Return the JSON representation of the current sequeue queue
         * @return Reference to JSON array containing sequence queue jobs.
         */
        const QJsonArray &getSequence() const
        {
            return m_SequenceArray;
        }

        /**
         * @brief setSettings Set capture settings
         * @param settings list of settings
         */
        void setPresetSettings(const QJsonObject &settings);

        /**
         * @brief getSettings get current capture settings as a JSON Object
         * @return settings as JSON object
         */
        QJsonObject getPresetSettings();

        /**
         * @brief setFileSettings Set File Settings
         * @param settings as JSON object
         */
        void setFileSettings(const QJsonObject &settings);
        /**
         * @brief getFileSettings Compile file setting
         * @return File settings as JSON object
         */
        QJsonObject getFileSettings();

        /**
         * @brief setCalibrationSettings Set Calibration settings
         * @param settings as JSON object
         */
        void setCalibrationSettings(const QJsonObject &settings);
        /**
         * @brief getCalibrationSettings Get Calibration settings
         * @return settings as JSON object
         */
        QJsonObject getCalibrationSettings();

        /**
         * @brief setLimitSettings Set limit settings
         * @param settings as JSON Object
         */
        void setLimitSettings(const QJsonObject &settings);
        /**
         * @brief getLimitSettings Get Limit Settings
         * @return settings as JSON Object
         */
        QJsonObject getLimitSettings();

        /**
         * @brief setVideoLimits sets the buffer size and max preview fps for live preview
         * @param maxBufferSize in bytes
         * @param maxPreviewFPS number of frames per second
         * @return True if value is updated, false otherwise.
         */
        bool setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS);

        // ////////////////////////////////////////////////////////////////////
        // DSLR handling
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief addDSLRInfo Save DSLR Info the in the database. If the interactive dialog was open, close it.
         * @param model Camera name
         * @param maxW Maximum width in pixels
         * @param maxH Maximum height in pixels
         * @param pixelW Pixel horizontal size in microns
         * @param pixelH Pizel vertical size in microns
         */
        void addDSLRInfo(const QString &model, uint32_t maxW, uint32_t maxH, double pixelW, double pixelH);

        /**
         * @brief syncDSLRToTargetChip Syncs INDI driver CCD_INFO property to the DSLR values.
         * This include Max width, height, and pixel sizes.
         * @param model Name of camera driver in the DSLR database.
         */
        void syncDSLRToTargetChip(const QString &model);

		void openExposureCalculatorDialog();

        QSharedPointer<CaptureDeviceAdaptor> m_captureDeviceAdaptor;

    public slots:
        // ////////////////////////////////////////////////////////////////////
        // Main capturing actions
        // ////////////////////////////////////////////////////////////////////

        /** \addtogroup CaptureDBusInterface
             *  @{
             */

        /** DBUS interface function.
             * @brief Start the execution of the Capture::SequenceJob list #jobs.
             *
             * Starting the execution of the Capture::SequenceJob list selects the first job
             * from the list that may be executed and starts to prepare the job (@see prepareJob()).
             *
             * Several factors determine, which of the jobs will be selected:
             * - First, the list is searched to find the first job that is marked as idle or aborted.
             * -  If none is found, it is checked whether ignoring job progress is set. If yes,
             *    all jobs are are reset (@see reset()) and the first one from the list is selected.
             *    If no, the user is asked whether the jobs should be reset. If the user declines,
             *    starting is aborted.
             */
        Q_SCRIPTABLE Q_NOREPLY void start();

        /** DBUS interface function.
             * Stops currently running jobs:
             *           CAPTURE_IDLE: capture in idle state waiting for further action (e.g. single sequence
             *                         is complete, next one starting)
             *       CAPTURE_COMPLETE: all capture sequences are complete
             *          CAPTURE_ABORT: capture aborted either by user interaction or by a technical error
             *        CAPTURE_SUSPEND: capture suspended and waiting to be restarted
             * @param targetState status of the job after stop
             */
        Q_SCRIPTABLE Q_NOREPLY void stop(CaptureState targetState = CAPTURE_IDLE);

        /** DBUS interface function.
             * Aborts all jobs and mark current state as ABORTED. It simply calls stop(CAPTURE_ABORTED)
             */
        Q_SCRIPTABLE Q_NOREPLY void abort()
        {
            stop(CAPTURE_ABORTED);
        }

        /** DBUS interface function.
             * Aborts all jobs and mark current state as SUSPENDED. It simply calls stop(CAPTURE_SUSPENDED)
             * The only difference between SUSPENDED and ABORTED it that capture module can automatically resume a suspended
             * state on its own without external trigger once the right conditions are met. When whatever reason caused the module
             * to go into suspended state ceases to exist, the capture module automatically resumes. On the other hand, ABORTED state
             * must be started via an external programmatic or user trigger (e.g. click the start button again).
             */
        Q_SCRIPTABLE Q_NOREPLY void suspend()
        {
            stop(CAPTURE_SUSPENDED);
        }
        /** DBUS interface function.
         * @brief pause Pauses the Sequence Queue progress AFTER the current capture is complete.
         */
        Q_SCRIPTABLE Q_NOREPLY void pause();

        /** DBUS interface function.
         * @brief toggleSequence Toggle sequence state depending on its current state.
         * 1. If paused, then resume sequence.
         * 2. If idle or completed, then start sequence.
         * 3. Otherwise, abort current sequence.
         */
        Q_SCRIPTABLE Q_NOREPLY void toggleSequence();


        /** DBUS interface function.
             * Toggle video streaming if supported by the device.
             * @param enabled Set to true to start video streaming, false to stop it if active.
             */
        Q_SCRIPTABLE Q_NOREPLY void toggleVideo(bool enabled);


        /** DBus interface function
         * @brief restartCamera Restarts the INDI driver associated with a camera. Remote and Local drivers are supported.
         * @param name Name of camera to restart. If a driver defined multiple cameras, they would be removed and added again
         * after driver restart.
         * @note Restarting camera should only be used as a last resort when it comes completely unresponsive. Due the complex
         * nature of driver interactions with Ekos, restarting cameras can lead to unexpected behavior.
         */
        Q_SCRIPTABLE Q_NOREPLY void restartCamera(const QString &name);

        /** DBus interface function
         * @brief Set the name of the target to be captured.
         */
        Q_SCRIPTABLE Q_NOREPLY void setTargetName(const QString &newTargetName);
        Q_SCRIPTABLE QString getTargetName()
        {
            return m_captureModuleState->targetName();
        }

        /** @}*/

        // ////////////////////////////////////////////////////////////////////
        // Capture actons
        // ////////////////////////////////////////////////////////////////////
        /**
             * @brief captureOne Capture one preview image
             */
        void captureOne();

        /**
         * @brief startFraming Like captureOne but repeating.
         */
        void startFraming();

        /**
         * @brief generateDarkFlats Generate a list of dark flat jobs from available flat frames.
         */
        void generateDarkFlats();

        /**
             * @brief addJob Add a new job to the sequence queue given the settings in the GUI.
             * @param preview True if the job is a preview job, otherwise, it is added as a batch job.
             * @param isDarkFlat True if the job is a dark flat job, false otherwise.
             * @param filenamePreview if the job is to generate a preview filename
             * @return True if job is added successfully, false otherwise.
             */
        bool addJob(bool preview = false, bool isDarkFlat = false, FilenamePreviewType filenamePreview = NOT_PREVIEW);

        // ////////////////////////////////////////////////////////////////////
        // public capture settings
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief seqCount Set required number of images to capture in one sequence job
         * @param count number of images to capture
         */
        void setCount(uint16_t count)
        {
            captureCountN->setValue(count);
        }

        /**
         * @brief setDelay Set delay between capturing images within a sequence in seconds
         * @param delay numbers of seconds to wait before starting the next image.
         */
        void setDelay(uint16_t delay)
        {
            captureDelayN->setValue(delay);
        }

        /**
         * @brief Slot receiving the update of the current target distance.
         * @param targetDiff distance to the target in arcseconds.
         */
        void updateTargetDistance(double targetDiff);

        /**
             * @brief checkCamera Refreshes the CCD information in the capture module.
             */
        void checkCamera();


        /**
             * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
             * @param nvp pointer to number property.
             */
        void processCameraNumber(INDI::Property prop);


        /**
         * @brief removeJob Remove a job sequence from the queue
         * @param index Row index for job to remove, if left as -1 (default), the currently selected row will be removed.
         *        if no row is selected, the last job shall be removed.
         * @param true if sequence is removed. False otherwise.
         */
        bool removeJob(int index = -1);

        /**
         * @brief MeridianFlipState Access to the meridian flip state machine
         */
        QSharedPointer<MeridianFlipState> getMeridianFlipState();
        void setMeridianFlipState(QSharedPointer<MeridianFlipState> state);

        // ////////////////////////////////////////////////////////////////////
        // UI controls
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief addSequenceJob Add a sequence job. This simply calls addJob below with both preview and isDarkFlat set to false.
         * @return return result of addJob(..)
         */
        bool addSequenceJob();

        void removeJobFromQueue();

        /**
             * @brief moveJobUp Move the job in the sequence queue one place up.
             */
        void moveJobUp();

        /**
             * @brief moveJobDown Move the job in the sequence queue one place down.
             */
        void moveJobDown();

        /**
             * @brief setTemperature Set the target CCD temperature in the GUI settings.
             */
        void setTargetTemperature(double temperature)
        {
            cameraTemperatureN->setValue(temperature);
        }

        void setForceTemperature(bool enabled)
        {
            cameraTemperatureS->setChecked(enabled);
        }

        /**
         * @brief showTemperatureRegulation Toggle temperature regulation dialog which sets temperature ramp and threshold
         */
        void showTemperatureRegulation();

        // Clear Camera Configuration
        void clearCameraConfiguration();

        // ////////////////////////////////////////////////////////////////////
        // slots handling device and module events
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief captureStarted Manage the result when capturing has been started
         */
        void captureStarted(CAPTUREResult rc);

        /**
             * @brief setGuideDeviation Set the guiding deviation as measured by the guiding module. Abort capture
             *        if deviation exceeds user value. Resume capture if capture was aborted and guiding
             *         deviations are below user value.
             * @param delta_ra Deviation in RA in arcsecs from the selected guide star.
             * @param delta_dec Deviation in DEC in arcsecs from the selected guide star.
             */
        void setGuideDeviation(double delta_ra, double delta_dec);

        void setGuideChip(ISD::CameraChip *guideChip);

        /**
             * @brief updateCCDTemperature Update CCD temperature in capture module.
             * @param value Temperature in celcius.
             */
        void updateCCDTemperature(double value);

        // Auto Focus
        /**
         * @brief setFocusStatus Forward the new focus state to the capture module state machine
         */
        void setFocusStatus(FocusState state);

        /**
         * @brief updateFocusStatus Handle new focus state
         */
        void updateFocusStatus(FocusState state);

        // Adaptive Focus
        /**
         * @brief focusAdaptiveComplete Forward the new focus state to the capture module state machine
         */
        void focusAdaptiveComplete(bool success);

        /**
         * @brief updateAdaptiveFocusStatus Handle new focus state
         */

        void setFocusTemperatureDelta(double focusTemperatureDelta, double absTemperature);

        /**
         * @brief setHFR Receive the measured HFR value of the latest frame
         */
        void setHFR(double newHFR, int)
        {
            m_captureModuleState->getRefocusState()->setFocusHFR(newHFR);
        }

        // Filter
        void setFilterStatus(FilterState filterState);

        // Guide
        void setGuideStatus(GuideState state);

        // Align

        void setAlignStatus(Ekos::AlignState state);
        void setAlignResults(double solverPA, double ra, double de, double pixscale);

        // Update Mount module status
        void setMountStatus(ISD::Mount::Status newState);

        // Meridian flip interface
        void updateMFMountState(MeridianFlipState::MeridianFlipMountState status);

        // ////////////////////////////////////////////////////////////////////
        // Module logging
        // ////////////////////////////////////////////////////////////////////
        void clearLog();
        void appendLogText(const QString &);


    private slots:

        // ////////////////////////////////////////////////////////////////////
        // UI controls
        // ////////////////////////////////////////////////////////////////////
        void checkFrameType(int index);
        void resetFrame();
        void updateCaptureCountDown(int deltaMillis);
        void saveFITSDirectory();

        // Sequence Queue
        void loadSequenceQueue();
        void saveSequenceQueue();
        void saveSequenceQueueAs();

        // Jobs
        void resetJobs();
        bool selectJob(QModelIndex i);
        void editJob(QModelIndex i);
        void resetJobEdit();
        /**
         * @brief executeJob Start the execution of #activeJob by initiating updatePreCaptureCalibrationStatus().
         */
        void executeJob();

        // Flat field
        void openCalibrationDialog();

        // Observer
        void showObserverDialog();

        // Cooler
        void setCoolerToggled(bool enabled);

        // ////////////////////////////////////////////////////////////////////
        // slots handling device and module events
        // ////////////////////////////////////////////////////////////////////
        void setExposureProgress(ISD::CameraChip *tChip, double value, IPState state);

        /**
         * @brief setNewRemoteFile Report a new remote file
         */
        void setNewRemoteFile(QString file);

        // AutoGuide
        void checkGuideDeviationTimeout();

        // Script Manager
        void handleScriptsManager();

        // capture scripts
        void scriptFinished(int exitCode, QProcess::ExitStatus status);

        void setVideoStreamEnabled(bool enabled);

        /**
         * @brief Listen to device property changes (temperature, rotator) that are triggered by
         *        SequenceJob.
         */
        void updatePrepareState(CaptureState prepareState);

        // Rotator
        void updateRotatorAngle(double value);
        void setRotatorReversed(bool toggled);

        /**
         * @brief setDownloadProgress update the Capture Module and Summary
         *        Screen's estimate of how much time is left in the download
         */
        void setDownloadProgress();

    signals:
        Q_SCRIPTABLE void newLog(const QString &text);
        Q_SCRIPTABLE void meridianFlipStarted();
        Q_SCRIPTABLE void guideAfterMeridianFlip();
        Q_SCRIPTABLE void newStatus(CaptureState status);
        Q_SCRIPTABLE void captureComplete(const QVariantMap &metadata);

        void newFilterStatus(FilterState state);

        void ready();

        // signals to the sequence job
        void prepareCapture();

        // device status
        void newGuiderDrift(double deviation_rms);

        // communication with other modules
        void checkFocus(double);
        void resetFocus();
        void abortFocus();
        void adaptiveFocus();
        void suspendGuiding();
        void resumeGuiding();
        void newImage(SequenceJob *job, const QSharedPointer<FITSData> &data);
        void newExposureProgress(SequenceJob *job);
        void newDownloadProgress(double);
        void sequenceChanged(const QJsonArray &sequence);
        void settingsUpdated(const QJsonObject &settings);
        void newLocalPreview(const QString &preview);
        void dslrInfoRequested(const QString &cameraName);
        void driverTimedout(const QString &deviceName);

        // Signals for the Analyze tab.
        void captureStarting(double exposureSeconds, const QString &filter);
        void captureAborted(double exposureSeconds);

        // Filter Manager
        void filterManagerUpdated(ISD::FilterWheel *device);

        void trainChanged();


    private:
        // ////////////////////////////////////////////////////////////////////
        // capture process steps
        // ////////////////////////////////////////////////////////////////////
        /**
             * @brief captureImage Initiates image capture in the active job.
             */
        void captureImage();

        /**
             * @brief newFITS process new FITS data received from camera. Update status of active job and overall sequence.
             * @param bp pointer to blob containing FITS data
             */
        void processData(const QSharedPointer<FITSData> &data);


        /**
             * @brief Connect or disconnect the camera device
             * @param connection flag if connect (=true) or disconnect (=false)
             */
        void connectCamera(bool connection);

        /**
         * @brief processPreCaptureCalibrationStage Execute the tasks that need to be completed before capturing may start.
         *
         * For light frames, checkLightFramePendingTasks() is called.
         *
         * @return IPS_OK if all necessary tasks have been completed
         */
        IPState processPreCaptureCalibrationStage();
        bool processPostCaptureCalibrationStage();

        /**
         * @brief updatePreCaptureCalibrationStatusThis is a wrapping loop for processPreCaptureCalibrationStage(),
         *        which contains all checks before captureImage() may be called.
         *
         * If processPreCaptureCalibrationStage() returns IPS_OK (i.e. everything is ready so that
         * capturing may be started), captureImage() is called. Otherwise, it waits for a second and
         * calls itself again.
         */
        void updatePreCaptureCalibrationStatus();

        /**
         * @brief prepareJob Update the counters of existing frames and continue with prepareActiveJob(), if there exist less
         *        images than targeted. If enough images exist, continue with processJobCompletion().
         */
        void prepareJob(SequenceJob *job);

        /**
         * @brief prepareActiveJobStage1 Check for pre job script to execute. If none, move to stage 2
         */
        void prepareActiveJobStage1();
        /**
         * @brief prepareActiveJobStage2 Reset #calibrationStage and continue with preparePreCaptureActions().
         */
        void prepareActiveJobStage2();

        /**
         * @brief preparePreCaptureActions Trigger setting the filter, temperature, (if existing) the rotator angle and
         *        let the #activeJob execute the preparation actions before a capture may
         *        take place (@see SequenceJob::prepareCapture()).
         *
         * After triggering the settings, this method returns. This mechanism is slightly tricky, since it
         * asynchronous and event based and works as collaboration between Capture and SequenceJob. Capture has
         * the connection to devices and SequenceJob knows the target values.
         *
         * Each time Capture receives an updated value - e.g. the current CCD temperature
         * (@see updateCCDTemperature()) - it informs the #activeJob about the current CCD temperature.
         * SequenceJob checks, if it has reached the target value and if yes, sets this action as as completed.
         *
         * As soon as all actions are completed, SequenceJob emits a prepareComplete() event, which triggers
         * executeJob() from the Capture module.
         */
        void preparePreCaptureActions();

        /**
         * @brief Check all tasks that might be pending before capturing may start.
         *
         * The following checks are executed:
         *  1. Are there any pending jobs that failed? If yes, return with IPS_ALERT.
         *  2. Has pausing been initiated (@see checkPausing()).
         *  3. Is a meridian flip already running (@see m_MeridianFlipState->checkMeridianFlipRunning()) or ready
         *     for execution (@see m_captureModuleState->checkMeridianFlipReady()).
         *  4. Is a post meridian flip alignment running (@see checkAlignmentAfterFlip()).
         *  5. Is post flip guiding required or running (@see checkGuidingAfterFlip().
         *  6. Is the guiding deviation below the expected limit (@see setGuideDeviation(double,double)).
         *  7. Is dithering required or ongoing (@see checkDithering()).
         *  8. Is re-focusing or adaptive focusing required or ongoing (@see startFocusIfRequired()).
         *  9. Has guiding been resumed and needs to be restarted (@see resumeGuiding())
         *
         * If none of this is true, everything is ready and capturing may be started.
         *
         * @return IPS_OK iff no task is pending, IPS_BUSY otherwise (or IPS_ALERT if a problem occured)
         */
        IPState checkLightFramePendingTasks();

        /**
         * @brief Manage the capture process after a captured image has been successfully downloaded from the camera.
         *
         * When a image frame has been captured and downloaded successfully, send the image to the client (if configured)
         * and execute the book keeping for the captured frame. After this, either processJobCompletion() is executed
         * in case that the job is completed, and resumeSequence() otherwise.
         *
         * Book keeping means:
         * - increase / decrease the counters for focusing and dithering
         * - increase the frame counter
         * - update the average download time
         *
         * @return IPS_BUSY if pausing is requested, IPS_OK otherwise.
         */
        IPState setCaptureComplete();

        /**
         * @brief processJobCompletionStage1 Process job completion. In stage 1 when simply check if the is a post-job script to be running
         * if yes, we run it and wait until it is done before we move to stage2
         */
        void processJobCompletionStage1();

        /**
         * @brief processJobCompletionStage2 Stop execution of the current sequence and check whether there exists a next sequence
         *        and start it, if there is a next one to be started (@see resumeSequence()).
         */
        void processJobCompletionStage2();

        /**
         * @brief checkNextExposure Try to start capturing the next exposure (@see startNextExposure()).
         *        If startNextExposure() returns, that there are still some jobs pending,
         *        we wait for 1 second and retry to start it again.
         *        If one of the pending preparation jobs has problems, the looping stops.
         */
        void checkNextExposure();

        // check if a pause has been planned
        bool checkPausing();

        /**
         * @brief resumeSequence Try to continue capturing.
         *
         * Take the active job, if there is one, or search for the next one that is either
         * idle or aborted. If a new job is selected, call prepareJob(*SequenceJob) to prepare it and
         * resume guiding (TODO: is this not part of the preparation?). If the current job is still active,
         * initiate checkNextExposure().
         *
         * @return IPS_OK if there is a job that may be continued, IPS_BUSY otherwise.
         */
        IPState resumeSequence();

        /**
         * @brief startNextExposure Ensure that all pending preparation tasks are be completed (focusing, dithering, etc.)
         *        and start the next exposure.
         *
         * Checks of pending preparations depends upon the frame type:
         *
         * - For light frames, pending preparations like focusing, dithering etc. needs
         *   to be checked before each single frame capture. efore starting to capture the next light frame,
         *   checkLightFramePendingTasks() is called to check if all pending preparation tasks have
         *   been completed successfully. As soon as this is the case, the sequence timer
         *   #seqTimer is started to wait the configured delay and starts capturing the next image.
         *
         * - For bias, dark and flat frames, preparation jobs are only executed when starting a sequence.
         *   Hence, for these frames we directly start the sequence timer #seqTimer.
         *
         * @return IPS_OK, iff all pending preparation jobs are completed (@see checkLightFramePendingTasks()).
         *         In that case, the #seqTimer is started to wait for the configured settling delay and then
         *         capture the next image (@see Capture::captureImage). In case that a pending task aborted,
         *         IPS_IDLE is returned.
         */
        IPState startNextExposure();

        // If exposure timed out, let's handle it.
        void processCaptureTimeout();

        /**
         * @brief processCaptureError Handle when image capture fails
         * @param type error type
         */
        void processCaptureError(ISD::Camera::ErrorType type);

        double setCurrentADU(double value);

        // Propagate meridian flip state changes to the UI
        void updateMeridianFlipStage(MeridianFlipState::MFStage stage);

        // ////////////////////////////////////////////////////////////////////
        // helper functions
        // ////////////////////////////////////////////////////////////////////

        // Filename preview
        void generatePreviewFilename();
        QString previewFilename(FilenamePreviewType = LOCAL_PREVIEW);

        /**
         * @brief Set the currently active sequence job. Use this function to ensure
         *        that everything is cleaned up properly.
         */
        void setActiveJob(SequenceJob *value);

        void setBusy(bool enable);

        /* Capture */
        void llsq(QVector<double> x, QVector<double> y, double &a, double &b);

        bool isModelinDSLRInfo(const QString &model);

        void createDSLRDialog();

        void resetFrameToZero();

        /**
         * @brief generateScriptArguments Generate argument list to pass to capture script
         * @return generates argument list consisting of one argument -metadata followed by JSON-formatted key:value pair:
         * -ts UNIX timestamp
         * -image full path to captured image (if any)
         * -size size of file in bytes (if any)
         * -job {name, index}
         * -capture {name, index}
         * -filter
         * TODO depending on user feedback.
         */
        QStringList generateScriptArguments() const;

        /**
         * @brief Sync refocus options to the GUI settings
         */
        void syncRefocusOptionsFromGUI();

        // ////////////////////////////////////////////////////////////////////
        // UI controls
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief setBinning Set binning
         * @param horBin Horizontal binning
         * @param verBin Vertical binning
         */
        void setBinning(int horBin, int verBin)
        {
            captureBinHN->setValue(horBin);
            captureBinVN->setValue(verBin);
        }

        /**
         * @brief setISO Set index of ISO list.
         * @param index index of ISO list.
         */
        void setISO(int index)
        {
            captureISOS->setCurrentIndex(index);
        }

        // reset = 0 --> Do not reset
        // reset = 1 --> Full reset
        // reset = 2 --> Only update limits if needed
        void updateFrameProperties(int reset = 0);
        void syncGUIToJob(SequenceJob *job);
        // DSLR Info
        void cullToDSLRLimits();
        //void syncDriverToDSLRLimits();

        // selection of a job
        void selectedJobChanged(QModelIndex current, QModelIndex previous);

        // Change filter name in INDI
        void editFilterName();

        // ////////////////////////////////////////////////////////////////////
        // device control
        // ////////////////////////////////////////////////////////////////////
        // Gain
        // This sets and gets the custom properties target gain
        // it does not access the ccd gain property
        void setGain(double value);
        double getGain();

        void setOffset(double value);
        double getOffset();

        /**
         * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
         * @param nvp pointer to number property.
         */
        void processCCDNumber(INumberVectorProperty *nvp);

        // ////////////////////////////////////////////////////////////////////
        // XML capture sequence file handling
        // ////////////////////////////////////////////////////////////////////
        bool processJobInfo(XMLEle *root, bool ignoreTarget = false);

        // ////////////////////////////////////////////////////////////////////
        // Attributes
        // ////////////////////////////////////////////////////////////////////
        double seqExpose { 0 };
        int seqTotalCount;
        int seqCurrentCount { 0 };
        // time left of the current exposure
        QTime imageCountDown;
        double lastRemainingFrameTimeMS;
        // time left for the current sequence
        QTime sequenceCountDown;

        // Timer for starting the next capture sequence with delay
        // @see startNextExposure()
        QTimer *seqDelayTimer { nullptr };
        int seqFileCount { 0 };
        bool isBusy { false };
        bool m_isFraming { false };

        // Capture timeout timer
        QTimer captureTimeout;
        uint8_t m_CaptureTimeoutCounter { 0 };
        uint8_t m_DeviceRestartCounter { 0 };

        bool useGuideHead { false };

        QString m_ObserverName;

        // Currently active sequence job.
        // DO NOT SET IT MANUALLY, USE {@see setActiveJob()} INSTEAD!
        SequenceJob *activeJob { nullptr };
        QSharedPointer<CaptureModuleState> m_captureModuleState;

        QPointer<QDBusInterface> mountInterface;

        QSharedPointer<FITSData> m_ImageData;

        QStringList m_LogText;
        QUrl m_SequenceURL;
        bool m_JobUnderEdit { false };

        // Fast Exposure
        bool m_RememberFastExposure {false};

        // Flat field automation
        QVector<double> ExpRaw, ADURaw;
        double targetADU { 0 };
        double targetADUTolerance { 1000 };
        ADUAlgorithm targetADUAlgorithm { ADU_LEAST_SQUARES};
        SkyPoint wallCoord;
        bool preMountPark { false };
        bool preDomePark { false };
        FlatFieldDuration flatFieldDuration { DURATION_MANUAL };
        FlatFieldSource flatFieldSource { SOURCE_MANUAL };
        bool lightBoxLightEnabled { false };
        QMap<ScriptTypes, QString> m_Scripts;

        QUrl dirPath;

        // Misc
        bool suspendGuideOnDownload { false };
        QJsonArray m_SequenceArray;

        // CCD Chip frame settings
        QMap<ISD::CameraChip *, QVariantMap> frameSettings;

        /// CCD device needed for focus operation
        ISD::Camera *m_Camera { nullptr };
        /// Optional device filter
        ISD::FilterWheel *m_FilterWheel { nullptr };
        /// Dust Cap
        ISD::DustCap *m_DustCap { nullptr };
        /// LightBox
        ISD::LightBox *m_LightBox { nullptr };
        /// Rotator
        ISD::Rotator *m_Rotator { nullptr };
        /// Dome
        ISD::Dome *m_Dome { nullptr };
        /// Mount
        ISD::Mount *m_Mount { nullptr };

        // Post capture script
        QProcess m_CaptureScript;
        uint8_t m_CaptureScriptType {0};

        std::unique_ptr<CustomProperties> customPropertiesDialog;
        std::unique_ptr<DSLRInfo> dslrInfoDialog;

        // DSLR Infos
        QList<QMap<QString, QVariant>> DSLRInfos;

        // Controls
        double GainSpinSpecialValue { INVALID_VALUE };
        double OffsetSpinSpecialValue { INVALID_VALUE };

        // Dark Processor
        QPointer<DarkProcessor> m_DarkProcessor;
        std::unique_ptr<Ui::Limits> m_LimitsUI;
        QPointer<QDialog> m_LimitsDialog;

        QElapsedTimer m_DownloadTimer;
        QTimer downloadProgressTimer;
        QVariantMap m_Metadata;

        QSharedPointer<FilterManager> m_FilterManager;
        QSharedPointer<RotatorSettings> m_RotatorControlPanel;

        bool FilterEnabled {false};
        bool ExpEnabled {false};
        bool TimeStampEnabled {false};

        double m_FocalLength {-1};
        double m_Aperture {-1};
        double m_FocalRatio {-1};
        double m_Reducer = {-1};

		
};

}
