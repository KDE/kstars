/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indicamera.h"
#include "indicamerachip.h"

#include "config-kstars.h"

#include "indi_debug.h"

#include "clientmanager.h"
#include "kstars.h"
#include "Options.h"
#include "streamwg.h"
//#include "ekos/manager.h"
#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsdata.h"
#endif

#include <knotification.h>
#include "auxiliary/ksmessagebox.h"
#include "ksnotification.h"
#include <QImageReader>
#include <QFileInfo>
#include <QStatusBar>
#include <QtConcurrent>

#include <basedevice.h>

const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw", "orf" };

const QString getFITSModeStringString(FITSMode mode)
{
    return FITSModes[mode].toString();
}

namespace ISD
{

Camera::Camera(GenericDevice *parent) : ConcreteDevice(parent)
{
    primaryChip.reset(new CameraChip(this, CameraChip::PRIMARY_CCD));

    m_Media.reset(new WSMedia(this));
    connect(m_Media.get(), &WSMedia::newFile, this, &Camera::setWSBLOB);

    connect(m_Parent->getClientManager(), &ClientManager::newBLOBManager, this, &Camera::setBLOBManager, Qt::UniqueConnection);
    m_LastNotificationTS = QDateTime::currentDateTime();
}

Camera::~Camera()
{
    if (m_ImageViewerWindow)
        m_ImageViewerWindow->close();
    if (fileWriteThread.isRunning())
        fileWriteThread.waitForFinished();
    if (fileWriteBuffer != nullptr)
        delete [] fileWriteBuffer;
}

void Camera::setBLOBManager(const char *device, INDI::Property prop)
{
    if (!prop.getRegistered())
        return;

    if (getDeviceName() == device)
        emit newBLOBManager(prop);
}

void Camera::registerProperty(INDI::Property prop)
{
    if (prop.isNameMatch("GUIDER_EXPOSURE"))
    {
        HasGuideHead = true;
        guideChip.reset(new CameraChip(this, CameraChip::GUIDE_CCD));
    }
    else if (prop.isNameMatch("CCD_FRAME_TYPE"))
    {
        primaryChip->clearFrameTypes();
        auto svp = INDI::PropertySwitch(prop);
        for (auto it : svp)
            primaryChip->addFrameLabel(it.getLabel());
    }
    else if (prop.isNameMatch("CCD_FRAME"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        if (nvp && nvp.getPermission() != IP_RO)
            primaryChip->setCanSubframe(true);
    }
    else if (prop.isNameMatch("GUIDER_FRAME"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        if (nvp && nvp.getPermission() != IP_RO)
            guideChip->setCanSubframe(true);
    }
    else if (prop.isNameMatch("CCD_BINNING"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        if (nvp && nvp.getPermission() != IP_RO)
            primaryChip->setCanBin(true);
    }
    else if (prop.isNameMatch("GUIDER_BINNING"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        if (nvp && nvp.getPermission() != IP_RO)
            guideChip->setCanBin(true);
    }
    else if (prop.isNameMatch("CCD_ABORT_EXPOSURE"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp && svp.getPermission() != IP_RO)
            primaryChip->setCanAbort(true);
    }
    else if (prop.isNameMatch("GUIDER_ABORT_EXPOSURE"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp && svp.getPermission() != IP_RO)
            guideChip->setCanAbort(true);
    }
    else if (prop.isNameMatch("CCD_TEMPERATURE"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        HasCooler = true;
        CanCool   = (nvp.getPermission() != IP_RO);
        if (nvp)
            emit newTemperatureValue(nvp[0].getValue());
    }
    else if (prop.isNameMatch("CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
    }
    else if (prop.isNameMatch("CCD_VIDEO_STREAM"))
    {
        // Has Video Stream
        HasVideoStream = true;
    }
    else if (prop.isNameMatch("CCD_CAPTURE_FORMAT"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            m_CaptureFormats.clear();
            for (const auto &oneSwitch : svp)
                m_CaptureFormats << oneSwitch.getLabel();

            m_CaptureFormatIndex = svp.findOnSwitchIndex();
        }
    }
    else if (prop.isNameMatch("CCD_STREAM_ENCODER"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            m_StreamEncodings.clear();
            for (const auto &oneSwitch : svp)
                m_StreamEncodings << oneSwitch.getLabel();

            auto format = svp.findOnSwitch();
            if (format)
                m_StreamEncoding = format->label;
        }
    }
    else if (prop.isNameMatch("CCD_TRANSFER_FORMAT"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            m_EncodingFormats.clear();
            for (const auto &oneSwitch : svp)
                m_EncodingFormats << oneSwitch.getLabel();

            auto format = svp.findOnSwitch();
            if (format)
                m_EncodingFormat = format->label;
        }
    }
    else if (prop.isNameMatch("CCD_STREAM_RECORDER"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            m_VideoFormats.clear();
            for (const auto &oneSwitch : svp)
                m_VideoFormats << oneSwitch.getLabel();

            auto format = svp.findOnSwitch();
            if (format)
                m_StreamRecording = format->label;
        }
    }
    else if (prop.isNameMatch("CCD_EXPOSURE_PRESETS"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            bool ok = false;
            auto separator = QDir::separator();
            for (const auto &oneSwitch : svp)
            {
                QString key = QString(oneSwitch.getLabel());
                double value = key.toDouble(&ok);
                if (!ok)
                {
                    QStringList parts = key.split(separator);
                    if (parts.count() == 2)
                    {
                        bool numOk = false, denOk = false;
                        double numerator = parts[0].toDouble(&numOk);
                        double denominator = parts[1].toDouble(&denOk);
                        if (numOk && denOk && denominator > 0)
                        {
                            ok = true;
                            value = numerator / denominator;
                        }
                    }
                }
                if (ok)
                    m_ExposurePresets.insert(key, value);

                double min = 1e6, max = 1e-6;
                for (auto oneValue : m_ExposurePresets.values())
                {
                    if (oneValue < min)
                        min = oneValue;
                    if (oneValue > max)
                        max = oneValue;
                }
                m_ExposurePresetsMinMax = qMakePair(min, max);
            }
        }
    }
    else if (prop.isNameMatch("CCD_FAST_TOGGLE"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
            m_FastExposureEnabled = svp.findOnSwitchIndex() == 0;
        else
            m_FastExposureEnabled = false;
    }
    else if (prop.isNameMatch("TELESCOPE_TYPE"))
    {
        auto svp = INDI::PropertySwitch(prop);
        if (svp)
        {
            auto format = svp.findWidgetByName("TELESCOPE_PRIMARY");
            if (format && format->getState() == ISS_ON)
                telescopeType = TELESCOPE_PRIMARY;
            else
                telescopeType = TELESCOPE_GUIDE;
        }
    }
    else if (prop.isNameMatch("CCD_WEBSOCKET_SETTINGS"))
    {
        auto nvp = INDI::PropertyNumber(prop);
        m_Media->setURL(QUrl(QString("ws://%1:%2").arg(m_Parent->getClientManager()->getHost()).arg(nvp[0].getValue())));
        m_Media->connectServer();
    }
    else if (prop.isNameMatch("CCD1"))
    {
        primaryCCDBLOB = prop;
    }
    else if (prop.isNameMatch("CCD_GAIN") && m_GainSource == GAIN_SOURCE_UNKNOWN)
    {
        m_GainProperty   = prop;
        m_GainSource     = GAIN_SOURCE_STANDALONE;
        m_GainPermission = m_GainProperty.getPermission();
    }
    else if (prop.isNameMatch("CCD_OFFSET") && m_OffsetSource == OFFSET_SOURCE_UNKNOWN)
    {
        m_OffsetProperty   = prop;
        m_OffsetSource     = OFFSET_SOURCE_STANDALONE;
        m_OffsetPermission = m_OffsetProperty.getPermission();
    }
    // Try to find gain and/or offset property within a general number property (e.g., CCD_CONTROLS)
    else if ((m_GainSource == GAIN_SOURCE_UNKNOWN || m_OffsetSource == OFFSET_SOURCE_UNKNOWN) && prop.getType() == INDI_NUMBER)
    {
        auto controlNP = INDI::PropertyNumber(prop);
        if (controlNP)
        {
            for (const auto &it : controlNP)
            {
                QString name  = QString(it.getName()).toLower();
                QString label = QString(it.getLabel()).toLower();

                if ((name == "gain" || label == "gain") && m_GainSource == GAIN_SOURCE_UNKNOWN)
                {
                    m_GainProperty   = prop;
                    m_GainSource     = GAIN_SOURCE_CONTROLS;
                    m_GainPermission = controlNP.getPermission();
                }
                else if ((name == "offset" || label == "offset") && m_OffsetSource == OFFSET_SOURCE_UNKNOWN)
                {
                    m_OffsetProperty   = prop;
                    m_OffsetSource     = OFFSET_SOURCE_CONTROLS;
                    m_OffsetPermission = controlNP.getPermission();
                }
            }
        }
    }

    ConcreteDevice::registerProperty(prop);
}

void Camera::removeProperty(INDI::Property prop)
{
    if (prop.isNameMatch("CCD_WEBSOCKET_SETTINGS"))
    {
        m_Media->disconnectServer();
    }
}

void Camera::processNumber(INDI::Property prop)
{
    auto nvp = INDI::PropertyNumber(prop);
    if (nvp.isNameMatch("CCD_EXPOSURE"))
    {
        auto np = nvp.findWidgetByName("CCD_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(primaryChip.get(), np->getValue(), nvp.getState());
        if (nvp.getState() == IPS_ALERT)
            emit error(ERROR_CAPTURE);
    }
    else if (prop.isNameMatch("CCD_TEMPERATURE"))
    {
        HasCooler   = true;
        auto np = nvp.findWidgetByName("CCD_TEMPERATURE_VALUE");
        if (np)
            emit newTemperatureValue(np->getValue());
    }
    else if (prop.isNameMatch("GUIDER_EXPOSURE"))
    {
        auto np = nvp.findWidgetByName("GUIDER_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(guideChip.get(), np->getValue(), nvp.getState());
    }
    else if (prop.isNameMatch("FPS"))
    {
        emit newFPS(nvp[0].getValue(), nvp[1].getValue());
    }
    else if (prop.isNameMatch("CCD_RAPID_GUIDE_DATA"))
    {
        if (nvp.getState() == IPS_ALERT)
        {
            emit newGuideStarData(primaryChip.get(), -1, -1, -1);
        }
        else
        {
            double dx = -1, dy = -1, fit = -1;

            auto np = nvp.findWidgetByName("GUIDESTAR_X");
            if (np)
                dx = np->getValue();
            np = nvp.findWidgetByName("GUIDESTAR_Y");
            if (np)
                dy = np->getValue();
            np = nvp.findWidgetByName("GUIDESTAR_FIT");
            if (np)
                fit = np->getValue();

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(primaryChip.get(), dx, dy, fit);
        }
    }
    else if (prop.isNameMatch("GUIDER_RAPID_GUIDE_DATA"))
    {
        if (nvp.getState() == IPS_ALERT)
        {
            emit newGuideStarData(guideChip.get(), -1, -1, -1);
        }
        else
        {
            double dx = -1, dy = -1, fit = -1;
            auto np = nvp.findWidgetByName("GUIDESTAR_X");
            if (np)
                dx = np->getValue();
            np = nvp.findWidgetByName("GUIDESTAR_Y");
            if (np)
                dy = np->getValue();
            np = nvp.findWidgetByName("GUIDESTAR_FIT");
            if (np)
                fit = np->getValue();

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(guideChip.get(), dx, dy, fit);
        }
    }
}

void Camera::processSwitch(INDI::Property prop)
{
    auto svp = INDI::PropertySwitch(prop);

    if (svp.isNameMatch("CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
        emit coolerToggled(svp[0].getState() == ISS_ON);
    }
    else if (QString(svp.getName()).endsWith("VIDEO_STREAM"))
    {
        // If BLOB is not enabled for this camera, then ignore all VIDEO_STREAM calls.
        if (isBLOBEnabled() == false || m_StreamingEnabled == false)
            return;

        HasVideoStream = true;

        if (svp[0].getState() == ISS_ON)
        {
            auto nvp = getNumber("CCD_STREAM_FRAME");
            uint32_t w = 0, h = 0;

            if (nvp)
            {
                w = nvp[0].getValue();
                h = nvp[1].getValue();
            }

            if (w && h)
            {
                streamW = w;
                streamH = h;
            }
            else
            {
                // Only use CCD dimensions if we are receiving raw stream and not stream of images (i.e. mjpeg..etc)
                auto rawBP = getBLOB("CCD1");
                if (rawBP)
                {
                    int x = 0, y = 0, w = 0, h = 0;
                    int binx = 0, biny = 0;

                    primaryChip->getFrame(&x, &y, &w, &h);
                    primaryChip->getBinning(&binx, &biny);
                    streamW = w / binx;
                    streamH = h / biny;
                }
            }

            emit updateVideoWindow(streamW, streamH, svp[0].getState() == ISS_ON);
        }

        m_isStreamEnabled = (svp[0].getState() == ISS_ON);
        emit videoStreamToggled(m_isStreamEnabled);
    }
    else if (svp.isNameMatch("CCD_CAPTURE_FORMAT"))
    {
        m_CaptureFormats.clear();
        for (const auto &it : svp)
            m_CaptureFormats << it.getLabel();
        m_CaptureFormatIndex = svp.findOnSwitchIndex();
    }
    else if (svp.isNameMatch("CCD_TRANSFER_FORMAT"))
    {
        auto format = svp.findOnSwitch();
        if (format)
            m_EncodingFormat = format->getLabel();;
    }
    else if (svp.isNameMatch("RECORD_STREAM"))
    {
        auto recordOFF = svp.findWidgetByName("RECORD_OFF");

        if (recordOFF && recordOFF->s == ISS_ON)
        {
            emit videoRecordToggled(false);
            if (m_isStreamEnabled)
            {
                m_isStreamEnabled = false;
                KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Stopped"), KSNotification::INDI);
            }
        }
        else if (m_isStreamEnabled == false)
        {
            emit videoRecordToggled(true);
            m_isStreamEnabled = true;
            KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Started"), KSNotification::INDI);
        }
    }
    else if (svp.isNameMatch("TELESCOPE_TYPE"))
    {
        auto format = svp.findWidgetByName("TELESCOPE_PRIMARY");
        if (format && format->s == ISS_ON)
            telescopeType = TELESCOPE_PRIMARY;
        else
            telescopeType = TELESCOPE_GUIDE;
    }
    else if (!strcmp(svp.getName(), "CCD_FAST_TOGGLE"))
    {
        m_FastExposureEnabled = svp.findOnSwitchIndex() == 0;
    }
    else if (svp.isNameMatch("CONNECTION"))
    {
        auto dSwitch = svp.findWidgetByName("DISCONNECT");

        if (dSwitch && dSwitch->getState() == ISS_ON)
        {
            emit videoStreamToggled(false);
            emit closeVideoWindow();

            // Clear the gain and offset properties on disconnect.
            m_GainProperty = INDI::Property();
            m_GainSource = GAIN_SOURCE_UNKNOWN;
            m_GainPermission = IP_RO;
            m_OffsetProperty = INDI::Property();
            m_OffsetSource = OFFSET_SOURCE_UNKNOWN;
            m_OffsetPermission = IP_RO;

            primaryCCDBLOB = INDI::Property();
        }
    }
}

void Camera::processText(INDI::Property prop)
{
    auto tvp = INDI::PropertyText(prop);
    if (tvp.isNameMatch("CCD_FILE_PATH"))
    {
        auto filepath = tvp.findWidgetByName("FILE_PATH");
        if (filepath)
            emit newRemoteFile(QString(filepath->getText()));
    }
}

void Camera::setWSBLOB(const QByteArray &message, const QString &extension)
{
    if (!primaryCCDBLOB)
        return;

    auto bvp = primaryCCDBLOB.getBLOB();
    auto bp = bvp->at(0);

    bp->setBlob(const_cast<char *>(message.data()));
    bp->setSize(message.size());
    bp->setFormat(extension.toLatin1().constData());
    processBLOB(primaryCCDBLOB);

    // Disassociate
    bp->setBlob(nullptr);
}

void Camera::processStream(INDI::Property prop)
{
    if (m_isStreamEnabled == false)
        return;

    auto streamFrame = getNumber("CCD_STREAM_FRAME");
    uint32_t w = 0, h = 0;

    if (streamFrame)
    {
        w = streamFrame[0].getValue();
        h = streamFrame[1].getValue();
    }

    if (w && h)
    {
        streamW = w;
        streamH = h;
    }
    else
    {
        int x = 0, y = 0, w = 0, h = 0;
        int binx = 1, biny = 1;

        primaryChip->getFrame(&x, &y, &w, &h);
        primaryChip->getBinning(&binx, &biny);
        streamW = w / binx;
        streamH = h / biny;
    }

    emit showVideoFrame(prop, streamW, streamH);
}

void ISD::Camera::updateFileBuffer(INDI::Property prop, bool is_fits)
{
    if (is_fits)
    {
        // Check if the last write is still ongoing, and if so wait.
        // It is using the fileWriteBuffer.
        if (fileWriteThread.isRunning())
        {
            fileWriteThread.waitForFinished();
        }
    }

    // Will write blob data in a separate thread, and can't depend on the blob
    // memory, so copy it first.

    auto bp = prop.getBLOB()->at(0);
    // Check buffer size.
    if (fileWriteBufferSize != bp->getBlobLen())
    {
        if (fileWriteBuffer != nullptr)
            delete [] fileWriteBuffer;
        fileWriteBufferSize = bp->getBlobLen();
        fileWriteBuffer = new char[fileWriteBufferSize];
    }

    // Copy memory, and write file on a separate thread.
    // Probably too late to return an error if the file couldn't write.
    memcpy(fileWriteBuffer, bp->getBlob(), bp->getBlobLen());
}

bool Camera::saveCurrentImage(QString &filename)
{
    // TODO: Not yet threading the writes for non-fits files.
    // Would need to deal with the raw conversion, etc.
    if (BType == BLOB_FITS)
    {
        // Check if the last write is still ongoing, and if so wait.
        // It is using the fileWriteBuffer.
        if (fileWriteThread.isRunning())
        {
            fileWriteThread.waitForFinished();
        }

        // Wait until the file is written before overwriting the filename.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        fileWriteThread = QtConcurrent::run(&ISD::Camera::WriteImageFileInternal, this, filename, fileWriteBuffer,
                                            fileWriteBufferSize);
#else
        fileWriteThread = QtConcurrent::run(this, &ISD::Camera::WriteImageFileInternal, filename, fileWriteBuffer,
                                            fileWriteBufferSize);
#endif
    }
    else if (!WriteImageFileInternal(filename, static_cast<char *>(fileWriteBuffer), fileWriteBufferSize))
        return false;

    return true;
}

bool Camera::processBLOB(INDI::Property prop)
{
    auto bvp = INDI::PropertyBlob(prop);
    auto size = bvp[0].getSize();
    // Ignore write-only BLOBs since we only receive it for state-change
    if (bvp.getPermission() == IP_WO || size == 0)
        return false;

    BType = BLOB_OTHER;
    auto format = QString(bvp[0].getFormat()).toLower();

    // If stream, process it first
    if (format.contains("stream"))
    {
        if (m_StreamingEnabled == false)
            return true;

        processStream(prop);
        return true;
    }

    // Format without leading . (.jpg --> jpg)
    QString shortFormat = format.mid(1);

    // If it's not FITS or an image, don't process it.
    if ((QImageReader::supportedImageFormats().contains(shortFormat.toLatin1())))
        BType = BLOB_IMAGE;
    else if (format.contains("fits"))
        BType = BLOB_FITS;
    else if (format.contains("xisf"))
        BType = BLOB_XISF;
    else if (RAWFormats.contains(shortFormat))
        BType = BLOB_RAW;

    if (BType == BLOB_OTHER)
        return false;

    CameraChip *targetChip = nullptr;

    if (bvp.isNameMatch("CCD2"))
        targetChip = guideChip.get();
    else
    {
        targetChip = primaryChip.get();
        qCDebug(KSTARS_INDI) << "Image received. Mode:" << getFITSModeStringString(targetChip->getCaptureMode()) << "Size:" <<
                             size;
    }

    // Create temporary name if ANY of the following conditions are met:
    // 1. file is preview or batch mode is not enabled
    // 2. file type is not FITS_NORMAL (focus, guide..etc)
    // create the file buffer only, saving the image file must be triggered from outside.
    updateFileBuffer(prop, BType == BLOB_FITS);

    // Don't spam, just one notification per 3 seconds
    if (QDateTime::currentDateTime().secsTo(m_LastNotificationTS) <= -3)
    {
        KNotification::event(QLatin1String("FITSReceived"), i18n("Image file is received"));
        m_LastNotificationTS = QDateTime::currentDateTime();
    }

    QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<char *>(bvp[0].getBlob()), size);
    QSharedPointer<FITSData> imageData;
    imageData.reset(new FITSData(targetChip->getCaptureMode()), &QObject::deleteLater);
    imageData->setExtension(shortFormat);

    // JM 2024.12.25: Only load from buffer if we need the imageData.
    // When neither FITS Viewer nor Summary view is used, and when the type is FITS_NORMAL in batch mode, then we save to disk directly
    // so that we do not incur delays in loading from buffer that may delay the sequence unnecessairly.
    if ((Options::useFITSViewer() || Options::useSummaryPreview() || targetChip->getCaptureMode() != FITS_NORMAL
            || !targetChip->isBatchMode()) &&
            !imageData->loadFromBuffer(buffer))
    {
        emit error(ERROR_LOAD);
        return true;
    }

    // Add metadata
    imageData->setProperty("device", getDeviceName());
    imageData->setProperty("blobVector", prop.getName());
    imageData->setProperty("blobElement", bvp[0].getName());
    imageData->setProperty("chip", targetChip->getType());

    // Retain a copy
    targetChip->setImageData(imageData);
    emit propertyUpdated(prop);
    emit newImage(imageData, QString(bvp[0].getFormat()).toLower());

    return true;
}

void Camera::StreamWindowHidden()
{
    if (isConnected())
    {
        // We can have more than one *_VIDEO_STREAM property active so disable them all
        auto streamSP = getSwitch("CCD_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP.reset();
            streamSP[0].setState(ISS_OFF);
            streamSP[1].setState(ISS_ON);
            streamSP.setState(IPS_IDLE);
            sendNewProperty(streamSP);
        }

        streamSP = getSwitch("VIDEO_STREAM");
        if (streamSP)
        {
            streamSP.reset();
            streamSP[0].setState(ISS_OFF);
            streamSP[1].setState(ISS_ON);
            streamSP.setState(IPS_IDLE);
            sendNewProperty(streamSP);
        }

        streamSP = getSwitch("AUX_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP.reset();
            streamSP[0].setState(ISS_OFF);
            streamSP[1].setState(ISS_ON);
            streamSP.setState(IPS_IDLE);
            sendNewProperty(streamSP);
        }
    }
}

bool Camera::hasGuideHead()
{
    return HasGuideHead;
}

bool Camera::hasCooler()
{
    return HasCooler;
}

bool Camera::hasCoolerControl()
{
    return HasCoolerControl;
}

bool Camera::setCoolerControl(bool enable)
{
    if (HasCoolerControl == false)
        return false;

    auto coolerSP = getSwitch("CCD_COOLER");

    if (!coolerSP)
        return false;

    // Cooler ON/OFF
    auto coolerON  = coolerSP.findWidgetByName("COOLER_ON");
    auto coolerOFF = coolerSP.findWidgetByName("COOLER_OFF");
    if (!coolerON || !coolerOFF)
        return false;

    coolerON->setState(enable ? ISS_ON : ISS_OFF);
    coolerOFF->setState(enable ? ISS_OFF : ISS_ON);
    sendNewProperty(coolerSP);

    return true;
}

CameraChip *Camera::getChip(CameraChip::ChipType cType)
{
    switch (cType)
    {
        case CameraChip::PRIMARY_CCD:
            return primaryChip.get();

        case CameraChip::GUIDE_CCD:
            return guideChip.get();
    }

    return nullptr;
}

void Camera::updateUploadSettings(const QString &uploadDirectory, const QString &uploadFile)
{
    auto uploadSettingsTP = getText("UPLOAD_SETTINGS");

    if (uploadSettingsTP)
    {
        auto dirT = uploadSettingsTP.findWidgetByName("UPLOAD_DIR");
        if (dirT && uploadDirectory.isEmpty() == false)
        {
            auto posixDirectory = uploadDirectory;
            // N.B. Need to convert any Windows directory separators / to Posix separators /
            posixDirectory.replace(QDir::separator(), "/");
            dirT->setText(posixDirectory.toLatin1().constData());
        }

        auto prefixT = uploadSettingsTP.findWidgetByName("UPLOAD_PREFIX");
        if (prefixT)
            prefixT->setText(uploadFile.toLatin1().constData());

        sendNewProperty(uploadSettingsTP);
    }
}

Camera::UploadMode Camera::getUploadMode()
{
    INDI::PropertySwitch uploadModeSP = getSwitch("UPLOAD_MODE");

    if (!uploadModeSP)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return UPLOAD_CLIENT;
    }

    if (uploadModeSP)
        return static_cast<UploadMode>(uploadModeSP.findOnSwitchIndex());

    // Default
    return UPLOAD_CLIENT;
}

bool Camera::setUploadMode(UploadMode mode)
{

    INDI::PropertySwitch uploadModeSP = getSwitch("UPLOAD_MODE");

    if (!uploadModeSP)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return false;
    }

    auto onIndex = uploadModeSP.findOnSwitchIndex();
    if (onIndex == mode)
        return true;

    uploadModeSP.reset();
    uploadModeSP[mode].setState(ISS_ON);
    sendNewProperty(uploadModeSP);

    return true;
}

bool Camera::getTemperature(double *value)
{
    if (HasCooler == false)
        return false;

    INDI::PropertyNumber temperatureNP = getNumber("CCD_TEMPERATURE");

    if (!temperatureNP)
        return false;

    *value = temperatureNP[0].getValue();

    return true;
}

bool Camera::setTemperature(double value)
{
    INDI::PropertyNumber nvp = getNumber("CCD_TEMPERATURE");

    if (!nvp)
        return false;

    auto np = nvp.findWidgetByName("CCD_TEMPERATURE_VALUE");

    if (!np)
        return false;

    np->setValue(value);

    sendNewProperty(nvp);

    return true;
}

bool Camera::setEncodingFormat(const QString &value)
{
    if (value.isEmpty() || value == m_EncodingFormat)
        return true;

    INDI::PropertySwitch svp = getSwitch("CCD_TRANSFER_FORMAT");

    if (!svp)
        return false;

    svp.reset();
    for (size_t i = 0; i < svp.count(); i++)
    {
        if (svp[i].getLabel() == value)
        {
            svp[i].setState(ISS_ON);
            break;
        }
    }

    m_EncodingFormat = value;
    sendNewProperty(svp);
    return true;
}

bool Camera::setStreamEncoding(const QString &value)
{
    if (value.isEmpty() || value == m_StreamEncoding)
        return true;

    INDI::PropertySwitch svp = getSwitch("CCD_STREAM_ENCODER");

    if (!svp)
        return false;

    svp.reset();
    for (size_t i = 0; i < svp.count(); i++)
    {
        if (svp[i].getLabel() == value)
        {
            svp[i].setState(ISS_ON);
            break;
        }
    }

    m_StreamEncoding = value;
    sendNewProperty(svp);
    return true;
}

bool Camera::setStreamRecording(const QString &value)
{
    if (value.isEmpty() || value == m_StreamRecording)
        return true;

    INDI::PropertySwitch svp = getSwitch("CCD_STREAM_RECORDER");

    if (!svp)
        return false;

    svp.reset();
    for (size_t i = 0; i < svp.count(); i++)
    {
        if (svp[i].getLabel() == value)
        {
            svp[i].setState(ISS_ON);
            break;
        }
    }

    m_StreamRecording = value;
    sendNewProperty(svp);
    return true;
}

bool Camera::setVideoStreamEnabled(bool enable)
{
    if (HasVideoStream == false)
        return false;

    m_isStreamEnabled = enable;

    INDI::PropertySwitch svp = getSwitch("CCD_VIDEO_STREAM");

    if (!svp)
        return false;

    // If already on and enable is set or vice versa no need to change anything we return true
    if ((enable && svp[0].getState() == ISS_ON) || (!enable && svp[1].getState() == ISS_ON))
        return true;

    svp[0].setState(enable ? ISS_ON : ISS_OFF);
    svp[1].setState(enable ? ISS_OFF : ISS_ON);

    sendNewProperty(svp);

    return true;
}

bool Camera::resetStreamingFrame()
{
    INDI::PropertyNumber frameProp = getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp.findWidgetByName("X");
    auto yarg = frameProp.findWidgetByName("Y");
    auto warg = frameProp.findWidgetByName("WIDTH");
    auto harg = frameProp.findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->getValue() - xarg->getMin()) &&
                !std::fabs(yarg->getValue() - yarg->getMin()) &&
                !std::fabs(warg->getValue() - warg->getMax()) &&
                !std::fabs(harg->getValue() - harg->getMax()))
            return false;

        xarg->setValue(xarg->getMin());
        yarg->setValue(yarg->getMin());
        warg->setValue(warg->getMax());
        harg->setValue(harg->getMax());

        sendNewProperty(frameProp);
        return true;
    }

    return false;
}

bool Camera::setStreamLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    INDI::PropertyNumber limitsProp = getNumber("LIMITS");

    if (!limitsProp)
        return false;

    auto bufferMax = limitsProp.findWidgetByName("LIMITS_BUFFER_MAX");
    auto previewFPS = limitsProp.findWidgetByName("LIMITS_PREVIEW_FPS");

    if (bufferMax && previewFPS)
    {
        if(std::fabs(bufferMax->getValue() - maxBufferSize) > 0 || std::fabs(previewFPS->getValue() - maxPreviewFPS) > 0)
        {
            bufferMax->setValue(maxBufferSize);
            previewFPS->setValue(maxPreviewFPS);
            sendNewProperty(limitsProp);
        }

        return true;
    }

    return false;
}

bool Camera::setStreamingFrame(int x, int y, int w, int h)
{
    INDI::PropertyNumber frameProp = getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp.findWidgetByName("X");
    auto yarg = frameProp.findWidgetByName("Y");
    auto warg = frameProp.findWidgetByName("WIDTH");
    auto harg = frameProp.findWidgetByName("HEIGHT");

    if (xarg && yarg && warg && harg)
    {
        if (!std::fabs(xarg->getValue() - x) &&
                !std::fabs(yarg->getValue() - y) &&
                !std::fabs(warg->getValue() - w) &&
                !std::fabs(harg->getValue() - h))
            return true;

        // N.B. We add offset since the X, Y are relative to whatever streaming frame is currently active
        xarg->value = qBound(xarg->getMin(), static_cast<double>(x) + xarg->getValue(), xarg->getMax());
        yarg->value = qBound(yarg->getMin(), static_cast<double>(y) + yarg->getValue(), yarg->getMax());
        warg->value = qBound(warg->getMin(), static_cast<double>(w), warg->getMax());
        harg->value = qBound(harg->getMin(), static_cast<double>(h), harg->getMax());

        sendNewProperty(frameProp);
        return true;
    }

    return false;
}

bool Camera::isStreamingEnabled()
{
    return (HasVideoStream && m_isStreamEnabled);
}

bool Camera::setSERNameDirectory(const QString &filename, const QString &directory)
{
    INDI::PropertyText tvp = getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp.findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp.findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filenameT->setText(filename.toLatin1().data());
    dirT->setText(directory.toLatin1().data());

    sendNewProperty(tvp);

    return true;
}

bool Camera::getSERNameDirectory(QString &filename, QString &directory)
{
    INDI::PropertyText tvp = getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp.findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp.findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filename  = QString(filenameT->getText());
    directory = QString(dirT->getText());

    return true;
}

bool Camera::startRecording()
{
    INDI::PropertySwitch svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp.findWidgetByName("RECORD_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    svp.reset();
    recordON->setState(ISS_ON);

    sendNewProperty(svp);

    return true;
}

bool Camera::startDurationRecording(double duration)
{
    INDI::PropertyNumber nvp = getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto durationN = nvp.findWidgetByName("RECORD_DURATION");

    if (!durationN)
        return false;

    INDI::PropertySwitch svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp.findWidgetByName("RECORD_DURATION_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    durationN->setValue(duration);
    sendNewProperty(nvp);

    svp.reset();
    recordON->setState(ISS_ON);

    sendNewProperty(svp);

    return true;
}

bool Camera::startFramesRecording(uint32_t frames)
{
    INDI::PropertyNumber nvp = getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto frameN = nvp.findWidgetByName("RECORD_FRAME_TOTAL");
    INDI::PropertySwitch svp = getSwitch("RECORD_STREAM");

    if (!frameN || !svp)
        return false;

    auto recordON = svp.findWidgetByName("RECORD_FRAME_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    frameN->setValue(frames);
    sendNewProperty(nvp);

    svp.reset();
    recordON->setState(ISS_ON);

    sendNewProperty(svp);

    return true;
}

bool Camera::stopRecording()
{
    INDI::PropertySwitch svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordOFF = svp.findWidgetByName("RECORD_OFF");

    if (!recordOFF)
        return false;

    // If already set
    if (recordOFF->getState() == ISS_ON)
        return true;

    svp.reset();
    recordOFF->setState(ISS_ON);

    sendNewProperty(svp);

    return true;
}

bool Camera::setFITSHeaders(const QList<FITSData::Record> &values)
{
    INDI::PropertyText tvp = getText("FITS_HEADER");

    // Only proceed if FITS header has 3 fields introduced with INDI v2.0.1
    if (!tvp || tvp.count() < 3)
        return false;

    for (auto &record : values)
    {
        tvp[0].setText(record.key.toLatin1().constData());
        tvp[1].setText(record.value.toString().toLatin1().constData());
        tvp[2].setText(record.comment.toLatin1().constData());

        sendNewProperty(tvp);
    }

    return true;
}

bool Camera::setGain(double value)
{
    if (m_GainSource == GAIN_SOURCE_UNKNOWN || !m_GainProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_GainProperty);

    if (m_GainSource == GAIN_SOURCE_STANDALONE)
    {
        nvp[0].setValue(value);
    }
    else if (m_GainSource == GAIN_SOURCE_CONTROLS)
    {
        for (auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "gain" || label == "gain")
            {
                it.setValue(value);
                break;
            }
        }
    }

    sendNewProperty(m_GainProperty);
    return true;
}

bool Camera::getGain(double *value)
{
    if (m_GainSource == GAIN_SOURCE_UNKNOWN || !m_GainProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_GainProperty);

    if (m_GainSource == GAIN_SOURCE_STANDALONE)
    {
        *value = nvp[0].getValue();
        return true;
    }
    else if (m_GainSource == GAIN_SOURCE_CONTROLS)
    {
        for (const auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "gain" || label == "gain")
            {
                *value = it.getValue();
                return true;
            }
        }
    }

    return false;
}

bool Camera::getGainMinMaxStep(double *min, double *max, double *step)
{
    if (m_GainSource == GAIN_SOURCE_UNKNOWN || !m_GainProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_GainProperty);

    if (m_GainSource == GAIN_SOURCE_STANDALONE)
    {
        *min  = nvp[0].getMin();
        *max  = nvp[0].getMax();
        *step = nvp[0].getStep();
        return true;
    }
    else if (m_GainSource == GAIN_SOURCE_CONTROLS)
    {
        for (auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "gain" || label == "gain")
            {
                *min  = it.getMin();
                *max  = it.getMax();
                *step = it.getStep();
                return true;
            }
        }
    }

    return false;
}

bool Camera::setOffset(double value)
{
    if (m_OffsetSource == OFFSET_SOURCE_UNKNOWN || !m_OffsetProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_OffsetProperty);

    if (m_OffsetSource == OFFSET_SOURCE_STANDALONE)
    {
        nvp[0].setValue(value);
    }
    else if (m_OffsetSource == OFFSET_SOURCE_CONTROLS)
    {
        for (auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "offset" || label == "offset")
            {
                it.setValue(value);
                break;
            }
        }
    }

    sendNewProperty(m_OffsetProperty);
    return true;
}

bool Camera::getOffset(double *value)
{
    if (m_OffsetSource == OFFSET_SOURCE_UNKNOWN || !m_OffsetProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_OffsetProperty);

    if (m_OffsetSource == OFFSET_SOURCE_STANDALONE)
    {
        *value = nvp[0].getValue();
        return true;
    }
    else if (m_OffsetSource == OFFSET_SOURCE_CONTROLS)
    {
        for (const auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "offset" || label == "offset")
            {
                *value = it.getValue();
                return true;
            }
        }
    }

    return false;
}

bool Camera::getOffsetMinMaxStep(double *min, double *max, double *step)
{
    if (m_OffsetSource == OFFSET_SOURCE_UNKNOWN || !m_OffsetProperty.isValid())
        return false;

    auto nvp = INDI::PropertyNumber(m_OffsetProperty);
    if (m_OffsetSource == OFFSET_SOURCE_STANDALONE)
    {
        *min  = nvp[0].getMin();
        *max  = nvp[0].getMax();
        *step = nvp[0].getStep();
        return true;
    }
    else if (m_OffsetSource == OFFSET_SOURCE_CONTROLS)
    {
        for (auto &it : nvp)
        {
            QString name  = QString(it.getName()).toLower();
            QString label = QString(it.getLabel()).toLower();
            if (name == "offset" || label == "offset")
            {
                *min  = it.getMin();
                *max  = it.getMax();
                *step = it.getStep();
                return true;
            }
        }
    }

    return false;
}

bool Camera::isBLOBEnabled()
{
    return (m_Parent->getClientManager()->isBLOBEnabled(getDeviceName(), "CCD1"));
}

bool Camera::setBLOBEnabled(bool enable, const QString &prop)
{
    m_Parent->getClientManager()->setBLOBEnabled(enable, getDeviceName(), prop);

    return true;
}

bool Camera::setFastExposureEnabled(bool enable)
{
    // Set value immediately
    m_FastExposureEnabled = enable;

    INDI::PropertySwitch svp = getSwitch("CCD_FAST_TOGGLE");

    if (!svp)
        return false;

    svp[0].setState(enable ? ISS_ON : ISS_OFF);
    svp[1].setState(enable ? ISS_OFF : ISS_ON);
    sendNewProperty(svp);

    return true;
}

bool Camera::setCaptureFormat(const QString &format)
{
    INDI::PropertySwitch svp = getSwitch("CCD_CAPTURE_FORMAT");
    if (!svp)
        return false;

    svp.reset();
    for (auto &it : svp)
    {
        if (it.getLabel() == format)
        {
            it.setState(ISS_ON);
            break;
        }
    }

    sendNewProperty(svp);
    return true;
}

bool Camera::setFastCount(uint32_t count)
{
    INDI::PropertyNumber nvp = getNumber("CCD_FAST_COUNT");

    if (!nvp)
        return false;

    nvp[0].setValue(count);

    sendNewProperty(nvp);

    return true;
}

bool Camera::setStreamExposure(double duration)
{
    INDI::PropertyNumber nvp = getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    nvp[0].setValue(duration);

    sendNewProperty(nvp);

    return true;
}

bool Camera::getStreamExposure(double *duration)
{
    INDI::PropertyNumber nvp = getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    *duration = nvp[0].getValue();

    return true;
}

bool Camera::isCoolerOn()
{
    INDI::PropertySwitch svp = getSwitch("CCD_COOLER");

    if (!svp)
        return false;

    return (svp[0].getState() == ISS_ON);
}

bool Camera::getTemperatureRegulation(double &ramp, double &threshold)
{
    auto regulation = getProperty("CCD_TEMP_RAMP");
    if (!regulation.isValid())
        return false;

    ramp = INDI::PropertyNumber(regulation)[0].getValue();
    threshold = INDI::PropertyNumber(regulation)[1].getValue();
    return true;
}

bool Camera::setTemperatureRegulation(double ramp, double threshold)
{
    auto regulation = getProperty("CCD_TEMP_RAMP");
    if (!regulation.isValid())
        return false;

    INDI::PropertyNumber(regulation)[0].setValue(ramp);
    INDI::PropertyNumber(regulation)[1].setValue(threshold);
    sendNewProperty(INDI::PropertyNumber(regulation));
    return true;
}

bool Camera::setScopeInfo(double focalLength, double aperture)
{
    auto scopeInfo = getProperty("SCOPE_INFO");
    if (!scopeInfo.isValid())
        return false;

    INDI::PropertyNumber nvp = getNumber("SCOPE_INFO");
    nvp[0].setValue(focalLength);
    nvp[1].setValue(aperture);
    sendNewProperty(nvp);
    return true;
}

// Internal function to write an image blob to disk.
bool Camera::WriteImageFileInternal(const QString &filename, char *buffer, const size_t size)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly))
    {
        qCCritical(KSTARS_INDI) << "ISD:CCD Error: Unable to open write file: " <<
                                filename;
        return false;
    }
    int n = 0;
    QDataStream out(&file);
    bool ok = true;
    for (size_t nr = 0; nr < size; nr += n)
    {
        n = out.writeRawData(buffer + nr, size - nr);
        if (n < 0)
        {
            ok = false;
            break;
        }
    }
    ok = file.flush() && ok;
    file.close();
    file.setPermissions(QFileDevice::ReadUser |
                        QFileDevice::WriteUser |
                        QFileDevice::ReadGroup |
                        QFileDevice::ReadOther);
    return ok;
}

QString Camera::getCaptureFormat() const
{
    if (m_CaptureFormatIndex < 0 || m_CaptureFormats.isEmpty() || m_CaptureFormatIndex >= m_CaptureFormats.size())
        return QLatin1String("NA");

    return m_CaptureFormats[m_CaptureFormatIndex];
}
}
