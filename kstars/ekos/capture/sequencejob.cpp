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

const QStringList SequenceJob::StatusStrings = QStringList() << i18n("Idle") << i18n("In Progress") << i18n("Error") <<
        i18n("Aborted")
        << i18n("Complete");


SequenceJob::SequenceJob()
{
    // Set default property values
    m_CoreProperties[SJ_Exposure] = -1;
    m_CoreProperties[SJ_Gain] = -1;
    m_CoreProperties[SJ_Offset] = -1;
    m_CoreProperties[SJ_ISOIndex] = -1;
    m_CoreProperties[SJ_Count] = -1;
    m_CoreProperties[SJ_Delay] = -1;
    m_CoreProperties[SJ_Binning] = QPoint(1, 1);
    m_CoreProperties[SJ_ROI] = QRect(0, 0, 0, 0);
    m_CoreProperties[SJ_ExpPrefixEnabled] = false;
    m_CoreProperties[SJ_TimeStampPrefixEnabled] = false;
    m_CoreProperties[SJ_EnforceTemperature] = false;
    m_CoreProperties[SJ_FilterPrefixEnabled] = false;
    m_CoreProperties[SJ_DarkFlat] = false;
    m_CoreProperties[SJ_GuiderActive] = false;
}

SequenceJob::SequenceJob(const QSharedPointer<CaptureCommandProcessor> cp,
                         const QSharedPointer<SequenceJobState::CaptureState> sharedState) : SequenceJob()
{
    commandProcessor = cp;
    // initialize the state machine
    stateMachine = new SequenceJobState(sharedState);

    // signal forwarding between this and the state machine
    connect(this, &SequenceJob::updateCCDTemperature, stateMachine, &SequenceJobState::setCurrentCCDTemperature);
    connect(this, &SequenceJob::updateRotatorAngle, stateMachine, &SequenceJobState::setCurrentRotatorAngle);
    connect(this, &SequenceJob::setGuiderActive, stateMachine, &SequenceJobState::setGuiderActive);
    connect(this, &SequenceJob::updateGuiderDrift, stateMachine, &SequenceJobState::setCurrentGuiderDrift);
    connect(stateMachine, &SequenceJobState::readCurrentState, this, &SequenceJob::readCurrentState);
    connect(stateMachine, &SequenceJobState::prepareState, this, &SequenceJob::prepareState);
    connect(stateMachine, &SequenceJobState::prepareComplete, this, &SequenceJob::processPrepareComplete);
    connect(stateMachine, &SequenceJobState::abortCapture, this, &SequenceJob::processAbortCapture);
    connect(stateMachine, &SequenceJobState::newLog, this, &SequenceJob::newLog);
}

/**
 * @brief SequenceJob::SequenceJob Construct job from XML source
 * @param root pointer to valid job stored in XML format.
 */
SequenceJob::SequenceJob(XMLEle *root): SequenceJob()
{
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    // set own unconnected state machine
    QSharedPointer<SequenceJobState::CaptureState> sharedState;
    sharedState.reset(new SequenceJobState::CaptureState);
    stateMachine = new SequenceJobState(sharedState);

    // We expect all data read from the XML to be in the C locale - QLocale::c().
    QLocale cLocale = QLocale::c();

    const QMap<QString, CCDFrameType> frameTypes =
    {
        { "Light", FRAME_LIGHT }, { "Dark", FRAME_DARK }, { "Bias", FRAME_BIAS }, { "Flat", FRAME_FLAT }
    };

    setFrameType(FRAME_NONE);
    setCoreProperty(SJ_Exposure, 0);
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
            setCoreProperty(SJ_Exposure, atof(pcdataXMLEle(ep)));
        }
        if (!strcmp(tagXMLEle(ep), "Format"))
        {
            setCoreProperty(SJ_Format, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            setCoreProperty(SJ_Filter, QString(pcdataXMLEle(ep)));
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
                setCoreProperty(SJ_RawPrefix, QString(pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                setCoreProperty(SJ_FilterPrefixEnabled, !strcmp("1", pcdataXMLEle(subEP)));

            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                setCoreProperty(SJ_ExpPrefixEnabled, (!strcmp("1", pcdataXMLEle(subEP))));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                setCoreProperty(SJ_TimeStampPrefixEnabled, (!strcmp("1", pcdataXMLEle(subEP))));

        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            setCoreProperty(SJ_Count, atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            setCoreProperty(SJ_Delay, atoi(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            setCoreProperty(SJ_LocalDirectory, (pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            setCoreProperty(SJ_RemoteDirectory, (pcdataXMLEle(ep)));
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
                const char * dark = findXMLAttValu(subEP, "dark");
                setCoreProperty(SJ_DarkFlat, !strcmp(dark, "true"));

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
                    setCoreProperty(SJ_TargetADU, cLocale.toDouble(pcdataXMLEle(aduEP)));
                }

                aduEP = findXMLEle(subEP, "Tolerance");
                if (aduEP)
                {
                    setCoreProperty(SJ_TargetADUTolerance, cLocale.toDouble(pcdataXMLEle(aduEP)));
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

void SequenceJob::resetStatus(JOBStatus status)
{
    setStatus(status);
    setCalibrationStage(SequenceJobState::CAL_NONE);
    switch (status)
    {
        case JOB_IDLE:
            setCompleted(0);
            INDI_FALLTHROUGH;
        case JOB_ERROR:
        case JOB_ABORTED:
        case JOB_DONE:
            m_ExposeLeft     = 0;
            m_CaptureRetires = 0;
            m_JobProgressIgnored = false;
            break;
        case JOB_BUSY:
            // do nothing
            break;
    }
}

void SequenceJob::abort()
{
    setStatus(JOB_ABORTED);
    if (commandProcessor.get()->activeChip->canAbort())
        commandProcessor.get()->activeChip->abortExposure();
    commandProcessor.get()->activeChip->setBatchMode(false);
}

void SequenceJob::done()
{
    setStatus(JOB_DONE);
}

int SequenceJob::getJobRemainingTime(double estimatedDownloadTime)
{
    double remaining = (getCoreProperty(SJ_Exposure).toDouble() +
                        estimatedDownloadTime +
                        getCoreProperty(SJ_Delay).toDouble() / 1000) *
                       (getCoreProperty(SJ_Count).toDouble() - getCompleted());

    if (getStatus() == JOB_BUSY)
    {
        if (getExposeLeft() > 0.0)
            remaining -= getCoreProperty(SJ_Exposure).toDouble() - getExposeLeft();
        else
            remaining += getExposeLeft() + estimatedDownloadTime;
    }

    return static_cast<int>(std::round(remaining));
}

void SequenceJob::setStatus(JOBStatus const in_status)
{
    stateMachine->reset(in_status);
    if( !getCoreProperty(SequenceJob::SJ_Preview).toBool() && nullptr != statusCell)
        statusCell->setText(StatusStrings[in_status]);
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

void SequenceJob::setCompleted(int value)
{
    m_Completed = value;
    if( !getCoreProperty(SequenceJob::SJ_Preview).toBool() && nullptr != countCell)
        countCell->setText(QString("%L1/%L2").arg(value).arg(getCoreProperty(SJ_Count).toInt()));
}

int SequenceJob::getCompleted() const
{
    return m_Completed;
}

void SequenceJob::setCountCell(QTableWidgetItem* cell)
{
    countCell = cell;
    if (nullptr != cell)
    {
        cell->setTextAlignment(Qt::AlignHCenter);
        cell->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        setCoreProperty(SJ_Count, getCoreProperty(SJ_Count));
    }
}



QMap<QString, QMap<QString, QVariant> > SequenceJob::getCustomProperties() const
{
    return m_CustomProperties;
}

void SequenceJob::setCustomProperties(const QMap<QString, QMap<QString, QVariant> > &value)
{
    m_CustomProperties = value;
}

const QMap<ScriptTypes, QString> &SequenceJob::getScripts() const
{
    return m_Scripts;
}

void SequenceJob::setScripts(const QMap<ScriptTypes, QString> &scripts)
{
    m_Scripts = scripts;
}

QString SequenceJob::getScript(ScriptTypes type) const
{
    return m_Scripts[type];
}

void SequenceJob::setScript(ScriptTypes type, const QString &value)
{
    m_Scripts[type] = value;
}

void SequenceJob::connectCommandProcessor()
{
    connect(stateMachine, &SequenceJobState::setRotatorAngle, commandProcessor.get(),
            &CaptureCommandProcessor::setRotatorAngle);
    connect(stateMachine, &SequenceJobState::setCCDTemperature, commandProcessor.get(),
            &CaptureCommandProcessor::setCCDTemperature);
    connect(stateMachine, &SequenceJobState::setCCDBatchMode, commandProcessor.get(),
            &CaptureCommandProcessor::enableCCDBatchMode);
    connect(stateMachine, &SequenceJobState::askManualScopeLightCover, commandProcessor.get(),
            &CaptureCommandProcessor::askManualScopeLightCover);
    connect(stateMachine, &SequenceJobState::askManualScopeLightOpen, commandProcessor.get(),
            &CaptureCommandProcessor::askManualScopeLightOpen);
    connect(stateMachine, &SequenceJobState::setLightBoxLight, commandProcessor.get(),
            &CaptureCommandProcessor::setLightBoxLight);
    connect(stateMachine, &SequenceJobState::setDustCapLight, commandProcessor.get(),
            &CaptureCommandProcessor::setDustCapLight);
    connect(stateMachine, &SequenceJobState::parkDustCap, commandProcessor.get(), &CaptureCommandProcessor::parkDustCap);
    connect(stateMachine, &SequenceJobState::slewTelescope, commandProcessor.get(), &CaptureCommandProcessor::slewTelescope);
    connect(stateMachine, &SequenceJobState::setScopeTracking, commandProcessor.get(),
            &CaptureCommandProcessor::setScopeTracking);
    connect(stateMachine, &SequenceJobState::setScopeParked, commandProcessor.get(), &CaptureCommandProcessor::setScopeParked);
    connect(stateMachine, &SequenceJobState::setDomeParked, commandProcessor.get(), &CaptureCommandProcessor::setDomeParked);
    connect(stateMachine, &SequenceJobState::flatSyncFocus, commandProcessor.get(), &CaptureCommandProcessor::flatSyncFocus);
    connect(stateMachine, &SequenceJobState::queryHasShutter, commandProcessor.get(),
            &CaptureCommandProcessor::queryHasShutter);
    // connect command processor with state machine
    connect(commandProcessor.get(), &CaptureCommandProcessor::manualScopeLightCover, stateMachine,
            &SequenceJobState::manualScopeLightCover);
    connect(commandProcessor.get(), &CaptureCommandProcessor::lightBoxLight, stateMachine, &SequenceJobState::lightBoxLight);
    connect(commandProcessor.get(), &CaptureCommandProcessor::dustCapLight, stateMachine, &SequenceJobState::dustCapLight);
    connect(commandProcessor.get(), &CaptureCommandProcessor::dustCapStatusChanged, stateMachine,
            &SequenceJobState::dustCapStatusChanged);
    connect(commandProcessor.get(), &CaptureCommandProcessor::scopeStatusChanged, stateMachine,
            &SequenceJobState::scopeStatusChanged);
    connect(commandProcessor.get(), &CaptureCommandProcessor::scopeParkStatusChanged, stateMachine,
            &SequenceJobState::scopeParkStatusChanged);
    connect(commandProcessor.get(), &CaptureCommandProcessor::domeStatusChanged, stateMachine,
            &SequenceJobState::domeStatusChanged);
    connect(commandProcessor.get(), &CaptureCommandProcessor::flatSyncFocusChanged, stateMachine,
            &SequenceJobState::flatSyncFocusChanged);
    connect(commandProcessor.get(), &CaptureCommandProcessor::hasShutter, stateMachine, &SequenceJobState::hasShutter);
}

void SequenceJob::disconnectCommandProcessor()
{
    disconnect(stateMachine, nullptr, commandProcessor.get(), nullptr);
    disconnect(commandProcessor.get(), nullptr, stateMachine, nullptr);
}

CAPTUREResult SequenceJob::capture(bool autofocusReady, FITSMode mode)
{
    commandProcessor.get()->activeChip->setBatchMode(!getCoreProperty(SequenceJob::SJ_Preview).toBool());
    commandProcessor.get()->activeCCD->setISOMode(getCoreProperty(SJ_TimeStampPrefixEnabled).toBool());
    commandProcessor.get()->activeCCD->setSeqPrefix(getCoreProperty(SJ_FullPrefix).toString());

    auto placeholderPath = Ekos::PlaceholderPath(getCoreProperty(SJ_LocalDirectory).toString() + "/sequence.esq");
    placeholderPath.setGenerateFilenameSettings(*this);
    commandProcessor.get()->activeCCD->setPlaceholderPath(placeholderPath);

    if (getCoreProperty(SequenceJob::SJ_Preview).toBool())
    {
        if (commandProcessor.get()->activeCCD->getUploadMode() != ISD::CCD::UPLOAD_CLIENT)
            commandProcessor.get()->activeCCD->setUploadMode(ISD::CCD::UPLOAD_CLIENT);
    }
    else
        commandProcessor.get()->activeCCD->setUploadMode(m_UploadMode);

    QMapIterator<QString, QMap<QString, QVariant>> i(m_CustomProperties);
    while (i.hasNext())
    {
        i.next();
        INDI::Property *customProp = commandProcessor.get()->activeCCD->getProperty(i.key());
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
                    commandProcessor.get()->activeCCD->getDriverInfo()->getClientManager()->sendNewSwitch(sp);
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
                    commandProcessor.get()->activeCCD->getDriverInfo()->getClientManager()->sendNewText(tp);
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
                    commandProcessor.get()->activeCCD->getDriverInfo()->getClientManager()->sendNewNumber(np);
                }
                break;
                default:
                    continue;
            }
        }
    }

    const auto remoteDirectory = getCoreProperty(SJ_RemoteDirectory).toString();
    if (commandProcessor.get()->activeChip->isBatchMode() && remoteDirectory.isEmpty() == false)
        commandProcessor.get()->activeCCD->updateUploadSettings(remoteDirectory + getCoreProperty(SJ_DirectoryPostfix).toString());

    const int ISOIndex = getCoreProperty(SJ_ISOIndex).toInt();
    if (ISOIndex != -1)
    {
        if (ISOIndex != commandProcessor.get()->activeChip->getISOIndex())
            commandProcessor.get()->activeChip->setISOIndex(ISOIndex);
    }

    const auto gain = getCoreProperty(SJ_Gain).toDouble();
    if (gain != -1)
    {
        commandProcessor.get()->activeCCD->setGain(gain);
    }

    const auto offset = getCoreProperty(SJ_Offset).toDouble();
    if (offset != -1)
    {
        commandProcessor.get()->activeCCD->setOffset(offset);
    }

    if (stateMachine->targetFilterID != -1 && commandProcessor.get()->activeFilterWheel != nullptr)
    {
        if (stateMachine->targetFilterID != stateMachine->m_CaptureState->currentFilterID)
        {
            emit prepareState(CAPTURE_CHANGING_FILTER);

            FilterManager::FilterPolicy policy = FilterManager::ALL_POLICIES;
            // Don't perform autofocus on preview or calibration frames or if Autofocus is not ready yet.
            if (getCoreProperty(SequenceJob::SJ_Preview).toBool() || getFrameType() != FRAME_LIGHT || autofocusReady == false)
                policy = static_cast<FilterManager::FilterPolicy>(policy & ~FilterManager::AUTOFOCUS_POLICY);

            m_FilterManager->setFilterPosition(stateMachine->targetFilterID, policy);
            return CAPTURE_FILTER_BUSY;
        }
    }

    if (!stateMachine->checkGuiderDriftForStarting())
    {
        emit prepareState(CAPTURE_GUIDER_DRIFT);
        return CAPTURE_GUIDER_DRIFT_WAIT;
    }

    // Only attempt to set ROI and Binning if CCD transfer format is FITS
    if (commandProcessor.get()->activeCCD->getTransferFormat() == ISD::CCD::FORMAT_FITS)
    {
        int currentBinX = 1, currentBinY = 1;
        commandProcessor.get()->activeChip->getBinning(&currentBinX, &currentBinY);

        const auto binning = getCoreProperty(SJ_Binning).toPoint();
        // N.B. Always set binning _before_ setting frame because if the subframed image
        // is problematic in 1x1 but works fine for 2x2, then it would fail it was set first
        // So setting binning first always ensures this will work.
        if (commandProcessor.get()->activeChip->canBin()
                && commandProcessor.get()->activeChip->setBinning(binning.x(), binning.y()) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_BIN_ERROR;
        }

        const auto roi = getCoreProperty(SJ_ROI).toRect();

        if ((roi.width() > 0 && roi.height() > 0) && commandProcessor.get()->activeChip->canSubframe()
                && commandProcessor.get()->activeChip->setFrame(roi.x(),
                        roi.y(),
                        roi.width(),
                        roi.height(),
                        currentBinX != binning.x()) == false)
        {
            setStatus(JOB_ERROR);
            return CAPTURE_FRAME_ERROR;
        }
    }

    commandProcessor.get()->activeCCD->setCaptureFormat(getCoreProperty(SJ_Format).toString());
    commandProcessor.get()->activeChip->setFrameType(getFrameType());

    // In case FITS Viewer is not enabled. Then for flat frames, we still need to keep the data
    // otherwise INDI CCD would simply discard loading the data in batch mode as the data are already
    // saved to disk and since no extra processing is required, FITSData is not loaded up with the data.
    // But in case of automatically calculated flat frames, we need FITSData.
    // Therefore, we need to explicitly set mode to FITS_CALIBRATE so that FITSData is generated.
    commandProcessor.get()->activeChip->setCaptureMode(mode);
    commandProcessor.get()->activeChip->setCaptureFilter(FITS_NONE);

    setStatus(getStatus());

    const auto exposure = getCoreProperty(SJ_Exposure).toDouble();
    m_ExposeLeft = exposure;
    commandProcessor.get()->activeChip->capture(exposure);

    return CAPTURE_OK;
}

void SequenceJob::setTargetFilter(int pos, const QString &name)
{
    stateMachine->targetFilterID = pos;
    setCoreProperty(SJ_Filter, name);
}

double SequenceJob::getExposeLeft() const
{
    return m_ExposeLeft;
}

void SequenceJob::setExposeLeft(double value)
{
    m_ExposeLeft = value;
}


int SequenceJob::getCaptureRetires() const
{
    return m_CaptureRetires;
}

void SequenceJob::setCaptureRetires(int value)
{
    m_CaptureRetires = value;
}

int SequenceJob::getCurrentFilter() const
{
    return stateMachine->m_CaptureState->currentFilterID;
}

void SequenceJob::setCurrentFilter(int value)
{
    // forward it to the state machine
    stateMachine->setCurrentFilterID(value);
}

// Setter: Set upload mode
void SequenceJob::setUploadMode(ISD::CCD::UploadMode value)
{
    m_UploadMode = value;
}
// Getter: get upload mode
ISD::CCD::UploadMode SequenceJob::getUploadMode() const
{
    return m_UploadMode;
}

// Setter: Set flat field source
void SequenceJob::setFlatFieldSource(FlatFieldSource value)
{
    stateMachine->m_CaptureState->flatFieldSource = value;
}
// Getter: Get flat field source
FlatFieldSource SequenceJob::getFlatFieldSource() const
{
    return stateMachine->m_CaptureState->flatFieldSource;
}

void SequenceJob::setWallCoord(const SkyPoint &value)
{
    stateMachine->m_CaptureState->wallCoord = value;
}

const SkyPoint &SequenceJob::getWallCoord() const
{
    return stateMachine->m_CaptureState->wallCoord;
}

// Setter: Set flat field duration
void SequenceJob::setFlatFieldDuration(FlatFieldDuration value)
{
    m_FlatFieldDuration = value;
}

// Getter: Get flat field duration
FlatFieldDuration SequenceJob::getFlatFieldDuration() const
{
    return m_FlatFieldDuration;
}

// Setter: Set transform format
void SequenceJob::setTransferFormat(ISD::CCD::TransferFormat value)
{
    m_TransferFormat = value;
}

// Getter: Get transform format
ISD::CCD::TransferFormat SequenceJob::getTransferFormat() const
{
    return m_TransferFormat;
}

void SequenceJob::setJobProgressIgnored(bool value)
{
    m_JobProgressIgnored = value;
}

bool SequenceJob::getJobProgressIgnored() const
{
    return m_JobProgressIgnored;
}

void SequenceJob::setLightBox(ISD::LightBox *lightBox)
{
    // forward it to the command processor
    commandProcessor.get()->setLightBox(lightBox);
    stateMachine->m_CaptureState->hasLightBox = (lightBox != nullptr);
}

void SequenceJob::setDustCap(ISD::DustCap *dustCap)
{
    // forward it to the command processor
    commandProcessor.get()->setDustCap(dustCap);
    stateMachine->m_CaptureState->hasDustCap = (dustCap != nullptr);
}

void SequenceJob::setTelescope(ISD::Telescope *scope)
{
    // forward it to the command processor
    commandProcessor.get()->setTelescope(scope);
    stateMachine->m_CaptureState->hasTelescope = (scope != nullptr);
}

void SequenceJob::setDome(ISD::Dome *dome)
{
    // forward it to the command processor
    commandProcessor.get()->setDome(dome);
    stateMachine->m_CaptureState->hasDome = (dome != nullptr);
}

void SequenceJob::setFilterManager(const QSharedPointer<FilterManager> &manager)
{
    m_FilterManager = manager;
    commandProcessor.get()->setFilterManager(manager);
}

void SequenceJob::setFrameType(CCDFrameType value)
{
    m_FrameType = value;
    // propagate the frame type to the state machine
    stateMachine->setFrameType(value);
}

// Getter: Get Frame Type
CCDFrameType SequenceJob::getFrameType() const
{
    return m_FrameType;
}

QString SequenceJob::getSignature()
{
    auto localDirectory = getCoreProperty(SJ_LocalDirectory).toString();
    auto directoryPostfix = getCoreProperty(SJ_DirectoryPostfix).toString();
    auto fullPrefix = getCoreProperty(SJ_FullPrefix).toString();

    return QString(localDirectory + directoryPostfix + QDir::separator() + fullPrefix).remove(ISOMarker);

}

void SequenceJob::prepareCapture()
{
    // create the event connections
    connectCommandProcessor();
    // simply forward it to the state machine
    switch (getFrameType())
    {
        case FRAME_LIGHT:
            stateMachine->prepareLightFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
                                                   getCoreProperty(SJ_EnforceStartGuiderDrift).toBool(),
                                                   getCoreProperty(SequenceJob::SJ_Preview).toBool());
            break;
        case FRAME_FLAT:
            stateMachine->prepareFlatFrameCapture();
            break;
        case FRAME_DARK:
            stateMachine->prepareDarkFrameCapture();
            break;
        default:
            // not refactored yet, immediately completed
            processPrepareComplete();
            break;
    }
}

void SequenceJob::processPrepareComplete()
{
    disconnectCommandProcessor();
    emit prepareComplete();
}

void SequenceJob::processAbortCapture()
{
    disconnectCommandProcessor();
    emit abortCapture();
}

IPState SequenceJob::checkFlatFramePendingTasksCompleted()
{
    // no further checks necessary
    return IPS_OK;
}

void SequenceJob::setCoreProperty(PropertyID id, const QVariant &value)
{
    m_CoreProperties[id] = value;

    // Handle special cases
    switch (id)
    {
        case SJ_Count:
            if( !getCoreProperty(SequenceJob::SJ_Preview).toBool() && nullptr != countCell)
                countCell->setText(QString("%L1/%L2").arg(m_Completed).arg(value.toInt()));
            break;

        case SJ_RemoteDirectory:
        {
            auto remoteDir = value.toString();
            if (remoteDir.endsWith('/'))
            {
                remoteDir.chop(1);
                m_CoreProperties[id] = remoteDir;
            }
        }
        break;

        default:
            break;
    }
}

QVariant SequenceJob::getCoreProperty(PropertyID id) const
{
    return m_CoreProperties[id];
}
}
