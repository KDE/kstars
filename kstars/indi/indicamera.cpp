/*
    SPDX-FileCopyrightText: 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "indicamera.h"
#include "indicamerachip.h"

#include "config-kstars.h"

#include "indi_debug.h"

#include "clientmanager.h"
#include "driverinfo.h"
#include "guimanager.h"
#include "kspaths.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "streamwg.h"
//#include "ekos/manager.h"
#ifdef HAVE_CFITSIO
#include "fitsviewer/fitsdata.h"
#endif

#include <KNotifications/KNotification>
#include "auxiliary/ksmessagebox.h"
#include "ksnotification.h"
#include <QImageReader>
#include <QFileInfo>
#include <QStatusBar>
#include <QtConcurrent>

#include <basedevice.h>

const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw", "orf" };

const QString &getFITSModeStringString(FITSMode mode)
{
    return FITSModes[mode];
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
    if (!prop->getRegistered())
        return;

    if (getDeviceName() == device)
        emit newBLOBManager(prop);
}

void Camera::registerProperty(INDI::Property prop)
{
    if (prop->isNameMatch("GUIDER_EXPOSURE"))
    {
        HasGuideHead = true;
        guideChip.reset(new CameraChip(this, CameraChip::GUIDE_CCD));
    }
    else if (prop->isNameMatch("CCD_FRAME_TYPE"))
    {
        auto ccdFrame = prop->getSwitch();

        primaryChip->clearFrameTypes();

        for (const auto &it : *ccdFrame)
            primaryChip->addFrameLabel(it.getLabel());
    }
    else if (prop->isNameMatch("CCD_FRAME"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            primaryChip->setCanSubframe(true);
    }
    else if (prop->isNameMatch("GUIDER_FRAME"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            guideChip->setCanSubframe(true);
    }
    else if (prop->isNameMatch("CCD_BINNING"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            primaryChip->setCanBin(true);
    }
    else if (prop->isNameMatch("GUIDER_BINNING"))
    {
        auto np = prop->getNumber();
        if (np && np->getPermission() != IP_RO)
            guideChip->setCanBin(true);
    }
    else if (prop->isNameMatch("CCD_ABORT_EXPOSURE"))
    {
        auto sp = prop->getSwitch();
        if (sp && sp->getPermission() != IP_RO)
            primaryChip->setCanAbort(true);
    }
    else if (prop->isNameMatch("GUIDER_ABORT_EXPOSURE"))
    {
        auto sp = prop->getSwitch();
        if (sp && sp->getPermission() != IP_RO)
            guideChip->setCanAbort(true);
    }
    else if (prop->isNameMatch("CCD_TEMPERATURE"))
    {
        auto np = prop->getNumber();
        HasCooler = true;
        CanCool   = (np->getPermission() != IP_RO);
        if (np)
            emit newTemperatureValue(np->at(0)->getValue());
    }
    else if (prop->isNameMatch("CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
    }
    else if (prop->isNameMatch("CCD_VIDEO_STREAM"))
    {
        // Has Video Stream
        HasVideoStream = true;
    }
    else if (prop->isNameMatch("CCD_CAPTURE_FORMAT"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            m_CaptureFormats.clear();
            for (const auto &oneSwitch : *sp)
                m_CaptureFormats << oneSwitch.getLabel();

            m_CaptureFormatIndex = sp->findOnSwitchIndex();
        }
    }
    else if (prop->isNameMatch("CCD_TRANSFER_FORMAT"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            m_EncodingFormats.clear();
            for (const auto &oneSwitch : *sp)
                m_EncodingFormats << oneSwitch.getLabel();

            auto format = sp->findOnSwitch();
            if (format)
                m_EncodingFormat = format->label;
        }
    }
    else if (prop->isNameMatch("CCD_EXPOSURE_PRESETS"))
    {
        auto svp = prop->getSwitch();
        if (svp)
        {
            bool ok = false;
            for (const auto &it : *svp)
            {
                QString key = QString(it.getLabel());
                double value = key.toDouble(&ok);
                if (!ok)
                {
                    QStringList parts = key.split("/");
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
                m_ExposurePresetsMinMax = qMakePair<double, double>(min, max);
            }
        }
    }
    else if (prop->isNameMatch("CCD_FAST_TOGGLE"))
    {
        auto sp = prop->getSwitch();
        if (sp)
            m_FastExposureEnabled = sp->findOnSwitchIndex() == 0;
        else
            m_FastExposureEnabled = false;
    }
    else if (prop->isNameMatch("TELESCOPE_TYPE"))
    {
        auto sp = prop->getSwitch();
        if (sp)
        {
            auto format = sp->findWidgetByName("TELESCOPE_PRIMARY");
            if (format && format->getState() == ISS_ON)
                telescopeType = TELESCOPE_PRIMARY;
            else
                telescopeType = TELESCOPE_GUIDE;
        }
    }
    else if (prop->isNameMatch("CCD_WEBSOCKET_SETTINGS"))
    {
        auto np = prop->getNumber();
        m_Media->setURL(QUrl(QString("ws://%1:%2").arg(m_Parent->getClientManager()->getHost()).arg(np->at(0)->getValue())));
        m_Media->connectServer();
    }
    else if (prop->isNameMatch("CCD1"))
    {
        IBLOBVectorProperty *bp = prop->getBLOB();
        primaryCCDBLOB = bp->bp;
        primaryCCDBLOB->bvp = bp;
    }
    // try to find gain and/or offset property, if any
    else if ( (gainN == nullptr || offsetN == nullptr) && prop->getType() == INDI_NUMBER)
    {
        // Since gain is spread among multiple property depending on the camera providing it
        // we need to search in all possible number properties
        auto controlNP = prop->getNumber();
        if (controlNP)
        {
            for (auto &it : *controlNP)
            {
                QString name  = QString(it.getName()).toLower();
                QString label = QString(it.getLabel()).toLower();

                if (name == "gain" || label == "gain")
                {
                    gainN = &it;
                    gainPerm = controlNP->getPermission();
                }
                else if (name == "offset" || label == "offset")
                {
                    offsetN = &it;
                    offsetPerm = controlNP->getPermission();
                }
            }
        }
    }

    ConcreteDevice::registerProperty(prop);
}

void Camera::removeProperty(const QString &name)
{
    if (name == "CCD_WEBSOCKET_SETTINGS")
    {
        m_Media->disconnectServer();
    }
}

void Camera::processNumber(INumberVectorProperty *nvp)
{
    if (!strcmp(nvp->name, "CCD_EXPOSURE"))
    {
        INumber *np = IUFindNumber(nvp, "CCD_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(primaryChip.get(), np->value, nvp->s);
        if (nvp->s == IPS_ALERT)
            emit error(ERROR_CAPTURE);
    }
    else if (!strcmp(nvp->name, "CCD_TEMPERATURE"))
    {
        HasCooler   = true;
        INumber *np = IUFindNumber(nvp, "CCD_TEMPERATURE_VALUE");
        if (np)
            emit newTemperatureValue(np->value);
    }
    else if (!strcmp(nvp->name, "GUIDER_EXPOSURE"))
    {
        INumber *np = IUFindNumber(nvp, "GUIDER_EXPOSURE_VALUE");
        if (np)
            emit newExposureValue(guideChip.get(), np->value, nvp->s);
    }
    else if (!strcmp(nvp->name, "FPS"))
    {
        emit newFPS(nvp->np[0].value, nvp->np[1].value);
    }
    else if (!strcmp(nvp->name, "CCD_RAPID_GUIDE_DATA"))
    {
        double dx = -1, dy = -1, fit = -1;
        INumber *np = nullptr;

        if (nvp->s == IPS_ALERT)
        {
            emit newGuideStarData(primaryChip.get(), -1, -1, -1);
        }
        else
        {
            np = IUFindNumber(nvp, "GUIDESTAR_X");
            if (np)
                dx = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_Y");
            if (np)
                dy = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_FIT");
            if (np)
                fit = np->value;

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(primaryChip.get(), dx, dy, fit);
        }
    }
    else if (!strcmp(nvp->name, "GUIDER_RAPID_GUIDE_DATA"))
    {
        double dx = -1, dy = -1, fit = -1;
        INumber *np = nullptr;

        if (nvp->s == IPS_ALERT)
        {
            emit newGuideStarData(guideChip.get(), -1, -1, -1);
        }
        else
        {
            np = IUFindNumber(nvp, "GUIDESTAR_X");
            if (np)
                dx = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_Y");
            if (np)
                dy = np->value;
            np = IUFindNumber(nvp, "GUIDESTAR_FIT");
            if (np)
                fit = np->value;

            if (dx >= 0 && dy >= 0 && fit >= 0)
                emit newGuideStarData(guideChip.get(), dx, dy, fit);
        }
    }
}

void Camera::processSwitch(ISwitchVectorProperty *svp)
{
    if (!strcmp(svp->name, "CCD_COOLER"))
    {
        // Can turn cooling on/off
        HasCoolerControl = true;
        emit coolerToggled(svp->sp[0].s == ISS_ON);
    }
    else if (QString(svp->name).endsWith("VIDEO_STREAM"))
    {
        // If BLOB is not enabled for this camera, then ignore all VIDEO_STREAM calls.
        if (isBLOBEnabled() == false)
            return;

        HasVideoStream = true;

        if (!streamWindow && svp->sp[0].s == ISS_ON)
        {
            streamWindow.reset(new StreamWG(this));

            INumberVectorProperty *streamFrame = getNumber("CCD_STREAM_FRAME");
            INumber *w = nullptr, *h = nullptr;

            if (streamFrame)
            {
                w = IUFindNumber(streamFrame, "WIDTH");
                h = IUFindNumber(streamFrame, "HEIGHT");
            }

            if (w && h)
            {
                streamW = w->value;
                streamH = h->value;
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

            streamWindow->setSize(streamW, streamH);
        }

        if (streamWindow)
        {
            connect(streamWindow.get(), &StreamWG::hidden, this, &Camera::StreamWindowHidden, Qt::UniqueConnection);
            connect(streamWindow.get(), &StreamWG::imageChanged, this, &Camera::newVideoFrame, Qt::UniqueConnection);

            streamWindow->enableStream(svp->sp[0].s == ISS_ON);
            emit videoStreamToggled(svp->sp[0].s == ISS_ON);
        }
    }
    else if (!strcmp(svp->name, "CCD_CAPTURE_FORMAT"))
    {
        m_CaptureFormats.clear();
        for (int i = 0; i < svp->nsp; i++)
        {
            m_CaptureFormats << svp->sp[i].label;
            if (svp->sp[i].s == ISS_ON)
                m_CaptureFormatIndex = i;
        }
    }
    else if (!strcmp(svp->name, "CCD_TRANSFER_FORMAT"))
    {
        ISwitch *format = IUFindOnSwitch(svp);
        if (format)
            m_EncodingFormat = format->label;
    }
    else if (!strcmp(svp->name, "RECORD_STREAM"))
    {
        ISwitch *recordOFF = IUFindSwitch(svp, "RECORD_OFF");

        if (recordOFF && recordOFF->s == ISS_ON)
        {
            emit videoRecordToggled(false);
            KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Stopped"), KSNotification::EVENT_INFO);
        }
        else
        {
            emit videoRecordToggled(true);
            KSNotification::event(QLatin1String("IndiServerMessage"), i18n("Video Recording Started"), KSNotification::EVENT_INFO);
        }
    }
    else if (!strcmp(svp->name, "TELESCOPE_TYPE"))
    {
        ISwitch *format = IUFindSwitch(svp, "TELESCOPE_PRIMARY");
        if (format && format->s == ISS_ON)
            telescopeType = TELESCOPE_PRIMARY;
        else
            telescopeType = TELESCOPE_GUIDE;
    }
    else if (!strcmp(svp->name, "CCD_FAST_TOGGLE"))
    {
        m_FastExposureEnabled = IUFindOnSwitchIndex(svp) == 0;
    }
    else if (!strcmp(svp->name, "CONNECTION"))
    {
        ISwitch *dSwitch = IUFindSwitch(svp, "DISCONNECT");

        if (dSwitch && dSwitch->s == ISS_ON)
        {
            if (streamWindow)
            {
                streamWindow->enableStream(false);
                emit videoStreamToggled(false);
                streamWindow->close();
                streamWindow.reset();
            }

            // Clear the pointers on disconnect.
            gainN = nullptr;
            offsetN = nullptr;
            primaryCCDBLOB = nullptr;
        }
    }
}

void Camera::processText(ITextVectorProperty *tvp)
{
    if (!strcmp(tvp->name, "CCD_FILE_PATH"))
    {
        IText *filepath = IUFindText(tvp, "FILE_PATH");
        if (filepath)
            emit newRemoteFile(QString(filepath->text));
    }
}

void Camera::setWSBLOB(const QByteArray &message, const QString &extension)
{
    if (!primaryCCDBLOB)
        return;

    primaryCCDBLOB->blob = const_cast<char *>(message.data());
    primaryCCDBLOB->size = message.size();
    strncpy(primaryCCDBLOB->format, extension.toLatin1().constData(), MAXINDIFORMAT - 1);
    processBLOB(primaryCCDBLOB);

    // Disassociate
    primaryCCDBLOB->blob = nullptr;
}

void Camera::processStream(IBLOB *bp)
{
    if (!streamWindow || streamWindow->isStreamEnabled() == false)
        return;

    INumberVectorProperty *streamFrame = getNumber("CCD_STREAM_FRAME");
    INumber *w = nullptr, *h = nullptr;

    if (streamFrame)
    {
        w = IUFindNumber(streamFrame, "WIDTH");
        h = IUFindNumber(streamFrame, "HEIGHT");
    }

    if (w && h)
    {
        streamW = w->value;
        streamH = h->value;
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

    streamWindow->setSize(streamW, streamH);

    streamWindow->show();
    streamWindow->newFrame(bp);
}

bool Camera::generateFilename(bool batch_mode, const QString &extension, QString *filename)
{

    placeholderPath.generateFilename("%p1/%t/%T/%F/%t_%T_%F_%e_%D_%s3", ISOMode,
                                     batch_mode, nextSequenceID, extension, filename);

    QDir currentDir = QFileInfo(*filename).dir();
    if (currentDir.exists() == false)
        QDir().mkpath(currentDir.path());

    QFile test_file(*filename);
    if (!test_file.open(QIODevice::WriteOnly))
        return false;
    test_file.flush();
    test_file.close();
    return true;
}

bool Camera::writeImageFile(const QString &filename, IBLOB *bp, bool is_fits)
{
    // TODO: Not yet threading the writes for non-fits files.
    // Would need to deal with the raw conversion, etc.
    if (is_fits)
    {
        // Check if the last write is still ongoing, and if so wait.
        // It is using the fileWriteBuffer.
        if (fileWriteThread.isRunning())
        {
            fileWriteThread.waitForFinished();
        }

        // Wait until the file is written before overwritting the filename.
        fileWriteFilename = filename;

        // Will write blob data in a separate thread, and can't depend on the blob
        // memory, so copy it first.

        // Check buffer size.
        if (fileWriteBufferSize != bp->size)
        {
            if (fileWriteBuffer != nullptr)
                delete [] fileWriteBuffer;
            fileWriteBufferSize = bp->size;
            fileWriteBuffer = new char[fileWriteBufferSize];
        }

        // Copy memory, and write file on a separate thread.
        // Probably too late to return an error if the file couldn't write.
        memcpy(fileWriteBuffer, bp->blob, bp->size);
        fileWriteThread = QtConcurrent::run(this, &ISD::Camera::WriteImageFileInternal, fileWriteFilename, fileWriteBuffer,
                                            bp->size);
    }
    else
    {
        if (!WriteImageFileInternal(filename, static_cast<char*>(bp->blob), bp->size))
            return false;
    }
    return true;
}

// Get or Create FITSViewer if we are using FITSViewer
// or if capture mode is calibrate since for now we are forced to open the file in the viewer
// this should be fixed in the future and should only use FITSData
QPointer<FITSViewer> Camera::getFITSViewer()
{
    // if the FITS viewer exists, return it
    if (m_FITSViewerWindow != nullptr && ! m_FITSViewerWindow.isNull())
        return m_FITSViewerWindow;

    // otherwise, create it
    normalTabID = calibrationTabID = focusTabID = guideTabID = alignTabID = -1;

    m_FITSViewerWindow = KStars::Instance()->createFITSViewer();

    // Check if ONE tab of the viewer was closed.
    connect(m_FITSViewerWindow, &FITSViewer::closed, this, [this](int tabIndex)
    {
        if (tabIndex == normalTabID)
            normalTabID = -1;
        else if (tabIndex == calibrationTabID)
            calibrationTabID = -1;
        else if (tabIndex == focusTabID)
            focusTabID = -1;
        else if (tabIndex == guideTabID)
            guideTabID = -1;
        else if (tabIndex == alignTabID)
            alignTabID = -1;
    });

    // If FITS viewer was completed closed. Reset everything
    connect(m_FITSViewerWindow, &FITSViewer::destroyed, this, [this]()
    {
        normalTabID = -1;
        calibrationTabID = -1;
        focusTabID = -1;
        guideTabID = -1;
        alignTabID = -1;
        m_FITSViewerWindow.clear();
    });

    return m_FITSViewerWindow;
}

bool Camera::processBLOB(IBLOB *bp)
{
    // Ignore write-only BLOBs since we only receive it for state-change
    if (bp->bvp->p == IP_WO || bp->size == 0)
        return false;

    BType = BLOB_OTHER;

    QString format = QString(bp->format).toLower();

    // If stream, process it first
    if (format.contains("stream") && streamWindow)
    {
        processStream(bp);
        return true;
    }

    // Format without leading . (.jpg --> jpg)
    QString shortFormat = format.mid(1);

    // If it's not FITS or an image, don't process it.
    if ((QImageReader::supportedImageFormats().contains(shortFormat.toLatin1())))
        BType = BLOB_IMAGE;
    else if (format.contains("fits"))
        BType = BLOB_FITS;
    else if (RAWFormats.contains(shortFormat))
        BType = BLOB_RAW;

    if (BType == BLOB_OTHER)
    {
        emit newImage(nullptr);
        return false;
    }

    CameraChip *targetChip = nullptr;

    if (!strcmp(bp->name, "CCD2"))
        targetChip = guideChip.get();
    else
    {
        targetChip = primaryChip.get();
        qCDebug(KSTARS_INDI) << "Image received. Mode:" << getFITSModeStringString(targetChip->getCaptureMode()) << "Size:" <<
                             bp->size;
    }

    // Create temporary name if ANY of the following conditions are met:
    // 1. file is preview or batch mode is not enabled
    // 2. file type is not FITS_NORMAL (focus, guide..etc)
    QString filename;
#if 0

    if (targetChip->isBatchMode() == false || targetChip->getCaptureMode() != FITS_NORMAL)
    {
        if (!writeTempImageFile(format, static_cast<char *>(bp->blob), bp->size, &filename))
        {
            emit BLOBUpdated(nullptr);
            return;
        }
        if (BType == BLOB_FITS)
            addFITSKeywords(filename, filter);

    }
#endif
    // Create file name for sequences.
    if (targetChip->isBatchMode() && targetChip->getCaptureMode() != FITS_CALIBRATE)
    {
        // If either generating file name or writing the image file fails
        // then return
        if (!generateFilename(targetChip->isBatchMode(), format, &filename) ||
                !writeImageFile(filename, bp, BType == BLOB_FITS))
        {
            connect(KSMessageBox::Instance(), &KSMessageBox::accepted, this, [ = ]()
            {
                KSMessageBox::Instance()->disconnect(this);
                emit error(ERROR_SAVE);
            });
            KSMessageBox::Instance()->error(i18n("Failed writing image to %1\nPlease check folder, filename & permissions.",
                                                 filename),
                                            i18n("Image Write Failed"), 30);

            emit BLOBUpdated(nullptr);
            return true;
        }
    }
    else
        filename = QDir::tempPath() + QDir::separator() + "image" + format;

    if (targetChip->getCaptureMode() == FITS_NORMAL && targetChip->isBatchMode() == true)
    {
        KStars::Instance()->statusBar()->showMessage(i18n("%1 file saved to %2", shortFormat.toUpper(), filename), 0);
        qCInfo(KSTARS_INDI) << shortFormat.toUpper() << "file saved to" << filename;
    }

    // Don't spam, just one notification per 3 seconds
    if (QDateTime::currentDateTime().secsTo(m_LastNotificationTS) <= -3)
    {
        KNotification::event(QLatin1String("FITSReceived"), i18n("Image file is received"));
        m_LastNotificationTS = QDateTime::currentDateTime();
    }

    // Check if we need to process RAW or regular image. Anything but FITS.
#if 0
    if (BType == BLOB_IMAGE || BType == BLOB_RAW)
    {
        bool useFITSViewer = Options::autoImageToFITS() &&
                             (Options::useFITSViewer() || (Options::useDSLRImageViewer() == false && targetChip->isBatchMode() == false));
        bool useDSLRViewer = (Options::useDSLRImageViewer() || targetChip->isBatchMode() == false);
        // For raw image, we only process them to JPG if we need to open them in the image viewer
        if (BType == BLOB_RAW && (useFITSViewer || useDSLRViewer))
        {
            QString rawFileName  = filename;
            rawFileName          = rawFileName.remove(0, rawFileName.lastIndexOf(QLatin1String("/")));

            QString templateName = QString("%1/%2.XXXXXX").arg(QDir::tempPath(), rawFileName);
            QTemporaryFile imgPreview(templateName);

            imgPreview.setAutoRemove(false);
            imgPreview.open();
            imgPreview.close();
            QString preview_filename = imgPreview.fileName();
            QString errorMessage;

            if (KSUtils::RAWToJPEG(filename, preview_filename, errorMessage) == false)
            {
                KStars::Instance()->statusBar()->showMessage(errorMessage);
                emit BLOBUpdated(bp);
                return;
            }

            // Remove tempeorary CR2 files
            if (targetChip->isBatchMode() == false)
                QFile::remove(filename);

            filename = preview_filename;
            format = ".jpg";
            shortFormat = "jpg";
        }

        // Convert to FITS if checked.
        QString output;
        if (useFITSViewer && (FITSData::ImageToFITS(filename, shortFormat, output)))
        {
            if (BType == BLOB_RAW || targetChip->isBatchMode() == false)
                QFile::remove(filename);

            filename = output;
            BType = BLOB_FITS;

            emit previewFITSGenerated(output);

            FITSData *blob_fits_data = new FITSData(targetChip->getCaptureMode());

            QFuture<bool> fitsloader = blob_fits_data->loadFromFile(filename, false);
            fitsloader.waitForFinished();
            if (!fitsloader.result())
            {
                // If reading the blob fails, we treat it the same as exposure failure
                // and recapture again if possible
                delete (blob_fits_data);
                qCCritical(KSTARS_INDI) << "failed reading FITS memory buffer";
                emit newExposureValue(targetChip, 0, IPS_ALERT);
                return;
            }
            displayFits(targetChip, filename, bp, blob_fits_data);
            return;
        }
        else if (useDSLRViewer)
        {
            if (m_ImageViewerWindow.isNull())
                m_ImageViewerWindow = new ImageViewer(getDeviceName(), KStars::Instance());

            m_ImageViewerWindow->loadImage(filename);

            emit previewJPEGGenerated(filename, m_ImageViewerWindow->metadata());
        }
    }
#endif

    // Load FITS if either:
    // #1 FITS Viewer is set to enabled.
    // #2 This is a preview, so we MUST open FITS Viewer even if disabled.
    //    if (BType == BLOB_FITS)
    //    {
    // Don't display image if the following conditions are met:
    // 1. Mode is NORMAL or CALIBRATE; and
    // 2. FITS Viewer is disabled; and
    // 3. Batch mode is enabled.
    // 4. Summary view is false.
    if (targetChip->getCaptureMode() == FITS_NORMAL &&
            Options::useFITSViewer() == false &&
            Options::useSummaryPreview() == false &&
            targetChip->isBatchMode())
    {
        emit BLOBUpdated(bp);
        emit newImage(nullptr);
        return true;
    }

    QByteArray buffer = QByteArray::fromRawData(reinterpret_cast<char *>(bp->blob), bp->size);
    QSharedPointer<FITSData> imageData;
    imageData.reset(new FITSData(targetChip->getCaptureMode()), &QObject::deleteLater);
    if (!imageData->loadFromBuffer(buffer, shortFormat, filename))
    {
        emit error(ERROR_LOAD);
        return true;
    }

    handleImage(targetChip, filename, bp, imageData);
    return true;
}

void Camera::handleImage(CameraChip *targetChip, const QString &filename, IBLOB *bp, QSharedPointer<FITSData> data)
{
    FITSMode captureMode = targetChip->getCaptureMode();

    // Add metadata
    data->setProperty("device", getDeviceName());
    data->setProperty("blobVector", bp->bvp->name);
    data->setProperty("blobElement", bp->name);
    data->setProperty("chip", targetChip->getType());
    // Retain a copy
    targetChip->setImageData(data);

    switch (captureMode)
    {
        case FITS_NORMAL:
        case FITS_CALIBRATE:
        {
            if (Options::useFITSViewer())
            {
                // No need to wait until the image is loaded in the view, but emit AFTER checking
                // batch mode, since newImage() may change it
                emit BLOBUpdated(bp);
                emit newImage(data);

                bool success = false;
                int tabIndex = -1;
                int *tabID = &normalTabID;
                QUrl fileURL = QUrl::fromLocalFile(filename);
                FITSScale captureFilter = targetChip->getCaptureFilter();
                if (*tabID == -1 || Options::singlePreviewFITS() == false)
                {
                    // If image is preview and we should display all captured images in a
                    // single tab called "Preview", then set the title to "Preview",
                    // Otherwise, the title will be the captured image name
                    QString previewTitle;
                    if (Options::singlePreviewFITS())
                    {
                        // If we are displaying all images from all cameras in a single FITS
                        // Viewer window, then we prefix the camera name to the "Preview" string
                        if (Options::singleWindowCapturedFITS())
                            previewTitle = i18n("%1 Preview", getDeviceName());
                        else
                            // Otherwise, just use "Preview"
                            previewTitle = i18n("Preview");
                    }

                    success = getFITSViewer()->loadData(data, fileURL, &tabIndex, captureMode, captureFilter, previewTitle);
                }
                else
                    success = getFITSViewer()->updateData(data, fileURL, *tabID, &tabIndex, captureFilter, captureMode);

                if (!success)
                {
                    // If opening file fails, we treat it the same as exposure failure
                    // and recapture again if possible
                    qCCritical(KSTARS_INDI) << "error adding/updating FITS";
                    emit error(ERROR_VIEWER);
                    return;
                }
                *tabID = tabIndex;
                if (Options::focusFITSOnNewImage())
                    getFITSViewer()->raise();

                return;
            }
        }
        break;
        default:
            break;
    }

    emit BLOBUpdated(bp);
    emit newImage(data);
}

void Camera::StreamWindowHidden()
{
    if (isConnected())
    {
        // We can have more than one *_VIDEO_STREAM property active so disable them all
        auto streamSP = getSwitch("CCD_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            sendNewSwitch(streamSP);
        }

        streamSP = getSwitch("VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            sendNewSwitch(streamSP);
        }

        streamSP = getSwitch("AUX_VIDEO_STREAM");
        if (streamSP)
        {
            streamSP->reset();
            streamSP->at(0)->setState(ISS_OFF);
            streamSP->at(1)->setState(ISS_ON);
            streamSP->setState(IPS_IDLE);
            sendNewSwitch(streamSP);
        }
    }

    if (streamWindow)
        streamWindow->disconnect();
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
    auto coolerON  = coolerSP->findWidgetByName("COOLER_ON");
    auto coolerOFF = coolerSP->findWidgetByName("COOLER_OFF");
    if (!coolerON || !coolerOFF)
        return false;

    coolerON->setState(enable ? ISS_ON : ISS_OFF);
    coolerOFF->setState(enable ? ISS_OFF : ISS_ON);
    sendNewSwitch(coolerSP);

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

bool Camera::setRapidGuide(CameraChip *targetChip, bool enable)
{
    ISwitchVectorProperty *rapidSP = nullptr;
    ISwitch *enableS               = nullptr;

    if (targetChip == primaryChip.get())
        rapidSP = getSwitch("CCD_RAPID_GUIDE");
    else
        rapidSP = getSwitch("GUIDER_RAPID_GUIDE");

    if (rapidSP == nullptr)
        return false;

    enableS = IUFindSwitch(rapidSP, "ENABLE");

    if (enableS == nullptr)
        return false;

    // Already updated, return OK
    if ((enable && enableS->s == ISS_ON) || (!enable && enableS->s == ISS_OFF))
        return true;

    IUResetSwitch(rapidSP);
    rapidSP->sp[0].s = enable ? ISS_ON : ISS_OFF;
    rapidSP->sp[1].s = enable ? ISS_OFF : ISS_ON;

    sendNewSwitch(rapidSP);

    return true;
}

bool Camera::configureRapidGuide(CameraChip *targetChip, bool autoLoop, bool sendImage, bool showMarker)
{
    ISwitchVectorProperty *rapidSP = nullptr;
    ISwitch *autoLoopS = nullptr, *sendImageS = nullptr, *showMarkerS = nullptr;

    if (targetChip == primaryChip.get())
        rapidSP = getSwitch("CCD_RAPID_GUIDE_SETUP");
    else
        rapidSP = getSwitch("GUIDER_RAPID_GUIDE_SETUP");

    if (rapidSP == nullptr)
        return false;

    autoLoopS   = IUFindSwitch(rapidSP, "AUTO_LOOP");
    sendImageS  = IUFindSwitch(rapidSP, "SEND_IMAGE");
    showMarkerS = IUFindSwitch(rapidSP, "SHOW_MARKER");

    if (!autoLoopS || !sendImageS || !showMarkerS)
        return false;

    // If everything is already set, let's return.
    if (((autoLoop && autoLoopS->s == ISS_ON) || (!autoLoop && autoLoopS->s == ISS_OFF)) &&
            ((sendImage && sendImageS->s == ISS_ON) || (!sendImage && sendImageS->s == ISS_OFF)) &&
            ((showMarker && showMarkerS->s == ISS_ON) || (!showMarker && showMarkerS->s == ISS_OFF)))
        return true;

    autoLoopS->s   = autoLoop ? ISS_ON : ISS_OFF;
    sendImageS->s  = sendImage ? ISS_ON : ISS_OFF;
    showMarkerS->s = showMarker ? ISS_ON : ISS_OFF;

    sendNewSwitch(rapidSP);

    return true;
}

void Camera::updateUploadSettings(const QString &remoteDir)
{
    QString filename = seqPrefix + (seqPrefix.isEmpty() ? "" : "_") + QString("XXX");

    ITextVectorProperty *uploadSettingsTP = nullptr;
    IText *uploadT                        = nullptr;

    uploadSettingsTP = getText("UPLOAD_SETTINGS");
    if (uploadSettingsTP)
    {
        uploadT = IUFindText(uploadSettingsTP, "UPLOAD_DIR");
        if (uploadT && remoteDir.isEmpty() == false)
            IUSaveText(uploadT, remoteDir.toLatin1().constData());

        uploadT = IUFindText(uploadSettingsTP, "UPLOAD_PREFIX");
        if (uploadT)
            IUSaveText(uploadT, filename.toLatin1().constData());

        sendNewText(uploadSettingsTP);
    }
}

Camera::UploadMode Camera::getUploadMode()
{
    ISwitchVectorProperty *uploadModeSP = nullptr;

    uploadModeSP = getSwitch("UPLOAD_MODE");

    if (uploadModeSP == nullptr)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return UPLOAD_CLIENT;
    }

    if (uploadModeSP)
    {
        ISwitch *modeS = nullptr;

        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_CLIENT");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_CLIENT;
        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_LOCAL");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_LOCAL;
        modeS = IUFindSwitch(uploadModeSP, "UPLOAD_BOTH");
        if (modeS && modeS->s == ISS_ON)
            return UPLOAD_BOTH;
    }

    // Default
    return UPLOAD_CLIENT;
}

bool Camera::setUploadMode(UploadMode mode)
{
    ISwitch *modeS = nullptr;

    auto uploadModeSP = getSwitch("UPLOAD_MODE");

    if (!uploadModeSP)
    {
        qWarning() << "No UPLOAD_MODE in CCD driver. Please update driver to INDI compliant CCD driver.";
        return false;
    }

    switch (mode)
    {
        case UPLOAD_CLIENT:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_CLIENT");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;

        case UPLOAD_BOTH:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_BOTH");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;

        case UPLOAD_LOCAL:
            modeS = uploadModeSP->findWidgetByName("UPLOAD_LOCAL");
            if (!modeS)
                return false;
            if (modeS->s == ISS_ON)
                return true;
            break;
    }

    uploadModeSP->reset();
    modeS->s = ISS_ON;

    sendNewSwitch(uploadModeSP);

    return true;
}

bool Camera::getTemperature(double *value)
{
    if (HasCooler == false)
        return false;

    auto temperatureNP = getNumber("CCD_TEMPERATURE");

    if (!temperatureNP)
        return false;

    *value = temperatureNP->at(0)->getValue();

    return true;
}

bool Camera::setTemperature(double value)
{
    auto nvp = getNumber("CCD_TEMPERATURE");

    if (!nvp)
        return false;

    auto np = nvp->findWidgetByName("CCD_TEMPERATURE_VALUE");

    if (!np)
        return false;

    np->setValue(value);

    sendNewNumber(nvp);

    return true;
}

bool Camera::setEncodingFormat(const QString &value)
{
    if (value == m_EncodingFormat)
        return true;

    auto svp = getSwitch("CCD_TRANSFER_FORMAT");

    if (!svp)
        return false;

    svp->reset();
    for (int i = 0; i < svp->nsp; i++)
    {
        if (svp->at(i)->getLabel() == value)
        {
            svp->at(i)->setState(ISS_ON);
            break;
        }
    }

    m_EncodingFormat = value;
    sendNewSwitch(svp);
    return true;
}

bool Camera::setTelescopeType(TelescopeType type)
{
    if (type == telescopeType)
        return true;

    auto svp = getSwitch("TELESCOPE_TYPE");

    if (!svp)
        return false;

    auto typePrimary = svp->findWidgetByName("TELESCOPE_PRIMARY");
    auto typeGuide   = svp->findWidgetByName("TELESCOPE_GUIDE");

    if (!typePrimary || !typeGuide)
        return false;

    telescopeType = type;

    if ( (telescopeType == TELESCOPE_PRIMARY && typePrimary->getState() == ISS_OFF) ||
            (telescopeType == TELESCOPE_GUIDE && typeGuide->getState() == ISS_OFF))
    {
        typePrimary->setState(telescopeType == TELESCOPE_PRIMARY ? ISS_ON : ISS_OFF);
        typeGuide->setState(telescopeType == TELESCOPE_PRIMARY ? ISS_OFF : ISS_ON);
        sendNewSwitch(svp);
        setConfig(SAVE_CONFIG);
    }

    return true;
}

bool Camera::setVideoStreamEnabled(bool enable)
{
    if (HasVideoStream == false)
        return false;

    auto svp = getSwitch("CCD_VIDEO_STREAM");

    if (!svp)
        return false;

    // If already on and enable is set or vice versa no need to change anything we return true
    if ((enable && svp->at(0)->getState() == ISS_ON) || (!enable && svp->at(1)->getState() == ISS_ON))
        return true;

    svp->at(0)->setState(enable ? ISS_ON : ISS_OFF);
    svp->at(1)->setState(enable ? ISS_OFF : ISS_ON);

    sendNewSwitch(svp);

    return true;
}

bool Camera::resetStreamingFrame()
{
    auto frameProp = getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp->findWidgetByName("X");
    auto yarg = frameProp->findWidgetByName("Y");
    auto warg = frameProp->findWidgetByName("WIDTH");
    auto harg = frameProp->findWidgetByName("HEIGHT");

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

        sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool Camera::setStreamLimits(uint16_t maxBufferSize, uint16_t maxPreviewFPS)
{
    auto limitsProp = getNumber("LIMITS");

    if (!limitsProp)
        return false;

    auto bufferMax = limitsProp->findWidgetByName("LIMITS_BUFFER_MAX");
    auto previewFPS = limitsProp->findWidgetByName("LIMITS_PREVIEW_FPS");

    if (bufferMax && previewFPS)
    {
        if(std::fabs(bufferMax->getValue() - maxBufferSize) > 0 || std::fabs(previewFPS->getValue() - maxPreviewFPS) > 0)
        {
            bufferMax->setValue(maxBufferSize);
            previewFPS->setValue(maxPreviewFPS);
            sendNewNumber(limitsProp);
        }

        return true;
    }

    return false;
}

bool Camera::setStreamingFrame(int x, int y, int w, int h)
{
    auto frameProp = getNumber("CCD_STREAM_FRAME");

    if (!frameProp)
        return false;

    auto xarg = frameProp->findWidgetByName("X");
    auto yarg = frameProp->findWidgetByName("Y");
    auto warg = frameProp->findWidgetByName("WIDTH");
    auto harg = frameProp->findWidgetByName("HEIGHT");

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

        sendNewNumber(frameProp);
        return true;
    }

    return false;
}

bool Camera::isStreamingEnabled()
{
    if (HasVideoStream == false || !streamWindow)
        return false;

    return streamWindow->isStreamEnabled();
}

bool Camera::setSERNameDirectory(const QString &filename, const QString &directory)
{
    auto tvp = getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp->findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp->findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filenameT->setText(filename.toLatin1().data());
    dirT->setText(directory.toLatin1().data());

    sendNewText(tvp);

    return true;
}

bool Camera::getSERNameDirectory(QString &filename, QString &directory)
{
    auto tvp = getText("RECORD_FILE");

    if (!tvp)
        return false;

    auto filenameT = tvp->findWidgetByName("RECORD_FILE_NAME");
    auto dirT      = tvp->findWidgetByName("RECORD_FILE_DIR");

    if (!filenameT || !dirT)
        return false;

    filename  = QString(filenameT->getText());
    directory = QString(dirT->getText());

    return true;
}

bool Camera::startRecording()
{
    auto svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    svp->reset();
    recordON->setState(ISS_ON);

    sendNewSwitch(svp);

    return true;
}

bool Camera::startDurationRecording(double duration)
{
    auto nvp = getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto durationN = nvp->findWidgetByName("RECORD_DURATION");

    if (!durationN)
        return false;

    auto svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_DURATION_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    durationN->setValue(duration);
    sendNewNumber(nvp);

    svp->reset();
    recordON->setState(ISS_ON);

    sendNewSwitch(svp);

    return true;
}

bool Camera::startFramesRecording(uint32_t frames)
{
    auto nvp = getNumber("RECORD_OPTIONS");

    if (!nvp)
        return false;

    auto frameN = nvp->findWidgetByName("RECORD_FRAME_TOTAL");
    auto svp = getSwitch("RECORD_STREAM");

    if (!frameN || !svp)
        return false;

    auto recordON = svp->findWidgetByName("RECORD_FRAME_ON");

    if (!recordON)
        return false;

    if (recordON->getState() == ISS_ON)
        return true;

    frameN->setValue(frames);
    sendNewNumber(nvp);

    svp->reset();
    recordON->setState(ISS_ON);

    sendNewSwitch(svp);

    return true;
}

bool Camera::stopRecording()
{
    auto svp = getSwitch("RECORD_STREAM");

    if (!svp)
        return false;

    auto recordOFF = svp->findWidgetByName("RECORD_OFF");

    if (!recordOFF)
        return false;

    // If already set
    if (recordOFF->getState() == ISS_ON)
        return true;

    svp->reset();
    recordOFF->setState(ISS_ON);

    sendNewSwitch(svp);

    return true;
}

bool Camera::setFITSHeader(const QMap<QString, QString> &values)
{
    auto tvp = getText("FITS_HEADER");

    if (!tvp)
        return false;

    QMapIterator<QString, QString> i(values);

    while (i.hasNext())
    {
        i.next();

        auto headerT = tvp->findWidgetByName(i.key().toLatin1().data());

        if (!headerT)
            continue;

        headerT->setText(i.value().toLatin1().data());
    }

    sendNewText(tvp);

    return true;
}

bool Camera::setGain(double value)
{
    if (!gainN)
        return false;

    gainN->value = value;
    sendNewNumber(gainN->nvp);
    return true;
}

bool Camera::getGain(double *value)
{
    if (!gainN)
        return false;

    *value = gainN->value;

    return true;
}

bool Camera::getGainMinMaxStep(double *min, double *max, double *step)
{
    if (!gainN)
        return false;

    *min  = gainN->min;
    *max  = gainN->max;
    *step = gainN->step;

    return true;
}

bool Camera::setOffset(double value)
{
    if (!offsetN)
        return false;

    offsetN->value = value;
    sendNewNumber(offsetN->nvp);
    return true;
}

bool Camera::getOffset(double *value)
{
    if (!offsetN)
        return false;

    *value = offsetN->value;

    return true;
}

bool Camera::getOffsetMinMaxStep(double *min, double *max, double *step)
{
    if (!offsetN)
        return false;

    *min  = offsetN->min;
    *max  = offsetN->max;
    *step = offsetN->step;

    return true;
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

    auto svp = getSwitch("CCD_FAST_TOGGLE");

    if (!svp)
        return false;

    svp->at(0)->setState(enable ? ISS_ON : ISS_OFF);
    svp->at(1)->setState(enable ? ISS_OFF : ISS_ON);
    sendNewSwitch(svp);

    return true;
}

bool Camera::setCaptureFormat(const QString &format)
{
    auto svp = getSwitch("CCD_CAPTURE_FORMAT");
    if (!svp)
        return false;

    for (auto &oneSwitch : *svp)
        oneSwitch.setState(oneSwitch.label == format ? ISS_ON : ISS_OFF);

    sendNewSwitch(svp);
    return true;
}

bool Camera::setFastCount(uint32_t count)
{
    auto nvp = getNumber("CCD_FAST_COUNT");

    if (!nvp)
        return false;

    nvp->at(0)->setValue(count);

    sendNewNumber(nvp);

    return true;
}

bool Camera::setStreamExposure(double duration)
{
    auto nvp = getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    nvp->at(0)->setValue(duration);

    sendNewNumber(nvp);

    return true;
}

bool Camera::getStreamExposure(double *duration)
{
    auto nvp = getNumber("STREAMING_EXPOSURE");

    if (!nvp)
        return false;

    *duration = nvp->at(0)->getValue();

    return true;
}

bool Camera::isCoolerOn()
{
    auto svp = getSwitch("CCD_COOLER");

    if (!svp)
        return false;

    return (svp->at(0)->getState() == ISS_ON);
}

bool Camera::getTemperatureRegulation(double &ramp, double &threshold)
{
    auto regulation = getProperty("CCD_TEMP_RAMP");
    if (!regulation->isValid())
        return false;

    ramp = regulation.getNumber()->at(0)->getValue();
    threshold = regulation.getNumber()->at(1)->getValue();
    return true;
}

bool Camera::setTemperatureRegulation(double ramp, double threshold)
{
    auto regulation = getProperty("CCD_TEMP_RAMP");
    if (!regulation->isValid())
        return false;

    regulation.getNumber()->at(0)->setValue(ramp);
    regulation.getNumber()->at(1)->setValue(threshold);
    sendNewNumber(regulation->getNumber());
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
    if (m_CaptureFormatIndex < 0 || m_CaptureFormats.isEmpty() || m_CaptureFormatIndex > m_CaptureFormats.size())
        return QLatin1String("NA");

    return m_CaptureFormats[m_CaptureFormatIndex];
}
}
