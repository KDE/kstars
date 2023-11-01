/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "sequencejob.h"

#include <KNotifications/KNotification>
#include <ekos_capture_debug.h>
#include "capturedeviceadaptor.h"
#include "skyobjects/skypoint.h"

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
    QSharedPointer<CaptureModuleState> sharedState;
    sharedState.reset(new CaptureModuleState);
    state.reset(new SequenceJobState(sharedState));
    // set simple device adaptor
    devices.reset(new CaptureDeviceAdaptor());

    init(SequenceJob::JOBTYPE_BATCH, root, sharedState, targetName);
}

SequenceJob::SequenceJob(const QSharedPointer<CaptureDeviceAdaptor> cp,
                         const QSharedPointer<CaptureModuleState> sharedState, SequenceJobType jobType, XMLEle *root,
                         QString targetName)
{
    devices = cp;
    init(jobType, root, sharedState, targetName);
}

void Ekos::SequenceJob::init(SequenceJobType jobType, XMLEle *root, QSharedPointer<CaptureModuleState> sharedState,
                             QString targetName)
{
    setJobType(jobType);
    // initialize the state machine
    state.reset(new SequenceJobState(sharedState));
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

    // signal forwarding between this and the state machine
    connect(this, &SequenceJob::updateGuiderDrift, state.data(), &SequenceJobState::setCurrentGuiderDrift);
    connect(state.data(), &SequenceJobState::prepareState, this, &SequenceJob::prepareState);
    connect(state.data(), &SequenceJobState::prepareComplete, this, &SequenceJob::processPrepareComplete);
    connect(state.data(), &SequenceJobState::abortCapture, this, &SequenceJob::processAbortCapture);
    connect(state.data(), &SequenceJobState::newLog, this, &SequenceJob::newLog);
    // start capturing as soon as the capture initialization is complete
    connect(state.data(), &SequenceJobState::initCaptureComplete, this, &SequenceJob::capture);

    // finish if XML document empty
    if (root == nullptr)
        return;

    bool isDarkFlat = false;
    sharedState->scripts().clear();
    QLocale cLocale = QLocale::c();
    XMLEle * ep;
    XMLEle * subEP;
    for (ep = nextXMLEle(root, 1); ep != nullptr; ep = nextXMLEle(root, 0))
    {
        if (!strcmp(tagXMLEle(ep), "Exposure"))
            setCoreProperty(SequenceJob::SJ_Exposure, cLocale.toDouble(pcdataXMLEle(ep)));
        else if (!strcmp(tagXMLEle(ep), "Format"))
            setCoreProperty(SequenceJob::SJ_Format, pcdataXMLEle(ep));
        else if (!strcmp(tagXMLEle(ep), "Encoding"))
        {
            setCoreProperty(SequenceJob::SJ_Encoding, pcdataXMLEle(ep));
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
            const auto index = std::max(1, filterLabels().indexOf(name) + 1);
            setTargetFilter(index, name);
        }
        else if (!strcmp(tagXMLEle(ep), "Type"))
        {
            int index = frameTypes().indexOf(pcdataXMLEle(ep));
            setFrameType(static_cast<CCDFrameType>(qMax(0, index)));
        }
        else if (!strcmp(tagXMLEle(ep), "TargetName"))
        {
            if (targetName == "")
            {
                setCoreProperty(SequenceJob::SJ_TargetName, pcdataXMLEle(ep));
            }
            else
            {
                // targetName overrides values from the file
                setCoreProperty(SequenceJob::SJ_TargetName, targetName);
                if (strcmp(pcdataXMLEle(ep), "") != 0)
                    qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job raw prefix %1 ignored.").arg(pcdataXMLEle(ep));
            }
        }
        else if (!strcmp(tagXMLEle(ep), "Prefix"))
        {
            // RawPrefix is outdated and will be ignored
            subEP = findXMLEle(ep, "RawPrefix");
            if (subEP)
            {
                if (targetName == "")
                {
                    setCoreProperty(SequenceJob::SJ_TargetName, pcdataXMLEle(subEP));
                }
                else
                {
                    // targetName overrides values from the file
                    setCoreProperty(SequenceJob::SJ_TargetName, targetName);
                    if (strcmp(pcdataXMLEle(subEP), "") != 0)
                        qWarning(KSTARS_EKOS_CAPTURE) << QString("Sequence job raw prefix %1 ignored.").arg(pcdataXMLEle(subEP));
                }
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
            sharedState->scripts()[SCRIPT_POST_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreCaptureScript"))
        {
            sharedState->scripts()[SCRIPT_PRE_CAPTURE] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PostJobScript"))
        {
            sharedState->scripts()[SCRIPT_POST_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "PreJobScript"))
        {
            sharedState->scripts()[SCRIPT_PRE_JOB] = pcdataXMLEle(ep);
        }
        else if (!strcmp(tagXMLEle(ep), "FITSDirectory"))
        {
            setCoreProperty(SequenceJob::SJ_LocalDirectory, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderFormat"))
        {
            setCoreProperty(SequenceJob::SJ_PlaceholderFormat, pcdataXMLEle(ep));
        }
        else if (!strcmp(tagXMLEle(ep), "PlaceholderSuffix"))
        {
            setCoreProperty(SequenceJob::SJ_PlaceholderSuffix, cLocale.toUInt(pcdataXMLEle(ep)));
        }
        else if (!strcmp(tagXMLEle(ep), "RemoteDirectory"))
        {
            setCoreProperty(SequenceJob::SJ_RemoteDirectory, pcdataXMLEle(ep));
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
                        elements[name] = xmlValue;
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
                    if (getCalibrationPreAction() == ACTION_WALL)
                    {
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            setCalibrationPreAction((getCalibrationPreAction() & ~ACTION_PARK_MOUNT) | ACTION_WALL);
                            SkyPoint wallCoord;
                            wallCoord.setAz(cLocale.toDouble(pcdataXMLEle(azEP)));
                            wallCoord.setAlt(cLocale.toDouble(pcdataXMLEle(altEP)));
                            setWallCoord(wallCoord);
                        }
                        else
                            setCalibrationPreAction((getCalibrationPreAction() & ~ACTION_WALL) | ACTION_NONE);
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
                    setCalibrationPreAction(ACTION_NONE);
                    if (!strcmp(pcdataXMLEle(typeEP), "Wall"))
                    {
                        XMLEle * azEP  = findXMLEle(subEP, "Az");
                        XMLEle * altEP = findXMLEle(subEP, "Alt");

                        if (azEP && altEP)
                        {
                            setCalibrationPreAction((getCalibrationPreAction() & ~ACTION_PARK_MOUNT) | ACTION_WALL);
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
                setCalibrationPreAction(getCalibrationPreAction() | ACTION_PARK_MOUNT);

            // SQ_FORMAT_VERSION < 2.7
            subEP = findXMLEle(ep, "PreDomePark");
            if (subEP && !strcmp(pcdataXMLEle(subEP), "True"))
                setCalibrationPreAction(getCalibrationPreAction() | ACTION_PARK_DOME);

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
            }
        }
    }
    if(isDarkFlat)
        setJobType(SequenceJob::JOBTYPE_DARKFLAT);

    // copy general state attributes
    // TODO: does this really make sense? This is a general setting - sterne-jaeger@openfuture.de, 2023-09-21
    setCoreProperty(SequenceJob::SJ_EnforceStartGuiderDrift, Options::enforceStartGuiderDrift());
    setTargetStartGuiderDrift(Options::startGuideDeviation());

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
    setCoreProperty(SequenceJob::SJ_ISOIndex, index);
    const auto isolist = devices->getActiveChip()->getISOList();
    if (isolist.count() > index && index >= 0)
        setCoreProperty(SequenceJob::SJ_ISO, isolist[index]);
}

QStringList SequenceJob::frameTypes()
{
    if (!devices->getActiveCamera())
        return QStringList({"Light", "Bias", "Dark", "Flat"});

    ISD::CameraChip *tChip = devices->getActiveCamera()->getChip(ISD::CameraChip::PRIMARY_CCD);

    return tChip->getFrameTypes();
}

QStringList SequenceJob::filterLabels()
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

void SequenceJob::capture(FITSMode mode)
{
    if (!devices.data()->getActiveCamera() || !devices.data()->getActiveChip())
        return;

    // initialize the log entry
    QString logentry = QString("Capture exposure = %1 sec, type = %2").arg(getCoreProperty(SJ_Exposure).toDouble()).arg(
                           CCDFrameTypeNames[getFrameType()]);
    logentry.append(QString(", filter = %1, upload mode = %2").arg(getCoreProperty(SJ_Filter).toString()).arg(getUploadMode()));

    devices.data()->getActiveChip()->setBatchMode(jobType() != SequenceJob::JOBTYPE_PREVIEW);
    devices.data()->getActiveCamera()->setSeqPrefix(getCoreProperty(SJ_FullPrefix).toString());
    logentry.append(QString(", batch mode = %1, seq prefix = %2").arg(jobType() != SequenceJob::JOBTYPE_PREVIEW ? "true" :
                    "false").arg(getCoreProperty(SJ_FullPrefix).toString()));

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

    const auto remoteFormatDirectory = getCoreProperty(SJ_RemoteFormatDirectory).toString();
    const auto remoteFormatFilename = getCoreProperty(SJ_RemoteFormatFilename).toString();
    if (devices.data()->getActiveChip()->isBatchMode() &&
            remoteFormatDirectory.isEmpty() == false &&
            remoteFormatFilename.isEmpty() == false)
    {
        devices.data()->getActiveCamera()->updateUploadSettings(remoteFormatDirectory, remoteFormatFilename);
        if (getUploadMode() != ISD::Camera::UPLOAD_CLIENT)
            logentry.append(QString(", remote dir = %1, remote format = %2").arg(remoteFormatDirectory).arg(remoteFormatFilename));
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

    devices.data()->getActiveCamera()->setCaptureFormat(getCoreProperty(SJ_Format).toString());
    devices.data()->getActiveCamera()->setEncodingFormat(getCoreProperty(SJ_Encoding).toString());
    devices.data()->getActiveChip()->setFrameType(getFrameType());
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
            emit captureStarted(CaptureModuleState::CAPTURE_BIN_ERROR);
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
            emit captureStarted(CaptureModuleState::CAPTURE_FRAME_ERROR);
        }
        else
            logentry.append(QString(", ROI = (%1+%2, %3+%4)").arg(roi.x()).arg(roi.width()).arg(roi.y()).arg(roi.width()));
    }
    else
        logentry.append(", Cannot subframe");

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
    // create log entry with settings
    qCInfo(KSTARS_EKOS_CAPTURE()) << logentry;

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
    state->m_CaptureModuleState->hasLightBox = (lightBox != nullptr);
}

void SequenceJob::setDustCap(ISD::DustCap * dustCap)
{
    state->m_CaptureModuleState->hasDustCap = (dustCap != nullptr);
}

void SequenceJob::addMount(ISD::Mount * scope)
{
    state->m_CaptureModuleState->hasTelescope = (scope != nullptr);
}

void SequenceJob::setDome(ISD::Dome * dome)
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
