/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "stackmemoryestimator.h"
#include "fitsviewer/bayerparameters.h"

#include <fitsio.h>

#include <QFile>

#include <algorithm>
#include <cmath>

namespace StackMemoryEstimator
{

bool peekFrameGeometry(const QString &path, FrameGeometry &out)
{
    fitsfile *fptr = nullptr;
    int status = 0;
    if (fits_open_file(&fptr, QFile::encodeName(path).constData(), READONLY, &status))
        return false;

    int naxis = 0;
    long naxes[3] = { 0, 0, 1 };
    if (fits_get_img_dim(fptr, &naxis, &status) != 0 || naxis < 2)
    {
        status = 0;
        fits_close_file(fptr, &status);
        return false;
    }
    fits_get_img_size(fptr, std::min(naxis, 3), naxes, &status);

    // Same test FITSData uses to decide a sub will be debayered: a BAYERPAT holding a
    // pattern it recognises.
    bool cfa = false;
    char bayerPattern[FLEN_VALUE] = { 0 };
    int keyStatus = 0;
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &keyStatus) == 0)
        cfa = BayerUtils::bayerPatternValid(QString(bayerPattern).remove('\'').trimmed());

    const int readStatus = status;
    status = 0;
    fits_close_file(fptr, &status);
    if (readStatus != 0 || naxes[0] <= 0 || naxes[1] <= 0)
        return false;

    out.width = static_cast<int>(naxes[0]);
    out.height = static_cast<int>(naxes[1]);
    if (naxis >= 3 && naxes[2] > 1)
        out.channels = static_cast<int>(naxes[2]);
    else
        out.channels = cfa ? 3 : 1;
    return true;
}

double downscaleFactor(StackDownscale downscale)
{
    switch (downscale)
    {
        case StackDownscale::X2:
            return 2.0;
        case StackDownscale::X3:
            return 3.0;
        case StackDownscale::X4:
            return 4.0;
        default:
            return 1.0;
    }
}

double frameBytes(const FrameGeometry &geometry, StackDownscale downscale)
{
    if (!geometry.isValid())
        return 0.0;
    const int factor = static_cast<int>(downscaleFactor(downscale));
    const double width = static_cast<double>(geometry.width / factor);
    const double height = static_cast<double>(geometry.height / factor);
    return width * height * geometry.channels * static_cast<double>(sizeof(float));
}

double engineFrames(const EngineConfig &config)
{
    // Linear stack + post-processed copy
    double frames = 2.0;
    if (config.method == StackingMethod::SIGMA || config.method == StackingMethod::WINDSOR)
        frames += 3.0; // lower bound, upper bound and weight per pixel
    else if (config.method == StackingMethod::IMAGEMM)
        frames += 1.0; // latent estimate
    if (config.masterDark)
        frames += 1.0;
    if (config.masterFlat)
        frames += 1.0;
    if (config.linearNormalization)
        frames += 1.0; // hit map
    return frames;
}

double framesPerBatchSub(StackingMethod method)
{
    return method == StackingMethod::IMAGEMM ? 2.0 : 1.0;
}

int minFirstBatch(StackingMethod method, int fileCount)
{
    const int files = std::max(1, fileCount);
    if (method == StackingMethod::SIGMA || method == StackingMethod::WINDSOR)
        return std::min(kMinRejectionBatch, files);
    return 1;
}

double peakBytes(double frameBytes, const EngineConfig &config, int batch, int channelStacks)
{
    const double frames = engineFrames(config) * std::max(1, channelStacks) + kWorkingFrames
                          + std::max(1, batch) * framesPerBatchSub(config.method);
    return frames * frameBytes;
}

int fitBatch(double frameBytes, StackingMethod method, double engineFramesToReserve, int fileCount,
             double availBytes)
{
    const int files = std::max(1, fileCount);
    if (!(frameBytes > 0.0) || !(availBytes > 0.0))
        return files; // nothing to judge by; the pre-flight is where a refusal belongs

    const double frames = availBytes * kBudgetFraction / frameBytes - engineFramesToReserve - kWorkingFrames;
    const int batch = static_cast<int>(std::floor(frames / framesPerBatchSub(method)));
    return std::clamp(batch, 1, files);
}

Estimate estimate(const FrameGeometry &geometry, int fileCount, const EngineConfig &config, double availBytes,
                  int channelStacks)
{
    Estimate result;
    result.availableBytes = availBytes;
    result.budgetBytes = availBytes * kBudgetFraction;
    result.recommended = StackDownscale::X4;

    const int files = std::max(1, fileCount);
    const int minBatch = minFirstBatch(config.method, files);
    const double engine = engineFrames(config) * std::max(1, channelStacks);

    for (StackDownscale downscale : { StackDownscale::NONE, StackDownscale::X2, StackDownscale::X3, StackDownscale::X4 })
    {
        Option option;
        option.downscale = downscale;
        const int factor = static_cast<int>(downscaleFactor(downscale));
        option.width = geometry.width / factor;
        option.height = geometry.height / factor;
        option.frameBytes = frameBytes(geometry, downscale);
        option.fullBatchBytes = peakBytes(option.frameBytes, config, files, channelStacks);
        option.minBatchBytes = peakBytes(option.frameBytes, config, minBatch, channelStacks);

        if (!(availBytes > 0.0) || !(option.frameBytes > 0.0))
        {
            // Unknown memory: nothing to hold the caller back with
            option.batchSize = files;
            option.quality = Quality::Full;
        }
        else if (option.fullBatchBytes <= result.budgetBytes)
        {
            option.batchSize = files;
            option.quality = Quality::Full;
        }
        else if (option.minBatchBytes <= result.budgetBytes)
        {
            option.batchSize = std::max(minBatch, fitBatch(option.frameBytes, config.method, engine, files, availBytes));
            option.quality = Quality::ReducedBatch;
        }
        else
        {
            option.batchSize = 0;
            option.quality = Quality::Insufficient;
        }

        if (!result.anyFits && option.quality != Quality::Insufficient)
        {
            result.anyFits = true;
            result.recommended = downscale;
        }
        result.options.append(option);
    }
    return result;
}

QString qualityName(Quality quality)
{
    switch (quality)
    {
        case Quality::Full:
            return QStringLiteral("full");
        case Quality::ReducedBatch:
            return QStringLiteral("reduced_batch");
        default:
            return QStringLiteral("insufficient");
    }
}

QStringList chooseEvictions(QVector<EvictionCandidate> candidates, double availBytes, double needBytes)
{
    QStringList chosen;
    if (availBytes >= needBytes)
        return chosen;

    std::stable_sort(candidates.begin(), candidates.end(), [](const EvictionCandidate & a, const EvictionCandidate & b)
    {
        return a.lastUsed < b.lastUsed;
    });

    double freed = 0.0;
    for (const auto &candidate : candidates)
    {
        if (availBytes + freed >= needBytes)
            break;
        chosen << candidate.id;
        freed += candidate.bytes;
    }
    return chosen;
}

}
