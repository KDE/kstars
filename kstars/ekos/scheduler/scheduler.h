/*
    SPDX-FileCopyrightText: 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    DBus calls from GSoC 2015 Ekos Scheduler project:
    SPDX-FileCopyrightText: 2015 Daniel Leu <daniel_mihai.leu@cti.pub.ro>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_scheduler.h"
#include "opsoffsetsettings.h"
#include "opsalignmentsettings.h"
#include "opsjobssettings.h"
#include "opsscriptssettings.h"
#include "schedulertypes.h"
#include "ekos/align/align.h"
#include "indi/indiweather.h"
#include "schedulerjob.h"

#include <lilxml.h>

#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QtDBus/QDBusInterface>

#include <cstdint>

class QProgressIndicator;

class GeoLocation;
class SkyObject;
class KConfigDialog;
class TestSchedulerUnit;
class SolverUtils;
class TestEkosSchedulerOps;

namespace Ekos
{

class SequenceJob;
class GreedyScheduler;
class SchedulerProcess;
class SchedulerModuleState;
class SequenceEditor;

/**
 * @brief The Ekos scheduler is a simple scheduler class to orchestrate automated multi object observation jobs.
 * @author Jasem Mutlaq
 * @version 1.2
 */
class Scheduler : public QWidget, public Ui::Scheduler
{
        Q_OBJECT

        friend class FramingAssistantUI;

    public:

        /** @brief Columns, in the same order as UI. */
        typedef enum
        {
            SCHEDCOL_NAME = 0,
            SCHEDCOL_STATUS,
            SCHEDCOL_CAPTURES,
            SCHEDCOL_ALTITUDE,
            SCHEDCOL_STARTTIME,
            SCHEDCOL_ENDTIME,
        } SchedulerColumns;

        /** @brief Constructor, the starndard scheduler constructor. */
        Scheduler();
        /** @brief DebugConstructor, a constructor used in testing with a mock ekos. */
        Scheduler(const QString path, const QString interface,
                  const QString &ekosPathStr, const QString &ekosInterfaceStr);
        ~Scheduler() = default;

        QString getCurrentJobName();

        // shortcut
        SchedulerJob *activeJob();

        /**
         * @brief handleConfigChanged Update UI after changes to the global configuration
         */
        void handleConfigChanged();

        void addObject(SkyObject *object);

        /**
         * @brief importMosaic Import mosaic into planner and generate jobs for the scheduler.
         * @param payload metadata for the mosaic information.
         * @note Only Telescopius.com mosaic format is now supported.
         */
        bool importMosaic(const QJsonObject &payload);

        /** @defgroup SchedulerDBusInterface Ekos DBus Interface - Scheduler Module
             * Ekos::Align interface provides primary functions to run and stop the scheduler.
            */

        /*@{*/

        /**
         * @brief Set the file URL pointing to the capture sequence file
         * @param sequenceFileURL URL of the capture sequence file
         */
        void setSequence(const QString &sequenceFileURL);

        /**
         * @brief prepareGUI Perform once only GUI prep processing
         */
        void prepareGUI();

        // Settings popup
        OpsOffsetSettings *m_OpsOffsetSettings { nullptr };
        OpsAlignmentSettings *m_OpsAlignmentSettings { nullptr };
        OpsJobsSettings *m_OpsJobsSettings { nullptr };
        OpsScriptsSettings *m_OpsScriptsSettings { nullptr };


        /** DBUS interface function.
             * @brief Resets all jobs to IDLE
             */
        Q_SCRIPTABLE void sortJobsPerAltitude();

        void setProfile(const QString &profile)
        {
            schedulerProfileCombo->setCurrentText(profile);
        }
        QString profile()
        {
            return schedulerProfileCombo->currentText();
        }

        /**
         * @brief retrieve the error handling strategy from the UI
         */
        ErrorHandlingStrategy getErrorHandlingStrategy();

        /**
         * @brief select the error handling strategy (no restart, restart after all terminated, restart immediately)
         */
        void setErrorHandlingStrategy (ErrorHandlingStrategy strategy);

        /** @}*/

        // TODO: This section of static public and private methods should someday
        // be moved from Scheduler and placed in a separate class,
        // e.g. SchedulerPlanner or SchedulerJobEval
        /**
         * @brief Remove a job from current table row.
         * @param index
         */
        void removeJob();

        /**
         * @brief Remove a job by selecting a table row.
         * @param index
         */
        void removeOneJob(int index);

        /**
         * @brief addJob Add a new job from form values
         */
        Q_INVOKABLE void addJob(SchedulerJob *job = nullptr);
        /**
         * @brief addJob Add a new job from form values
         */
        Q_INVOKABLE void updateJob(int index = -1);

        /**
         * @brief createJob Create a new job from form values.
         * @param job job to be filled from UI values
         * @return true iff update was successful
         */
        bool fillJobFromUI(SchedulerJob *job);

        /**
         * @brief addToQueue Construct a SchedulerJob and add it to the queue or save job settings from current form values.
         * jobUnderEdit determines whether to add or edit
         */
        void saveJob(SchedulerJob *job = nullptr);

        void toggleScheduler();

        QJsonObject getSchedulerSettings();

        /**
         * @brief Toggle the Framing Assistant UI
         */
        Q_INVOKABLE void setFramingAssistantEnabled(bool enabled);

        /**
        * @brief settleSettings Run this function after timeout from debounce timer to update database
        * and emit settingsChanged signal. This is required so we don't overload output.
        */
        void settleSettings();

        // the state machine
        QSharedPointer<SchedulerModuleState> moduleState() const
        {
            return m_moduleState;
        }
        // the process engine
        QSharedPointer<SchedulerProcess> process()
        {
            return m_process;
        }

        /**
         * @brief saveFile Save scheduler jobs to disk
         * @param path Path to esl file to save jobs to
         * @return True if successful, false otherwise
         */
        bool saveFile(const QUrl &path);

        /**
         * @brief loadFile Load scheduler jobs from disk
         * @param path Oath to esl file to load jobs from
         * @return True if successful, false otherwise
         */
        bool loadFile(const QUrl &path);

        // Settings
        QVariantMap getAllSettings() const;
        void setAllSettings(const QVariantMap &settings);

    private:

        void setAlgorithm(int alg);

        friend TestSchedulerUnit;

        // TODO: See above TODO. End of static methods that might be moved to
        // a separate Scheduler-related class.

        /*@{*/
        /** @internal Safeguard flag to avoid registering signals from widgets multiple times.
         */
        bool jobChangesAreWatched { false };

    protected:

        /** @internal Enables signal watch on SchedulerJob form values in order to apply changes to current job.
          * @param enable is the toggle flag, true to watch for changes, false to ignore them.
          */
        void watchJobChanges(bool enable);

        /** @internal Marks the currently selected SchedulerJob as modified change.
         *
         * This triggers job re-evaluation.
         * Next time save button is invoked, the complete content is written to disk.
          */
        void setDirty();
        /** @} */

        /**
         * @brief updateJobTable Update the job's row in the job table. If the row does not exist, it will
         * be created on the fly. If job is null, update the entire table
         * @param job
         */
        void updateJobTable(SchedulerJob *job = nullptr);

        /**
         * @brief insertJobTableRow Insert a new row (empty) into the job table
         * @param row row number (starting with 0)
         * @param above insert above the given row (=true) or below (=false)
         */
        void insertJobTableRow(int row, bool above = true);

        /**
         * @brief Update the style of a cell, depending on the job's state
         */
        void updateCellStyle(SchedulerJob *job, QTableWidgetItem *cell);

    protected slots:

        /**
         * @brief checkInterfaceReady Sometimes syncProperties() is not sufficient since the ready signal could have fired already
         * and cannot be relied on to know once a module interface is ready. Therefore, we explicitly check if the module interface
         * is ready.
         * @param iface interface to test for readiness.
         */
        void interfaceReady(QDBusInterface *iface);

        void setWeatherStatus(ISD::Weather::Status status);

        /**
             * @brief select object from KStars's find dialog.
             */
        void selectObject();

        /**
             * @brief Selects FITS file for solving.
             */
        void selectFITS();

        /**
             * @brief Selects sequence queue.
             */
        void selectSequence();

        /**
             * @brief Selects sequence queue.
             */
        void selectStartupScript();

        /**
             * @brief Selects sequence queue.
             */
        void selectShutdownScript();

        /**
             * @brief editJob Edit an observation job
             * @param i index model in queue table
             */
        Q_INVOKABLE void loadJob(QModelIndex i);

        Q_INVOKABLE bool modifyJob(int index = -1);

        /**
         * @brief updateSchedulerURL Update scheduler URL after succesful loading a new file.
         */
        void updateSchedulerURL(const QString &fileURL);

        /**
             * @brief setJobAddApply Set first button state to add new job or apply changes.
             */
        void setJobAddApply(bool add_mode);

        /**
             * @brief setJobManipulation Enable or disable job manipulation buttons.
             */
        void setJobManipulation(bool can_reorder, bool can_delete, bool is_lead);

        /**
         * @brief set all GUI fields to the values of the given scheduler job
         */
        void syncGUIToJob(SchedulerJob *job);

        /**
         * @brief syncGUIToGeneralSettings set all UI fields that are not job specific
         */
        void syncGUIToGeneralSettings();

        /**
             * @brief jobSelectionChanged Update UI state when the job list is clicked once.
             */
        void clickQueueTable(QModelIndex index);

        /**
         * @brief clearJobTable delete all rows in the job table
         */
        Q_INVOKABLE void clearJobTable();

        /**
         * @brief clearLog Clears log entry
         */
        Q_INVOKABLE void clearLog();

        /**
         * @brief Update scheduler parameters to the currently selected scheduler job
         * @param selected table position
         * @param deselected table position
         */
        void queueTableSelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);

        /**
             * @brief reorderJobs Change the order of jobs in the UI based on a subset of its jobs.
             */
        bool reorderJobs(QList<SchedulerJob*> reordered_sublist);

        /**
             * @brief moveJobUp Move the selected job up in the job list.
             */
        void moveJobUp();

        /**
            * @brief moveJobDown Move the selected job down in the list.
            */
        void moveJobDown();

        /**
         * @brief handleSchedulerSleeping Update UI if scheduler is set to sleep
         * @param shutdown flag if a preemptive shutdown is executed
         * @param sleep flag if the scheduler will sleep
         */
        void handleSchedulerSleeping(bool shutdown, bool sleep);

        /**
         * @brief handleSchedulerStateChanged Update UI when the scheduler state changes
         */
        void handleSchedulerStateChanged(SchedulerState newState);

        /**
         * @brief handleSetPaused Update the UI when {@see #setPaused()} is called.
         */
        void handleSetPaused();

        void pause();
        bool save();
        void saveAs();

        /**
         * @brief load Open a file dialog to select an ESL file, and load its contents.
         * @param clearQueue Clear the queue before loading, or append ESL contents to queue.
         * @param filename If not empty, this file will be used instead of poping up a dialog.
         */
        Q_INVOKABLE bool load(bool clearQueue, const QString &filename = QString());

        Q_INVOKABLE void resetJobEdit();

        /**
         * @brief updateNightTime update the Twilight restriction with the argument job properties.
         * @param job SchedulerJob for which to display the next dawn and dusk, or the job currently selected if null, or today's next dawn and dusk if no job is selected.
         */
        void updateNightTime(SchedulerJob const * job = nullptr);

        /**
         * @brief schedulerStopped React when the process engine has stopped the scheduler
         */
        void schedulerStopped();

        /**
             * @brief resumeCheckStatus If the scheduler primary loop was suspended due to weather or sleep event, resume it again.
             */
        void resumeCheckStatus();

        /**
             * @brief checkWeather Check weather status and act accordingly depending on the current status of the scheduler and running jobs.
             */
        //void checkWeather();

        /**
             * @brief displayTwilightWarning Display twilight warning to user if it is unchecked.
             */
        void checkTwilightWarning(bool enabled);

    signals:
        void newStatus(Ekos::SchedulerState state);
        void weatherChanged(ISD::Weather::Status state);
        void newTarget(const QString &);
        // distance in arc-seconds measured by plate solving the a captured image and
        // comparing that position to the target position.
        void targetDistance(double distance);
        // Below 2 are for the Analyze timeline.
        void jobStarted(const QString &jobName);
        void jobEnded(const QString &jobName, const QString &endReason);
        void jobsUpdated(QJsonArray jobsList);
        void settingsUpdated(const QVariantMap &settings);

    private:
        /**
         * @brief handleJobsUpdated Update UI when jobs have been updated
         * @param jobsList
         */
        void handleJobsUpdated(QJsonArray jobsList);

        /**
         * @brief handleShutdownStarted Show that the shutdown has been started.
         */
        void handleShutdownStarted();

        /**
         * @brief processFITSSelection When a FITS file is selected, open it and try to guess
         * the object name, and its J2000 RA/DE to fill the UI with such info automatically.
         */
        void processFITSSelection(const QUrl &url);

        /**
         * @brief processCoordinates Parse coordinate values from UI.
         * @return true if both values are valid
         */
        bool processCoordinates(dms &ra, dms &dec);

        /**
         * @brief updateProfiles React upon changed profiles and update the UI
         */
        void updateProfiles();

        /**
         * @brief readCoordsFromUI Helper function reading the target coordinates from the UI.
         */
        bool readCoordsFromUI();

        /**
         * @brief updateStageLabel Helper function that updates the stage label.
         */
        void updateJobStageUI(SchedulerJobStage stage);

        // Target coordinates
        void setTargetCoords(const dms ra, const dms dec, bool isJ2000 = true);
        void displayTargetCoords();

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

        // ////////////////////////////////////////////////////////////////////
        // Optical Train handling
        // ////////////////////////////////////////////////////////////////////
        void refreshOpticalTrain();

        /**
         * @brief checkJobInputComplete Check if all inputs are filled such that a new job could be added.
         */
        void checkJobInputComplete();

        Ekos::Scheduler *ui { nullptr };

        // Interface strings for the dbus. Changeable for mocks when testing. Private so only tests can change.
        QString schedulerPathString { "/KStars/Ekos/Scheduler" };
        QString kstarsInterfaceString { "org.kde.kstars" };
        // This is only used in the constructor
        QString ekosInterfaceString { "org.kde.kstars.Ekos" };
        QString ekosPathString { "/KStars/Ekos" };

        // the state machine holding all states
        QSharedPointer<SchedulerModuleState> m_moduleState;
        // process engine implementing all process steps
        QSharedPointer<SchedulerProcess> m_process;

        // react upon changes of EKOS and INDI state
        void ekosStateChanged(EkosState state);
        void indiStateChanged(INDIState state);
        void indiCommunicationStatusChanged(CommunicationStatus status);

        // react upon state changes
        void startupStateChanged(StartupState state);
        void shutdownStateChanged(ShutdownState state);
        void parkWaitStateChanged(ParkWaitState state);

        /// URL to store the scheduler file
        QUrl schedulerURL;
        /// URL for Ekos Sequence
        QUrl sequenceURL;
        /// FITS URL to solve
        QUrl fitsURL;
        /// Busy indicator widget
        QProgressIndicator *pi { nullptr };
        /// Are we editing a job right now? Job row index
        int jobUnderEdit { -1 };
        /// Pointer to Geographic location
        GeoLocation *geo { nullptr };
        /// Current target coordinates
        SkyPoint targetCoords;

        /// Call checkWeather when weatherTimer time expires. It is equal to the UpdatePeriod time in INDI::Weather device.
        //QTimer weatherTimer;

        QUrl dirPath;

        // update the sleep label and its visibility
        void changeSleepLabel(QString text, bool show = true);
        // Used by the constructor in testing mainly so a mock ekos could be used.
        void setupScheduler(const QString &ekosPathStr, const QString &ekosInterfaceStr);


        /// Target coordinates for pointing check
        QSharedPointer<SolverUtils> m_Solver;

        void syncGreedyParams();

        friend TestEkosSchedulerOps;

        QSharedPointer<SequenceEditor> m_SequenceEditor;

        QTimer m_JobUpdateDebounce;
        QVariantMap m_Settings;
        QVariantMap m_GlobalSettings;
        QTimer m_DebounceTimer;
};
}
