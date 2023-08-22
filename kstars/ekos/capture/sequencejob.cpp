/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejob.h"

#include <KNotifications/KNotification>
#include <ekos_capture_debug.h>

#define MF_TIMER_TIMEOUT    90000
#define MF_RA_DIFF_LIMIT    4

namespace Ekos
{
QString const &SequenceJob::ISOMarker("_ISO8601");

const QStringList SequenceJob::StatusStrings()
{
    static const QStringList names = {i18n("Idle"), i18n("In Progress"), i18n("Error"), i18n("Aborted"),
                                      i18n("Complete")
                                     };
    return names;
}


void SequenceJob::init(SequenceJobType jobType)
{
    setJobType(jobType);
    // Set default property values
    m_CoreProperties[SJ_Exposure] = -1;
    m_CoreProperties[SJ_Gain] = -1;
    m_CoreProperties[SJ_Offset] = -1;
    m_CoreProperties[SJ_ISOIndex] = -1;
    m_CoreProperties[SJ_Count] = -1;
    m_CoreProperties[SJ_Delay] = -1;
    m_CoreProperties[SJ_Binning] = QPoint(1, 1);
    m_CoreProperties[SJ_ROI] = QRect(0, 0, 0, 0);
    m_CoreProperties[SJ_EnforceTemperature] = false;
    m_CoreProperties[SJ_GuiderActive] = false;
    m_CoreProperties[SJ_Encoding] = "FITS";
}

SequenceJob::SequenceJob(const QSharedPointer<CaptureDeviceAdaptor> cp,
                         const QSharedPointer<CaptureModuleState> sharedState, SequenceJobType jobType)
{
    init(jobType);
    devices = cp;
    // initialize the state machine
    state.reset(new SequenceJobState(sharedState));

    // signal forwarding between this and the state machine
    connect(this, &SequenceJob::updateGuiderDrift, state.data(), &SequenceJobState::setCurrentGuiderDrift);
    connect(state.data(), &SequenceJobState::prepareState, this, &SequenceJob::prepareState);
    connect(state.data(), &SequenceJobState::prepareComplete, this, &SequenceJob::processPrepareComplete);
    connect(state.data(), &SequenceJobState::abortCapture, this, &SequenceJob::processAbortCapture);
    connect(state.data(), &SequenceJobState::newLog, this, &SequenceJob::newLog);
    // start capturing as soon as the capture initialization is complete
    connect(state.data(), &SequenceJobState::initCaptureComplete, this, &SequenceJob::capture);
}

/**
 * @brief SequenceJob::SequenceJob Construct job from XML source
 * @param root pointer to valid job stored in XML format.
 */
SequenceJob::SequenceJob(XMLEle *root)
{
    init(SequenceJob::JOBTYPE_BATCH);
    XMLEle *ep    = nullptr;
    XMLEle *subEP = nullptr;

    // set own unconnected state machine
    QSharedPointer<CaptureModuleState> sharedState;
    sharedState.reset(new CaptureModuleState);
    state.reset(new SequenceJobState(sharedState));

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
        else if (!strcmp(tagXMLEle(ep), "Format"))
        {
            setCoreProperty(SJ_Format, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderFormat"))
        {
            setCoreProperty(SJ_PlaceholderFormat, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderSuffix"))
        {
            setCoreProperty(SJ_PlaceholderSuffix, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "Encoding"))
        {
            setCoreProperty(SJ_Encoding, pcdataXMLEle(ep));
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
            bool filterEnabled = false, expEnabled = false, tsEnabled = false;
            subEP = findXMLEle(ep, "FilterEnabled");
            if (subEP)
                filterEnabled = !strcmp("1", pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "ExpEnabled");
            if (subEP)
                expEnabled = !strcmp("1", pcdataXMLEle(subEP));

            subEP = findXMLEle(ep, "TimeStampEnabled");
            if (subEP)
                tsEnabled = !strcmp("1", pcdataXMLEle(subEP));

            // build default format
            setCoreProperty(SJ_PlaceholderFormat, PlaceholderPath::defaultFormat(filterEnabled, expEnabled, tsEnabled));
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
            setUploadMode(static_cast<ISD::Camera::UploadMode>(atoi(pcdataXMLEle(ep))));
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
                if (!strcmp(dark, "true"))
                    setJobType(JOBTYPE_DARKFLAT);

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
            // 2022.03.10: Keeps failing on Windows despite installing latest libindi
#ifndef Q_OS_WIN
            INDI_FALLTHROUGH;
#endif
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
    if (devices.data()->getActiveChip())
    {
        if (devices.data()->getActiveChip()->canAbort())
            devices.data()->getActiveChip()->abortExposure();
        devices.data()->getActiveChip()->setBatchMode(false);
    }
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
    state->reset(in_status);
}

void SequenceJob::connectDeviceAdaptor()
{
    devices->setCurrentSequenceJobState(state);
    // connect state machine with device adaptor
    connect(state.data(), &SequenceJobState::readCurrentState, devices.data(),
            &CaptureDeviceAdaptor::readCurrentState);
    connect(state.data(), &SequenceJobState::flatSyncFocus, devices.data(),
            &CaptureDeviceAdaptor::flatSyncFocus);
    // connect device adaptor with state machine
    connect(devices.data(), &CaptureDeviceAdaptor::flatSyncFocusChanged, state.data(),
            &SequenceJobState::flatSyncFocusChanged);
}

void SequenceJob::disconnectDeviceAdaptor()
{
    devices->disconnectDevices(state.data());
    disconnect(state.data(), &SequenceJobState::readCurrentState, devices.data(),
               &CaptureDeviceAdaptor::readCurrentState);
    disconnect(state.data(), &SequenceJobState::flatSyncFocus, devices.data(),
               &CaptureDeviceAdaptor::flatSyncFocus);
    disconnect(devices.data(), &CaptureDeviceAdaptor::flatSyncFocusChanged, state.data(),
               &SequenceJobState::flatSyncFocusChanged);
}

void SequenceJob::startCapturing(bool autofocusReady, FITSMode mode)
{
    state->initCapture(getFrameType(), jobType() == SequenceJob::JOBTYPE_PREVIEW, autofocusReady, mode);
}

void SequenceJob::capture(FITSMode mode)
{
    if (!devices.data()->getActiveCamera() || !devices.data()->getActiveChip())
        return;

    devices.data()->getActiveChip()->setBatchMode(jobType() != SequenceJob::JOBTYPE_PREVIEW);
    devices.data()->getActiveCamera()->setSeqPrefix(getCoreProperty(SJ_FullPrefix).toString());

    if (jobType() == SequenceJob::JOBTYPE_PREVIEW)
    {
        if (devices.data()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            devices.data()->getActiveCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }
    else
        devices.data()->getActiveCamera()->setUploadMode(m_UploadMode);

    QMapIterator<QString, QMap<QString, QVariant>> i(m_CustomProperties);
    while (i.hasNext())
    {
        i.next();
        auto customProp = devices.data()->getActiveCamera()->getProperty(i.key());
        if (customProp)
        {
            QMap<QString, QVariant> elements = i.value();
            QMapIterator<QString, QVariant> j(elements);

            switch (customProp.getType())
            {
                case INDI_SWITCH:
                {
                    auto sp = customProp.getSwitch();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneSwitch = sp->findWidgetByName(j.key().toLatin1().data());
                        if (oneSwitch)
                            oneSwitch->setState(static_cast<ISState>(j.value().toInt()));
                    }
                    devices.data()->getActiveCamera()->sendNewProperty(sp);
                }
                break;
                case INDI_TEXT:
                {
                    auto tp = customProp.getText();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneText = tp->findWidgetByName(j.key().toLatin1().data());
                        if (oneText)
                            oneText->setText(j.value().toString().toLatin1().constData());
                    }
                    devices.data()->getActiveCamera()->sendNewProperty(tp);
                }
                break;
                case INDI_NUMBER:
                {
                    auto np = customProp.getNumber();
                    while (j.hasNext())
                    {
                        j.next();
                        auto oneNumber = np->findWidgetByName(j.key().toLatin1().data());
                        if (oneNumber)
                            oneNumber->setValue(j.value().toDouble());
                    }
                    devices.data()->getActiveCamera()->sendNewProperty(np);
                }
                break;
                default:
                    continue;
            }
        }
    }

    const auto remoteDirectory = getCoreProperty(SJ_RemoteDirectory).toString();
    if (devices.data()->getActiveChip()->isBatchMode() && remoteDirectory.isEmpty() == false)
    {
        devices.data()->getActiveCamera()->updateUploadSettings(remoteDirectory + getCoreProperty(
                    SJ_DirectoryPostfix).toString());
    }

    const int ISOIndex = getCoreProperty(SJ_ISOIndex).toInt();
    if (ISOIndex != -1)
    {
        if (ISOIndex != devices.data()->getActiveChip()->getISOIndex())
            devices.data()->getActiveChip()->setISOIndex(ISOIndex);
    }

    const auto gain = getCoreProperty(SJ_Gain).toDouble();
    if (gain >= 0)
    {
        devices.data()->getActiveCamera()->setGain(gain);
    }

    const auto offset = getCoreProperty(SJ_Offset).toDouble();
    if (offset >= 0)
    {
        devices.data()->getActiveCamera()->setOffset(offset);
    }

    // Only attempt to set ROI and Binning if CCD transfer format is FITS or XISF
    if (devices.data()->getActiveCamera()->getEncodingFormat() == QLatin1String("FITS")
            || devices.data()->getActiveCamera()->getEncodingFormat() == QLatin1String("XISF"))
    {
        int currentBinX = 1, currentBinY = 1;
        devices.data()->getActiveChip()->getBinning(&currentBinX, &currentBinY);

        const auto binning = getCoreProperty(SJ_Binning).toPoint();
        // N.B. Always set binning _before_ setting frame because if the subframed image
        // is problematic in 1x1 but works fine for 2x2, then it would fail it was set first
        // So setting binning first always ensures this will work.
        if (devices.data()->getActiveChip()->canBin()
                && devices.data()->getActiveChip()->setBinning(binning.x(), binning.y()) == false)
        {
            setStatus(JOB_ERROR);
            emit captureStarted(CaptureModuleState::CAPTURE_BIN_ERROR);
        }

        const auto roi = getCoreProperty(SJ_ROI).toRect();

        if ((roi.width() > 0 && roi.height() > 0) && devices.data()->getActiveChip()->canSubframe()
                && devices.data()->getActiveChip()->setFrame(roi.x(),
                        roi.y(),
                        roi.width(),
                        roi.height(),
                        currentBinX != binning.x()) == false)
        {
            setStatus(JOB_ERROR);
            emit captureStarted(CaptureModuleState::CAPTURE_FRAME_ERROR);
        }
    }

    devices.data()->getActiveCamera()->setCaptureFormat(getCoreProperty(SJ_Format).toString());
    devices.data()->getActiveCamera()->setEncodingFormat(getCoreProperty(SJ_Encoding).toString());
    devices.data()->getActiveChip()->setFrameType(getFrameType());

    // In case FITS Viewer is not enabled. Then for flat frames, we still need to keep the data
    // otherwise INDI CCD would simply discard loading the data in batch mode as the data are already
    // saved to disk and since no extra processing is required, FITSData is not loaded up with the data.
    // But in case of automatically calculated flat frames, we need FITSData.
    // Therefore, we need to explicitly set mode to FITS_CALIBRATE so that FITSData is generated.
    devices.data()->getActiveChip()->setCaptureMode(mode);
    devices.data()->getActiveChip()->setCaptureFilter(FITS_NONE);

    setStatus(getStatus());

    const auto exposure = getCoreProperty(SJ_Exposure).toDouble();
    m_ExposeLeft = exposure;
    devices.data()->getActiveChip()->capture(exposure);

    emit captureStarted(CaptureModuleState::CAPTURE_OK);
}

void SequenceJob::setTargetFilter(int pos, const QString &name)
{
    state->targetFilterID = pos;
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
    return state->m_CaptureModuleState->currentFilterID;
}

ISD::Mount::PierSide SequenceJob::getPierSide() const
{
    return state->m_CaptureModuleState->getPierSide();
}

// Setter: Set upload mode
void SequenceJob::setUploadMode(ISD::Camera::UploadMode value)
{
    m_UploadMode = value;
}
// Getter: get upload mode
ISD::Camera::UploadMode SequenceJob::getUploadMode() const
{
    return m_UploadMode;
}

// Setter: Set flat field source
void SequenceJob::setFlatFieldSource(FlatFieldSource value)
{
    state->flatFieldSource = value;
}
// Getter: Get flat field source
FlatFieldSource SequenceJob::getFlatFieldSource() const
{
    return state->flatFieldSource;
}

void SequenceJob::setWallCoord(const SkyPoint &value)
{
    state->wallCoord = value;
}

const SkyPoint &SequenceJob::getWallCoord() const
{
    return state->wallCoord;
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

void SequenceJob::setJobProgressIgnored(bool value)
{
    m_JobProgressIgnored = value;
}

bool SequenceJob::getJobProgressIgnored() const
{
    return m_JobProgressIgnored;
}

void SequenceJob::updateDeviceStates()
{
    setLightBox(devices->lightBox());
    addMount(devices->mount());
    setDome(devices->dome());
    setDustCap(devices->dustCap());
}

void SequenceJob::setLightBox(ISD::LightBox *lightBox)
{
    state->m_CaptureModuleState->hasLightBox = (lightBox != nullptr);
}

void SequenceJob::setDustCap(ISD::DustCap *dustCap)
{
    state->m_CaptureModuleState->hasDustCap = (dustCap != nullptr);
}

void SequenceJob::addMount(ISD::Mount *scope)
{
    state->m_CaptureModuleState->hasTelescope = (scope != nullptr);
}

void SequenceJob::setDome(ISD::Dome *dome)
{
    state->m_CaptureModuleState->hasDome = (dome != nullptr);
}

void SequenceJob::prepareCapture()
{
    // simply forward it to the state machine
    switch (getFrameType())
    {
        case FRAME_LIGHT:
            state->prepareLightFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
                                            getCoreProperty(SJ_EnforceStartGuiderDrift).toBool() && getCoreProperty(SJ_GuiderActive).toBool(),
                                            jobType() == SequenceJob::JOBTYPE_PREVIEW);
            break;
        case FRAME_FLAT:
            state->prepareFlatFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
                                           jobType() == SequenceJob::JOBTYPE_PREVIEW);
            break;
        case FRAME_DARK:
            state->prepareDarkFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
                                           jobType() == SequenceJob::JOBTYPE_PREVIEW);
            break;
        case FRAME_BIAS:
            state->prepareBiasFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
                                           jobType() == SequenceJob::JOBTYPE_PREVIEW);
            break;
        default:
            // not refactored yet, immediately completed
            processPrepareComplete();
            break;
    }
}

void SequenceJob::processPrepareComplete(bool success)
{
    emit prepareComplete(success);
}

void SequenceJob::processAbortCapture()
{
    disconnectDeviceAdaptor();
    emit abortCapture();
}

IPState SequenceJob::checkFlatFramePendingTasksCompleted()
{
    // no further checks necessary
    return IPS_OK;
}

void SequenceJob::setCoreProperty(PropertyID id, const QVariant &value)
{
    // Handle special cases
    switch (id)
    {
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

        case SJ_GuiderActive:
            // Inform the state machine if guiding is running. This is necessary during the preparation phase
            // where the state machine might wait for guide deviations if enforcing initial guiding drift is selected.
            // If guiding aborts after the preparation has started, the state machine might wait infinitely for an
            // updated guide drift.
            if (m_CoreProperties[SJ_GuiderActive] != value)
            {
                state->setEnforceInitialGuidingDrift(value.toBool() &&
                                                     m_CoreProperties[SJ_EnforceStartGuiderDrift].toBool());
            }
            break;
        default:
            break;
    }
    // store value
    m_CoreProperties[id] = value;
}

QVariant SequenceJob::getCoreProperty(PropertyID id) const
{
    return m_CoreProperties[id];
}
}
