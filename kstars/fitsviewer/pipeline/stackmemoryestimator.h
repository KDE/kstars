/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitscommon.h"

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief Memory model for batch stacking (FITSStack/FITSData), as plain arithmetic so it
 * can be unit tested without a FITS file or a real machine's free memory.
 *
 * Everything is counted in "frames": one sub decoded to CV_32F at the stacking
 * resolution (width x height x channels x 4 bytes, after downscaling, with a CFA sub
 * counted as the three channels it becomes once debayered). While a channel is being
 * stacked, memory is roughly
 *
 *   engine frames (per channel stack) + working frames + batch x per-sub frames
 *
 * where the engine frames stay resident until the stack finishes (sigma-clip state,
 * linear and post-processed stack, calibration masters), the working frames cover the
 * per-sub decode/calibrate/warp temporaries and the final combine, and the batch is the
 * subs held resident at once (numInMem). The constants were calibrated against a
 * 26 MP mono SHO data set.
 *
 * A finished batch session releases everything except its result (see
 * FITSData::setStackBatchMode()), so sessions stacked earlier don't count here — the
 * caller subtracts whatever they still hold from the available memory it passes in.
 */
namespace StackMemoryEstimator
{

/** @brief Share of the available memory a stack may plan to use; the rest is margin
 * for estimation error, the GUI and everything else on the machine. */
constexpr double kBudgetFraction = 0.8;

/** @brief Per-sub decode/calibrate/warp temporaries plus the final combine and FITS
 * encode, in frames. */
constexpr double kWorkingFrames = 6.0;

/** @brief Smallest first batch for SIGMA/WINDSOR. The clip bounds every later batch is
 * judged against come from the first batch, and with 3 or fewer samples there are no
 * bounds at all — so a smaller first batch means little or no outlier rejection. */
constexpr int kMinRejectionBatch = 5;

/** @brief Frame geometry at native resolution, as the stacking engine will see it */
struct FrameGeometry
{
    int width { 0 };
    int height { 0 };
    // After debayering: 3 for a CFA (BAYERPAT) sub or a 3-plane image, else 1
    int channels { 1 };

    bool isValid() const
    {
        return width > 0 && height > 0 && channels > 0;
    }
};

/** @brief What the engine will keep resident, taken from the stacking parameters */
struct EngineConfig
{
    StackingMethod method { StackingMethod::MEAN };
    bool masterDark { false };
    bool masterFlat { false };
    bool linearNormalization { false };
};

enum class Quality
{
    // Every sub fits in one batch: the same result as a machine with unlimited memory
    Full,
    // Fits, but in several batches: SIGMA/WINDSOR reject against the first batch's bounds
    ReducedBatch,
    // Not even the smallest acceptable first batch fits
    Insufficient
};

/** @brief The estimate for one downscale factor */
struct Option
{
    StackDownscale downscale { StackDownscale::NONE };
    int width { 0 };
    int height { 0 };
    double frameBytes { 0 };
    // Peak with every sub in one batch, and with the smallest acceptable first batch
    double fullBatchBytes { 0 };
    double minBatchBytes { 0 };
    // First batch this machine can hold (fileCount when everything fits)
    int batchSize { 0 };
    Quality quality { Quality::Insufficient };
};

struct Estimate
{
    QVector<Option> options;
    // Finest factor that isn't Insufficient; X4 when nothing fits
    StackDownscale recommended { StackDownscale::NONE };
    bool anyFits { false };
    double availableBytes { 0 };
    double budgetBytes { 0 };
};

/**
 * @brief Read a sub's geometry from its header only (no pixel decode). A 2D frame with
 * a recognised BAYERPAT is reported as 3 channels, which is what it becomes in the engine.
 * @return false if the file can't be opened or has no usable image dimensions
 */
bool peekFrameGeometry(const QString &path, FrameGeometry &out);

double downscaleFactor(StackDownscale downscale);

/** @brief Bytes of one CV_32F frame after downscaling (integer division, as FITSStack::convertMat()) */
double frameBytes(const FrameGeometry &geometry, StackDownscale downscale);

/** @brief Frames one channel stack keeps resident until it finishes */
double engineFrames(const EngineConfig &config);

/** @brief Frames each batch sub costs: ImageMM keeps a history as large as the batch */
double framesPerBatchSub(StackingMethod method);

/** @brief Smallest acceptable first batch (never more than fileCount) */
int minFirstBatch(StackingMethod method, int fileCount);

/**
 * @brief Peak while stacking one channel with @p batch subs resident
 * @param channelStacks how many channel stacks the session holds at once (positional
 * RGB/RGBL sessions keep one per colour until the end)
 */
double peakBytes(double frameBytes, const EngineConfig &config, int batch, int channelStacks = 1);

/**
 * @brief How many subs to hold resident for the next batch
 * @param engineFramesToReserve engine frames not yet allocated (the whole engine before
 * the first batch; 0 once the stack is running, since it's already out of availBytes)
 * @return fileCount when everything fits, otherwise the largest batch that does, at least 1
 */
int fitBatch(double frameBytes, StackingMethod method, double engineFramesToReserve, int fileCount,
             double availBytes);

/**
 * @brief Estimate every downscale factor for a stack of @p fileCount subs
 * @param availBytes memory that can be used, including anything that can be evicted first
 */
Estimate estimate(const FrameGeometry &geometry, int fileCount, const EngineConfig &config, double availBytes,
                  int channelStacks = 1);

QString qualityName(Quality quality);

/** @brief A finished session that could be dropped and later reloaded from its file */
struct EvictionCandidate
{
    QString id;
    double bytes { 0 };
    // Larger is more recent
    qint64 lastUsed { 0 };
};

/**
 * @brief Pick sessions to evict, least recently used first, until availBytes plus what
 * they hold reaches needBytes. Nothing when availBytes already suffices; every candidate
 * when even that isn't enough (each one freed still helps, and eviction is lossless).
 */
QStringList chooseEvictions(QVector<EvictionCandidate> candidates, double availBytes, double needBytes);

}
