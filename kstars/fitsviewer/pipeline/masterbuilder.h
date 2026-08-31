/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <opencv2/core/core.hpp>

/**
 * @class MasterBuilder
 * @brief Combines a folder of raw calibration subs (bias, dark, or flat) into a single
 * master frame.
 *
 * `FITSStack::addMaster()` already applies a pre-built master dark/flat to light subs
 * during calibration (see `fitsstack.cpp` — dark: rescale to sensor bit depth if given
 * normalized; flat: normalize by its own per-channel median before use, so this builder
 * doesn't need to match a particular output scale). What was missing is the other side:
 * turning a folder of raw calibration subs into that master in the first place —
 * `DarkLibrary` builds master darks but only from its own live-capture database, and
 * there's no flat equivalent at all. This class is the "simplest case of stacking": no
 * alignment, no debayer handling beyond what the raw subs already are, no registration —
 * combine N same-sized frames with sigma-clip rejection (flats additionally get
 * per-frame median normalization first, to correct for illumination drift between subs).
 */
class MasterBuilder
{
    public:
        enum class Type { BIAS, DARK, FLAT };

        /**
         * @brief Combine every FITS-loadable file directly inside `dir` (non-recursive,
         * matching ekoslive-offline's darkFolder/flatFolder wizard semantics) into one
         * master frame.
         * @param dir folder of calibration subs
         * @param type controls whether per-frame median normalization is applied before
         * combining (FLAT only) — see class comment for why BIAS/DARK don't need it
         * @param outMaster receives the combined master (CV_32F, 1 or 3 channels)
         * @param error receives a human-readable failure reason on failure
         * @param lowSigma / highSigma single-pass sigma-clip rejection thresholds; frames
         * with fewer than 3 usable subs fall back to a plain mean (not enough samples to
         * estimate per-pixel stddev usefully)
         * @param subtractPath optional master bias (or matching-exposure dark) to
         * subtract from each raw sub immediately after loading, before any other
         * processing — this is FLAT's real use case: flats are usually taken at a much
         * shorter exposure than lights, where dark current is negligible but the sensor's
         * bias/offset pattern still isn't, so a proper master flat needs its own bias
         * subtracted first. Passing this for BIAS/DARK is accepted but unusual — a dark
         * taken at the light's own exposure already has bias baked in, which is exactly
         * what's wanted when it's later subtracted from a light sub.
         * @param matchExptime when >= 0, only combine files whose EXPTIME header is
         * within exptimeTolerance seconds of this value; files outside that window are
         * skipped entirely (not loaded as pixel data, just header-checked). Negative
         * (the default) disables filtering — every FITS-loadable file in `dir` is used,
         * matching the original behavior. Real-world calibration folders often mix
         * multiple exposure lengths under one "Dark Frame" IMAGETYP with no other way to
         * tell them apart (e.g. a shared library holding both light-darks and
         * flat-darks) — blindly combining all of them averages together frames whose
         * dark current doesn't match anything, silently producing a master that's wrong
         * for every use. This is the fix: match against the one output actually wanted
         * on THIS call, matching the exposure length the caller is calibrating.
         * @param exptimeTolerance seconds of slack around matchExptime — needed because
         * real exposures rarely land on an exact nominal value (e.g. auto-exposed flats,
         * or a camera's actual shutter timing vs. the requested duration).
         * @param outUsedCount when non-null, receives the number of files actually
         * combined — equal to the full directory listing unless matchExptime filtered it
         * down. buildAndSave() uses this for an accurate NCOMBINE header.
         * @return success (fails with a clear error if matchExptime filtering leaves
         * zero usable files)
         */
        static bool build(const QString &dir, Type type, cv::Mat &outMaster, QString &error,
                           double lowSigma = 3.0, double highSigma = 3.0, const QString &subtractPath = QString(),
                           double matchExptime = -1.0, double exptimeTolerance = 0.5, int *outUsedCount = nullptr);

        /**
         * @brief Build a master (see build()) and write it to outputPath as a FITS file,
         * so it can be fed straight into StackData::masterDark/masterFlat unchanged —
         * no changes needed to the existing (file-path-based) calibration-loading code
         * in FITSData::processMasters().
         * @param outMaster when non-null, receives a copy of the built master — lets a
         * caller (e.g. for a preview) reuse the in-memory result instead of re-reading
         * the just-written file back off disk.
         */
        static bool buildAndSave(const QString &dir, Type type, const QString &outputPath, QString &error,
                                  double lowSigma = 3.0, double highSigma = 3.0, const QString &subtractPath = QString(),
                                  double matchExptime = -1.0, double exptimeTolerance = 0.5, cv::Mat *outMaster = nullptr);

    private:
        // Loads one FITS file into a CV_32F Mat (1 or 3 channels), via FITSData's public
        // loadFromFile() — not the private stackLoadImage() fast path, which is tightly
        // coupled to FITSData's live-stack session state.
        static bool loadFrame(const QString &path, cv::Mat &outFrame, double &outMedian, QString &error);

        // Header-only EXPTIME read via cfitsio directly (fits_read_key), not FITSData —
        // avoids decoding pixel data just to pre-filter a file list by exposure length.
        static bool readExptime(const QString &path, double &outExptime, QString &error);

        // Single-pass per-pixel sigma-clip mean across same-sized/same-type frames.
        static cv::Mat combineSigmaClip(const std::vector<cv::Mat> &frames, double lowSigma, double highSigma);
};
