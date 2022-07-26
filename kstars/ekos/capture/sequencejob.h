/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "sequencejobstate.h"
#include "capturedeviceadaptor.h"
#include "indi/indistd.h"
#include "indi/indiccd.h"
#include "ekos/auxiliary/filtermanager.h"
#include "skyobjects/skypoint.h"

#include <QTableWidgetItem>

class SchedulerJob;
/**
 * @class SequenceJob
 * @short Sequence Job is a container for the details required to capture a series of images.
 *
 * @author Jasem Mutlaq
 * @version 1.0
 */
namespace Ekos
{
class SequenceJob : public QObject
{
        Q_OBJECT

    public:
        static QString const &ISOMarker;
        static const QStringList StatusStrings;

        // Core Properties
        typedef enum
        {
            // Bool
            SJ_Preview,
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
            SJ_LocalDirectory,
            // QString
            SJ_RemoteDirectory,
            // QString
            SJ_DirectoryPostfix,
            // Bool
            SJ_FilterPrefixEnabled,
            // Bool
            SJ_ExpPrefixEnabled,
            // Bool
            SJ_TimeStampPrefixEnabled,
            // QString
            SJ_RawPrefix,
            // QString
            SJ_TargetName,
            // QString
            SJ_Filename,
            // Bool
            SJ_DarkFlat,
            // Double
            SJ_TargetADU,
            // Double
            SJ_TargetADUTolerance,
        } PropertyID;

        ////////////////////////////////////////////////////////////////////////
        /// Constructors
        ////////////////////////////////////////////////////////////////////////
        SequenceJob(const QSharedPointer<CaptureDeviceAdaptor> cp, const QSharedPointer<SequenceJobState::CaptureState> sharedState);
        SequenceJob(XMLEle *root);
        ~SequenceJob() = default;

        ////////////////////////////////////////////////////////////////////////
        /// Capture Fuctions
        ////////////////////////////////////////////////////////////////////////
        CAPTUREResult capture(bool autofocusReady, FITSMode mode);
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
            return StatusStrings[getStatus()];
        }
        // Setter: Set how many captures have completed thus far
        void setCompleted(int value);
        // Getter: How many captured have completed thus far.
        int getCompleted() const;
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
        // Setter: Set Target Filter Name
        void setTargetFilter(int pos, const QString &name);
        // Setter: Set Current filter slot
        void setCurrentFilter(int value);
        // Getter: Get Current Filter Slot
        int getCurrentFilter() const;
        // Setter: Set active filter manager.
        void setFilterManager(const QSharedPointer<FilterManager> &manager);

        ////////////////////////////////////////////////////////////////////////
        /// GUI Related Functions
        ////////////////////////////////////////////////////////////////////////
        void setStatusCell(QTableWidgetItem * cell);
        void setCountCell(QTableWidgetItem * cell);

        ////////////////////////////////////////////////////////////////////////
        /// Job Attribute Functions
        ////////////////////////////////////////////////////////////////////////
        QString getSignature();
        // Scripts
        const QMap<ScriptTypes, QString> &getScripts() const;
        void setScripts(const QMap<ScriptTypes, QString> &scripts);
        QString getScript(ScriptTypes type) const;
        void setScript(ScriptTypes type, const QString &value);
        // Custome Properties
        QMap<QString, QMap<QString, QVariant> > getCustomProperties() const;
        void setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value);

        // Setter: Set Frame Type
        void setFrameType(CCDFrameType value);
        // Getter: Get Frame Type
        CCDFrameType getFrameType() const;

        // Setter: Set upload mode
        void setUploadMode(ISD::CCD::UploadMode value);
        // Getter: get upload mode
        ISD::CCD::UploadMode getUploadMode() const;

        // Setter: Set flat field source
        void setFlatFieldSource(FlatFieldSource value);
        // Getter: Get flat field source
        FlatFieldSource getFlatFieldSource() const;

        // Setter: Set Wall SkyPoint Azimuth coords
        void setWallCoord(const SkyPoint& value);
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
        void setTelescope(ISD::Telescope *scope);

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
            return stateMachine->getStatus();
        }

        int getTargetFilter()
        {
            return stateMachine->targetFilterID;
        }

        double getTargetTemperature()
        {
            return stateMachine->targetTemperature;
        }
        void setTargetTemperature(double value)
        {
            stateMachine->targetTemperature = value;
        }

        double getTargetStartGuiderDrift() const
        {
            return stateMachine->targetStartGuiderDrift;
        }
        void setTargetStartGuiderDrift(double value)
        {
            stateMachine->targetStartGuiderDrift = value;
        }

        double getTargetRotation() const
        {
            return stateMachine->targetPositionAngle;
        }
        void setTargetRotation(double value)
        {
            stateMachine->targetPositionAngle = value;
        }

        bool getPreMountPark() const
        {
            return stateMachine->preMountPark;
        }
        void setPreMountPark(bool value)
        {
            stateMachine->preMountPark = value;
        }

        bool getPreDomePark() const
        {
            return stateMachine->preDomePark;
        }
        void setPreDomePark(bool value)
        {
            stateMachine->preDomePark = value;
        }

        SequenceJobState::CalibrationStage getCalibrationStage() const
        {
            return stateMachine->calibrationStage;
        }
        void setCalibrationStage(SequenceJobState::CalibrationStage value)
        {
            stateMachine->calibrationStage = value;
        }

        SequenceJobState::PreparationState getPreparationState() const
        {
            return stateMachine->m_PreparationState;
        }
        void setPreparationState(SequenceJobState::PreparationState value)
        {
            stateMachine->m_PreparationState = value;
        }

        bool getAutoFocusReady() const
        {
            return stateMachine->autoFocusReady;
        }
        void setAutoFocusReady(bool value)
        {
            stateMachine->autoFocusReady = value;
        }

        /**
         * @brief Central entry point to start all activities that are necessary
         *        before capturing may start. Signals {@see prepareComplete()} as soon as
         *        everything is ready.
         */
        void prepareCapture();
        /**
         * @brief All preparations necessary for capturing are completed
         */
        void processPrepareComplete();
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
        void prepareComplete();
        // Abort capturing
        void abortCapture();
        // log entry
        void newLog(QString);

        void prepareState(CaptureState state);
        void checkFocus();
        // signals to be forwarded to the state machine
        void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);
        // update the current guiding deviation
        void updateGuiderDrift(double deviation_rms);


private:
        // should not be used from outside
        SequenceJob();

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
        ISD::CCD::UploadMode m_UploadMode { ISD::CCD::UPLOAD_CLIENT };
        // Transfer Format
        QString m_TransferFormat { "FITS" };
        // capture frame type (light, flat, dark, bias)
        CCDFrameType m_FrameType { FRAME_LIGHT };

        //////////////////////////////////////////////////////////////
        /// Filter Manager
        //////////////////////////////////////////////////////////////
        QSharedPointer<FilterManager> m_FilterManager;

        //////////////////////////////////////////////////////////////
        /// Status Variable
        //////////////////////////////////////////////////////////////
        int m_CaptureRetires { 0 };
        uint32_t m_Completed { 0 };
        double m_ExposeLeft { 0 };
        bool m_JobProgressIgnored {false};
        QTableWidgetItem * statusCell { nullptr };
        QTableWidgetItem * countCell { nullptr };

        //////////////////////////////////////////////////////////////
        /// State machines encapsulating the state of this capture sequence job
        //////////////////////////////////////////////////////////////
        QSharedPointer<CaptureDeviceAdaptor> captureDeviceAdaptor;
        SequenceJobState *stateMachine { nullptr };
        /**
         * @brief Create all event connections between the state machine and the command processor
         */
        void connectDeviceAdaptor();
        /**
         * @brief Disconnect all event connections between the state machine and the command processor
         */
        void disconnectDeviceAdaptor();

};
}
