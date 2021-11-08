/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "darkprocessor.h"
#include "darklibrary.h"

#include "ekos_debug.h"

namespace Ekos
{

DarkProcessor::DarkProcessor(QObject *parent) : QObject(parent)
{
    connect(&m_Watcher, &QFutureWatcher<bool>::finished, this, [this]()
    {
        emit darkFrameCompleted(m_Watcher.result());
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
    const uint32_t width = lightData->width();

    // Account for offset X and Y
    // e.g. if we send a subframed light frame 100x100 pixels wide
    // but the source defect map covers 1000x1000 pixels array, then we need to only compensate
    // for the 100x100 region.
    for (BadPixelSet::const_iterator onePixel = defectMap->hotThreshold();
            onePixel != defectMap->hotPixels().cend(); ++onePixel)
    {
        const uint16_t x = (*onePixel).x;
        const uint16_t y = (*onePixel).y;

        if (x <= offsetX || y <= offsetY)
            continue;

        uint32_t offset = (x - offsetX) + (y - offsetY) * width;

        lightBuffer[offset] = median3x3Filter(x - offsetX, y - offsetY, width, lightBuffer);
    }

    for (BadPixelSet::const_iterator onePixel = defectMap->coldPixels().cbegin();
            onePixel != defectMap->coldThreshold(); ++onePixel)
    {
        const uint16_t x = (*onePixel).x;
        const uint16_t y = (*onePixel).y;

        if (x <= offsetX || y <= offsetY)
            continue;

        uint32_t offset = (x - offsetX) + (y - offsetY) * width;

        lightBuffer[offset] = median3x3Filter(x - offsetX, y - offsetY, width, lightBuffer);
    }

    lightData->calculateStats(true);

}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
template <typename T>
T DarkProcessor::median3x3Filter(uint16_t x, uint16_t y, uint32_t width, T *buffer)
{
    T *top = buffer + (y - 1) * width + (x - 1);
    T *mid = buffer + (y - 0) * width + (x - 1);
    T *bot = buffer + (y + 1) * width + (x - 1);

    std::array<T, 8> elements;

    // Top
    elements[0] = *(top + 0);
    elements[1] = *(top + 1);
    elements[2] = *(top + 2);
    // Mid
    elements[3] = *(mid + 0);
    // Mid+1 is the defective value, so we skip and go for + 2
    elements[4] = *(mid + 2);
    // Bottom
    elements[5] = *(bot + 0);
    elements[6] = *(bot + 1);
    elements[7] = *(bot + 2);

    std::sort(elements.begin(), elements.end());
    auto median = (elements[3] + elements[4]) / 2;
    return median;
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkProcessor::subtractDarkData(const QSharedPointer<FITSData> &darkData, const QSharedPointer<FITSData> &lightData,
                                     uint16_t offsetX, uint16_t offsetY)
{
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
            break;
    }
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
    T *lightBuffer = reinterpret_cast<T *>(lightData->getWritableImageBuffer());

    const uint32_t darkStride = darkData->width();
    const uint32_t darkoffset = offsetX + offsetY * darkStride;
    T const *darkBuffer  = reinterpret_cast<T const*>(darkData->getImageBuffer()) + darkoffset;

    for (uint32_t y = 0; y < height; y++)
    {
        for (uint32_t x = 0; x < width; x++)
            lightBuffer[x] = (lightBuffer[x] > darkBuffer[x]) ? (lightBuffer[x] - darkBuffer[x]) : 0;

        lightBuffer += width;
        darkBuffer += darkStride;
    }

    lightData->calculateStats(true);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
void DarkProcessor::denoise(ISD::CCDChip *m_TargetChip, const QSharedPointer<FITSData> &targetData,
                            double duration, uint16_t offsetX, uint16_t offsetY)
{
    info = {m_TargetChip, targetData, duration, offsetX, offsetY};
    QFuture<bool> result = QtConcurrent::run(this, &DarkProcessor::denoiseInternal);
    m_Watcher.setFuture(result);
}

///////////////////////////////////////////////////////////////////////////////////////
///
///////////////////////////////////////////////////////////////////////////////////////
bool DarkProcessor::denoiseInternal()
{
    const QString device = info.targetChip->getCCD()->getDeviceName();

    // Check if we have preference for defect map
    // If yes, check if defect map exists
    // If not, we check if we have regular dark frame as backup.
    if (DarkLibrary::Instance()->cameraHasDefectMaps(device))
    {
        QSharedPointer<DefectMap> targetDefectMap;
        if (DarkLibrary::Instance()->findDefectMap(info.targetChip, info.duration, targetDefectMap))
        {
            normalizeDefects(targetDefectMap, info.targetData, info.offsetX, info.offsetY);
            qCDebug(KSTARS_EKOS) << "Defect map denoising applied";
            return true;
        }
    }

    // Check if we have valid dark data and then use it.
    QSharedPointer<FITSData> darkData;
    if (DarkLibrary::Instance()->findDarkFrame(info.targetChip, info.duration, darkData))
    {
        subtractDarkData(darkData, info.targetData, info.offsetX, info.offsetY);
        qCDebug(KSTARS_EKOS) << "Dark frame subtraction applied";
        return true;
    }

    emit newLog(i18n("No suitable dark frames or defect maps found. Please run the Dark Library wizard in Capture module."));
    return false;
}

}
