/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

// Include Windows-specific headers first with protective macros
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#undef NOMINMAX  // Remove after windows.h
#endif

#include "fitscommon.h"
#include "ekos/auxiliary/solverutils.h"
#include "fits_debug.h"
#include "fitsstackmonitor.h"

#include <QObject>
#include <QPointer>

// Include OpenCV headers after Windows headers
#ifdef _WIN32
#pragma push_macro("NOMINMAX")
#define NOMINMAX
#endif

#include "opencv2/opencv.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/photo.hpp"

#ifdef _WIN32
#pragma pop_macro("NOMINMAX")
#endif

#include <wcs.h>
#include <fitsio.h>

namespace Ekos
{
class StellarSolverProfileEditor;
}

/**
 * @class FITSStack
 * @brief Live stacking engine for real-time astrophotography image integration.
 *
 * This class provides the core functionality for the KStars Live Stacker, enabling
 * real-time stacking of incoming FITS frames (from a camera or image sequence)
 * to improve signal-to-noise ratio and build up a final integrated image.
 *
 * ### Overview:
 * - **Start Up**: Live Stacker is selected from SkyMap. Depending on FITS options, either
 *   a new FITSViewer window is started, or a new tab in an existing FITSViewer window is started.
 *   Again controlled by FITS options, Live Stacker can be started as a separate process (as
 *   opposed to just being a window with the KStars process).
 *
 * - **Initialization**: The Live Stacker is initialized with stack parameters and
 *   image dimensions when the first frame arrives. Output format and stack state
 *   are set up accordingly.
 *
 * - **Directory Watcher**: FITSDirWatcher reads files in the selected directory. These are
 *   stacked. In addition, the directory is watched for new files when are incrementally
 *   stacked.
 *
 * - **Frame Input**: New frames are received via addImage(). Each frame is loaded to memory.
 *
 * - **Alignment**: An alignment master is selected and all frames aligned to the master.
 *   Plate solving is used for alignment. WCS is used to workout the transformation from the
 *   existing sub to the aligned sub and openCV functions warp the sub based on the
 *   transformation.
 *
 * - **Calibration Support**: Master darks and flats can be optionally applied before stacking.
 *   Flats and darks may be stacked separately and saved as masters to be applied during
 *   live stacking of light frames.
 *
 * - **Stacking**: Stacking is performed in chunks. When there are enough subs for the initial
 *   stack, these are stacked and intermediate results calculated for use in subsequent stacks.
 *   If there are not enough subs for a full initial stack, then whatever is available is stacked
 *   and displayed. Once a full initial stack is complete, memory for the initial subs is released,
 *   intermediate results calculated and the stackk displayed. The system may or may not have more
 *   subs available for stacking, in which case a running stack is performed on the next chunk of
 *   subs. These are added to the existing stack, intermediate results updated and the stack
 *   displayed. The system then repeats until all subs have been processed, then waits for new
 *   subs to arrives and adds these to the stack.
 *
 * - **Live Output**: The integrated stack is updated after new subs are added.
 *
 * - **Post Processing**: Optionally, simple routines for deconvolution, unsharp mask and
 *   denoise may be applied to the stacked image to enhance its visual appearance.
 *
 * The class works closely with FITSData, FITSView, FITSViewer, FITSDirWatcher and the Live
 * Stack UI controller. Plate solving is done in FITSTab and StellarSolver.
 */

class FITSStack : public QObject
{
        Q_OBJECT

    public:
        explicit FITSStack(FITSData *parent, LiveStackChannel channel, LiveStackData params);
        virtual ~FITSStack() override;

        /**
         * @brief Prepare FITSStack for the next image sub. Call before addSub.
         * @param sub
         */
        void setupNextSub(const LiveStackFile &sub);

        /**
         * @brief add an image sub to the stack.
         * @param imageBuffer is the in-memory buffer
         * @param cvType is the openCV Mat type
         * @param width is image width
         * @param height is image height
         * @param bytesPerPixel
         * @param snr (calculated and output)
         * @return success
         */
        bool addSub(void *imageBuffer, const int cvType, const int width, const int height,
                    const int bytesPerPixel, double &snr);

        /**
         * @brief add an align master
         * @param wcs of alignment master
         */
        void addAlignMasterWCS(const QSharedPointer<wcsprm> &wcs);

        /**
         * @brief add a master dark or flat.
         * @param dark (or flat)
         * @param image buffer
         * @param width is master width
         * @param height is master height
         * @param bytesPerPixel. Set to zero to skip this check
         * @param cvType
         */
        void addMaster(const bool dark, void *imageBuffer, const int width, const int height, const int bytesPerPixel,
                       const int cvType);

        /**
         * @brief solverDone called when plate solving completes, so start next action.
         * @param wcsHandle of the solved image
         * @param timedOut (or not)
         * @param success (or not)
         * @param median HFR of stars
         * @param number of stars
         * @return success
         */
        bool solverDone(const wcsprm * wcsHandle, const bool timedOut, const bool success, const double hfr, const int numStars);

        /**
         * @brief Update the sub datastructure's status.
         * @param OK (or not)
         */
        void addSubStatus(const bool ok);

        /**
         * @brief Perform an initial stack
         */
        bool stack();

        /**
         * @brief Perform an incremental stack (add new subs to an existing stack)
         */
        bool stackn();

        /**
         * @brief Redo post-processing on the stack
         * @param Post processing parameters
         */
        void redoPostProcessStack(const LiveStackPPData &ppParams);

        /**
         * @brief Get the WCS data structure for stacked image
         * @return wcsprm pointer
         */
        const struct wcsprm * getWCSStackImage() const
        {
            return m_WCSStackImage;
        }

        /**
         * @brief Get whether a stacking operation is in progress
         * @return stack in progress
         */
        const bool &getStackInProgress() const
        {
            return m_StackInProgress;
        }

        /**
         * @brief Set the initial stack state
         * @param initial stack state
         */
        void setInitalStackDone(bool done);

        /**
         * @brief Get whether the initial stack has been completed
         * @return initial stack done
         */
        const bool &getInitialStackDone() const
        {
            return m_InitialStackDone;
        }

        /**
         * @brief Set the stack operation in progress state
         * @param stack operation in progress
         */
        void setStackInProgress(bool inProgress);

        /**
         * @brief Get stack data structure
         * @return stack data structure
         */
        const LiveStackData &getStackData() const
        {
            return m_StackData;
        }

        /**
         * @brief Return the stacked image
         * @return stacked image
         */
        cv::Mat getStackImage() const
        {
            return m_StackedImageFinal.clone();
        }

        /**
         * @brief Gets the downscaling factor for the use downscale option
         * @return downscale factor
         */
        double getDownscaleFactor();

        const double &getMeanSubSNR() const
        {
            return m_MeanSubSNR;
        }

        const double &getMinSubSNR() const
        {
            return m_MinSubSNR;
        }

        const double &getMaxSubSNR() const
        {
            return m_MaxSubSNR;
        }

    signals:
        /**
         * @brief Update the Stack Monitor
         * @param subs is a vector of subs to update
         * @param info is a structure containing details of the update
         */
        void updateStackMon(const QVector<LiveStackFile> &subs, const QVector<LiveStackStageInfo> &infos);

    public slots:
    private:
        typedef enum
        {
            PLATESOLVE_IN_PROGRESS,
            PLATESOLVE_FAILED,
            CALIBRATION_FAILED,
            CORRECTION_FAILED,
            ALIGNMENT_FAILED,
            OK
        } StackSubStatus;

        struct AdamState
        {
            cv::Mat m, v;
            int t = 0;
        };

        struct StackImageData
        {
            LiveStackFile sub;
            cv::Mat image;
            cv::Mat psfKernel;
            StackSubStatus status = StackSubStatus::OK;
            bool isCalibrated = false;
            bool isCorrected = false;
            bool isAligned = false;
            struct wcsprm * wcsprm;
            double hfr = -1;
            int numStars = 0;
            float weight = -1.0f;
        };

        /**
         * @brief Check that a new image is consistent with previous images in size, datatype, etc
         * @return success (or not)
         */
        bool checkSub(const int width, const int height, const int bytesPerPixel, const int channels);

        /**
         * @brief Convert the input image to float and downscale if required
         * @param input image
         * @param output converted image
         * @return success (or not)
         */
        bool convertMat(const cv::Mat &input, cv::Mat &output);

        /**
         * @brief Template function to convert planar buffer to cv::Mat (interleaved)
         * @param buffer is the data buffer of image
         * @param width of image
         * @param height of image
         * @return the cv::Mat
         */
        template <typename T>
        cv::Mat convertToCV(T *buffer, int width, int height)
        {
            try
            {
                int totalPixels = width * height;

                // Construct one Mat per channel from planar layout
                cv::Mat channelR(height, width, cv::DataType<T>::type, buffer);
                cv::Mat channelG(height, width, cv::DataType<T>::type, buffer + totalPixels);
                cv::Mat channelB(height, width, cv::DataType<T>::type, buffer + 2 * totalPixels);

                // Merge to interleaved RGB
                std::vector<cv::Mat> channels = { channelR, channelG, channelB };
                cv::Mat interleaved;
                cv::merge(channels, interleaved);

                return interleaved;
            }
            catch (const cv::Exception &ex)
            {
                QString s1 = ex.what();
                qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from convertToCV").arg(s1);
                return cv::Mat();
            }
        }

        /**
         * @brief Calculate the warp matrix to warp image 2 to image 1 (reference)
         * @param wcs1 WCS structure for image 1 (reference)
         * @param wcs2 WCS structure for image 2
         * @param warp matrix
         * @return success (or not)
         */
        bool calcWarpMatrix(struct wcsprm * wcs1, struct wcsprm * wcs2, cv::Mat &warp);

        /**
         * @brief Decompose the warp matrix into transation and rotation elements
         * @param warp matrix
         * @param image size
         * @param dx x-translation
         * @param dy y-translation
         * @param rotationDeg (rotation angle in degrees)
         */
        void decomposeWarpMatrix(const cv::Mat &warp, const cv::Size &imageSize, double &dx, double &dy, double &rotationDeg);

        /**
         * @brief Calibrate the passed in sub
         * @param subFile file structure
         * @param sub to be calibrated
         * @return success (or not)
         */
        bool calibrateSub(const LiveStackFile &subFile, cv::Mat &sub);

        /**
         * @brief Correct sub by removing hot/cold pixels
         * @param subFile file structure
         * @param sub
         * @return success (or not)
         */
        bool correctSub(const LiveStackFile &subFile, cv::Mat &sub);

        /**
         * @brief Stack the passed in vector of subs
         * @param initial stack (or incremental)
         * @param totalWeight is the weight of the current stack if this is an incremental stack
         * @param stack is returned to the caller
         * @return success (or not)
         */
        bool stackSubs(const bool initial, float &totalWeight, cv::Mat &stack);

        /**
         * @brief Stack the passed in vector of subs using Sigma Clipping
         * @param weights of each sub for the stack
         * @return stack is returned to the caller
         */
        cv::Mat stackSubsSigmaClipping(const QVector<float> &weights);

        /**
         * @brief Called by stackSubsSigmaClipping to do the sigma clipping on pixel at position x
         * @param x position to process
         * @param imagesPtrs array of pointers to each image
         * @param finalImagePtr results image
         * @param sigmaClipPtr intermediate results pointer
         * @param weights to apply to sigma clipping
         */
        void stackSigmaClipPixel(int x, const std::vector<const float *> &imagesPtrs, float* finalImagePtr,
                                 const QVector<cv::Vec4f *> &sigmaClipPtr, const QVector<float> &weights);

        /**
         * @brief Stack the passed in vector of subs to an existing stack using Sigma Clipping
         * @param weights of each sub for the stack
         * @return stack
         */
        cv::Mat stacknSubsSigmaClipping(const QVector<float> &weights);

        /**
         * @brief Runs full ImageMM stacking on the current subframe set.
         * @param weights     Per-subframe relative weights (same size as current subframe list).
         * @param lsd         User specified parameters.
         * @return The stacked image.
         */
        cv::Mat stackSubsImageMM(const QVector<float> &weights, const LiveStackData &lsd);

        /**
         * @brief Incrementally update the ImageMM stack using new subframes.
         * @param weights     Per-subframe relative weights (same size as current subframe list).
         * @param lsd         User specified parameters.
         * @return The updated latent floating-point image (`cv::Mat`, type `CV_32F`).
         */
        cv::Mat stacknSubsImageMM(const QVector<float> &weights, const LiveStackData &lsd);

        /**
         * @brief Build a complete list of subframes and corresponding weights for ImageMM stacking.
         * @param[in]  newWeights   Vector of weights associated with the new subframes being added.
         * @param[out] allSubs      Combined list of all subframes (existing + new) to be used for stacking.
         * @param[out] allWeights   Combined, normalized list of weights matching `allSubs`.
         * @return True if all subframes and weights were successfully combined; false on error or mismatch.
         */
        bool imageMMBuildAllSubs(const QVector<float> &newWeights, QVector<FITSStack::StackImageData> &allSubs,
                                 QVector<float> &allWeights);

        /**
         * @brief Perform one complete ImageMM robust stacking run. This is the core function for ImageMM
         * @param subs          Vector of subframe data and metadata.
         * @param latent        Latent image estimate (initialized if empty).
         * @param sigma         Robust scale parameter, updated per iteration.
         * @param weights       Per-subframe scalar weights (same size as subs).
         * @param lsd           User specified parameters.
         * @param incremental   If true, performs an incremental stack update (state persists).
         * @return Final latent image (same size/channels as inputs).
         */
        cv::Mat imageMMCore(QVector<StackImageData> &subs, cv::Mat &latent, double &sigma,
                            const QVector<float> &weights, const LiveStackData &lsd, bool incremental);

        /**
         * @brief Initialize latent image if empty, using weighted mean of subs.
         * @param latent   image
         * @param subs     Vector of new subs
         * @param weights  Vector of normalised weights of subs
         */
        void imageMMInitializeLatent(cv::Mat &latent, const QVector<StackImageData> &subs,
                                     const QVector<float> &weights);

        /**
         * @brief Estimate the global noise scale (σ) of the ImageMM model using the Median Absolute Deviation (MAD) of residuals.
         * @param[in]  subs          Vector of subframes to analyze.
         * @param[in]  latent        Current latent (mean) image against which residuals are computed.
         * @param[in]  pixelSample   Pixel subsampling step (e.g., 4 means sample every 4th pixel).
         * @param[in]  frameSample   Maximum number of frames to include in the estimation.
         * @param[in]  sigmaScale    Scale multiplier applied to the final σ estimate.
         * @param[in]  prevSigma     Previous σ estimate (for temporal blending).
         * @param[in]  sigmaBlend    Blending factor between old and new σ (0.0 = only new, 1.0 = only old).
         * @return The updated global σ estimate, blended if applicable.
         */
        double imageMMEstimateSigma(const QVector<StackImageData> &subs, const cv::Mat &latent, int pixelSample,
                                    int frameSample, double sigmaScale, double prevSigma, double sigmaBlend);

        /**
         * @brief Accumulate per-subframe contributions for one color channel in the ImageMM iteration.
         * @param subs           Vector of subframe metadata (including PSFs and weights).
         * @param subsChannels   For each subframe, its color channels as cv::Mat images.
         * @param latentChannel  The current latent image estimate for this color channel.
         * @param normWeights    Optional normalized scalar weights per subframe (same size as `subs`).
         * @param sigma          Robustness parameter for residual weighting.
         * @param channelIndex   The current color channel index (0=R, 1=G, 2=B, etc).
         * @return A pair `{accumNum, accumDen}`:
         *         - `accumNum`: accumulated numerator term (Σ Fᵀ(w·y))
         *         - `accumDen`: accumulated denominator term (Σ Fᵀ(w·F·x))
         */
        std::pair<cv::Mat, cv::Mat> imageMMAccumulateChannel(const QVector<StackImageData> &subs,
                const std::vector<std::vector<cv::Mat>> &subsChannels, const cv::Mat &latentChannel,
                const QVector<float> &normWeights, double sigma, int channelIndex);

        /**
         * @brief Perform a multiplicative pixel-wise update to the latent image channel.
         * @param channel The latent image channel to be updated in place. Must be a
         *                single-channel `CV_32F` matrix (converted if not).
         * @param FiT_wi_y Per-frame matrices representing `Fiᵀ·(w·y)` for each subframe.
         * @param FiT_wi_Fix Per-frame matrices representing `Fiᵀ·(w·Fi·x)` for each subframe.
         * @param kappa The multiplicative update clamp factor. Each pixel update
         *              is restricted to the range `[1/kappa, kappa]`.
         */
        void imageMMPixelwiseUpdate(cv::Mat &channel, const std::vector<cv::Mat> &FiT_wi_y,
                                    const std::vector<cv::Mat> &FiT_wi_Fix, float kappa);

        /**
         * @brief Refine per-subframe PSFs using gradient-based optimization.
         * @param subs           Vector of subframe data. Each element must contain
         *                       a non-empty PSF kernel (`psfKernel`) and an image (`image`).
         * @param latent         Current latent model image used to estimate PSF gradients.
         * @param learningRate   Step size for PSF updates. Typically a small value (e.g., 1e-3–1e-2)
         *                       to prevent over-correction and maintain stability.
         */
        void imageMMRefinePSFs(QVector<StackImageData> &subs, const cv::Mat &latent, float learningRate);

        /**
         * @brief Apply a simple gradient-based update to the PSF kernel.
         * @param psf  [in,out] Current PSF kernel (`CV_32F`, normalized, non-negative).
         * @param grad [in] Gradient matrix of same size as `psf`, representing ∇L(psf).
         * @param lr   Learning rate (step size) controlling the strength of the update.
         */
        inline void imageMMUpdatePSF(cv::Mat &psf, const cv::Mat &grad, float lr);

        /**
         * @brief Compute relative change between two images.
         * @param a  Current image.
         * @param b  Previous image.
         * @return   Relative L2 norm difference (0 = identical, >1 = large change).
         */
        double imageMMComputeRelChange(const cv::Mat &a, const cv::Mat &b);

        /**
         * @brief Build a synthetic 2D Gaussian PSF kernel from a given HFR value.
         * @param hfr  Half-flux radius in pixels, describing the PSF width.
         * @return     Normalized 2D Gaussian PSF matrix (float type), or empty on error.
        */
        cv::Mat buildPSFFromHFR(const double hfr);

        /**
         * @brief Store the WCS for the stack image based on the WCS for the master alignment sub
         * @param wcs is the master alignment sub WCS
         */
        void setWCSStackImage(const QSharedPointer<wcsprm> &wcs);

        /**
         * @brief Post process the passed in stack
         * @param Image to process
         * @return Processed image
         */
        cv::Mat postProcessImage(const cv::Mat &image);

        /**
         * @brief Return the weights for each sub for the stacking process
         * @return weights
         */
        QVector<float> getWeights();

        /**
         * @brief Return the Signal-To-Noise ratio of the passed in image
         * @param image
         * @return SNR (0.0 on failure)
         */
        double getSNR(const cv::Mat &image);

        /**
         * @brief Return a PSF for stars (upto 20) in the passed in image
         * @param image
         * @param patchSize is the patch region around a star - multiple stars are ignored. Must be odd
         * @return PSF (empty Mat on failure)
         */
        cv::Mat calculatePSF(const cv::Mat &image, int patchSize = 21);

        /**
         * @brief Apply Wiener deconvolution to the passed in image, using psf
         * @param image
         * @param psf (previously calculated by calculatePSF)
         * @return deconvolved image (empty Mat on failure)
         */
        cv::Mat wienerDeconvolution(const cv::Mat &image, const cv::Mat &psf);

        /**
         * @brief Called to transition initial stack data to running stack
         * @param numSubs in the initial stack
         * @param totalWeight of subs processed so far
         */
        void setupRunningStack(const int numSubs, const float totalWeight);

        /**
         * @brief Called to update running stack data
         * @param numSubs processed so far
         * @param totalWeight of subs processed so far
         */
        void updateRunningStack(const int numSubs, const float totalWeight);

        /**
         * @brief Tidy up initial stack data, e.g. free heap
         */
        void tidyUpInitalStack();

        /**
         * @brief Tidy up running stack data, e.g. free heap
         */
        void tidyUpRunningStack();

        FITSData *m_Data;
        QSharedPointer<SolverUtils> m_Solver;
        bool m_ReadyToStack { false };
        QString m_BayerPattern;
        int m_BayerOffsetX { 0 };
        int m_BayerOffsetY { 0 };

        // Global incremental ImageMM state
        struct ImageMMState
        {
            cv::Mat accumNum;   // Σ Fᵀ(w·y)
            cv::Mat accumDen;   // Σ Fᵀ(w·F·x)
            cv::Mat latent;     // Current estimate x̂
            double sigma = 0.0; // Blended sigma
        };

        QVector < StackImageData > m_StackImageData;

        struct RunningStackImageData
        {
            int numSubs;
            double ref_hfr;
            int ref_numStars;
            float totalWeight;
            ImageMMState imageMMState;
            QVector < StackImageData > runningSubs;
        };
        RunningStackImageData m_RunningStackImageData { 0, -1.0, 0, 0.0, {}, {} };

        // SNR of subs
        double m_MeanSubSNR { 0 };
        double m_MinSubSNR { 0 };
        double m_MaxSubSNR { 0 };

        // Stack status
        bool m_StackInProgress { false };
        bool m_InitialStackDone { false };

        // Channel associated with this stack
        LiveStackChannel m_Channel;

        // Stack data user options
        LiveStackData m_StackData;

        // Calibration
        cv::Mat m_MasterDark;
        cv::Mat m_MasterFlatInv;

        // Aligning
        QSharedPointer < wcsprm > m_AlignMasterWCS;

        // Stacking
        cv::Mat m_StackedImage32F;
        QVector < cv::Mat > m_SigmaClip32FC4;
        cv::Mat m_StackedImageFinal;
        double m_ImageMMLastSigma = -1.0;
        float m_ImageMMTotalWeight = 0.0f;
        int m_ImageMMFrameCount = 0;
        cv::Mat m_ImageMMMean32F;
        cv::Mat m_ImageMMVar32F;

        // Stack Image
        struct wcsprm * m_WCSStackImage
        {
            nullptr
        };

        float m_Width { 0.0f };
        float m_Height { 0.0f };
        int m_Channels { 0 };
        int m_BytesPerPixel { 0 };
        int m_CVType { 0 };
};
