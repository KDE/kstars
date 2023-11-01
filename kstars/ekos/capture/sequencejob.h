/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "sequencejobstate.h"
#include "indi/indistd.h"
#include "indi/indicamera.h"
////#include "ekos/auxiliary/filtermanager.h"

#include <QTableWidgetItem>

class SkyPoint;

/**
 * @class SequenceJob
 * @short Sequence Job is a container for the details required to capture a series of images.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
namespace Ekos
{

class CaptureDeviceAdaptor;

class SequenceJob : public QObject
{
        Q_OBJECT

    public:
        static QString const &ISOMarker;
        static const QStringList StatusStrings();

        // Core Properties
        typedef enum
        {
            // Bool
            SJ_EnforceTemperature,
            // Bool
            SJ_EnforceStartGuiderDrift,
            // Bool
            SJ_GuiderActive,
            // Double
            SJ_Exposure,
            // QString
            SJ_Filter,
            // QString
            SJ_Format,
            // QString
            SJ_Encoding,
            // QPoint
            SJ_Binning,
            // QRect
            SJ_ROI,
            // QString
            SJ_FullPrefix,
            // Int
            SJ_Count,
            // Int
            SJ_Delay,
            // Int
            SJ_ISOIndex,
            // Double
            SJ_Gain,
            // Double
            SJ_Offset,
            // QString
            SJ_TargetName,
            //QString
            SJ_LocalDirectory,
            // QString
            SJ_PlaceholderFormat,
            // Uint
            SJ_PlaceholderSuffix,
            // QString
            SJ_RemoteDirectory,
            // QString
            SJ_RemoteFormatDirectory,
            // QString
            SJ_RemoteFormatFilename,
            // QString
            SJ_Filename,
            // Double
            SJ_TargetADU,
            // Double
            SJ_TargetADUTolerance,
            // QString
            SJ_Signature,
        } PropertyID;

        typedef enum
        {
            JOBTYPE_BATCH,    /* regular batch job            */
            JOBTYPE_PREVIEW,  /* previews (single or looping) */
            JOBTYPE_DARKFLAT  /* capturing dark flats         */
        } SequenceJobType;

        ////////////////////////////////////////////////////////////////////////
        /// Constructors
        ////////////////////////////////////////////////////////////////////////
        SequenceJob(const QSharedPointer<CaptureDeviceAdaptor> cp, const QSharedPointer<CaptureModuleState> sharedState, SequenceJobType jobType, XMLEle *root = nullptr, QString targetName = "");
        SequenceJob(XMLEle *root, QString targetName);
        ~SequenceJob() = default;

        ////////////////////////////////////////////////////////////////////////
        /// Capture Fuctions
        ////////////////////////////////////////////////////////////////////////
        /**
         * @brief startCapturing Initialize the camera and start capturing
         *
         * This step calls {@see SequenceJobState::initCapture()}, which triggers
         * all actions before the camera may start to capture. If the initialization
         * is completed, the sequence job state machine sends the signal
         * {@see SequenceJobState::initCaptureComplete()} which will trigger the
         * camera device to finally start capturing ({@see capture()}).
         *
         * @param autofocusReady was there a successful focus run previously?
         * @param mode what is the purpose of capturing?
         */
        void startCapturing(bool autofocusReady, FITSMode mode);
        /**
         * @brief capture As soon as everything is ready for the camera to start
         * capturing, this method triggers the camera device to start capturing.
         * @param mode what is the purpose of capturing?
         */
        void capture(FITSMode mode);
        void abort();
        void done();

        ////////////////////////////////////////////////////////////////////////
        /// Core Properties
        ////////////////////////////////////////////////////////////////////////
        void setCoreProperty(PropertyID id, const QVariant &value);
        QVariant getCoreProperty(PropertyID id) const;

        ////////////////////////////////////////////////////////////////////////
        /// Job Status Functions
        ////////////////////////////////////////////////////////////////////////
        const QString &getStatusString()
        {
            return StatusStrings()[getStatus()];
        }
        // Setter: Set how many captures have completed thus far
        void setCompleted(int value)
        {
            m_Completed = value;
        }
        // Getter: How many captured have completed thus far.
        int getCompleted() const
        {
            return m_Completed;
        }
        // Setter: Set how many more seconds to expose in this job
        void setExposeLeft(double value);
        // Getter: Get how many more seconds are left to expose.
        double getExposeLeft() const;
        // Reset: Reset the job status
        void resetStatus(JOBStatus status = JOB_IDLE);
        // Setter: Set how many times we re-try this job.
        void setCaptureRetires(int value);
        // Getter: Get many timed we retried this job already.
        int getCaptureRetires() const;
        // Getter: How many more seconds are remaining in this job given the
        // estimated download time.
        int getJobRemainingTime(double estimatedDownloadTime);

        ////////////////////////////////////////////////////////////////////////
        /// State Machine Functions
        ////////////////////////////////////////////////////////////////////////
        // Create all event connections between the state machine and the command processor
        void connectDeviceAdaptor();
        // Disconnect all event connections between the state machine and the command processor
        void disconnectDeviceAdaptor();
        // Setter: Set Target Filter Name
        void setTargetFilter(int pos, const QString &name);
        // Getter: Get Current Filter Slot
        int getCurrentFilter() const;
        // Retrieve the pier side from the state
        ISD::Mount::PierSide getPierSide() const;

        ////////////////////////////////////////////////////////////////////////
        /// Job Attribute Functions
        ////////////////////////////////////////////////////////////////////////
        // job type
        SequenceJobType jobType() const
        {
            return m_jobType;
        }
        void setJobType(SequenceJobType newJobType)
        {
            m_jobType = newJobType;
        }
        QString getSignature()
        {
            return (getCoreProperty(SJ_Signature).toString()).remove(".fits");
        }
        // Scripts
        const QMap<ScriptTypes, QString> &getScripts() const
        {
            return m_Scripts;
        }
        void setScripts(const QMap<ScriptTypes, QString> &scripts)
        {
            m_Scripts = scripts;
        }
        const QString getScript(ScriptTypes type) const
        {
            return m_Scripts[type];
        }
        void setScript(ScriptTypes type, const QString &value)
        {
            m_Scripts[type] = value;
        }
        // Custom Properties
        const QMap<QString, QMap<QString, QVariant> > getCustomProperties() const
        {
            return m_CustomProperties;
        }
        void setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value)
        {
            m_CustomProperties = value;
        }

        // Setter: Set upload mode
        void setUploadMode(ISD::Camera::UploadMode value);
        // Getter: get upload mode
        ISD::Camera::UploadMode getUploadMode() const;

        // Setter: Set flat field source
        void setCalibrationPreAction(uint32_t value);
        // Getter: Get flat field source
        uint32_t getCalibrationPreAction() const;

        // Setter: Set Wall SkyPoint Azimuth coords
        void setWallCoord(const SkyPoint &value);
        // Getter: Get Flat field source wall coords
        const SkyPoint &getWallCoord() const;

        // Setter: Set flat field duration
        void setFlatFieldDuration(FlatFieldDuration value);
        // Getter: Get flat field duration
        FlatFieldDuration getFlatFieldDuration() const;

        // Setter: Set job progress ignored flag
        void setJobProgressIgnored(bool value);
        bool getJobProgressIgnored() const;

        /**
         * @brief updateDeviceStates Update for all used device types whether there
         * is one connected.
         */
        void updateDeviceStates();
        /**
         * @brief Set the light box device
         */
        void setLightBox(ISD::LightBox *lightBox);

        /**
         * @brief Set the dust cap device
         */
        void setDustCap(ISD::DustCap *dustCap);

        /**
         * @brief Set the telescope device
         */
        void addMount(ISD::Mount *scope);

        /**
         * @brief Set the dome device
         */
        void setDome(ISD::Dome *dome);


        // ////////////////////////////////////////////////////////////////////////////
        // Facade to state machine
        // ////////////////////////////////////////////////////////////////////////////
        /**
         * @brief Retrieve the current status of the capture sequence job from the state machine
         */
        JOBStatus getStatus()
        {
            return state->getStatus();
        }

        void setFrameType(CCDFrameType value)
        {
            state->setFrameType(value);
        }
        CCDFrameType getFrameType() const
        {
            return state->getFrameType();
        }

        int getTargetFilter()
        {
            return state->targetFilterID;
        }

        double getTargetTemperature() const
        {
            return state->targetTemperature;
        }
        void setTargetTemperature(double value)
        {
            state->targetTemperature = value;
        }

        void setFocusStatus(FocusState value)
        {
            state->setFocusStatus(value);
        }

        double getTargetStartGuiderDrift() const
        {
            return state->targetStartGuiderDrift;
        }
        void setTargetStartGuiderDrift(double value)
        {
            state->targetStartGuiderDrift = value;
        }

        double getTargetRotation() const
        {
            return state->targetPositionAngle;
        }
        void setTargetRotation(double value)
        {
            state->targetPositionAngle = value;
        }

        SequenceJobState::CalibrationStage getCalibrationStage() const
        {
            return state->calibrationStage;
        }
        void setCalibrationStage(SequenceJobState::CalibrationStage value)
        {
            state->calibrationStage = value;
        }

        SequenceJobState::PreparationState getPreparationState() const
        {
            return state->m_PreparationState;
        }
        void setPreparationState(SequenceJobState::PreparationState value)
        {
            state->m_PreparationState = value;
        }

        bool getAutoFocusReady() const
        {
            return state->autoFocusReady;
        }
        void setAutoFocusReady(bool value)
        {
            state->autoFocusReady = value;
        }

        /**
         * @brief Central entry point to start all activities that are necessary
         *        before capturing may start. Signals {@see prepareComplete()} as soon as
         *        everything is ready.
         */
        void prepareCapture();
        /**
         * @brief processPrepareComplete All preparations necessary for capturing are completed
         * @param success true iff preparation succeeded
         */
        void processPrepareComplete(bool success = true);
        /**
         * @brief Abort capturing
         */
        void processAbortCapture();

        /**
         * @brief Check if all initial tasks are completed so that capturing
         *        of flats may start.
         * @return IPS_OK if cap is closed, IPS_BUSY if not and IPS_ALERT if the
         *         process should be aborted.
         */
        IPState checkFlatFramePendingTasksCompleted();

    signals:
        // All preparations necessary for capturing are completed
        void prepareComplete(bool success = true);
        // Manage the result when capturing has been started
        void captureStarted(CaptureModuleState::CAPTUREResult rc);
        // Abort capturing
        void abortCapture();
        // log entry
        void newLog(QString);

        void prepareState(CaptureState state);
        // signals to be forwarded to the state machine
        void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);
        // update the current guiding deviation
        void updateGuiderDrift(double deviation_rms);

private:
        /**
         * @brief init Initialize the sequence job from its XML representation
         */
        void init(SequenceJobType jobType, XMLEle *root, QSharedPointer<CaptureModuleState> sharedState, QString targetName);

        // job type (batch, preview, ...)
        SequenceJobType m_jobType;

        void setStatus(JOBStatus const);

        //////////////////////////////////////////////////////////////
        /// Custom Types
        /// We save all core sequence properties in QVariant map
        //////////////////////////////////////////////////////////////
        QMap<PropertyID, QVariant> m_CoreProperties;

        //////////////////////////////////////////////////////////////
        /// Custom Types
        /// We don't use Q_PROPERTY for these to simplify use
        //////////////////////////////////////////////////////////////
        QMap<QString, QMap<QString, QVariant>> m_CustomProperties;
        FlatFieldDuration m_FlatFieldDuration { DURATION_MANUAL };
        // Capture Scripts
        QMap<ScriptTypes, QString> m_Scripts;
        // Upload Mode
        ISD::Camera::UploadMode m_UploadMode { ISD::Camera::UPLOAD_CLIENT };
        // Transfer Format
        QString m_TransferFormat { "FITS" };

        //////////////////////////////////////////////////////////////
        /// Status Variables
        //////////////////////////////////////////////////////////////
        int m_CaptureRetires { 0 };
        uint32_t m_Completed { 0 };
        double m_ExposeLeft { 0 };
        bool m_JobProgressIgnored {false};

        //////////////////////////////////////////////////////////////
        /// Device access
        //////////////////////////////////////////////////////////////

        /**
         * @brief frameTypes Retrieve the frame types from the active camera's primary chip.
         */
        QStringList frameTypes();
        /**
         * @brief filterLabels list of currently available filter labels
         */
        QStringList filterLabels();

        //////////////////////////////////////////////////////////////
        /// State machines encapsulating the state of this capture sequence job
        //////////////////////////////////////////////////////////////
        QSharedPointer<CaptureDeviceAdaptor> devices;
        QSharedPointer<SequenceJobState> state;
};
}
