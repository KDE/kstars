/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "sequencejobstatemachine.h"
#include "capturecommandprocessor.h"
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
        Q_PROPERTY(QString filename MEMBER m_filename)
        Q_PROPERTY(QString rawPrefix MEMBER m_RawPrefix)

    public:
        static QString const &ISOMarker;

        SequenceJob();
        SequenceJob(XMLEle *root); //, SchedulerJob *schedJob);
        ~SequenceJob() = default;

        CAPTUREResult capture(bool autofocusReady, FITSMode mode);
        void abort();
        void done();

        const QString &getStatusString()
        {
            return statusStrings[getStatus()];
        }
        int getDelay()
        {
            return delay;
        }
        int getCount()
        {
            return count;
        }
        int getCompleted()
        {
            return completed;
        }

        /**
         * @brief Remaining execution time in seconds (including download times).
         */
        int getJobRemainingTime(double estimatedDownloadTime);

        double getExposure() const
        {
            return exposure;
        }

        void setLocalDir(const QString &dir)
        {
            localDirectory = dir;
        }
        const QString &getLocalDir()
        {
            return localDirectory;
        }
        QString getSignature()
        {
            return QString(getLocalDir() + getDirectoryPostfix() + '/' + getFullPrefix()).remove(ISOMarker);
        }

        void setTargetFilter(int pos, const QString &name);
        int getCurrentFilter() const;
        void setCurrentFilter(int value);

        const QString &getFilterName() const
        {
            return filter;
        }

        void setFilterManager(const QSharedPointer<FilterManager> &manager)
        {
            filterManager = manager;
        }

        void setCaptureFilter(FITSScale capFilter)
        {
            captureFilter = capFilter;
        }
        FITSScale getCaptureFilter()
        {
            return captureFilter;
        }

        void setFullPrefix(const QString &cprefix)
        {
            fullPrefix = cprefix;
        }
        const QString &getFullPrefix()
        {
            return fullPrefix;
        }
        void setFrame(int in_x, int in_y, int in_w, int in_h)
        {
            x = in_x;
            y = in_y;
            w = in_w;
            h = in_h;
        }

        int getSubX()
        {
            return x;
        }
        int getSubY()
        {
            return y;
        }
        int getSubW()
        {
            return w;
        }
        int getSubH()
        {
            return h;
        }

        void setBin(int xbin, int ybin)
        {
            binX = xbin;
            binY = ybin;
        }
        int getXBin()
        {
            return binX;
        }
        int getYBin()
        {
            return binY;
        }

        void setDelay(int in_delay)
        {
            delay = in_delay;
        }
        void setCount(int in_count);
        void setExposure(double duration)
        {
            exposure = duration;
        }
        void setStatusCell(QTableWidgetItem * cell);
        void setCountCell(QTableWidgetItem * cell);
        void setCompleted(int in_completed);
        int getISOIndex() const;
        void setISOIndex(int value);

        double getExposeLeft() const;
        void setExposeLeft(double value);
        void resetStatus();

        void setPrefixSettings(const QString &rawFilePrefix, bool filterEnabled, bool exposureEnabled, bool tsEnabled);
        void getPrefixSettings(QString &rawFilePrefix, bool &filterEnabled, bool &exposureEnabled, bool &tsEnabled);

        bool isFilterPrefixEnabled() const
        {
            return filterPrefixEnabled;
        }
        bool isExposurePrefixEnabled() const
        {
            return expPrefixEnabled;
        }
        bool isTimestampPrefixEnabled() const
        {
            return timeStampPrefixEnabled;
        }

        void setTargetName(QString name)
        {
            targetName = name;
        }
        QString getTargetName() const
        {
            return targetName;
        }

        void setCurrentTemperature(double value);

        void resetCurrentGuiderDrift();

        double getTargetADU() const;
        void setTargetADU(double value);

        double getTargetADUTolerance() const;
        void setTargetADUTolerance(double value);

        int getCaptureRetires() const;
        void setCaptureRetires(int value);

        FlatFieldSource getFlatFieldSource() const;
        void setFlatFieldSource(const FlatFieldSource &value);

        FlatFieldDuration getFlatFieldDuration() const;
        void setFlatFieldDuration(const FlatFieldDuration &value);

        SkyPoint getWallCoord() const;
        void setWallCoord(const SkyPoint &value);

        bool isPreMountPark() const;
        void setPreMountPark(bool value);

        bool isPreDomePark() const;
        void setPreDomePark(bool value);

        const QMap<ScriptTypes, QString> &getScripts() const
        {
            return m_Scripts;
        }
        void setScripts(const QMap<ScriptTypes, QString> &scripts)
        {
            m_Scripts = scripts;
        }
        QString getScript(ScriptTypes type) const
        {
            return m_Scripts[type];
        }
        void setScript(ScriptTypes type, const QString &value)
        {
            m_Scripts[type] = value;
        }

        ISD::CCD::UploadMode getUploadMode() const;
        void setUploadMode(const ISD::CCD::UploadMode &value);

        QString getRemoteDir() const;
        void setRemoteDir(const QString &value);

        ISD::CCD::TransferFormat getTransforFormat() const;
        void setTransforFormat(const ISD::CCD::TransferFormat &value);

        double getGain() const;
        void setGain(double value);

        double getOffset() const;
        void setOffset(double value);

        QMap<QString, QMap<QString, double> > getCustomProperties() const;
        void setCustomProperties(const QMap<QString, QMap<QString, double> > &value);

        QString getDirectoryPostfix() const;
        void setDirectoryPostfix(const QString &value);

        bool getJobProgressIgnored() const;
        void setJobProgressIgnored(bool JobProgressIgnored);

        // Command processor implementing actions to be triggered by the state machine
        CaptureCommandProcessor commandProcessor;

        // ////////////////////////////////////////////////////////////////////
        // capture preparation relevant flags
        // ////////////////////////////////////////////////////////////////////
        // capture frame type (light, flat, dark, bias)
        CCDFrameType m_frameType { FRAME_LIGHT };
        CCDFrameType getFrameType() const { return m_frameType; }
        void setFrameType(CCDFrameType value) { m_frameType = value; }

        // is the sequence a preview or will the captures be stored?
        bool m_preview { false };
        bool isPreview() const { return m_preview; }
        void setPreview(bool value) { m_preview = value; }

        // should a certain temperature should be enforced?
        bool m_enforceTemperature { false };
        bool getEnforceTemperature() const { return m_enforceTemperature; }
        void setEnforceTemperature(bool value) { m_enforceTemperature = value; }

        // Ensure that guiding deviation is below a configured thresholdß
        bool m_enforceStartGuiderDrift { false };
        // Is the guider active?
        bool m_guiderActive { false };
        bool getEnforceStartGuiderDrift() const { return m_enforceStartGuiderDrift; }
        void setEnforceStartGuiderDrift(bool value) { m_enforceStartGuiderDrift= value; }
        bool isGuiderActive() const { return m_guiderActive; }
        void setGuiderActive(bool value) { m_guiderActive = value; }

        // ////////////////////////////////////////////////////////////////////////////
        // Facade to state machine
        // ////////////////////////////////////////////////////////////////////////////
        /**
         * @brief Retrieve the current status of the capture sequence job from the state machine
         */
        JOBStatus getStatus() { return stateMachine.getStatus(); }

        int getTargetFilter() { return stateMachine.targetFilterID; }

        double getTargetTemperature() { return stateMachine.targetTemperature; }
        void setTargetTemperature(double value) { stateMachine.targetTemperature = value; }

        double getTargetStartGuiderDrift() const { return stateMachine.targetStartGuiderDrift; }
        void setTargetStartGuiderDrift(double value) { stateMachine.targetStartGuiderDrift = value; }

        double getTargetRotation() const { return stateMachine.targetRotation; }
        void setTargetRotation(double value) { stateMachine.targetRotation = value; }

        void startPrepareCapture() { emit prepareCapture(getFrameType(), getEnforceTemperature(),
                                                         isGuiderActive() && getEnforceStartGuiderDrift(), isPreview()); }


signals:
        void prepareComplete();
        void prepareState(Ekos::CaptureState state);
        void checkFocus();
        // signals to be forwarded to the state machine
        void prepareCapture(CCDFrameType frameType, bool enforceCCDTemp, bool enforceStartGuiderDrift, bool isPreview);
        // update the current CCD temperature
        void updateCCDTemperature(double value);
        // update the current guiding deviation
        void updateGuiderDrift(double deviation_rms);
        // update the current camera rotator position
        void updateRotatorAngle(double value);


private:
        void setStatus(JOBStatus const);

        QStringList statusStrings;

        double exposure { -1 };

        QString filter;
        int binX { 0 };
        int binY { 0 };
        int x { 0 };
        int y { 0 };
        int w { 0 };
        int h { 0 };
        QString fullPrefix;
        int count { -1 };
        int delay { -1 };
        bool m_JobProgressIgnored { false };
        int isoIndex { -1 };
        int captureRetires { 0 };
        unsigned int completed { 0 };
        double exposeLeft { 0 };
        double gain { -1 };
        double offset { -1 };
        FITSScale captureFilter { FITS_NONE };
        QTableWidgetItem * statusCell { nullptr };
        QTableWidgetItem * countCell { nullptr };
        QMap<ScriptTypes, QString> m_Scripts;

        ISD::CCD::UploadMode uploadMode { ISD::CCD::UPLOAD_CLIENT };

        // Transfer Format
        ISD::CCD::TransferFormat transforFormat { ISD::CCD::FORMAT_FITS };

        // Directory Settings
        QString localDirectory;
        QString remoteDirectory;
        QString directoryPostfix;

        bool filterPrefixEnabled { false };
        bool expPrefixEnabled { false };
        bool timeStampPrefixEnabled { false };
        QString m_RawPrefix;
        QString targetName;

        QString m_filename;

        // Flat field variables
        struct
        {
            double targetADU { 0 };
            double targetADUTolerance { 250 };
            FlatFieldSource flatFieldSource { SOURCE_MANUAL };
            FlatFieldDuration flatFieldDuration { DURATION_MANUAL };
            SkyPoint wallCoord;
            bool preMountPark { false };
            bool preDomePark { false };

        } calibrationSettings;

        QMap<QString, QMap<QString, double>> customProperties;

        // Filter Manager
        QSharedPointer<FilterManager> filterManager;

        // State machine encapsulating the state of this capture sequence job
        SequenceJobStateMachine stateMachine;
};
}
