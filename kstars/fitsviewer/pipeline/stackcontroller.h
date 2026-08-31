/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitsviewer/fitscommon.h"

#include <QByteArray>
#include <QObject>
#include <QPointF>
#include <QRect>
#include <QSharedPointer>
#include <QVector>
#include <QFutureWatcher>
#include <opencv2/core/core.hpp>

namespace FITSImage
{
struct Solution;
}
class FITSData;
class SolverUtils;
struct wcsprm;

/**
 * @class StackController
 * @brief Owns a FITSData instance and drives the shared stacking/
 * post-processing engine (FITSData/FITSStack) without requiring a
 * FITSViewer/FITSTab/FITSView widget stack.
 *
 * FITSData and FITSStack are not LiveStacker-specific — they're the one
 * engine behind both the LiveStacker GUI feature (incremental, folder-
 * watching) and batch image post-processing (one-shot, folder-in/image-
 * out). This class is the entry point for the latter (and any other
 * non-interactive caller): it factors out the non-GUI parts of what
 * FITSView::loadStack() does today (creating a fresh FITSData, the
 * noimage.png placeholder-load workaround for a real async race, signal
 * forwarding) so a caller with no need for a visible viewer can drive the
 * engine directly, on equal footing with the LiveStacker GUI rather than
 * through it.
 *
 * This still runs inside the existing KStars process; it does not attempt
 * to run without KStars. It just avoids instantiating a new
 * FITSViewer/FITSTab for the job.
 */
class StackController : public QObject
{
        Q_OBJECT

    public:
        explicit StackController(QObject *parent = nullptr, FITSMode mode = FITS_LIVESTACKING);
        ~StackController() override;

        /**
         * @brief Start (or restart) stacking a directory of subs.
         * @param inDir directories of FITS files to watch/process
         * @param params stacking parameters
         */
        void start(const QStringList &inDir, const StackData &params);

        /**
         * @brief User/caller request to cancel an in-progress stack.
         */
        void cancel();

        /**
         * @brief Re-run post processing (denoise, sharpen, gradient removal,
         * stretch, etc.) on the existing stack with new parameters.
         */
        void redoPostProcess(const StackPPData &ppParams);

        /**
         * @brief Crop the current combined stacked image, adjusting its WCS reference
         * pixel to match (if the stack was plate-solved). Must be called after a stack
         * has produced a combined result.
         * @param roi region to keep, in 0-indexed pixel coordinates
         * @param error receives a human-readable failure reason on failure
         * @return success
         */
        bool crop(const QRect &roi, QString &error);

        /**
         * @brief Bake a one-shot MTF autostretch into the current combined result.
         * @param targetBackground where the background level lands post-stretch, [0,1]
         * @param shadowsClipping MADN units below/above the median to clip at
         * @param error receives a human-readable failure reason on failure
         */
        bool applyAutoStretch(double targetBackground, double shadowsClipping, QString &error, bool linked = true);

        /**
         * @brief Bake a control-point tone curve, identically on every channel.
         */
        bool applyCurve(const QVector<QPointF> &controlPoints, QString &error);

        /**
         * @brief Bake independent per-channel tone curves (per-channel color
         * grading).
         */
        bool applyCurvePerChannel(const QVector<QVector<QPointF>> &channelPoints, QString &error);

        /**
         * @brief Bake an HSV saturation scale into the current combined result.
         * @param amt 1.0 = unchanged, 0.0 = grayscale, >1.0 = more saturated
         */
        bool applySaturation(double amt, QString &error);

        /**
         * @brief Bake a contrast adjustment into the current combined result.
         * @param amt 1.0 = unchanged, 0.0 = flat, >1.0 = more contrast
         */
        bool applyContrast(double amt, QString &error);

        /**
         * @brief Bake noise reduction into the current combined result — an
         * independent, composable post-combine step (see DenoiseOperation::apply()).
         */
        bool applyDenoise(double amt, DenoiseMethod method, double chromaAmt, QString &error);

        /**
         * @brief Bake background/gradient extraction into the current combined
         * result — an independent, composable post-combine step (see
         * BGEOperation::apply()).
         */
        bool applyBGE(double strength, QString &error);

        /**
         * @brief Write the current combined stacked image straight to disk.
         */
        bool save(const QString &path, QString &error);

        /**
         * @brief Render the session's current working image to raw JPEG bytes (see
         * PreviewRenderer — headless, no FITSView/GUI dependency, cheap enough to call
         * after every step). Useful after any command that mutates the working image
         * (crop, denoise, BGE, ...), not just once per session. The caller sends this
         * over the wsMedia binary channel (see Media::uploadPreview()) rather than
         * embedding it inline in a JSON response.
         */
        QByteArray getPreviewJpegBytes(QString &error, int maxDimension = 1024);

        /**
         * @brief Adopt an externally-computed image (e.g. ChannelBlendOperation's
         * weighted narrowband-palette blend) as this session's stacked result, creating
         * the underlying FITSData if this controller has never been start()ed — so a
         * blend result gets the same crop/apply/save session lifecycle as a real
         * stack, without needing a folder to stack in the first place.
         * @param sourceWcs optional WCS the image is registered to (see
         * FITSData::setStackedImage()) — forwarded as-is so a blend result can carry a
         * real WCS, enabling crop()/applyPhotometricCalibration() on it afterward.
         */
        bool adopt(const cv::Mat &image, QString &error, const struct wcsprm *sourceWcs = nullptr);

        /**
         * @brief Bake a catalog-based star color calibration into the current combined
         * result — an independent, composable, opt-in post-combine step (see
         * PhotometricCalibrationOperation and FITSData::applyPhotometricCalibration()).
         * Requires the session to carry a WCS (a plate-solved stack, or a blend whose
         * adopt() was given one).
         * @param strength [0,1]; how much of the computed per-star correction to apply
         * @param maxCatalogMagnitude faintest catalog star to consider a candidate match
         * @param matchRadiusArcsec how close a catalog star must be (on sky) to a
         * detected star's position to count as a match
         * @param starsDetected receives how many star-like blobs were found in the image
         * @param starsMatched receives how many of those were matched to a catalog star
         * and corrected
         * @param photometricCatalogPath optional supplementary (RA, Dec, V, B-V)
         * catalog path — see FITSData::applyPhotometricCalibration().
         */
        bool applyPhotometricCalibration(double strength, double maxCatalogMagnitude, double matchRadiusArcsec,
                                          QString &error, int &starsDetected, int &starsMatched,
                                          const QString &photometricCatalogPath = QString());

        /**
         * @brief Access the underlying FITSData for the current session,
         * e.g. to read back the stacked buffer or save it to disk.
         */
        const QSharedPointer<FITSData> &imageData() const
        {
            return m_ImageData;
        }

    Q_SIGNALS:
        void plateSolveSub(const double ra, const double dec, const double pixScale, const int index,
                            const int healpix, const StackFrameWeighting &weighting);
        void alignMasterChosen(const QString &alignMaster);
        void stackInProgress();
        void stackReady(const bool cancelled);
        void stackFailed(const QString &reason);
        void stackUpdateStats(const bool ok, const int sub, const int total, const double meanSNR, const double minSNR,
                              const double maxSNR);

    private:
        /**
         * @brief Handle a plate-solve request emitted by FITSData while stacking with
         * StackAlignMethod::PLATE_SOLVE. Runs a headless SolverUtils solve — the same
         * engine and profile FITSTab::plateSolveSub() drives for the GUI LiveStacker,
         * minus the QDialog — and reports the outcome back via FITSData::solverDone()
         * so the stacking chain (which is waiting on that call) can resume.
         *
         * Mirrors FITSTab::plateSolveSub()'s two-phase behavior: if the caller needs
         * star-quality weighting (HFR or NUM_STARS), extract stars first for the
         * median HFR / count, then solve; a failed solve on a tight (already-known)
         * search index/healpix is retried once with widened criteria before giving up.
         */
        void handlePlateSolveSub(const double ra, const double dec, const double pixScale, const int index,
                                  const int healpix, const StackFrameWeighting &weighting);
        void runExtract(const double ra, const double dec, const double pixScale, const int index, const int healpix);
        void runSolve(const double ra, const double dec, const double pixScale, const int index, const int healpix);
        void handleExtractDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);
        void handleSolveDone(bool timedOut, bool success, const FITSImage::Solution &solution, double elapsedSeconds);

        FITSMode m_Mode;
        QSharedPointer<FITSData> m_ImageData;
        QFutureWatcher<bool> m_FitsWatcher;
        QSharedPointer<SolverUtils> m_Solver;
        double m_StackMedianHFR { -1.0 };
        int m_StackNumStars { 0 };
        bool m_StackExtendedPlateSolve { false };
        // Cached from the plateSolveSub() request that's currently in flight, so the
        // solve phase (which may run after a separate extract phase) still has the
        // original position/scale hint to work with.
        double m_PendingRa { 0.0 };
        double m_PendingDec { 0.0 };
        double m_PendingPixScale { 0.0 };
        int m_PendingIndex { -1 };
        int m_PendingHealpix { -1 };
};
