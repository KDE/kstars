/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <KMessageBox>
#include <KDirWatch>
#include <KLocalizedString>
#include <KNotifications/KNotification>


#include "sequencejob.h"
#include "Options.h"

#include "kstars.h"
#include "kstarsdata.h"

#include "ui_calibrationoptions.h"

#define INVALID_TEMPERATURE 10000
#define INVALID_HA          10000
#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4
#define MAX_CAPTURE_RETRIES 3

namespace Ekos
{

SequenceJob::SequenceJob()
{
    statusStrings = QStringList() << i18n("Idle") << i18n("In Progress") << i18n("Error") << i18n("Aborted") << i18n("Complete");
    status = JOB_IDLE;
    exposure=count=delay=frameType=targetFilter=isoIndex=-1;
    currentTemperature=targetTemperature=INVALID_TEMPERATURE;
    captureFilter=FITS_NONE;
    preview=false;
    filterReady=temperatureReady=filterPostFocusReady=prepareReady=true;
    enforceTemperature=false;
    activeChip=NULL;
    activeCCD=NULL;
    activeFilter= NULL;
    statusCell = NULL;
    completed=0;
    captureRetires=0;

    calibrationSettings.flatFieldSource   = SOURCE_MANUAL;
    calibrationSettings.flatFieldDuration = DURATION_MANUAL;
    calibrationSettings.targetADU         = 0;
    calibrationSettings.preMountPark           = false;
    calibrationSettings.preDomePark            = false;

    typePrefixEnabled     = false;
    filterPrefixEnabled   = false;
    expPrefixEnabled      = false;
    timeStampPrefixEnabled= false;
}

void SequenceJob::reset()
{
    // Reset to default values
    activeChip->setBatchMode(false);
}

void SequenceJob::resetStatus()
{
    status = JOB_IDLE;
    completed=0;
    exposeLeft=0;
    captureRetires=0;
    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);
}

void SequenceJob::abort()
{
    status = JOB_ABORTED;
    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);
    if (activeChip->canAbort())
        activeChip->abortExposure();
    activeChip->setBatchMode(false);
}

void SequenceJob::done()
{
    status = JOB_DONE;

    if (statusCell)
        statusCell->setText(statusStrings[status]);

}

void SequenceJob::prepareCapture()
{
    prepareReady=false;

    activeChip->setBatchMode(!preview);

    activeCCD->setFITSDir(fitsDir);

    activeCCD->setISOMode(timeStampPrefixEnabled);

    activeCCD->setSeqPrefix(fullPrefix);

    activeCCD->setUploadMode(uploadMode);

    if (activeChip->isBatchMode())
        activeCCD->updateUploadSettings(remoteDir);

    if (isoIndex != -1)
    {
        if (isoIndex != activeChip->getISOIndex())
             activeChip->setISOIndex(isoIndex);
    }

    if (frameType == FRAME_DARK || frameType == FRAME_BIAS)
        filterReady = true;
    else if (targetFilter != -1 && activeFilter != NULL)
    {
        if (targetFilter == currentFilter)
            filterReady = true;
        else
        {
            filterReady = false;

            // Post Focus on Filter change. If frame is NOT light, then we do not perform autofocusing on filter change
            filterPostFocusReady = (!Options::autoFocusOnFilterChange() || frameType != FRAME_LIGHT);

            activeFilter->runCommand(INDI_SET_FILTER, &targetFilter);
        }
    }

    if (enforceTemperature && fabs(targetTemperature - currentTemperature) > Options::maxTemperatureDiff())
    {
        temperatureReady = false;
        activeCCD->setTemperature(targetTemperature);
    }

    if (prepareReady == false && temperatureReady && filterReady)
    {
        prepareReady = true;
        emit prepareComplete();
    }

}

//SequenceJob::CAPTUREResult SequenceJob::capture(bool isDark)
SequenceJob::CAPTUREResult SequenceJob::capture(bool noCaptureFilter)
{
    // If focusing is busy, return error
    //if (activeChip->getCaptureMode() == FITS_FOCUS)
      //  return CAPTURE_FOCUS_ERROR;

    activeChip->setBatchMode(!preview);

    if (targetFilter != -1 && activeFilter != NULL)
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

   if ((w > 0 && h > 0) && activeChip->canSubframe() && activeChip->setFrame(x, y, w, h) == false)
   {
        status = JOB_ERROR;

        if (preview == false && statusCell)
            statusCell->setText(statusStrings[status]);

        return CAPTURE_FRAME_ERROR;

   }

   if (activeChip->canBin() && activeChip->setBinning(binX, binY) == false)
   {
       status = JOB_ERROR;

       if (preview == false && statusCell)
           statusCell->setText(statusStrings[status]);

       return CAPTURE_BIN_ERROR;
   }

   activeChip->setFrameType(frameTypeName);
   activeChip->setCaptureMode(FITS_NORMAL);

   if (noCaptureFilter)
       activeChip->setCaptureFilter(FITS_NONE);
   else
       activeChip->setCaptureFilter(captureFilter);

   // If filter is different that CCD, send the filter info
   if (activeFilter && activeFilter != activeCCD)
       activeCCD->setFilter(filter);

   status = JOB_BUSY;

    if (preview == false && statusCell)
        statusCell->setText(statusStrings[status]);

    exposeLeft = exposure;

    activeChip->capture(exposure);

    return CAPTURE_OK;

}

void SequenceJob::setTargetFilter(int pos, const QString & name)
{
    targetFilter = pos;
    filter    = name;
}

void SequenceJob::setFrameType(int type, const QString & name)
{
    frameType = type;
    frameTypeName = name;
}

double SequenceJob::getExposeLeft() const
{
    return exposeLeft;
}

void SequenceJob::setExposeLeft(double value)
{
    exposeLeft = value;
}

void SequenceJob::setPrefixSettings(const QString &rawFilePrefix, bool filterEnabled, bool exposureEnabled, bool tsEnabled)
{
    rawPrefix               = rawFilePrefix;
    filterPrefixEnabled     = filterEnabled;
    expPrefixEnabled        = exposureEnabled;
    timeStampPrefixEnabled  = tsEnabled;
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
        temperatureReady = true;

    if (prepareReady == false && filterReady && temperatureReady && filterPostFocusReady && (status == JOB_IDLE || status == JOB_ABORTED))
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

QString SequenceJob::getRootFITSDir() const
{
    return rootFITSDir;
}

void SequenceJob::setRootFITSDir(const QString &value)
{
    rootFITSDir = value;
}

bool SequenceJob::getFilterPostFocusReady() const
{
    return filterPostFocusReady;
}

void SequenceJob::setFilterPostFocusReady(bool value)
{
    filterPostFocusReady = value;

    if (prepareReady == false && filterPostFocusReady && filterReady && temperatureReady && (status == JOB_IDLE || status == JOB_ABORTED))
    {
        prepareReady = true;
        emit prepareComplete();
    }
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
    return remoteDir;
}

void SequenceJob::setRemoteDir(const QString &value)
{
    remoteDir = value;
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
        filterReady = true;

    if (prepareReady == false && filterReady && temperatureReady && filterPostFocusReady && (status == JOB_IDLE || status == JOB_ABORTED))
    {
        prepareReady = true;
        emit prepareComplete();
    }
    else if (filterReady && filterPostFocusReady == false)
        emit checkFocus();
}

}
