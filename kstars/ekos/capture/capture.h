/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_capture.h"
#include "captureprocess.h"
#include "capturemodulestate.h"
#include "capturedeviceadaptor.h"
#include "sequencejob.h"
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
 * The capture class support capturing single or multiple images from a CCD, it provides a
 * powerful sequence queue with filter selection. Any sequence queue can be saved as
 * Ekos Sequence Queue (.esq). All image capture operations are saved as Sequence Jobs
 * that encapsulate all the different options in a capture process. The user may select in
 * sequence autofocusing by setting limits for HFR, execution time or temperature delta. When the limit
 * is exceeded, it automatically trigger autofocus operation. The capture process can also be
 * linked with guide module. If guiding deviations exceed a certain threshold, the capture operation aborts until
 * the guiding deviation resume to acceptable levels and the capture operation is resumed.
 *
 * Controlling the capturing execution is a complex process, that is controlled by
 * these classes:
 * - this class, that controll the UI and is the interface for all DBUS functions
 * - {@see CaptureModuleState} holds all state informations
 * - {@see CaptureProcess} holds the business logic that controls the process
 * For ore details about the capturing execution process, please visit {@see CaptureProcess}.
 *
 *@author Jasem Mutlaq
 *@version 1.4
 */
class Capture : public QWidget, public Ui::Capture
{
        Q_OBJECT
        Q_CLASSINFO("D-Bus Interface", "org.kde.kstars.Ekos.Capture")
        Q_PROPERTY(Ekos::CaptureState status READ status NOTIFY newStatus)
        Q_PROPERTY(QString targetName READ getTargetName WRITE setTargetName)
        Q_PROPERTY(QString observerName READ getObserverName WRITE setObserverName)
        Q_PROPERTY(QString opticalTrain READ opticalTrain WRITE setOpticalTrain)
        Q_PROPERTY(QString camera READ camera)
        Q_PROPERTY(QString filterWheel READ filterWheel)
        Q_PROPERTY(QString filter READ filter WRITE setFilter)
        Q_PROPERTY(bool coolerControl READ hasCoolerControl WRITE setCoolerControl)
        Q_PROPERTY(QStringList logText READ logText NOTIFY newLog)

    public:
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
        Q_SCRIPTABLE QString getSequenceQueueStatus()
        {
            return state()->sequenceQueueStatus();
        }

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
        Q_SCRIPTABLE bool hasCoolerControl()
        {
            return process()->hasCoolerControl();
        }

        /** DBUS interface function.
             * Set the CCD cooler ON/OFF
             *
             */
        Q_SCRIPTABLE bool setCoolerControl(bool enable)
        {
            return process()->setCoolerControl(enable);
        }

        /** DBUS interface function.
             * @return Returns the percentage of completed captures in all active jobs
             */
        Q_SCRIPTABLE double getProgressPercentage()
        {
            return state()->progressPercentage();
        }

        /** DBUS interface function.
             * @return Returns the number of jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getJobCount()
        {
            return state()->allJobs().count();
        }

        /** DBUS interface function.
             * @return Returns the number of pending uncompleted jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getPendingJobCount()
        {
            return state()->pendingJobCount();
        }

        /** DBUS interface function.
             * @return Returns ID of current active job if any, or -1 if there are no active jobs.
             */
        Q_SCRIPTABLE int getActiveJobID()
        {
            return state()->activeJobID();
        }

        /** DBUS interface function.
             * @return Returns time left in seconds until active job is estimated to be complete.
             */
        Q_SCRIPTABLE int getActiveJobRemainingTime()
        {
            return state()->activeJobRemainingTime();
        }

        /** DBUS interface function.
             * @return Returns overall time left in seconds until all jobs are estimated to be complete
             */
        Q_SCRIPTABLE int getOverallRemainingTime()
        {
            return state()->overallRemainingTime();
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job state (Idle, In Progress, Error, Aborted, Complete)
             */
        Q_SCRIPTABLE QString getJobState(int id)
        {
            return state()->jobState(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job filter name.
             */
        Q_SCRIPTABLE QString getJobFilterName(int id)
        {
            return state()->jobFilterName(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns The number of images completed capture in the job.
             */
        Q_SCRIPTABLE int getJobImageProgress(int id)
        {
            return state()->jobImageProgress(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total number of images to capture in the job.
             */
        Q_SCRIPTABLE int getJobImageCount(int id)
        {
            return state()->jobImageCount(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the number of seconds left in an exposure operation.
             */
        Q_SCRIPTABLE double getJobExposureProgress(int id)
        {
            return state()->jobExposureProgress(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total requested exposure duration in the job.
             */
        Q_SCRIPTABLE double getJobExposureDuration(int id)
        {
            return state()->jobExposureDuration(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the frame type (light, dark, ...) of the job.
             */
        Q_SCRIPTABLE CCDFrameType getJobFrameType(int id)
        {
            return state()->jobFrameType(id);
        }

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
        Q_SCRIPTABLE Q_NOREPLY void setCapturedFramesMap(const QString &signature, int count)
        {
            state()->setCapturedFramesCount(signature, static_cast<ushort>(count));
        };


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
            return state()->getCaptureState();
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
        bool updateCamera();

        /**
         * @brief Add new Filter Wheel
         * @param name device name of the new filter wheel
        */
        void setFilterWheel(QString name);

        /**
         * @brief Add new Rotator
         * @param name name of the new rotator
        */
        void setRotator(QString name);

        /**
         * @brief setDome Set dome device
         * @param device pointer to dome device
         * @return true if successfull, false otherewise.
         */
        bool setDome(ISD::Dome *device);

        /**
         * @brief Generic method for removing any connected device.
         */
        void removeDevice(const QSharedPointer<ISD::GenericDevice> &device)
        {
            process()->removeDevice(device);
        }

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
        void syncCameraInfo();

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
        void refreshFilterSettings();

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
            return state()->getSequence();
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
        void setCalibrationSettings(const QJsonObject &settings)
        {
            state()->setCalibrationSettings(settings);
        }

        /**
         * @brief getCalibrationSettings Get Calibration settings
         * @return settings as JSON object
         */
        QJsonObject getCalibrationSettings()
        {
            return state()->calibrationSettings();
        }

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
        Q_SCRIPTABLE Q_NOREPLY void start()
        {
            process()->startNextPendingJob();
        }

        /** DBUS interface function.
             * Stops currently running jobs:
             *           CAPTURE_IDLE: capture in idle state waiting for further action (e.g. single sequence
             *                         is complete, next one starting)
             *       CAPTURE_COMPLETE: all capture sequences are complete
             *          CAPTURE_ABORT: capture aborted either by user interaction or by a technical error
             *        CAPTURE_SUSPEND: capture suspended and waiting to be restarted
             * @param targetState status of the job after stop
             */
        Q_SCRIPTABLE Q_NOREPLY void stop(CaptureState targetState = CAPTURE_IDLE)
        {
            process()->stopCapturing(targetState);
        };

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
        Q_SCRIPTABLE Q_NOREPLY void toggleVideo(bool enabled)
        {
            process()->toggleVideo(enabled);
        };


        /** DBus interface function
         * @brief restartCamera Restarts the INDI driver associated with a camera. Remote and Local drivers are supported.
         * @param name Name of camera to restart. If a driver defined multiple cameras, they would be removed and added again
         * after driver restart.
         */
        Q_SCRIPTABLE Q_NOREPLY void restartCamera(const QString &name)
        {
            process()->restartCamera(name);
        };

        /** DBus interface function
         * @brief Set the name of the target to be captured.
         */
        Q_SCRIPTABLE Q_NOREPLY void setTargetName(const QString &newTargetName)
        {
            state()->setTargetName(newTargetName);
        };
        Q_SCRIPTABLE QString getTargetName()
        {
            return state()->targetName();
        }

        /** DBus interface function
         * @brief Set the observer name.
         */
        Q_SCRIPTABLE Q_NOREPLY void setObserverName(const QString &value)
        {
            state()->setObserverName(value);
        };
        Q_SCRIPTABLE QString getObserverName()
        {
            return state()->observerName();
        }


        /** @}*/

        /**
         * @brief process shortcut for the process engine
         */
        QPointer<CaptureProcess> process() const
        {
            return m_captureProcess;
        }

        // ////////////////////////////////////////////////////////////////////
        // Capture actions
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief captureStarted Change the UI after the capturing process
         * has been started.
         */
        void jobStarting();
        /**
         * @brief capturePreview Capture a single preview image
         */
        void capturePreview()
        {
            process()->capturePreview();
        }

        /**
         * @brief startFraming Like captureOne but repeating.
         */
        void startFraming()
        {
            process()->capturePreview(true);
        }

        /**
         * @brief generateDarkFlats Generate a list of dark flat jobs from available flat frames.
         */
        void generateDarkFlats();

        /**
         * @brief updateJobFromUI Update all job attributes from the UI settings.
         */
        void updateJobFromUI(SequenceJob *job, FilenamePreviewType filenamePreview = NOT_PREVIEW);

        /**
         * @brief addJob Add a new job to the UI. This is used when a job is loaded from a capture sequence file. In
         * contrast to {@see #createJob()}, the job's attributes are taken from the file and only the UI gehts updated.
         */
        void addJob(SequenceJob *job);

        /**
         * @brief createJob Create a new job with the settings given in the GUI.
         * @param jobtype batch, preview, looping or dark flat job.
         * @param filenamePreview if the job is to generate a preview filename
         * @return pointer to job created or nullptr otherwise.
         */
        SequenceJob *createJob(SequenceJob::SequenceJobType jobtype = SequenceJob::JOBTYPE_BATCH, FilenamePreviewType filenamePreview = NOT_PREVIEW);

        /**
         * @brief jobEditFinished Editing of an existing job finished, update its
         *        attributes from the UI settings. The job under edit is taken from the
         *        selection in the job table.
         * @return true if job updated succeeded.
         */
        void editJobFinished();

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
        void refreshCameraSettings();

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
        QSharedPointer<MeridianFlipState> getMeridianFlipState()
        {
            return state()->getMeridianFlipState();
        }
        void setMeridianFlipState(QSharedPointer<MeridianFlipState> newstate);

        // ////////////////////////////////////////////////////////////////////
        // UI controls
        // ////////////////////////////////////////////////////////////////////

        void removeJobFromQueue();

        /**
          * @brief moveJobUp Move the job in the sequence queue one place up or down.
          */
        void moveJob(bool up);

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
         * @brief updateTargetName React upon a new capture target name
         */
        void newTargetName(const QString &name);

        /**
         * @brief showTemperatureRegulation Toggle temperature regulation dialog which sets temperature ramp and threshold
         */
        void showTemperatureRegulation();

        /**
         * @brief updateStartButtons Update the start and the pause button to new states of capturing
         * @param start start capturing
         * @param pause pause capturing
         */
        void updateStartButtons(bool start, bool pause = false);

        // Clear Camera Configuration
        void clearCameraConfiguration();

        // ////////////////////////////////////////////////////////////////////
        // slots handling device and module events
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief captureStarted Manage the result when capturing has been started
         */
        void captureRunning();

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
        void setFocusStatus(FocusState newstate);

        /**
         * @brief updateFocusStatus Handle new focus state
         */
        void updateFocusStatus(FocusState newstate);

        // Adaptive Focus
        /**
         * @brief focusAdaptiveComplete Forward the new focus state to the capture module state machine
         */
        void focusAdaptiveComplete(bool success)
        {
            // directly forward it to the state machine
            state()->updateAdaptiveFocusState(success);
        }

        /**
         * @brief updateAdaptiveFocusStatus Handle new focus state
         */

        void setFocusTemperatureDelta(double focusTemperatureDelta, double absTemperature);

        /**
         * @brief setHFR Receive the measured HFR value of the latest frame
         */
        void setHFR(double newHFR, int)
        {
            state()->getRefocusState()->setFocusHFR(newHFR);
        }

        // Filter
        void setFilterStatus(FilterState filterState);

        // Guide
        void setGuideStatus(GuideState newstate);

        // Align

        void setAlignStatus(Ekos::AlignState newstate);
        void setAlignResults(double solverPA, double ra, double de, double pixscale);

        // Update Mount module status
        void setMountStatus(ISD::Mount::Status newState);

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
        void resetJobEdit(bool cancelled = false);

        // Flat field
        void openCalibrationDialog();

        // Observer
        void showObserverDialog();

        // Cooler
        void setCoolerToggled(bool enabled);

        // ////////////////////////////////////////////////////////////////////
        // slots handling device and module events
        // ////////////////////////////////////////////////////////////////////

        // Script Manager
        void handleScriptsManager();

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
        void updateDownloadProgress(double downloadTimeLeft);

    signals:
        Q_SCRIPTABLE void newLog(const QString &text);
        Q_SCRIPTABLE void meridianFlipStarted();
        Q_SCRIPTABLE void guideAfterMeridianFlip();
        Q_SCRIPTABLE void newStatus(CaptureState status);
        Q_SCRIPTABLE void captureComplete(const QVariantMap &metadata);

        void newFilterStatus(FilterState state);

        void ready();

        // communication with other modules
        void checkFocus(double);
        void runAutoFocus(bool);
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
         * @brief captureImageStarted Image capturing for the active job has started.
         */
        void captureImageStarted();

        /**
         * @brief jobPreparationStarted Preparation actions for the current active job have beenstarted.
         */
        void jobExecutionPreparationStarted();

        /**
         * @brief jobPrepared Select the job that is currently in preparation.
         */
        void jobPrepared(SequenceJob *job);

        /**
         * @brief imageCapturingCompleted Capturing a single frame completed
         */
        void imageCapturingCompleted();

        /**
         * @brief captureStopped Capturing has stopped
         */
        void captureStopped();

        /**
         * @brief processFITSfinished processing new FITS data received from camera finished.
         * @param success true iff processing was successful
         */
        void processingFITSfinished(bool success);

        // Propagate meridian flip state changes to the UI
        void updateMeridianFlipStage(MeridianFlipState::MFStage stage);

        // ////////////////////////////////////////////////////////////////////
        // Job table handling
        // ////////////////////////////////////////////////////////////////////


        /**
         * @brief updateJobTable Update the table row values for the given sequence job. If the job
         * is null, all rows will be updated
         * @param job as identifier for the row
         * @param full if false, then only the status and the counter will be updated.
         */
        void updateJobTable(SequenceJob *job, bool full = false);

        /**
         * @brief updateJobTableCountCell Update the job counter in the job table of a sigle job
         */
        void updateJobTableCountCell(SequenceJob *job, QTableWidgetItem *countCell);

        // ////////////////////////////////////////////////////////////////////
        // helper functions
        // ////////////////////////////////////////////////////////////////////
        // check if the upload paths are filled correctly
        bool checkUploadPaths(FilenamePreviewType filenamePreview);

        // create a new row in the job table and fill it with the given job's values
        void createNewJobTableRow(SequenceJob *job);

        // Create a Json job from the current job table row
        QJsonObject createJsonJob(SequenceJob *job, int currentRow);

        // shortcut for the module state
        QSharedPointer<CaptureModuleState> state() const
        {
            return m_captureModuleState;
        }
        // shortcut to device adapter
        QSharedPointer<CaptureDeviceAdaptor> devices()
        {
            return m_captureDeviceAdaptor;
        }
        // shortcut for the active job
        SequenceJob *activeJob() const
        {
            return state()->getActiveJob();
        }
        // Shortcut to the active camera held in the device adaptor
        ISD::Camera *activeCamera()
        {
            return m_captureDeviceAdaptor->getActiveCamera();
        }

        // Filename preview
        void generatePreviewFilename();
        QString previewFilename(FilenamePreviewType = LOCAL_PREVIEW);

        void setBusy(bool enable);

        /* Capture */
        void createDSLRDialog();

        void resetFrameToZero();

        /**
         * @brief Sync refocus options to the GUI settings
         */
        void syncRefocusOptionsFromGUI();

        /**
         * @brief currentScope Retrieve the scope parameters from the optical train.
         */
        QJsonObject currentScope();

        /**
         * @brief currentReducer Retrieve the reducer parameters from the optical train.
         */
        double currentReducer();

        /**
         * @brief currentAperture Determine the current aperture
         * @return
         */
        double currentAperture();

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

        /**
         * @brief updateCaptureFormats Update encoding and transfer formats
         */
        void updateCaptureFormats();

        /**
         * @brief syncGUIToJob Update UI to job settings
         */
        void syncGUIToJob(SequenceJob *job);
        /**
         * @brief syncGUIToState Update UI to general settings from Options
         */
        void syncGUIToGeneralSettings();

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
        double getGain()
        {
            return process()->getGain(customPropertiesDialog->getCustomProperties());
        }

        void setOffset(double value);
        double getOffset()
        {
            return process()->getOffset(customPropertiesDialog->getCustomProperties());
        }

        /**
         * @brief processCCDNumber Process number properties arriving from CCD. Currently, only CCD and Guider frames are processed.
         * @param nvp pointer to number property.
         */
        void processCCDNumber(INumberVectorProperty *nvp);

        // ////////////////////////////////////////////////////////////////////
        // Attributes
        // ////////////////////////////////////////////////////////////////////
        double seqExpose { 0 };
        int seqTotalCount;
        int seqCurrentCount { 0 };

        QPointer<CaptureProcess> m_captureProcess;
        QSharedPointer<CaptureModuleState> m_captureModuleState;

        QPointer<QDBusInterface> mountInterface;

        QStringList m_LogText;
        bool m_JobUnderEdit { false };

        // Flat field automation
        QMap<ScriptTypes, QString> m_Scripts;

        QUrl dirPath;

        std::unique_ptr<CustomProperties> customPropertiesDialog;
        std::unique_ptr<DSLRInfo> dslrInfoDialog;

        // Controls
        double GainSpinSpecialValue { INVALID_VALUE };
        double OffsetSpinSpecialValue { INVALID_VALUE };

        // Dark Processor
        std::unique_ptr<Ui::Limits> m_LimitsUI;
        QPointer<QDialog> m_LimitsDialog;

        QVariantMap m_Metadata;

        QSharedPointer<FilterManager> m_FilterManager;
        QSharedPointer<RotatorSettings> m_RotatorControlPanel;
};

}
