/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_capture.h"

#include "camera.h"

#include "opsmiscsettings.h"
#include "opsdslrsettings.h"
#include "capturemodulestate.h"
#include "ekos/manager/meridianflipstate.h"
#include "ekos/ekos.h"
#include "ekos/auxiliary/opticaltrainsettings.h"
#include "ekos/focus/focusutils.h"
#include "indi/indicamera.h"
#include "indi/indidustcap.h"
#include "indi/indidome.h"
#include "indi/indilightbox.h"
#include "indi/indimount.h"

#include <QTimer>
#include <QUrl>
#include <QtDBus/QDBusInterface>

#include <memory>

class DSLRInfo;
class QTableWidgetItem;

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

class ScriptsManager;

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
 * - {@see CameraState} holds all state informations
 * - {@see CameraProcess} holds the business logic that controls the process
 * For ore details about the capturing execution process, please visit {@see CameraProcess}.
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
        Capture();

        /** @defgroup CaptureDBusInterface Ekos DBus Interface - Capture Module
             * Ekos::Capture interface provides advanced scripting capabilities to capture image sequences.
             * @{
            */

        /** DBUS interface function.
             * @return device The CCD device name
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
        Q_SCRIPTABLE Q_NOREPLY void clearSequenceQueue()
        {
            mainCamera()->clearSequenceQueue();
        }

        /** DBUS interface function.
             * Returns the overall sequence queue status. If there are no jobs pending, it returns "Invalid". If all jobs are idle, it returns "Idle". If all jobs are complete, it returns "Complete". If one or more jobs are aborted
             * it returns "Aborted" unless it was temporarily aborted due to guiding deviations, then it would return "Suspended". If one or more jobs have errors, it returns "Error". If any jobs is under progress, returns "Running".
             */
        Q_SCRIPTABLE QString getSequenceQueueStatus()
        {
            return mainCameraState()->sequenceQueueStatus();
        }

        /** DBUS interface function.
             * Loads the Ekos Sequence Queue file in the Sequence Queue. Jobs are appended to existing jobs.
             * @param fileURL full URL of the filename
             * @param train name of the optical train to be used
             * @param lead lead or follower job?
             * @param targetName override the target in the sequence queue file (necessary for using the target of the scheduler)
             */
        Q_SCRIPTABLE bool loadSequenceQueue(const QString &fileURL, QString train = "", bool isLead = true, QString targetName = "");

        /** DBUS interface function.
             * Saves the Sequence Queue to the Ekos Sequence Queue file.
             * @param fileURL full URL of the filename
             */
        Q_SCRIPTABLE bool saveSequenceQueue(const QString &path)
        {
            return mainCamera()->saveSequenceQueue(path);
        }

        /** DBUS interface function.
             * Enables or disables the maximum guiding deviation and sets its value.
             * @param enable If true, enable the guiding deviation check, otherwise, disable it.
             * @param value if enable is true, it sets the maximum guiding deviation in arcsecs. If the value is exceeded, the capture operation is aborted until the value falls below the value threshold.
             */
        Q_SCRIPTABLE Q_NOREPLY void setMaximumGuidingDeviation(bool enable, double value)
        {
            mainCamera()->setMaximumGuidingDeviation(enable, value);
        }

        /** DBUS interface function.
             * Enables or disables the in sequence focus and sets Half-Flux-Radius (HFR) limit.
             * @param enable If true, enable the in sequence auto focus check, otherwise, disable it.
             * @param HFR if enable is true, it sets HFR in pixels. After each exposure, the HFR is re-measured and if it exceeds the specified value, an autofocus operation will be commanded.
             */
        Q_SCRIPTABLE Q_NOREPLY void setInSequenceFocus(bool enable, double HFR)
        {
            mainCamera()->setInSequenceFocus(enable, HFR);
        }

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
        Q_SCRIPTABLE double getProgressPercentage()
        {
            return mainCameraState()->progressPercentage();
        }

        /** DBUS interface function.
             * @return Returns true if an ongoing capture is a preview capture.
             */
        Q_SCRIPTABLE bool isActiveJobPreview()
        {
            return mainCamera()->isActiveJobPreview();
        }

        /** DBUS interface function.
             * @return Returns the number of jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getJobCount()
        {
            return mainCameraState()->allJobs().count();
        }

        /** DBUS interface function.
             * @return Returns the number of pending uncompleted jobs in the sequence queue.
             */
        Q_SCRIPTABLE int getPendingJobCount()
        {
            return mainCameraState()->pendingJobCount();
        }

        /** DBUS interface function.
             * @return Returns ID of current active job if any, or -1 if there are no active jobs.
             */
        Q_SCRIPTABLE int getActiveJobID()
        {
            return mainCameraState()->activeJobID();
        }

        /** DBUS interface function.
             * @return Returns time left in seconds until active job is estimated to be complete.
             */
        Q_SCRIPTABLE int getActiveJobRemainingTime()
        {
            return mainCameraState()->activeJobRemainingTime();
        }

        /** DBUS interface function.
             * @return Returns overall time left in seconds until all jobs are estimated to be complete
             */
        Q_SCRIPTABLE int getOverallRemainingTime()
        {
            return mainCameraState()->overallRemainingTime();
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job state (Idle, In Progress, Error, Aborted, Complete)
             */
        Q_SCRIPTABLE QString getJobState(int id)
        {
            return mainCameraState()->jobState(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the job filter name.
             */
        Q_SCRIPTABLE QString getJobFilterName(int id)
        {
            return mainCameraState()->jobFilterName(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns The number of images completed capture in the job.
             */
        Q_SCRIPTABLE int getJobImageProgress(int id)
        {
            return mainCameraState()->jobImageProgress(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total number of images to capture in the job.
             */
        Q_SCRIPTABLE int getJobImageCount(int id)
        {
            return mainCameraState()->jobImageCount(id);
        }

        /**
         * @brief prepareGUI Perform once only GUI prep processing
         */
        void setupOptions();

        // Settings popup
        OpsMiscSettings *m_OpsMiscSettings { nullptr };
        OpsDslrSettings *m_OpsDslrSettings { nullptr };

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the number of seconds left in an exposure operation.
             */
        Q_SCRIPTABLE double getJobExposureProgress(int id)
        {
            return mainCameraState()->jobExposureProgress(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the total requested exposure duration in the job.
             */
        Q_SCRIPTABLE double getJobExposureDuration(int id)
        {
            return mainCameraState()->jobExposureDuration(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the frame type (light, dark, ...) of the job.
             */
        Q_SCRIPTABLE CCDFrameType getJobFrameType(int id)
        {
            return mainCameraState()->jobFrameType(id);
        }

        /** DBUS interface function.
             * @param id job number. Job IDs start from 0 to N-1.
             * @return Returns the placeholder format of the job.
             */
        Q_SCRIPTABLE QString getJobPlaceholderFormat()
        {
            return mainCamera()->placeholderFormatT->text();
        }

        /** DBUS interface function.
             * @return Returns the Preview Filename of the job.
             */
        Q_SCRIPTABLE QString getJobPreviewFileName()
        {
            return mainCamera()->previewFilename();
        }

        /** DBUS interface function.
             * Clear in-sequence focus settings. It sets the autofocus HFR to zero so that next autofocus value is remembered for the in-sequence focusing.
             */
        Q_SCRIPTABLE Q_NOREPLY void clearAutoFocusHFR(const QString &trainname);

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
        Q_SCRIPTABLE Q_NOREPLY void setCapturedFramesMap(const QString &signature, int count, QString train = "");

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
            return mainCameraState()->getCaptureState();
        }
        /** @} end of group CaptureDBusInterface */

        QSharedPointer<CaptureModuleState> moduleState() const
        {
            return m_moduleState;
        }

        // ////////////////////////////////////////////////////////////////////
        // Access to the cameras
        // ////////////////////////////////////////////////////////////////////

        QSharedPointer<Camera> &camera(int i);

        void checkCloseCameraTab(int tabIndex);

        const QSharedPointer<Camera> mainCamera() const;

        /**
         * @brief find the camera using the given train
         * @param train optical train name
         * @param addIfNecessary if true, add a new camera with the given train, if none uses this train
         * @return index in the lost of cameras (@see #camera(int))
         */
        int findCamera(QString train, bool addIfNecessary);

        // ////////////////////////////////////////////////////////////////////
        // Changing the devices used by Capture
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief Update the camera
         * @param ID that holds the camera
         * @param current camera is valid
        */
        void updateCamera(int tabID, bool isValid);

        /**
         * @brief setDome Set dome device
         * @param device pointer to dome device
         * @return true if successfull, false otherewise.
         */
        bool setDome(ISD::Dome *device);

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
        // Optical Train handling
        // ////////////////////////////////////////////////////////////////////

        QString opticalTrain() const
        {
            return mainCamera()->opticalTrainCombo->currentText();
        }
        void setOpticalTrain(const QString &value)
        {
            mainCamera()->opticalTrainCombo->setCurrentText(value);
        }

        // ////////////////////////////////////////////////////////////////////
        // Read and write access for EkosLive
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief getSequence Return the JSON representation of the current sequeue queue
         * @return Reference to JSON array containing sequence queue jobs.
         */
        const QJsonArray &getSequence() const
        {
            return mainCameraState()->getSequence();
        }        

        /**
         * @brief setVideoLimits sets the buffer size and max preview fps for live preview
         * @param maxBufferSize in bytes
         * @param maxPreviewFPS number of frames per second
         * @return True if value is updated, false otherwise.
         */
        bool setVideoLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS);

        const QList<QSharedPointer<Camera>> &cameras() const
        {
            return moduleState()->cameras();
        }

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
             *    @return train name of the camera
             */
        Q_SCRIPTABLE QString start(QString train = "");

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
            mainCamera()->stop(targetState);
        }

        /** DBUS interface function.
             * Aborts all jobs and mark current state as ABORTED. It simply calls stop(CAPTURE_ABORTED)
             */
        Q_SCRIPTABLE Q_NOREPLY void abort(QString train = "");

        /** DBUS interface function.
             * Aborts all jobs and mark current state as SUSPENDED. It simply calls stop(CAPTURE_SUSPENDED)
             * The only difference between SUSPENDED and ABORTED it that capture module can automatically resume a suspended
             * state on its own without external trigger once the right conditions are met. When whatever reason caused the module
             * to go into suspended state ceases to exist, the capture module automatically resumes. On the other hand, ABORTED state
             * must be started via an external programmatic or user trigger (e.g. click the start button again).
             */
        Q_SCRIPTABLE Q_NOREPLY void suspend()
        {
            mainCamera()->suspend();
        }
        /** DBUS interface function.
         * @brief pause Pauses the Sequence Queue progress AFTER the current capture is complete.
         */
        Q_SCRIPTABLE Q_NOREPLY void pause()
        {
            mainCamera()->pause();
        }

        /** DBUS interface function.
         * @brief toggleSequence Toggle sequence state depending on its current state.
         * 1. If paused, then resume sequence.
         * 2. If idle or completed, then start sequence.
         * 3. Otherwise, abort current sequence.
         */
        Q_SCRIPTABLE Q_NOREPLY void toggleSequence()
        {
            mainCamera()->toggleSequence();
        }


        /** DBUS interface function.
             * Toggle video streaming if supported by the device.
             * @param enabled Set to true to start video streaming, false to stop it if active.
             */
        Q_SCRIPTABLE Q_NOREPLY void toggleVideo(bool enabled)
        {
            mainCamera()->toggleVideo(enabled);
        }


        /** DBus interface function
         * @brief restartCamera Restarts the INDI driver associated with a camera. Remote and Local drivers are supported.
         * @param name Name of camera to restart. If a driver defined multiple cameras, they would be removed and added again
         * after driver restart.
         */
        Q_SCRIPTABLE Q_NOREPLY void restartCamera(const QString &name)
        {
            mainCamera()->restartCamera(name);
        }

        /** DBus interface function
         * @brief Set the name of the target to be captured.
         */
        Q_SCRIPTABLE Q_NOREPLY void setTargetName(const QString &newTargetName)
        {
            mainCamera()->setTargetName(newTargetName);
        }

        Q_SCRIPTABLE QString getTargetName();

        /** DBus interface function
         * @brief Set the observer name.
         */
        Q_SCRIPTABLE Q_NOREPLY void setObserverName(const QString &value)
        {
            mainCamera()->setObserverName(value);
        };
        Q_SCRIPTABLE QString getObserverName()
        {
            return mainCamera()->getObserverName();
        }

        /**
         * @brief Name of the main camera's device
         */
        Q_SCRIPTABLE QString mainCameraDeviceName()
        {
            return mainCamera()->activeCamera()->getDeviceName();
        }

        /** @}*/

        /**
         * @brief process shortcut for the process engine
         */
        QSharedPointer<CameraProcess> process() const
        {
            return mainCamera()->process();
        }

        // ////////////////////////////////////////////////////////////////////
        // public capture settings
        // ////////////////////////////////////////////////////////////////////
        /**
         * @brief Slot receiving the update of the current target distance.
         * @param targetDiff distance to the target in arcseconds.
         */
        void updateTargetDistance(double targetDiff)
        {
            mainCamera()->updateTargetDistance(targetDiff);
        }

        void setMeridianFlipState(QSharedPointer<MeridianFlipState> newstate);

        // ////////////////////////////////////////////////////////////////////
        // slots handling device and module events
        // ////////////////////////////////////////////////////////////////////

        /**
             * @brief setGuideDeviation Set the guiding deviation as measured by the guiding module. Abort capture
             *        if deviation exceeds user value. Resume capture if capture was aborted and guiding
             *         deviations are below user value.
             * @param delta_ra Deviation in RA in arcsecs from the selected guide star.
             * @param delta_dec Deviation in DEC in arcsecs from the selected guide star.
             */
        void setGuideDeviation(double delta_ra, double delta_dec);

        void setGuideChip(ISD::CameraChip *guideChip);

        // Auto Focus
        /**
         * @brief setFocusStatus Forward the new focus state to the capture module state machine
         * @param trainname name of the optical train to select the focuser
         */
        void setFocusStatus(FocusState newstate, const QString &trainname);

        // Adaptive Focus
        /**
         * @brief focusAdaptiveComplete Forward the new focus state to the capture module state machine
         * @param trainname name of the optical train to select the focuser
         */
        void focusAdaptiveComplete(bool success, const QString &trainname);

        /**
         * @brief setFocusTemperatureDelta update the focuser's temperature delta
         * @param trainname name of the optical train to select the focuser
         */
        void setFocusTemperatureDelta(double focusTemperatureDelta, double absTemperature, const QString &trainname);

        /**
         * @brief setHFR Receive the measured HFR value of the latest frame
         * @param trainname name of the optical train to select the focuser
         */
        void setHFR(double newHFR, int position, bool inAutofocus, const QString &trainname);

        /**
         * @brief inSequenceAFRequested Focuser informs that the user wishes an AF run as soon as possible.
         * @param requested true iff AF is requested.
         * @param trainname name of the optical train to select the focuser
         */
        void inSequenceAFRequested(bool requested, const QString &trainname);

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
        Q_SCRIPTABLE void clearLog();
        void appendLogText(const QString &);

    private slots:

        // ////////////////////////////////////////////////////////////////////
        // UI controls
        // ////////////////////////////////////////////////////////////////////

    signals:
        Q_SCRIPTABLE void newLog(const QString &text);
        Q_SCRIPTABLE void meridianFlipStarted(const QString &trainname);
        Q_SCRIPTABLE void guideAfterMeridianFlip();
        Q_SCRIPTABLE void newStatus(CaptureState status, const QString &trainname, int cameraID);
        Q_SCRIPTABLE void captureComplete(const QVariantMap &metadata, const QString &trainname);

        void newFilterStatus(FilterState state);

        void ready();

        // communication with other modules
        void checkFocus(double, const QString &opticaltrain);
        void runAutoFocus(AutofocusReason autofocusReason, const QString &reasonInfo, const QString &trainname);
        void resetFocusFrame(const QString &trainname);
        void abortFocus(const QString &trainname);
        void adaptiveFocus(const QString &trainname);
        void suspendGuiding();
        void resumeGuiding();
        void dither();
        void resetNonGuidedDither();
        void captureTarget(QString targetName);
        void newImage(SequenceJob *job, const QSharedPointer<FITSData> &data, const QString &trainname);
        void newExposureProgress(SequenceJob *job, const QString &trainname);
        void newDownloadProgress(double, const QString &trainname);
        void sequenceChanged(const QJsonArray &sequence);
        void settingsUpdated(const QVariantMap &settings);        
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
        // camera handling
        // ////////////////////////////////////////////////////////////////////

        /**
         * @brief addCamera Add a new camera under capture control
         */
        QSharedPointer<Camera> addCamera();

        // ////////////////////////////////////////////////////////////////////
        // helper functions
        // ////////////////////////////////////////////////////////////////////

        // shortcut for the module state
        QSharedPointer<CameraState> mainCameraState() const
        {
            return mainCamera()->state();
        }
        // shortcut to device adapter
        QSharedPointer<CaptureDeviceAdaptor> mainCameraDevices()
        {
            return mainCamera()->devices();
        }
        // shortcut for the active job
        SequenceJob *activeJob() const
        {
            return mainCameraState()->getActiveJob();
        }

        /**
         * @brief findUnusedOpticalTrain Find the name of the first optical train that is not used by another tab
         * @return
         */
        const QString findUnusedOpticalTrain();

        /**
         * @brief Sync refocus options to the GUI settings
         */
        //void syncRefocusOptionsFromGUI();

        // ////////////////////////////////////////////////////////////////////
        // device control
        // ////////////////////////////////////////////////////////////////////

        // ////////////////////////////////////////////////////////////////////
        // Attributes
        // ////////////////////////////////////////////////////////////////////
        double seqExpose { 0 };
        int seqTotalCount;
        int seqCurrentCount { 0 };

        // overall state
        QSharedPointer<CaptureModuleState> m_moduleState;

        QPointer<QDBusInterface> mountInterface;

        QStringList m_LogText;

        // Flat field automation
        QMap<ScriptTypes, QString> m_Scripts;

        // Controls
        double GainSpinSpecialValue { INVALID_VALUE };
        double OffsetSpinSpecialValue { INVALID_VALUE };

        QVariantMap m_Metadata;
        void closeCameraTab(int tabIndex);
};

}
