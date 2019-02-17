/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "sequencejob.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"

#include <KNotifications/KNotification>

#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4

namespace Ekos
{
QString const &SequenceJob::ISOMarker("_ISO8601");

SequenceJob::SequenceJob()
{
    statusStrings = QStringList() << i18n("Idle") << i18n("In Progress") << i18n("Error") << i18n("Aborted")
                    << i18n("Complete");
    currentTemperature = targetTemperature = Ekos::INVALID_VALUE;
    targetRotation = currentRotation = Ekos::INVALID_VALUE;

    prepareActions[ACTION_FILTER] = true;
    prepareActions[ACTION_TEMPERATURE] = true;
    //prepareActions[ACTION_POST_FOCUS] = true;
    prepareActions[ACTION_ROTATOR] = true;
}

void SequenceJob::reset()
{
    // Reset to default values
    activeChip->setBatchMode(false);
}

void SequenceJob::resetStatus()
{
    setStatus(JOB_IDLE);
    setCompleted(0);
    exposeLeft     = 0;
    captureRetires = 0;
    m_JobProgressIgnored = false;
}

void SequenceJob::abort()
{
    setStatus(JOB_ABORTED);
    if (activeChip->canAbort())
        activeChip->abortExposure();
    activeChip->setBatchMode(false);
}

void SequenceJob::done()
{
    setStatus(JOB_DONE);
}

void SequenceJob::prepareCapture()
{
    status = JOB_BUSY;

    prepareReady = false;

    // Reset all prepare actions
    setAllActionsReady();

    activeChip->setBatchMode(!preview);

    if (localDirectory.isEmpty() == false)
        activeCCD->setFITSDir(localDirectory + directoryPostfix);

    activeCCD->setISOMode(timeStampPrefixEnabled);

    activeCCD->setSeqPrefix(fullPrefix);

    activeCCD->setUploadMode(uploadMode);

    // Set all custom properties
    //if (preview == false)
    //{
    QMapIterator<QString, QMap<QString, double>> i(customProperties);
    while (i.hasNext())
    {
        i.next();
        INDI::Property *customProp = activeCCD->getProperty(i.key());
        if (customProp)
        {
            QMap<QString, double> numbers = i.value();
            QMapIterator<QString, double> j(numbers);
            INumberVectorProperty *np = customProp->getNumber();
            while (j.hasNext())
            {
                j.next();
                INumber *oneNumber = IUFindNumber(np, j.key().toLatin1().data());
                if (oneNumber)
                    oneNumber->value = j.value();
            }

            activeCCD->getDriverInfo()->getClientManager()->sendNewNumber(np);
        }
    }
    //}

    if (activeChip->isBatchMode() && remoteDirectory.isEmpty() == false)
        activeCCD->updateUploadSettings(remoteDirectory + directoryPostfix);

    if (isoIndex != -1)
    {
        if (isoIndex != activeChip->getISOIndex())
            activeChip->setISOIndex(isoIndex);
    }

    if (gain != -1)
    {
        activeCCD->setGain(gain);
    }

    // Check if we need to change filter wheel
    if (frameType == FRAME_LIGHT && targetFilter != -1 && activeFilter != nullptr)
    {
        if (targetFilter == currentFilter)
            prepareActions[ACTION_FILTER] = true;
        else
        {
            prepareActions[ACTION_FILTER] = false;

            emit prepareState(CAPTURE_CHANGING_FILTER);

            FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES;
            // Don't perform autofocus on preview
            if (isPreview())
                policy = static_cast<FilterManager::FilterPolicy>(policy & ~FilterManager::AUTOFOCUS_POLICY);

            filterManager->setFilterPosition(targetFilter, policy);
        }
    }
    else
    {
        prepareActions[ACTION_FILTER] = true;
    }

    // Check if we need to update temperature
    if (enforceTemperature && fabs(targetTemperature - currentTemperature) > Options::maxTemperatureDiff())
    {
        prepareActions[ACTION_TEMPERATURE] = false;
        emit prepareState(CAPTURE_SETTING_TEMPERATURE);
        activeCCD->setTemperature(targetTemperature);
    }

    // Check if we need to update rotator
    if (targetRotation != Ekos::INVALID_VALUE && fabs(currentRotation - targetRotation) * 60 > Options::astrometryRotatorThreshold())
    {
        // PA = RawAngle * Multiplier + Offset
        double rawAngle = (targetRotation - Options::pAOffset()) / Options::pAMultiplier();
        prepareActions[ACTION_ROTATOR] = false;
        emit prepareState(CAPTURE_SETTING_ROTATOR);
        activeRotator->runCommand(INDI_SET_ROTATOR_ANGLE, &rawAngle);
    }

    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }
}

void SequenceJob::setAllActionsReady()
{
    QMutableMapIterator<PrepareActions, bool> i(prepareActions);

    while (i.hasNext())
    {
        i.next();
        i.setValue(true);
    }
}

void SequenceJob::setStatus(JOBStatus const in_status)
{
    status = in_status;
    if( !preview && nullptr != statusCell)
        statusCell->setText(statusStrings[in_status]);
}

void SequenceJob::setStatusCell(QTableWidgetItem* cell)
{
    statusCell = cell;
    if (nullptr != cell)
    {
        cell->setTextAlignment(Qt::AlignHCenter);
        cell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        setStatus(getStatus());
    }
}

void SequenceJob::setCount(int in_count)
{
    count = in_count;
    if( !preview && nullptr != countCell)
        countCell->setText(QString("%L1/%L2").arg(completed).arg(in_count));
}

void SequenceJob::setCompleted(int in_completed)
{
    completed = in_completed;
    if( !preview && nullptr != countCell)
        countCell->setText(QString("%L1/%L2").arg(in_completed).arg(count));
}

void SequenceJob::setCountCell(QTableWidgetItem* cell)
{
    countCell = cell;
    if (nullptr != cell)
    {
        cell->setTextAlignment(Qt::AlignHCenter);
        cell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        setCount(getCount());
    }
}

bool SequenceJob::getJobProgressIgnored() const
{
    return m_JobProgressIgnored;
}

void SequenceJob::setJobProgressIgnored(bool JobProgressIgnored)
{
    m_JobProgressIgnored = JobProgressIgnored;
}

QString SequenceJob::getDirectoryPostfix() const
{
    return directoryPostfix;
}

void SequenceJob::setDirectoryPostfix(const QString &value)
{
    directoryPostfix = value;
}

QMap<QString, QMap<QString, double> > SequenceJob::getCustomProperties() const
{
    return customProperties;
}

void SequenceJob::setCustomProperties(const QMap<QString, QMap<QString, double> > &value)
{
    customProperties = value;
}

bool SequenceJob::areActionsReady()
{
    for (bool &ready : prepareActions.values())
    {
        if (ready == false)
            return false;
    }

    return true;
}

//SequenceJob::CAPTUREResult SequenceJob::capture(bool isDark)
SequenceJob::CAPTUREResult SequenceJob::capture(bool noCaptureFilter)
{
    // If focusing is busy, return error
    //if (activeChip->getCaptureMode() == FITS_FOCUS)
    //  return CAPTURE_FOCUS_ERROR;

    activeChip->setBatchMode(!preview);

    if (targetFilter != -1 && activeFilter != nullptr)
    {
        if (targetFilter != currentFilter)
        {
            activeFilter->runCommand(INDI_SET_FILTER, &targetFilter);
            return CAPTURE_FILTER_BUSY;
        }
    }

    if (isoIndex != -1)
    {
        if (isoIndex != activeChip->getISOIndex())
            activeChip->setISOIndex(isoIndex);
    }

    // Only attempt to set ROI and Binning if CCD transfer format is FITS
    if (activeCCD->getTransferFormat() == ISD::CCD::FORMAT_FITS)
    {
        int currentBinX = 1, currentBinY = 1;
        activeChip->getBinning(&currentBinX, &currentBinY);

        // N.B. Always set binning _before_ setting frame because if the subframed image
        // is problematic in 1x1 but works fine for 2x2, then it would fail it was set first
        // So setting binning first always ensures this will work.
        if (activeChip->canBin() && activeChip->setBinning(binX, binY) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_BIN_ERROR;
        }

        if ((w > 0 && h > 0) && activeChip->canSubframe() && activeChip->setFrame(x, y, w, h, currentBinX != binX) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_FRAME_ERROR;
        }
    }

    activeChip->setFrameType(frameType);
    activeChip->setCaptureMode(FITS_NORMAL);

    if (noCaptureFilter)
        activeChip->setCaptureFilter(FITS_NONE);
    else
        activeChip->setCaptureFilter(captureFilter);

    // If filter is different that CCD, send the filter info
    if (activeFilter && activeFilter != activeCCD)
        activeCCD->setFilter(filter);

    //status = JOB_BUSY;
    setStatus(getStatus());

    exposeLeft = exposure;

    activeChip->capture(exposure);

    return CAPTURE_OK;
}

void SequenceJob::setTargetFilter(int pos, const QString &name)
{
    targetFilter = pos;
    filter       = name;
}

void SequenceJob::setFrameType(CCDFrameType type)
{
    frameType = type;
}

double SequenceJob::getExposeLeft() const
{
    return exposeLeft;
}

void SequenceJob::setExposeLeft(double value)
{
    exposeLeft = value;
}

void SequenceJob::setPrefixSettings(const QString &rawFilePrefix, bool filterEnabled, bool exposureEnabled,
                                    bool tsEnabled)
{
    rawPrefix              = rawFilePrefix;
    filterPrefixEnabled    = filterEnabled;
    expPrefixEnabled       = exposureEnabled;
    timeStampPrefixEnabled = tsEnabled;
}

void SequenceJob::getPrefixSettings(QString &rawFilePrefix, bool &filterEnabled, bool &exposureEnabled, bool &tsEnabled)
{
    rawFilePrefix   = rawPrefix;
    filterEnabled   = filterPrefixEnabled;
    exposureEnabled = expPrefixEnabled;
    tsEnabled       = timeStampPrefixEnabled;
}

double SequenceJob::getCurrentTemperature() const
{
    return currentTemperature;
}

void SequenceJob::setCurrentTemperature(double value)
{
    currentTemperature = value;

    if (enforceTemperature == false || fabs(targetTemperature - currentTemperature) <= Options::maxTemperatureDiff())
        prepareActions[ACTION_TEMPERATURE] = true;

    //if (prepareReady == false && areActionsReady() && (status == JOB_IDLE || status == JOB_ABORTED))
    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }
}

double SequenceJob::getTargetTemperature() const
{
    return targetTemperature;
}

void SequenceJob::setTargetTemperature(double value)
{
    targetTemperature = value;
}

double SequenceJob::getTargetADU() const
{
    return calibrationSettings.targetADU;
}

void SequenceJob::setTargetADU(double value)
{
    calibrationSettings.targetADU = value;
}

double SequenceJob::getTargetADUTolerance() const
{
    return calibrationSettings.targetADUTolerance;
}

void SequenceJob::setTargetADUTolerance(double value)
{
    calibrationSettings.targetADUTolerance = value;
}

int SequenceJob::getCaptureRetires() const
{
    return captureRetires;
}

void SequenceJob::setCaptureRetires(int value)
{
    captureRetires = value;
}

FlatFieldSource SequenceJob::getFlatFieldSource() const
{
    return calibrationSettings.flatFieldSource;
}

void SequenceJob::setFlatFieldSource(const FlatFieldSource &value)
{
    calibrationSettings.flatFieldSource = value;
}

FlatFieldDuration SequenceJob::getFlatFieldDuration() const
{
    return calibrationSettings.flatFieldDuration;
}

void SequenceJob::setFlatFieldDuration(const FlatFieldDuration &value)
{
    calibrationSettings.flatFieldDuration = value;
}

SkyPoint SequenceJob::getWallCoord() const
{
    return calibrationSettings.wallCoord;
}

void SequenceJob::setWallCoord(const SkyPoint &value)
{
    calibrationSettings.wallCoord = value;
}

bool SequenceJob::isPreMountPark() const
{
    return calibrationSettings.preMountPark;
}

void SequenceJob::setPreMountPark(bool value)
{
    calibrationSettings.preMountPark = value;
}

bool SequenceJob::isPreDomePark() const
{
    return calibrationSettings.preDomePark;
}

void SequenceJob::setPreDomePark(bool value)
{
    calibrationSettings.preDomePark = value;
}

bool SequenceJob::getEnforceTemperature() const
{
    return enforceTemperature;
}

void SequenceJob::setEnforceTemperature(bool value)
{
    enforceTemperature = value;
}

QString SequenceJob::getPostCaptureScript() const
{
    return postCaptureScript;
}

void SequenceJob::setPostCaptureScript(const QString &value)
{
    postCaptureScript = value;
}

ISD::CCD::UploadMode SequenceJob::getUploadMode() const
{
    return uploadMode;
}

void SequenceJob::setUploadMode(const ISD::CCD::UploadMode &value)
{
    uploadMode = value;
}

QString SequenceJob::getRemoteDir() const
{
    return remoteDirectory;
}

void SequenceJob::setRemoteDir(const QString &value)
{
    remoteDirectory = value;
    if (remoteDirectory.endsWith('/'))
        remoteDirectory.chop(1);
}

ISD::CCD::TransferFormat SequenceJob::getTransforFormat() const
{
    return transforFormat;
}

void SequenceJob::setTransforFormat(const ISD::CCD::TransferFormat &value)
{
    transforFormat = value;
}

double SequenceJob::getGain() const
{
    return gain;
}

void SequenceJob::setGain(double value)
{
    gain = value;
}

double SequenceJob::getTargetRotation() const
{
    return targetRotation;
}

void SequenceJob::setTargetRotation(double value)
{
    targetRotation = value;
}

int SequenceJob::getISOIndex() const
{
    return isoIndex;
}

void SequenceJob::setISOIndex(int value)
{
    isoIndex = value;
}

int SequenceJob::getCurrentFilter() const
{
    return currentFilter;
}

void SequenceJob::setCurrentFilter(int value)
{
    currentFilter = value;

    if (currentFilter == targetFilter)
        prepareActions[ACTION_FILTER] = true;

    //if (prepareReady == false && areActionsReady() && (status == JOB_IDLE || status == JOB_ABORTED))
    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }
    //else if (prepareActions[ACTION_FILTER] == true && prepareActions[ACTION_POST_FOCUS] == false)
    //emit checkFocus();
}

void SequenceJob::setCurrentRotation(double value)
{
    currentRotation = value;

    if (fabs(currentRotation - targetRotation) * 60 <= Options::astrometryRotatorThreshold())
        prepareActions[ACTION_ROTATOR] = true;

    //if (prepareReady == false && areActionsReady() && (status == JOB_IDLE || status == JOB_ABORTED))
    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }

}
}
