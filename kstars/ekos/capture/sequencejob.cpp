/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejob.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "indi/driverinfo.h"
#include "indi/clientmanager.h"
#include "ekos/scheduler/schedulerjob.h"

#include <KNotifications/KNotification>
#include <ekos_capture_debug.h>

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
    currentGuiderDrift = targetStartGuiderDrift = Ekos::INVALID_VALUE;
    targetRotation = currentRotation = Ekos::INVALID_VALUE;

    prepareActions[ACTION_FILTER] = true;
    prepareActions[ACTION_TEMPERATURE] = true;
    prepareActions[ACTION_ROTATOR] = true;
    prepareActions[ACTION_GUIDER_DRIFT] = true;
}

SequenceJob::SequenceJob(XMLEle *root):
    SequenceJob()
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    // We expect all data read from the XML to be in the C locale - QLocale::c().
    QLocale cLocale = QLocale::c();

    const QMap<QString, CCDFrameType> frameTypes =
    {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    frameType = FRAME_NONE;
    exposure = 0;
    /* Reset light frame presence flag before enumerating */
    // JM 2018-09-14: If last sequence job is not LIGHT
    // then scheduler job light frame is set to whatever last sequence job is
    // so if it was non-LIGHT, this value is set to false which is wrong.
    //if (nullptr != schedJob)
    //    schedJob->setLightFramesRequired(false);

    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
        {
            exposure = atof(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            filter = QString(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            /* Record frame type and mark presence of light frames for this sequence */
            QString frameTypeStr = QString(pcdataXMLEle(ep));
            if (frameTypes.contains(frameTypeStr))
            {
                frameType = frameTypes[frameTypeStr];
            }
            //if (FRAME_LIGHT == frameType && nullptr != schedJob)
            //schedJob->setLightFramesRequired(true);
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
                m_RawPrefix = QString(pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterPrefixEnabled = !strcmp("1", pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expPrefixEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                timeStampPrefixEnabled = (!strcmp("1", pcdataXMLEle(subEP)));

        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            setCount(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            setDelay(atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            setLocalDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            setRemoteDir(pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            setUploadMode(static_cast<ISD::CCD::UploadMode>(atoi(pcdataXMLEle(ep))));
        }
        else if (!strcmp(tagXMLEle(ep), "Calibration"))
        {
            subEP = findXMLEle(ep, "FlatSource");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    if (!strcmp(pcdataXMLEle(typeEP), "Manual"))
                        setFlatFieldSource(SOURCE_MANUAL);
                    else if (!strcmp(pcdataXMLEle(typeEP), "FlatCap"))
                        setFlatFieldSource(SOURCE_FLATCAP);
                    else if (!strcmp(pcdataXMLEle(typeEP), "DarkCap"))
                        setFlatFieldSource(SOURCE_DARKCAP);
                    else if (!strcmp(pcdataXMLEle(typeEP), "Wall"))
                    {
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            setFlatFieldSource(SOURCE_WALL);
                            SkyPoint wallCoord;
                            wallCoord.setAz(dms::fromString(pcdataXMLEle(azEP), true));
                            wallCoord.setAlt(dms::fromString(pcdataXMLEle(altEP), true));
                            setWallCoord(wallCoord);
                        }
                    }
                    else
                        setFlatFieldSource(SOURCE_DAWN_DUSK);
                }
            }

            subEP = findXMLEle(ep, "FlatDuration");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    if (!strcmp(pcdataXMLEle(typeEP), "Manual"))
                        setFlatFieldDuration(DURATION_MANUAL);
                }

                XMLEle * aduEP = findXMLEle(subEP, "Value");
                if (aduEP)
                {
                    setFlatFieldDuration(DURATION_ADU);
                    setTargetADU(cLocale.toDouble(pcdataXMLEle(aduEP)));
                }

                aduEP = findXMLEle(subEP, "Tolerance");
                if (aduEP)
                {
                    setTargetADUTolerance(cLocale.toDouble(pcdataXMLEle(aduEP)));
                }
            }

            subEP = findXMLEle(ep, "PreMountPark");
            if (subEP)
            {
                setPreMountPark(!strcmp(pcdataXMLEle(subEP), "True"));
            }

            subEP = findXMLEle(ep, "PreDomePark");
            if (subEP)
            {
                setPreDomePark(!strcmp(pcdataXMLEle(subEP), "True"));
            }
        }
    }
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

    // Filter changes are actually done in capture();
    prepareActions[ACTION_FILTER] = true;
    if (targetFilter != -1 && activeFilter != nullptr &&
            frameType == FRAME_LIGHT && targetFilter != currentFilter)
        emit prepareState(CAPTURE_CHANGING_FILTER);

    // Check if we need to update temperature
    if (enforceTemperature && fabs(targetTemperature - currentTemperature) > Options::maxTemperatureDiff())
    {
        prepareActions[ACTION_TEMPERATURE] = false;
        emit prepareState(CAPTURE_SETTING_TEMPERATURE);
        activeCCD->setTemperature(targetTemperature);
    }

    // Check if we need to wait for the guider to settle.
    if (!guiderDriftOK())
    {
        prepareActions[ACTION_GUIDER_DRIFT] = false;
        emit prepareState(CAPTURE_GUIDER_DRIFT);
    }

    // Check if we need to update rotator
    if (targetRotation != Ekos::INVALID_VALUE
            && fabs(currentRotation - targetRotation) * 60 > Options::astrometryRotatorThreshold())
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

SequenceJob::CAPTUREResult SequenceJob::capture(bool autofocusReady)
{
    activeChip->setBatchMode(!preview);

    if (localDirectory.isEmpty() == false)
        activeCCD->setFITSDir(localDirectory + directoryPostfix);

    activeCCD->setISOMode(timeStampPrefixEnabled);

    activeCCD->setSeqPrefix(fullPrefix);

    activeCCD->setUploadMode(uploadMode);

    QMapIterator<QString, QMap<QString, double>> i(customProperties);
    while (i.hasNext())
    {
        i.next();
        INDI::Property *customProp = activeCCD->getProperty(i.key());
        if (customProp)
        {
            QMap<QString, double> numbers = i.value();
            QMapIterator<QString, double> j(numbers);
            auto np = customProp->getNumber();
            while (j.hasNext())
            {
                j.next();
                auto oneNumber = np->findWidgetByName(j.key().toLatin1().data());
                if (oneNumber)
                    oneNumber->setValue(j.value());
            }

            activeCCD->getDriverInfo()->getClientManager()->sendNewNumber(np);
        }
    }

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

    if (offset != -1)
    {
        activeCCD->setOffset(offset);
    }

    if (targetFilter != -1 && activeFilter != nullptr)
    {
        if (targetFilter != currentFilter)
        {
            emit prepareState(CAPTURE_CHANGING_FILTER);

            FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES;
            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (isPreview() || frameType != FRAME_LIGHT || autofocusReady == false)
                policy = static_cast<FilterManager::FilterPolicy>(policy & ~FilterManager::AUTOFOCUS_POLICY);

            filterManager->setFilterPosition(targetFilter, policy);
            return CAPTURE_FILTER_BUSY;
        }
    }

    if (!guiderDriftOK())
    {
        emit prepareState(CAPTURE_GUIDER_DRIFT);
        return CAPTURE_GUIDER_DRIFT_WAIT;
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
    activeChip->setCaptureFilter(FITS_NONE);

    // If filter is different that CCD, send the filter info
    //    if (activeFilter && activeFilter != activeCCD)
    //        activeCCD->setFilter(filter);

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
    m_RawPrefix            = rawFilePrefix;
    filterPrefixEnabled    = filterEnabled;
    expPrefixEnabled       = exposureEnabled;
    timeStampPrefixEnabled = tsEnabled;
}

void SequenceJob::getPrefixSettings(QString &rawFilePrefix, bool &filterEnabled, bool &exposureEnabled, bool &tsEnabled)
{
    rawFilePrefix   = m_RawPrefix;
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

void SequenceJob::setTargetStartGuiderDrift(double value)
{
    targetStartGuiderDrift = value;
}

double SequenceJob::getTargetStartGuiderDrift() const
{
    return targetStartGuiderDrift;
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

bool SequenceJob::getEnforceStartGuiderDrift() const
{
    return enforceStartGuiderDrift;
}

void SequenceJob::setEnforceStartGuiderDrift(bool value)
{
    enforceStartGuiderDrift = value;
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

double SequenceJob::getOffset() const
{
    return offset;
}

void SequenceJob::setOffset(double value)
{
    offset = value;
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

    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }
}

void SequenceJob::setCurrentRotation(double value)
{
    currentRotation = value;

    if (fabs(currentRotation - targetRotation) * 60 <= Options::astrometryRotatorThreshold())
        prepareActions[ACTION_ROTATOR] = true;

    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }

}

double SequenceJob::getCurrentGuiderDrift() const
{
    return currentGuiderDrift;
}

void SequenceJob::resetCurrentGuiderDrift()
{
    setCurrentGuiderDrift(1e8);
}

bool SequenceJob::guiderDriftOK() const
{
    return (!guiderActive ||
            !enforceStartGuiderDrift ||
            frameType != FRAME_LIGHT ||
            currentGuiderDrift <= targetStartGuiderDrift);
}

void SequenceJob::setCurrentGuiderDrift(double value)
{
    currentGuiderDrift = value;
    prepareActions[ACTION_GUIDER_DRIFT] = guiderDriftOK();

    if (prepareReady == false && areActionsReady())
    {
        prepareReady = true;
        emit prepareComplete();
    }
}

}
