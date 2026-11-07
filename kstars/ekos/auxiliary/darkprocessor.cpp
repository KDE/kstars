/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "darkprocessor.h"
#include "darklibrary.h"
#include "ekos/auxiliary/opticaltrainsettings.h"

#include <array>

#include "ekos_debug.h"

namespace Ekos
{

DarkProcessor::DarkProcessor(QObject *parent) : QObject(parent)
{
    connect(&m_Watcher, &QFutureWatcher<bool>::finished, this, [this]()
    {
        Q_EMIT darkFrameCompleted(m_Watcher.result());
    });

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkProcessor::normalizeDefects(const QSharedPointer<DefectMap> &defectMap, const QSharedPointer<FITSData> &lightData,
                                     uint16_t offsetX, uint16_t offsetY)
{
    switch (lightData->dataType())
    {
        case TBYTE:
            normalizeDefectsInternal<uint8_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TSHORT:
            normalizeDefectsInternal<int16_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TUSHORT:
            normalizeDefectsInternal<uint16_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TLONG:
            normalizeDefectsInternal<int32_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TULONG:
            normalizeDefectsInternal<uint32_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TFLOAT:
            normalizeDefectsInternal<float>(defectMap, lightData, offsetX, offsetY);
            break;

        case TLONGLONG:
            normalizeDefectsInternal<int64_t>(defectMap, lightData, offsetX, offsetY);
            break;

        case TDOUBLE:
            normalizeDefectsInternal<double>(defectMap, lightData, offsetX, offsetY);
            break;

        default:
            break;
    }
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DarkProcessor::normalizeDefectsInternal(const QSharedPointer<DefectMap> &defectMap,
        const QSharedPointer<FITSData> &lightData, uint16_t offsetX, uint16_t offsetY)
{

    T *lightBuffer = reinterpret_cast<T *>(lightData->getWritableImageBuffer());
    const int32_t width = lightData->width();
    const int32_t height = lightData->height();
    const uint32_t planeSize = lightData->samplesPerChannel();
    const int channels = std::max(1, lightData->channels());

    // On a raw colour frame the neighbours of a pixel have other colours, which have other levels. Replacing
    // a defect with them would leave a pixel of the wrong colour, so use the same colour, two pixels away.
    // A debayered frame keeps BAYERPAT in its header, but has a value of each colour for every pixel, even
    // when only its first plane is in use (Guide, Focus and Align with the DC1394 engine).
    QVariant bayerPattern;
    const int32_t stride = (channels == 1 && !lightData->hasDebayer() && lightData->getRecordValue("BAYERPAT", bayerPattern) &&
                            !bayerPattern.toString().trimmed().isEmpty()) ? 2 : 1;

    // The defect map covers the whole sensor, while the light frame may be a subframe starting at
    // offsetX, offsetY. Only the defects inside it are corrected, and only where all neighbours are
    // inside it as well.
    const auto correct = [&](const BadPixel & onePixel)
    {
        const int32_t x = static_cast<int32_t>(onePixel.x) - offsetX;
        const int32_t y = static_cast<int32_t>(onePixel.y) - offsetY;
        if (x < stride || y < stride || x >= width - stride || y >= height - stride)
            return;

        for (int channel = 0; channel < channels; channel++)
        {
            T *plane = lightBuffer + channel * planeSize;
            plane[x + y * width] = medianFilter(x, y, width, stride, plane);
        }
    };

    for (BadPixelSet::const_iterator onePixel = defectMap->hotThreshold();
            onePixel != defectMap->hotPixels().cend(); ++onePixel)
        correct(*onePixel);

    for (BadPixelSet::const_iterator onePixel = defectMap->coldPixels().cbegin();
            onePixel != defectMap->coldThreshold(); ++onePixel)
        correct(*onePixel);

    lightData->calculateStats(true);

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
T DarkProcessor::medianFilter(int32_t x, int32_t y, uint32_t width, int32_t stride, const T *buffer)
{
    std::array<T, 8> elements;
    int i = 0;
    for (int32_t dy = -stride; dy <= stride; dy += stride)
    {
        const T *row = buffer + (y + dy) * static_cast<int64_t>(width);
        for (int32_t dx = -stride; dx <= stride; dx += stride)
        {
            // The center is the defective value
            if (dx != 0 || dy != 0)
                elements[i++] = row[x + dx];
        }
    }

    std::sort(elements.begin(), elements.end());
    auto median = (elements[3] + elements[4]) / 2;
    return median;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkProcessor::subtractDarkData(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                                     uint16_t offsetX, uint16_t offsetY, QString *reason)
{
    // The buffers are walked plane by plane with the same element type, so the dark must provide every plane
    // of the light. It may have more: Guide, Focus and Align use only the first plane of a debayered colour
    // frame with the DC1394 engine, while the master dark is loaded for calibration with all three planes.
    // A raw colour dark cannot be subtracted from a debayered light.
    QString mismatch;
    if (darkData->channels() < lightData->channels())
        mismatch = i18n("The dark frame has %1 color channels while the frame being calibrated has %2.",
                        darkData->channels(), lightData->channels());
    else if (darkData->dataType() != lightData->dataType())
        mismatch = i18n("The dark frame and the frame being calibrated have different data types.");
    else if (offsetX + lightData->width() > darkData->width() || offsetY + lightData->height() > darkData->height())
        mismatch = i18n("The dark frame is %1x%2 pixels and does not cover the %3x%4 frame at %5,%6.",
                        darkData->width(), darkData->height(), lightData->width(), lightData->height(), offsetX, offsetY);

    if (!mismatch.isEmpty())
    {
        qCWarning(KSTARS_EKOS) << "Dark frame not subtracted:" << mismatch;
        if (reason)
            *reason = mismatch;
        return false;
    }

    switch (darkData->dataType())
    {
        case TBYTE:
            subtractInternal<uint8_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TSHORT:
            subtractInternal<int16_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TUSHORT:
            subtractInternal<uint16_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TLONG:
            subtractInternal<int32_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TULONG:
            subtractInternal<uint32_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TFLOAT:
            subtractInternal<float>(darkData, lightData, offsetX, offsetY);
            break;

        case TLONGLONG:
            subtractInternal<int64_t>(darkData, lightData, offsetX, offsetY);
            break;

        case TDOUBLE:
            subtractInternal<double>(darkData, lightData, offsetX, offsetY);
            break;

        default:
            if (reason)
                *reason = i18n("Unsupported data type of the dark frame.");
            return false;
    }

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
void DarkProcessor::subtractInternal(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                                     uint16_t offsetX, uint16_t offsetY)
{
    const uint32_t width = lightData->width();
    const uint32_t height = lightData->height();
    const uint32_t lightPlaneSize = lightData->samplesPerChannel();
    const uint32_t darkPlaneSize = darkData->samplesPerChannel();
    const int channels = std::max(1, lightData->channels());

    const uint32_t darkStride = darkData->width();
    const uint32_t darkoffset = offsetX + offsetY * darkStride;

    // A debayered frame has one plane per color, all of them are calibrated
    for (int channel = 0; channel < channels; channel++)
    {
        T *lightBuffer = reinterpret_cast<T *>(lightData->getWritableImageBuffer()) + channel * lightPlaneSize;
        T const *darkBuffer  = reinterpret_cast<T const*>(darkData->getImageBuffer()) + channel * darkPlaneSize + darkoffset;

        for (uint32_t y = 0; y < height; y++)
        {
            for (uint32_t x = 0; x < width; x++)
                lightBuffer[x] = (lightBuffer[x] > darkBuffer[x]) ? (lightBuffer[x] - darkBuffer[x]) : 0;

            lightBuffer += width;
            darkBuffer += darkStride;
        }
    }

    lightData->calculateStats(true);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkProcessor::denoise(int trainID, ISD::CameraChip *m_TargetChip, const QSharedPointer<FITSData> &targetData,
                            double duration, uint16_t offsetX, uint16_t offsetY)
{
    info = {trainID, m_TargetChip, targetData, duration, offsetX, offsetY};

    auto useDefect = false;
    // Get the train settings
    OpticalTrainSettings::Instance()->setOpticalTrainID(trainID);
    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::DarkLibrary);
    if (settings.isValid())
        useDefect = settings.toMap().contains("preferDefectsRadio");
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QFuture<bool> result = QtConcurrent::run(&DarkProcessor::denoiseInternal, this, useDefect);
#else
    QFuture<bool> result = QtConcurrent::run(this, &DarkProcessor::denoiseInternal, useDefect);
#endif
    m_Watcher.setFuture(result);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkProcessor::denoiseSynchronous(int trainID, ISD::CameraChip *targetChip,
                                       const QSharedPointer<FITSData> &targetData,
                                       double duration, uint16_t offsetX, uint16_t offsetY)
{
    info = {trainID, targetChip, targetData, duration, offsetX, offsetY};

    bool useDefect = false;
    // Get the train settings
    OpticalTrainSettings::Instance()->setOpticalTrainID(trainID);
    auto settings = OpticalTrainSettings::Instance()->getOneSetting(OpticalTrainSettings::DarkLibrary);
    if (settings.isValid())
        useDefect = settings.toMap().contains("preferDefectsRadio");

    return denoiseInternal(useDefect);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkProcessor::denoiseInternal(bool useDefect)
{
    // Filled with the reason whenever nothing matched, so the user is told what to fix instead of
    // getting a generic message.
    QString darkFailureReason;
    const auto emitNoDarkMessage = [&]()
    {
        Q_EMIT newLog(darkFailureReason.isEmpty()
                      ? i18n("No suitable dark frames or defect maps found. Please run the Dark Library wizard in Capture module.")
                      : i18n("No suitable dark frames or defect maps found. %1 Please run the Dark Library wizard in Capture module.",
                             darkFailureReason));
    };

    // Check if we have preference for defect map
    // If yes, check if defect map exists
    // If not, we check if we have regular dark frame as backup.
    if (useDefect)
    {
        QSharedPointer<DefectMap> targetDefectMap;
        if (DarkLibrary::Instance()->findDefectMap(info.targetChip, info.duration, targetDefectMap))
        {
            normalizeDefects(targetDefectMap, info.targetData, info.offsetX, info.offsetY);
            qCDebug(KSTARS_EKOS) << "Defect map denoising applied";
            return true;
        }
        else
        {
            qCWarning(KSTARS_EKOS) << "Failed to find suitable defect map, will try dark frame instead";
        }
    }

    // Check if we have valid dark data and then use it.
    QSharedPointer<FITSData> darkData;
    if (DarkLibrary::Instance()->findDarkFrame(info.targetChip, info.duration, darkData, &darkFailureReason))
    {
        // Make sure it's the same dimension if there is no offset
        if (info.offsetX == 0 && info.offsetY == 0 &&
                (info.targetData->width() != darkData->width() || info.targetData->height() != darkData->height()))
        {
            qCWarning(KSTARS_EKOS) << "Dark frame dimension mismatch: target"
                                   << info.targetData->width() << "x" << info.targetData->height()
                                   << "vs dark" << darkData->width() << "x" << darkData->height();
            darkFailureReason = i18n("The dark frame is %1x%2 pixels while the frame being calibrated is %3x%4 pixels.",
                                     darkData->width(), darkData->height(),
                                     info.targetData->width(), info.targetData->height());
            darkData.clear();
            emitNoDarkMessage();
            return false;
        }
        if (!subtractDarkData(darkData, info.targetData, info.offsetX, info.offsetY, &darkFailureReason))
        {
            emitNoDarkMessage();
            return false;
        }
        qCDebug(KSTARS_EKOS) << "Dark frame subtraction applied";
        return true;
    }

    int binX = 1, binY = 1;
    info.targetChip->getBinning(&binX, &binY);

    qCWarning(KSTARS_EKOS) << "Failed to find suitable dark frame or defect map for"
                           << info.targetChip->getCCD()->getDeviceName()
                           << "duration:" << info.duration << "s"
                           << "binning:" << binX << "x" << binY;
    // Tell the user why nothing matched, e.g. that the recorded darks were taken at another gain.
    emitNoDarkMessage();
    return false;
}

}
