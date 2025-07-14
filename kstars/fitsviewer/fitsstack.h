/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"
#include "ekos/auxiliary/solverutils.h"
#include <fits_debug.h>

#include <QObject>
#include <QPointer>

#include "opencv2/opencv.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/video/tracking.hpp"
#include "opencv2/calib3d.hpp"
#include "opencv2/photo.hpp"

#include <wcs.h>

namespace Ekos
{
class StellarSolverProfileEditor;
}

/**
 * @brief The FITSStack class for Live Stacking within the Fitsviewer
 *        The functionality is as follows:
 *        1. A directory is selected and any subs already in the directory are stacked
 *          a) Each sub may be calibrated with a dark and / or a flat
 *          b) A reference frame is chosen as master for alignment.
 *          c) All subs are aligned to the master by plate solving each subs and using
 *             WCS for alignment.
 *          d) Subs are stacked. Modes are basic addition of pixel values or more complex
 *             statistical processing like sigma clipping or winsorized sigma clipping
 *          e) Basic post-processing options such as sharpening and denoising are offered
 *        2. The stack is displayed in the FITSViewer.
 *        3. As new subs are added to the directory these are detected by FitsDirWatcher
 *        4. The new sub(s) are added to the already existing stack.
 *        5. The new stack is post-processed and displayed.
 *
 * @author John Evans
 */
class FITSStack : public QObject
{
        Q_OBJECT

    public:
        explicit FITSStack(FITSData *parent, LiveStackData params);
        virtual ~FITSStack() override;

        /**
         * @brief Prepare FITSStack for the next image sub. Call before addSub.
         */
        void setupNextSub();

        /**
         * @brief add an image sub to the stack.
         * @param imageBuffer is the in-memory buffer
         * @param cvType is the openCV Mat type
         * @param width is image width
         * @param height is image height
         * @param bytesPerPixel
         * @return success
         */
        bool addSub(void *imageBuffer, const int cvType, const int width, const int height, const int bytesPerPixel);

        /**
         * @brief add a master dark or flat.
         * @param dark (or flat)
         * @param image buffer
         * @param width is master width
         * @param height is master height
         * @param bytesPerPixel. Set to zero to skip this check
         * @param cvType
         */
        void addMaster(const bool dark, void *imageBuffer, const int width, const int height, const int bytesPerPixel, const int cvType);

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
         * @brief Perform admin within FITSStack for case where we couldn't add an image.
         */
        void addSubFailed();

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
         * @brief Get the WCS data structure for the reference alignment frame
         * @return wcsprm pointer to reference alignment frame
         */
        struct wcsprm * getWCSRef();

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
         * @brief Get the stacked image
         * @return stacked image
         */
        QByteArray getStackedImage() const
        {
            return (m_StackedBuffer) ? *m_StackedBuffer : QByteArray();
        }

        /**
         * @brief Determine whether a stacked image exists
         * @return exists (or not)
         */
        bool isStackedImageEmpty() const
        {
            return !m_StackedBuffer || m_StackedBuffer->isEmpty();
        }

        void resetStackedImage();

        void setBayerPattern(const QString pattern, const int offsetX, const int offsetY);

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

        const double &getStackSNR() const
        {
            return m_StackSNR;
        }

    signals:
        void stackChanged();

    public slots:
    private:      
        typedef enum
        {
            PLATESOLVE_IN_PROGRESS,
            PLATESOLVE_FAILED,
            CALIBRATION_FAILED,
            ALIGNMENT_FAILED,
            OK
        } Status;

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
         * @brief Gets the downscaling factor for the passed downscale
         * @param downscale
         * @return downscale factor
         */
        int getDownscaleFactor(LiveStackDownscale downscale);

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
         * @brief Convert passed in Mat to FITS format
         * @param image
         * @return success (or not)
         */
        bool convertMatToFITS(const cv::Mat &image);

        /**
         * @brief Calibrate the passed in sub
         * @param sub to be calibrated
         * @return success (or not)
         */
        bool calibrateSub(cv::Mat &sub);

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
         * @param Reference frame WCS
         * @param numSubs in the initial stack
         * @param totalWeight of subs processed so far
         */
        void setupRunningStack(struct wcsprm * refWCS, const int numSubs, const float totalWeight);

        /**
         * @brief Called to update running stack data
         * @param numSubs processed so far
         * @param totalWeight of subs processed so far
         */
        void updateRunningStack(const int numSubs, const float totalWeight);

        /**
         * @brief Tidy up initial stack data, e.g. free heap
         * @param refWCS
         */
        void tidyUpInitalStack(struct wcsprm * refWCS);

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

        typedef struct
        {
            cv::Mat image;
            Status status;
            bool isCalibrated;
            bool isAligned;
            struct wcsprm * wcsprm;
            double hfr;
            int numStars;
        } StackImageData;
        QVector<StackImageData> m_StackImageData;

        typedef struct
        {
            int numSubs;
            struct wcsprm * ref_wcsprm;
            double ref_hfr;
            int ref_numStars;
            float totalWeight;
        } RunningStackImageData;
        RunningStackImageData m_RunningStackImageData { 0, nullptr, -1.0, 0, 0.0 };

        // SNR of subs
        double m_MeanSubSNR { 0 };
        double m_MinSubSNR { 0 };
        double m_MaxSubSNR { 0 };

        // Stack status
        bool m_StackInProgress { false };
        bool m_InitialStackDone { false };

        // Stack data user options
        LiveStackData m_StackData;

        // Calibration
        cv::Mat m_MasterDark;
        cv::Mat m_MasterFlat;

        // Aligning
        int m_InitialStackRef = -1;

        // Stacking
        cv::Mat m_StackedImage32F;
        QVector<cv::Mat> m_SigmaClip32FC4;
        QSharedPointer<QByteArray> m_StackedBuffer { nullptr };

        double m_StackSNR { 0.0 };
        float m_Width { 0.0f };
        float m_Height { 0.0f };
        int m_Channels { 0 };
        int m_BytesPerPixel { 0 };
        int m_CVType { 0 };
};
