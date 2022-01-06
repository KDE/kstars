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

    // signal forwarding between this and the state machine
    connect(this, &SequenceJob::prepareCapture, &stateMachine, &SequenceJobState::prepareCapture);
    connect(this, &SequenceJob::updateCCDTemperature, &stateMachine, &SequenceJobState::setCurrentCCDTemperature);
    connect(this, &SequenceJob::updateRotatorAngle, &stateMachine, &SequenceJobState::setCurrentRotatorAngle);
    connect(this, &SequenceJob::updateGuiderDrift, &stateMachine, &SequenceJobState::setCurrentGuiderDrift);
    connect(&stateMachine, &SequenceJobState::readCurrentState, this, &SequenceJob::readCurrentState);
    connect(&stateMachine, &SequenceJobState::prepareState, this, &SequenceJob::prepareState);
    connect(&stateMachine, &SequenceJobState::prepareComplete, this, &SequenceJob::prepareComplete);
    // connect state machine with command processor
    connect(&stateMachine, &SequenceJobState::setRotatorAngle, &commandProcessor, &CaptureCommandProcessor::setRotatorAngle);
    connect(&stateMachine, &SequenceJobState::setCCDTemperature, &commandProcessor, &CaptureCommandProcessor::setCCDTemperature);
    connect(&stateMachine, &SequenceJobState::setCCDBatchMode, &commandProcessor, &CaptureCommandProcessor::enableCCDBatchMode);
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

    setFrameType(FRAME_NONE);
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
                setFrameType(frameTypes[frameTypeStr]);
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
    if (commandProcessor.activeChip->canAbort())
        commandProcessor.activeChip->abortExposure();
    commandProcessor.activeChip->setBatchMode(false);
}

void SequenceJob::done()
{
    setStatus(JOB_DONE);
}

int SequenceJob::getJobRemainingTime(double estimatedDownloadTime)
{
    double remaining = (getExposure() + estimatedDownloadTime + getDelay() / 1000) *
                       (getCount() - getCompleted());

    if (getStatus() == JOB_BUSY)
    {
        if (getExposeLeft() > 0.0)
            remaining -= getExposure() - getExposeLeft();
        else
            remaining += getExposeLeft() + estimatedDownloadTime;
    }

    return static_cast<int>(std::round(remaining));
}

void SequenceJob::setStatus(JOBStatus const in_status)
{
    stateMachine.reset(in_status);
    if( !isPreview() && nullptr != statusCell)
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
    if( !isPreview() && nullptr != countCell)
        countCell->setText(QString("%L1/%L2").arg(completed).arg(in_count));
}

void SequenceJob::setCompleted(int in_completed)
{
    completed = in_completed;
    if( !isPreview() && nullptr != countCell)
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

QMap<QString, QMap<QString, QVariant> > SequenceJob::getCustomProperties() const
{
    return customProperties;
}

void SequenceJob::setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value)
{
    customProperties = value;
}

CAPTUREResult SequenceJob::capture(bool autofocusReady, FITSMode mode)
{
    commandProcessor.activeChip->setBatchMode(!isPreview());

    commandProcessor.activeCCD->setISOMode(timeStampPrefixEnabled);

    commandProcessor.activeCCD->setSeqPrefix(fullPrefix);

    auto placeholderPath = Ekos::PlaceholderPath(localDirectory + "/sequence.esq");
    placeholderPath.setGenerateFilenameSettings(*this);
    commandProcessor.activeCCD->setPlaceholderPath(placeholderPath);

    if (isPreview())
    {
        if (commandProcessor.activeCCD->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            commandProcessor.activeCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }
    else
        commandProcessor.activeCCD->setUploadMode(uploadMode);

    QMapIterator<QString, QMap<QString, QVariant>> i(customProperties);
    while (i.hasNext())
    {
        i.next();
        INDI::Property *customProp = commandProcessor.activeCCD->getProperty(i.key());
        if (customProp)
        {
            QMap<QString, QVariant> elements = i.value();
            QMapIterator<QString, QVariant> j(elements);

            switch (customProp->getType())
            {
                case INDI_SWITCH:
                {
                    auto sp = customProp->getSwitch();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneSwitch = sp->findWidgetByName(j.key().toLatin1().data());
                        if (oneSwitch)
                            oneSwitch->setState(static_cast<ISState>(j.value().toInt()));
                    }
                    commandProcessor.activeCCD->getDriverInfo()->getClientManager()->sendNewSwitch(sp);
                }
                break;
                case INDI_TEXT:
                {
                    auto tp = customProp->getText();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneText = tp->findWidgetByName(j.key().toLatin1().data());
                        if (oneText)
                            oneText->setText(j.value().toString().toLatin1().constData());
                    }
                    commandProcessor.activeCCD->getDriverInfo()->getClientManager()->sendNewText(tp);
                }
                break;
                case INDI_NUMBER:
                {
                    auto np = customProp->getNumber();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneNumber = np->findWidgetByName(j.key().toLatin1().data());
                        if (oneNumber)
                            oneNumber->setValue(j.value().toDouble());
                    }
                    commandProcessor.activeCCD->getDriverInfo()->getClientManager()->sendNewNumber(np);
                }
                break;
                default:
                    continue;
            }
        }
    }

    if (commandProcessor.activeChip->isBatchMode() && remoteDirectory.isEmpty() == false)
        commandProcessor.activeCCD->updateUploadSettings(remoteDirectory + directoryPostfix);

    if (isoIndex != -1)
    {
        if (isoIndex != commandProcessor.activeChip->getISOIndex())
            commandProcessor.activeChip->setISOIndex(isoIndex);
    }

    if (gain != -1)
    {
        commandProcessor.activeCCD->setGain(gain);
    }

    if (offset != -1)
    {
        commandProcessor.activeCCD->setOffset(offset);
    }

    if (stateMachine.targetFilterID != -1 && commandProcessor.activeFilterWheel != nullptr)
    {
        if (stateMachine.targetFilterID != stateMachine.currentFilterID)
        {
            emit prepareState(CAPTURE_CHANGING_FILTER);

            FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES;
            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (isPreview() || getFrameType() != FRAME_LIGHT || autofocusReady == false)
                policy = static_cast<FilterManager::FilterPolicy>(policy & ~FilterManager::AUTOFOCUS_POLICY);

            filterManager->setFilterPosition(stateMachine.targetFilterID, policy);
            return CAPTURE_FILTER_BUSY;
        }
    }

    if (!stateMachine.guiderDriftOkForStarting())
    {
        emit prepareState(CAPTURE_GUIDER_DRIFT);
        return CAPTURE_GUIDER_DRIFT_WAIT;
    }

    // Only attempt to set ROI and Binning if CCD transfer format is FITS
    if (commandProcessor.activeCCD->getTransferFormat() == ISD::CCD::FORMAT_FITS)
    {
        int currentBinX = 1, currentBinY = 1;
        commandProcessor.activeChip->getBinning(&currentBinX, &currentBinY);

        // N.B. Always set binning _before_ setting frame because if the subframed image
        // is problematic in 1x1 but works fine for 2x2, then it would fail it was set first
        // So setting binning first always ensures this will work.
        if (commandProcessor.activeChip->canBin() && commandProcessor.activeChip->setBinning(binX, binY) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_BIN_ERROR;
        }

        if ((w > 0 && h > 0) && commandProcessor.activeChip->canSubframe() && commandProcessor.activeChip->setFrame(x, y, w, h, currentBinX != binX) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_FRAME_ERROR;
        }
    }

    commandProcessor.activeChip->setFrameType(getFrameType());


    // In case FITS Viewer is not enabled. Then for flat frames, we still need to keep the data
    // otherwise INDI CCD would simply discard loading the data in batch mode as the data are already
    // saved to disk and since no extra processing is required, FITSData is not loaded up with the data.
    // But in case of automatically calculated flat frames, we need FITSData.
    // Therefore, we need to explicitly set mode to FITS_CALIBRATE so that FITSData is generated.
    commandProcessor.activeChip->setCaptureMode(mode);

    commandProcessor.activeChip->setCaptureFilter(FITS_NONE);

    //status = JOB_BUSY;
    setStatus(getStatus());

    exposeLeft = exposure;

    commandProcessor.activeChip->capture(exposure);

    return CAPTURE_OK;
}

void SequenceJob::setTargetFilter(int pos, const QString &name)
{
    stateMachine.targetFilterID = pos;
    filter       = name;
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

void SequenceJob::setCurrentTemperature(double value)
{
    // forward it to the state machine
    stateMachine.setCurrentCCDTemperature(value);
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
    return stateMachine.currentFilterID;
}

void SequenceJob::setCurrentFilter(int value)
{
    // forward it to the state machine
    stateMachine.setCurrentFilterID(value);
}

void SequenceJob::resetCurrentGuiderDrift()
{
    stateMachine.setCurrentGuiderDrift(1e8);
}

}
