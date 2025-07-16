/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejob.h"

#include <knotification.h>
#include <ekos_capture_debug.h>
#include "capturedeviceadaptor.h"
#include "skyobjects/skypoint.h"
#include "ksnotification.h"

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


/**
 * @brief SequenceJob::SequenceJob Construct job from XML source
 * @param root pointer to valid job stored in XML format.
 */
SequenceJob::SequenceJob(XMLEle * root, QString targetName)
{
    // set own unconnected state machine
    QSharedPointer<CameraState> sharedState;
    sharedState.reset(new CameraState);
    state.reset(new SequenceJobState(sharedState));
    // set simple device adaptor
    devices.reset(new CaptureDeviceAdaptor());

    init(SequenceJob::JOBTYPE_BATCH, root, sharedState, targetName);
}

SequenceJob::SequenceJob(const QSharedPointer<CaptureDeviceAdaptor> cp,
                         const QSharedPointer<CameraState> sharedState,
                         SequenceJobType jobType, XMLEle *root, QString targetName)
{
    devices = cp;
    init(jobType, root, sharedState, targetName);
}

void Ekos::SequenceJob::init(SequenceJobType jobType, XMLEle *root,
                             QSharedPointer<Ekos::CameraState> sharedState,
                             const QString &targetName)
{
    // initialize the state machine
    state.reset(new SequenceJobState(sharedState));

    loadFrom(root, targetName, jobType);

    // signal forwarding between this and the state machine
    connect(state.data(), &SequenceJobState::prepareState, this, &SequenceJob::prepareState);
    connect(state.data(), &SequenceJobState::prepareComplete, this, &SequenceJob::processPrepareComplete);
    connect(state.data(), &SequenceJobState::abortCapture, this, &SequenceJob::processAbortCapture);
    connect(state.data(), &SequenceJobState::newLog, this, &SequenceJob::newLog);
    // start capturing as soon as the capture initialization is complete
    connect(state.data(), &SequenceJobState::initCaptureComplete, this, &SequenceJob::capture);

    // finish if XML document empty
    if (root == nullptr)
        return;

    // create signature with current target
    auto placeholderPath = Ekos::PlaceholderPath();
    placeholderPath.processJobInfo(this);
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

void SequenceJob::setISO(int index)
{
    if (devices->getActiveChip())
    {
        setCoreProperty(SequenceJob::SJ_ISOIndex, index);
        const auto isolist = devices->getActiveChip()->getISOList();
        if (isolist.count() > index && index >= 0)
            setCoreProperty(SequenceJob::SJ_ISO, isolist[index]);
    }
}

const QVariant SequenceJob::getRemoteDirectory() const
{
    if (getCoreProperty(SJ_RemoteDirectory).toString().isEmpty())
        return getCoreProperty(SJ_LocalDirectory);
    else
        return getCoreProperty(SJ_RemoteDirectory);
}

QStringList SequenceJob::frameTypes() const
{
    // Define the standard frame types in the correct order
    QStringList standardTypes = {"Light", "Dark", "Bias", "Flat", "Video"};

    if (!devices->getActiveCamera())
        return standardTypes;

    ISD::CameraChip *tChip = devices->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);
    QStringList types = tChip->getFrameTypes();
    if (devices->getActiveCamera()->hasVideoStream())
        types.append(CAPTURE_TYPE_VIDEO);

    // If camera doesn't provide frame types or returns empty list, use standard types
    if (types.isEmpty())
        return standardTypes;

    // Ensure all standard types are available, adding any missing ones
    for (const QString &standardType : standardTypes)
    {
        if (!types.contains(standardType))
            types.append(standardType);
    }

    return types;
}

QStringList SequenceJob::filterLabels() const
{
    if (devices->getFilterManager().isNull())
        return QStringList();

    return devices->getFilterManager()->getFilterLabels();

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

QString SequenceJob::setCameraDeviceProperties()
{
    // initialize the log entry
    QString logentry = QString("Capture exposure = %1 sec, type = %2").arg(getCoreProperty(SJ_Exposure).toDouble()).arg(
                           CCDFrameTypeNames[getFrameType()]);
    logentry.append(QString(", filter = %1, upload mode = %2").arg(getCoreProperty(SJ_Filter).toString()).arg(getUploadMode()));


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


    const int ISOIndex = getCoreProperty(SJ_ISOIndex).toInt();
    if (ISOIndex != -1)
    {
        logentry.append(QString(", ISO index = %1").arg(ISOIndex));
        if (ISOIndex != devices.data()->getActiveChip()->getISOIndex())
            devices.data()->getActiveChip()->setISOIndex(ISOIndex);
    }

    const auto gain = getCoreProperty(SJ_Gain).toDouble();
    if (gain >= 0)
    {
        logentry.append(QString(", gain = %1").arg(gain));
        devices.data()->getActiveCamera()->setGain(gain);
    }

    const auto offset = getCoreProperty(SJ_Offset).toDouble();
    if (offset >= 0)
    {
        logentry.append(QString(", offset = %1").arg(offset));
        devices.data()->getActiveCamera()->setOffset(offset);
    }

    const auto remoteFormatDirectory = getCoreProperty(SJ_RemoteFormatDirectory).toString();
    const auto remoteFormatFilename = getCoreProperty(SJ_RemoteFormatFilename).toString();

    if (jobType() == SequenceJob::JOBTYPE_PREVIEW && !isVideo())
    {
        if (devices.data()->getActiveCamera()->getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            devices.data()->getActiveCamera()->setUploadMode(ISD::Camera::UPLOAD_CLIENT);
    }
    else
        devices.data()->getActiveCamera()->setUploadMode(m_UploadMode);

    if (devices.data()->getActiveChip()->isBatchMode() &&
            remoteFormatDirectory.isEmpty() == false &&
            remoteFormatFilename.isEmpty() == false)
    {
        if (isVideo())
            devices.data()->getActiveCamera()->setSERNameDirectory(remoteFormatFilename, remoteFormatDirectory);
        else
            devices.data()->getActiveCamera()->updateUploadSettings(remoteFormatDirectory, remoteFormatFilename);

    }

    if (isVideo())
    {
        // video settings
        devices.data()->getActiveCamera()->setUploadMode(m_UploadMode);

        devices.data()->getActiveCamera()->setStreamRecording(getCoreProperty(SJ_Format).toString());
        devices.data()->getActiveCamera()->setStreamEncoding(getCoreProperty(SJ_Encoding).toString());
    }
    else
    {
        devices.data()->getActiveChip()->setBatchMode(jobType() != SequenceJob::JOBTYPE_PREVIEW);
        devices.data()->getActiveCamera()->setSeqPrefix(getCoreProperty(SJ_FullPrefix).toString());
        logentry.append(QString(", batch mode = %1, seq prefix = %2").arg(jobType() != SequenceJob::JOBTYPE_PREVIEW ? "true" :
                        "false").arg(getCoreProperty(SJ_FullPrefix).toString()));


        devices.data()->getActiveCamera()->setCaptureFormat(getCoreProperty(SJ_Format).toString());
        devices.data()->getActiveCamera()->setEncodingFormat(getCoreProperty(SJ_Encoding).toString());
        devices.data()->getActiveChip()->setFrameType(getFrameType());
    }
    if (getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
        logentry.append(QString(", remote dir = %1, remote format = %2").arg(remoteFormatDirectory).arg(remoteFormatFilename));

    logentry.append(QString(", format = %1, encoding = %2").arg(getCoreProperty(SJ_Format).toString()).arg(getCoreProperty(
                        SJ_Encoding).toString()));

    // Only attempt to set ROI and Binning if CCD transfer format is FITS or XISF
    int currentBinX = 1, currentBinY = 1;
    devices.data()->getActiveChip()->getBinning(&currentBinX, &currentBinY);

    const auto binning = getCoreProperty(SJ_Binning).toPoint();
    // N.B. Always set binning _before_ setting frame because if the subframed image
    // is problematic in 1x1 but works fine for 2x2, then it would fail it was set first
    // So setting binning first always ensures this will work.
    if (devices.data()->getActiveChip()->canBin())
    {
        if (devices.data()->getActiveChip()->setBinning(binning.x(), binning.y()) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE()) << "Cannot set binning to " << "x =" << binning.x() << ", y =" << binning.y();
            setStatus(JOB_ERROR);
            emit captureStarted(CAPTURE_BIN_ERROR);
        }
        else
            logentry.append(QString(", binning = %1x%2").arg(binning.x()).arg(binning.y()));
    }
    else
        logentry.append(QString(", Cannot bin"));


    const auto roi = getCoreProperty(SJ_ROI).toRect();

    if (devices.data()->getActiveChip()->canSubframe())
    {
        if ((roi.width() > 0 && roi.height() > 0) && devices.data()->getActiveChip()->setFrame(roi.x(),
                roi.y(),
                roi.width(),
                roi.height(),
                currentBinX != binning.x()) == false)
        {
            qCWarning(KSTARS_EKOS_CAPTURE()) << "Cannot set ROI to " << "x =" << roi.x() << ", y =" << roi.y() << ", widht =" <<
                                             roi.width() << "height =" << roi.height();
            setStatus(JOB_ERROR);
            emit captureStarted(CAPTURE_FRAME_ERROR);
        }
        else
            logentry.append(QString(", ROI = (%1+%2, %3+%4)").arg(roi.x()).arg(roi.width()).arg(roi.y()).arg(roi.width()));
    }
    else
        logentry.append(", Cannot subframe");

    return logentry;
}

void SequenceJob::capture(FITSMode mode)
{
    if (!devices.data()->getActiveCamera() || !devices.data()->getActiveChip())
        return;

    QString logentry = setCameraDeviceProperties();

    // update the status
    setStatus(getStatus());

    if (isVideo())
    {
        devices.data()->getActiveCamera()->setStreamExposure(getCoreProperty(SJ_Exposure).toDouble());
        auto frames = getCoreProperty(SJ_Count).toUInt();
        auto success = devices.data()->getActiveCamera()->startFramesRecording(frames);
        if (! success)
        {
            qCWarning(KSTARS_EKOS_CAPTURE) << "Start recording failed!";
            emit captureStarted(CAPTURE_FRAME_ERROR);
            return;
        }

    }
    else
    {
        // In case FITS Viewer is not enabled. Then for flat frames, we still need to keep the data
        // otherwise INDI CCD would simply discard loading the data in batch mode as the data are already
        // saved to disk and since no extra processing is required, FITSData is not loaded up with the data.
        // But in case of automatically calculated flat frames, we need FITSData.
        // Therefore, we need to explicitly set mode to FITS_CALIBRATE so that FITSData is generated.
        devices.data()->getActiveChip()->setCaptureMode(mode);
        devices.data()->getActiveChip()->setCaptureFilter(FITS_NONE);

        m_ExposeLeft = getCoreProperty(SJ_Exposure).toDouble();
        devices.data()->getActiveChip()->capture(m_ExposeLeft);
    }

    emit captureStarted(CAPTURE_OK);

    // create log entry with settings
    qCInfo(KSTARS_EKOS_CAPTURE) << logentry;
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
    return state->m_CameraState->currentFilterID;
}

ISD::Mount::PierSide SequenceJob::getPierSide() const
{
    return state->m_CameraState->getPierSide();
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
void SequenceJob::setCalibrationPreAction(uint32_t value)
{
    state->m_CalibrationPreAction = value;
}
// Getter: Get calibration pre action
uint32_t SequenceJob::getCalibrationPreAction() const
{
    return state->m_CalibrationPreAction;
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

void SequenceJob::setLightBox(ISD::LightBox * lightBox)
{
    state->m_CameraState->hasLightBox = (lightBox != nullptr);
}

void SequenceJob::setDustCap(ISD::DustCap * dustCap)
{
    state->m_CameraState->hasDustCap = (dustCap != nullptr);
}

void SequenceJob::addMount(ISD::Mount * scope)
{
    state->m_CameraState->hasTelescope = (scope != nullptr);
}

void SequenceJob::setDome(ISD::Dome * dome)
{
    state->m_CameraState->hasDome = (dome != nullptr);
}

double SequenceJob::currentTemperature() const
{
    return devices->cameraTemperature();
}

double SequenceJob::currentGain() const
{
    return devices->cameraGain();
}

double SequenceJob::currentOffset() const
{
    return devices->cameraOffset();
}

void SequenceJob::prepareCapture()
{
    // simply forward it to the state machine
    switch (getFrameType())
    {
        case FRAME_LIGHT:
        case FRAME_VIDEO:
            state->prepareLightFrameCapture(getCoreProperty(SJ_EnforceTemperature).toBool(),
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
    qDebug(KSTARS_EKOS_CAPTURE) << "Sequence job: capture preparation" << (success ? "succeeded" : "failed");
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

void SequenceJob::loadFrom(XMLEle *root, const QString &targetName, SequenceJobType jobType)
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
    m_CoreProperties[SJ_DitherPerJobEnabled] = true;
    m_CoreProperties[SJ_DitherPerJobFrequency] = 0;
    m_CoreProperties[SJ_Encoding] = "FITS";

    // targetName overrides values from the XML document
    if (targetName != "")
        setCoreProperty(SequenceJob::SJ_TargetName, targetName);

    if (root == nullptr)
        return;

    bool isDarkFlat = false;

    QLocale cLocale = QLocale::c();
    XMLEle * ep;
    XMLEle * subEP;
    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            setCoreProperty(SequenceJob::SJ_Exposure, cLocale.toDouble(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Format"))
            setCoreProperty(SequenceJob::SJ_Format, QString(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Encoding"))
        {
            setCoreProperty(SequenceJob::SJ_Encoding, QString(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Binning"))
        {
            QPoint binning(1, 1);
            subEP = findXMLEle(ep, "X");
            if (subEP)
                binning.setX(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                binning.setY(cLocale.toInt(pcdataXMLEle(subEP)));

            setCoreProperty(SequenceJob::SJ_Binning, binning);
        }
        else if (!strcmp(tagXMLEle(ep), "Frame"))
        {
            QRect roi(0, 0, 0, 0);
            subEP = findXMLEle(ep, "X");
            if (subEP)
                roi.setX(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "Y");
            if (subEP)
                roi.setY(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "W");
            if (subEP)
                roi.setWidth(cLocale.toInt(pcdataXMLEle(subEP)));
            subEP = findXMLEle(ep, "H");
            if (subEP)
                roi.setHeight(cLocale.toInt(pcdataXMLEle(subEP)));

            setCoreProperty(SequenceJob::SJ_ROI, roi);
        }
        else if (!strcmp(tagXMLEle(ep), "Temperature"))
        {
            setTargetTemperature(cLocale.toDouble(pcdataXMLEle(ep)));

            // If force attribute exist, we change cameraTemperatureS, otherwise do nothing.
            if (!strcmp(findXMLAttValu(ep, "force"), "true"))
                setCoreProperty(SequenceJob::SJ_EnforceTemperature, true);
            else if (!strcmp(findXMLAttValu(ep, "force"), "false"))
                setCoreProperty(SequenceJob::SJ_EnforceTemperature, false);
        }
        else if (!strcmp(tagXMLEle(ep), "Filter"))
        {
            const auto name = pcdataXMLEle(ep);
            const auto index = std::max(1, (int)(filterLabels().indexOf(name) + 1));
            setTargetFilter(index, name);
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            int index = frameTypes().indexOf(pcdataXMLEle(ep));
            setFrameType(static_cast<CCDFrameType>(qMax(0, index)));
        }
        else if (!strcmp(tagXMLEle(ep), "TargetName"))
        {
            QString jobTarget = pcdataXMLEle(ep);

            if (targetName.isEmpty())
                // use the target from the XML document
                setCoreProperty(SequenceJob::SJ_TargetName, QString(jobTarget));
            else if (!jobTarget.isEmpty())
                // issue a warning that target from the XML document is ignored
                qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job target name %1 ignored in favor of %2.").arg(jobTarget, targetName);
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job is using outdated format. Please create a new sequence file");
            // RawPrefix is outdated and will be ignored
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
            {
                QString jobTarget = pcdataXMLEle(subEP);

                if (targetName.isEmpty())
                    // use the target from the XML document
                    setCoreProperty(SequenceJob::SJ_TargetName, QString(jobTarget));
                else if (!jobTarget.isEmpty())
                    // issue a warning that target from the XML document is ignored
                    qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job target name %1 ignored in favor of %2.").arg(jobTarget, targetName);
            }
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
            setCoreProperty(SequenceJob::SJ_PlaceholderFormat,
                            PlaceholderPath::defaultFormat(filterEnabled, expEnabled, tsEnabled));
        }
        else if (!strcmp(tagXMLEle(ep), "Count"))
        {
            setCoreProperty(SequenceJob::SJ_Count, cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Delay"))
        {
            setCoreProperty(SequenceJob::SJ_Delay, cLocale.toInt(pcdataXMLEle(ep)) * 1000);
        }
        else if (!strcmp(tagXMLEle(ep), "PostCaptureScript"))
        {
            m_Scripts[SCRIPT_POST_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreCaptureScript"))
        {
            m_Scripts[SCRIPT_PRE_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PostJobScript"))
        {
            m_Scripts[SCRIPT_POST_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreJobScript"))
        {
            m_Scripts[SCRIPT_PRE_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "GuideDitherPerJob"))
        {
            const int value = cLocale.toInt(pcdataXMLEle(ep));
            if (value >= 0)
            {
                setCoreProperty(SequenceJob::SJ_DitherPerJobFrequency, value);
                setCoreProperty(SequenceJob::SJ_DitherPerJobEnabled, true);
            }
            else
                setCoreProperty(SequenceJob::SJ_DitherPerJobEnabled, false);
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            setCoreProperty(SequenceJob::SJ_LocalDirectory, QString(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderFormat"))
        {
            setCoreProperty(SequenceJob::SJ_PlaceholderFormat, QString(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderSuffix"))
        {
            setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, cLocale.toUInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            setCoreProperty(SequenceJob::SJ_RemoteDirectory, QString(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "UploadMode"))
        {
            setUploadMode(static_cast<ISD::Camera::UploadMode>(cLocale.toInt(pcdataXMLEle(ep))));
        }
        else if (!strcmp(tagXMLEle(ep), "ISOIndex"))
        {
            setISO(cLocale.toInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Rotation"))
        {
            setTargetRotation(cLocale.toDouble(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "Properties"))
        {
            QMap<QString, QMap<QString, QVariant>> propertyMap;

            for (subEP = nextXMLEle(ep, 1); subEP != nullptr; subEP = nextXMLEle(ep, 0))
            {
                QMap<QString, QVariant> elements;
                XMLEle * oneElement = nullptr;
                for (oneElement = nextXMLEle(subEP, 1); oneElement != nullptr; oneElement = nextXMLEle(subEP, 0))
                {
                    const char * name = findXMLAttValu(oneElement, "name");
                    bool ok = false;
                    // String
                    auto xmlValue = pcdataXMLEle(oneElement);
                    // Try to load it as double
                    auto value = cLocale.toDouble(xmlValue, &ok);
                    if (ok)
                        elements[name] = value;
                    else
                        elements[name] = *xmlValue;
                }

                const char * name = findXMLAttValu(subEP, "name");
                propertyMap[name] = elements;
            }

            setCustomProperties(propertyMap);
            // read the gain and offset values from the custom properties
            setCoreProperty(SequenceJob::SJ_Gain, devices->cameraGain(propertyMap));
            setCoreProperty(SequenceJob::SJ_Offset, devices->cameraOffset(propertyMap));
        }
        else if (!strcmp(tagXMLEle(ep), "Calibration"))
        {
            // SQ_FORMAT_VERSION >= 2.7
            subEP = findXMLEle(ep, "PreAction");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    setCalibrationPreAction(cLocale.toUInt(pcdataXMLEle(typeEP)));
                    if (getCalibrationPreAction() & CAPTURE_PREACTION_WALL)
                    {
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            setCalibrationPreAction((getCalibrationPreAction() & ~CAPTURE_PREACTION_PARK_MOUNT) | CAPTURE_PREACTION_WALL);
                            SkyPoint wallCoord;
                            wallCoord.setAz(cLocale.toDouble(pcdataXMLEle(azEP)));
                            wallCoord.setAlt(cLocale.toDouble(pcdataXMLEle(altEP)));
                            setWallCoord(wallCoord);
                        }
                        else
                        {
                            qCWarning(KSTARS_EKOS_CAPTURE) << "Wall position coordinates missing, disabling slew to wall position action.";
                            setCalibrationPreAction((getCalibrationPreAction() & ~CAPTURE_PREACTION_WALL) | CAPTURE_PREACTION_NONE);
                        }
                    }
                }
            }

            // SQ_FORMAT_VERSION < 2.7
            subEP = findXMLEle(ep, "FlatSource");
            if (subEP)
            {
                XMLEle * typeEP = findXMLEle(subEP, "Type");
                if (typeEP)
                {
                    // default
                    setCalibrationPreAction(CAPTURE_PREACTION_NONE);
                    if (!strcmp(pcdataXMLEle(typeEP), "Wall"))
                    {
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            setCalibrationPreAction((getCalibrationPreAction() & ~CAPTURE_PREACTION_PARK_MOUNT) | CAPTURE_PREACTION_WALL);
                            SkyPoint wallCoord;
                            wallCoord.setAz(cLocale.toDouble(pcdataXMLEle(azEP)));
                            wallCoord.setAlt(cLocale.toDouble(pcdataXMLEle(altEP)));
                            setWallCoord(wallCoord);
                        }
                    }
                }
            }

            // SQ_FORMAT_VERSION < 2.7
            subEP = findXMLEle(ep, "PreMountPark");
            if (subEP && !strcmp(pcdataXMLEle(subEP), "True"))
                setCalibrationPreAction(getCalibrationPreAction() | CAPTURE_PREACTION_PARK_MOUNT);

            // SQ_FORMAT_VERSION < 2.7
            subEP = findXMLEle(ep, "PreDomePark");
            if (subEP && !strcmp(pcdataXMLEle(subEP), "True"))
                setCalibrationPreAction(getCalibrationPreAction() | CAPTURE_PREACTION_PARK_DOME);

            subEP = findXMLEle(ep, "FlatDuration");
            if (subEP)
            {
                const char * dark = findXMLAttValu(subEP, "dark");
                isDarkFlat = !strcmp(dark, "true");

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
                    setCoreProperty(SequenceJob::SJ_TargetADU, QVariant(cLocale.toDouble(pcdataXMLEle(aduEP))));
                }

                aduEP = findXMLEle(subEP, "Tolerance");
                if (aduEP)
                {
                    setCoreProperty(SequenceJob::SJ_TargetADUTolerance, QVariant(cLocale.toDouble(pcdataXMLEle(aduEP))));
                }
                aduEP = findXMLEle(subEP, "SkyFlat");
                if (aduEP)
                {
                    setCoreProperty(SequenceJob::SJ_SkyFlat, (bool)!strcmp(pcdataXMLEle(aduEP), "true"));
                }
            }
        }
    }
    if(isDarkFlat)
        setJobType(SequenceJob::JOBTYPE_DARKFLAT);
}

void SequenceJob::saveTo(QTextStream &outstream, const QLocale &cLocale) const
{
    auto roi = getCoreProperty(SequenceJob::SJ_ROI).toRect();
    auto ditherPerJobEnabled = getCoreProperty(SequenceJob::SJ_DitherPerJobEnabled).toBool();
    auto ditherPerJobFrequency = getCoreProperty(SequenceJob::SJ_DitherPerJobFrequency).toInt();

    outstream << "<Job>" << Qt::endl;

    outstream << "<Exposure>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_Exposure).toDouble()) << "</Exposure>" <<
              Qt::endl;
    outstream << "<Format>" << getCoreProperty(SequenceJob::SJ_Format).toString() << "</Format>" << Qt::endl;
    outstream << "<Encoding>" << getCoreProperty(SequenceJob::SJ_Encoding).toString() << "</Encoding>" << Qt::endl;
    outstream << "<Binning>" << Qt::endl;
    outstream << "<X>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_Binning).toPoint().x()) << "</X>" << Qt::endl;
    outstream << "<Y>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_Binning).toPoint().y()) << "</Y>" << Qt::endl;
    outstream << "</Binning>" << Qt::endl;
    outstream << "<Frame>" << Qt::endl;
    outstream << "<X>" << cLocale.toString(roi.x()) << "</X>" << Qt::endl;
    outstream << "<Y>" << cLocale.toString(roi.y()) << "</Y>" << Qt::endl;
    outstream << "<W>" << cLocale.toString(roi.width()) << "</W>" << Qt::endl;
    outstream << "<H>" << cLocale.toString(roi.height()) << "</H>" << Qt::endl;
    outstream << "</Frame>" << Qt::endl;
    if (getTargetTemperature() != Ekos::INVALID_VALUE)
        outstream << "<Temperature force='" << (getCoreProperty(SequenceJob::SJ_EnforceTemperature).toBool() ? "true" :
                                                "false") << "'>"
                  << cLocale.toString(getTargetTemperature()) << "</Temperature>" << Qt::endl;
    if (getTargetFilter() >= 0)
        outstream << "<Filter>" << getCoreProperty(SequenceJob::SJ_Filter).toString() << "</Filter>" << Qt::endl;

    // Safely get frame type name with bounds checking
    const QStringList frameTypesList = frameTypes();
    const int frameTypeIndex = static_cast<int>(getFrameType());
    QString frameTypeName;
    if (frameTypeIndex >= 0 && frameTypeIndex < frameTypesList.size())
        frameTypeName = frameTypesList[frameTypeIndex];
    else
        frameTypeName = "Unknown";

    outstream << "<Type>" << frameTypeName << "</Type>" << Qt::endl;
    outstream << "<Count>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_Count).toInt()) << "</Count>" << Qt::endl;
    // ms to seconds
    outstream << "<Delay>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_Delay).toInt() / 1000.0) << "</Delay>" <<
              Qt::endl;
    if (getCoreProperty(SequenceJob::SJ_TargetName) != "")
        outstream << "<TargetName>" << getCoreProperty(SequenceJob::SJ_TargetName).toString() << "</TargetName>" << Qt::endl;
    if (getScript(SCRIPT_PRE_CAPTURE).isEmpty() == false)
        outstream << "<PreCaptureScript>" << getScript(SCRIPT_PRE_CAPTURE) << "</PreCaptureScript>" << Qt::endl;
    if (getScript(SCRIPT_POST_CAPTURE).isEmpty() == false)
        outstream << "<PostCaptureScript>" << getScript(SCRIPT_POST_CAPTURE) << "</PostCaptureScript>" << Qt::endl;
    if (getScript(SCRIPT_PRE_JOB).isEmpty() == false)
        outstream << "<PreJobScript>" << getScript(SCRIPT_PRE_JOB) << "</PreJobScript>" << Qt::endl;
    if (getScript(SCRIPT_POST_JOB).isEmpty() == false)
        outstream << "<PostJobScript>" << getScript(SCRIPT_POST_JOB) << "</PostJobScript>" << Qt::endl;
    outstream << "<GuideDitherPerJob>"
              << cLocale.toString(ditherPerJobEnabled ? ditherPerJobFrequency : -1) << "</GuideDitherPerJob>" <<
              Qt::endl;
    outstream << "<FITSDirectory>" << getCoreProperty(SequenceJob::SJ_LocalDirectory).toString() << "</FITSDirectory>" <<
              Qt::endl;
    outstream << "<PlaceholderFormat>" << getCoreProperty(SequenceJob::SJ_PlaceholderFormat).toString() <<
              "</PlaceholderFormat>" <<
              Qt::endl;
    outstream << "<PlaceholderSuffix>" << getCoreProperty(SequenceJob::SJ_PlaceholderSuffix).toUInt() <<
              "</PlaceholderSuffix>" <<
              Qt::endl;
    outstream << "<UploadMode>" << getUploadMode() << "</UploadMode>" << Qt::endl;
    if (getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString().isEmpty() == false)
        outstream << "<RemoteDirectory>" << getCoreProperty(SequenceJob::SJ_RemoteDirectory).toString() << "</RemoteDirectory>"
                  << Qt::endl;
    if (getCoreProperty(SequenceJob::SJ_ISOIndex).toInt() != -1)
        outstream << "<ISOIndex>" << (getCoreProperty(SequenceJob::SJ_ISOIndex).toInt()) << "</ISOIndex>" << Qt::endl;
    if (getTargetRotation() != Ekos::INVALID_VALUE)
        outstream << "<Rotation>" << (getTargetRotation()) << "</Rotation>" << Qt::endl;
    QMapIterator<QString, QMap<QString, QVariant>> customIter(getCustomProperties());
    outstream << "<Properties>" << Qt::endl;
    while (customIter.hasNext())
    {
        customIter.next();
        outstream << "<PropertyVector name='" << customIter.key() << "'>" << Qt::endl;
        QMap<QString, QVariant> elements = customIter.value();
        QMapIterator<QString, QVariant> iter(elements);
        while (iter.hasNext())
        {
            iter.next();
            if (iter.value().type() == QVariant::String)
            {
                outstream << "<OneElement name='" << iter.key()
                          << "'>" << iter.value().toString() << "</OneElement>" << Qt::endl;
            }
            else
            {
                outstream << "<OneElement name='" << iter.key()
                          << "'>" << iter.value().toDouble() << "</OneElement>" << Qt::endl;
            }
        }
        outstream << "</PropertyVector>" << Qt::endl;
    }
    outstream << "</Properties>" << Qt::endl;

    outstream << "<Calibration>" << Qt::endl;
    outstream << "<PreAction>" << Qt::endl;
    outstream << QString("<Type>%1</Type>").arg(getCalibrationPreAction()) << Qt::endl;
    if (getCalibrationPreAction() & CAPTURE_PREACTION_WALL)
    {
        outstream << "<Az>" << cLocale.toString(getWallCoord().az().Degrees()) << "</Az>" << Qt::endl;
        outstream << "<Alt>" << cLocale.toString(getWallCoord().alt().Degrees()) << "</Alt>" << Qt::endl;
    }
    outstream << "</PreAction>" << Qt::endl;

    outstream << "<FlatDuration dark='" << (jobType() == SequenceJob::JOBTYPE_DARKFLAT ? "true" : "false")
              << "'>" << Qt::endl;
    if (getFlatFieldDuration() == DURATION_MANUAL)
        outstream << "<Type>Manual</Type>" << Qt::endl;
    else
    {
        outstream << "<Type>ADU</Type>" << Qt::endl;
        outstream << "<Value>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_TargetADU).toDouble()) << "</Value>" <<
                  Qt::endl;
        outstream << "<Tolerance>" << cLocale.toString(getCoreProperty(SequenceJob::SJ_TargetADUTolerance).toDouble()) <<
                  "</Tolerance>" << Qt::endl;
        outstream << "<SkyFlat>" << (getCoreProperty(SequenceJob::SJ_SkyFlat).toBool() ? "true" : "false") <<
                  "</SkyFlat>" << Qt::endl;
    }
    outstream << "</FlatDuration>" << Qt::endl;
    outstream << "</Calibration>" << Qt::endl;
    outstream << "</Job>" << Qt::endl;
}
}

