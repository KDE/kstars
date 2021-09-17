/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
        typedef enum { JOB_IDLE, JOB_BUSY, JOB_ERROR, JOB_ABORTED, JOB_DONE } JOBStatus;
        typedef enum
        {
            CAPTURE_OK,
            CAPTURE_FRAME_ERROR,
            CAPTURE_BIN_ERROR,
            CAPTURE_FILTER_BUSY,
            CAPTURE_FOCUS_ERROR,
            CAPTURE_GUIDER_DRIFT_WAIT
        } CAPTUREResult;
        typedef enum
        {
            ACTION_FILTER,
            ACTION_TEMPERATURE,
            ACTION_ROTATOR,
            ACTION_GUIDER_DRIFT
        } PrepareActions;

        static QString const &ISOMarker;

        SequenceJob();
        SequenceJob(XMLEle *root); //, SchedulerJob *schedJob);
        ~SequenceJob() = default;

        CAPTUREResult capture(bool autofocusReady);
        void abort();
        void done();
        void prepareCapture();

        JOBStatus getStatus()
        {
            return status;
        }
        const QString &getStatusString()
        {
            return statusStrings[status];
        }
        bool isPreview()
        {
            return preview;
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
        double getExposure() const
        {
            return exposure;
        }

        void setActiveCCD(ISD::CCD * ccd)
        {
            activeCCD = ccd;
        }
        ISD::CCD * getActiveCCD()
        {
            return activeCCD;
        }

        void setActiveFilter(ISD::GDInterface * filter)
        {
            activeFilter = filter;
        }
        ISD::GDInterface * getActiveFilter()
        {
            return activeFilter;
        }

        void setActiveRotator(ISD::GDInterface * rotator)
        {
            activeRotator = rotator;
        }
        ISD::GDInterface * getActiveRotator()
        {
            return activeRotator;
        }

        void setActiveChip(ISD::CCDChip * chip)
        {
            activeChip = chip;
        }
        ISD::CCDChip * getActiveChip()
        {
            return activeChip;
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
        int getTargetFilter()
        {
            return targetFilter;
        }
        int getCurrentFilter() const;
        void setCurrentFilter(int value);

        const QString &getFilterName()
        {
            return filter;
        }

        void setFrameType(CCDFrameType type);
        CCDFrameType getFrameType()
        {
            return frameType;
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

        void setPreview(bool enable)
        {
            preview = enable;
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

        bool isFilterPrefixEnabled()
        {
            return filterPrefixEnabled;
        }
        bool isExposurePrefixEnabled()
        {
            return expPrefixEnabled;
        }
        bool isTimestampPrefixEnabled()
        {
            return timeStampPrefixEnabled;
        }

        double getCurrentTemperature() const;
        void setCurrentTemperature(double value);

        double getTargetTemperature() const;
        void setTargetTemperature(double value);

        double getCurrentGuiderDrift() const;
        void setCurrentGuiderDrift(double value);
        void resetCurrentGuiderDrift();

        double getTargetStartGuiderDrift() const;
        void setTargetStartGuiderDrift(double value);

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

        bool getEnforceTemperature() const;
        void setEnforceTemperature(bool value);

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
        bool getEnforceStartGuiderDrift() const;
        void setEnforceStartGuiderDrift(bool value);
        void setGuiderActive(bool value)
        {
            guiderActive = value;
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

        double getTargetRotation() const;
        void setTargetRotation(double value);

        void setCurrentRotation(double value);

        QMap<QString, QMap<QString, double> > getCustomProperties() const;
        void setCustomProperties(const QMap<QString, QMap<QString, double> > &value);

        QString getDirectoryPostfix() const;
        void setDirectoryPostfix(const QString &value);

        bool getJobProgressIgnored() const;
        void setJobProgressIgnored(bool JobProgressIgnored);

    signals:
        void prepareComplete();
        void prepareState(Ekos::CaptureState state);
        void checkFocus();

    private:
        bool areActionsReady();
        void setAllActionsReady();
        void setStatus(JOBStatus const);
        bool guiderDriftOK() const;

        QStringList statusStrings;
        ISD::CCDChip * activeChip { nullptr };
        ISD::CCD * activeCCD { nullptr };
        ISD::GDInterface * activeFilter { nullptr };
        ISD::GDInterface * activeRotator { nullptr };

        double exposure { -1 };
        CCDFrameType frameType { FRAME_LIGHT };
        int targetFilter { -1 };
        int currentFilter { 0 };

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
        bool preview { false };
        bool prepareReady { true };
        bool enforceTemperature { false };
        bool enforceStartGuiderDrift { false };
        bool guiderActive { false };
        bool m_JobProgressIgnored { false };
        int isoIndex { -1 };
        int captureRetires { 0 };
        unsigned int completed { 0 };
        double exposeLeft { 0 };
        double currentTemperature { 0 };
        double targetTemperature { 0 };
        double currentGuiderDrift { 1e8 };
        double targetStartGuiderDrift { 0 };
        double gain { -1 };
        double offset { -1 };
        // Rotation in absolute ticks, NOT angle
        double targetRotation { 0 };
        double currentRotation { 0 };
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

        QString m_filename;

        JOBStatus status { JOB_IDLE };

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

        QMap<PrepareActions, bool> prepareActions;

        QMap<QString, QMap<QString, double>> customProperties;

        // Filter Manager
        QSharedPointer<FilterManager> filterManager;
};
}
