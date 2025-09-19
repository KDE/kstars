/*
    SPDX-FileCopyrightText: 2024 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "focus.h"
#include "fitsviewer/fitsdata.h"
#include "auxiliary/imagemask.h"
#include "auxiliary/robuststatistics.h"
#include "../ekos.h"
#include <ekos_focus_debug.h>
#include "opencv2/opencv.hpp"
#include "opencv2/imgcodecs.hpp"

namespace Ekos
{

// This class uses "blurriness" measures to calculate focus. The following are supported:
// Star Fields
// FOCUS_STAR_STDDEV     - This measure uses the variance / mean of pixels. JEE need to rename.
//
// Non-star fields: Lunar, Solar & Planetary
// FOCUS_STAR_SOBEL      - This measure uses the Sobel Edge Detection openCV algorithm. The measure is variance / mean.
// FOCUS_STAR_LAPLASSIAN - This measure uses the Laplassian openCV algorithm. The measure is mean ^ 2.
// FOCUS_STAR_CANNY      - This measure uses the Canny openCV algorithm. The measure is mean.
//
// This class uses openCV to perform calculations on the images.

using namespace cv;

class FocusBlurriness
{
    private:
        /**
             * @brief Get the openCV image datatype for the passed in fits image type
             * @param fits image type
             * @return openCV image type
             */
        int getCVType(const int type);

        /**
             * @brief Get the Mosaic Mask pointer if a mosaic mask is being used
             * @param mask pointer
             * @param tile
             * @return Mosaic Mask
             */
        ImageMosaicMask * getMosaicMask(const QSharedPointer<ImageMask> &mask, const int tile);

        /**
             * @brief Calculate Region of Interest for the passed in tile
             * @param tile
             * @param Mosaic Mask
             * @return openCV ROI
             */
        cv::Rect calcROIfromTile(const int tile, ImageMosaicMask *mosaicMask);

        /**
             * @brief Apply Ring Mask to Image
             * @param img is the image
             * @param width of image
             * @param height of image
             * @param ringMask
             */
        void applyRingMaskToImage(cv::Mat &img, const int width, const int height, ImageRingMask *ringMask);

    public:

        FocusBlurriness();
        ~FocusBlurriness();

        template <typename T>
        void processBlurriness(const T &imageBuffer, const QSharedPointer<FITSData> &imageData, const bool denoise,
                               const QSharedPointer<ImageMask> &mask, const int &tile, const QRect &roi, const Focus::StarMeasure starMeasure,
                               double *blurriness, double *weight)
        {
            // Initialise outputs
            *blurriness = INVALID_STAR_MEASURE;
            *weight = 1.0;

            // Check mask & tile inputs
            ImageMosaicMask *mosaicMask = nullptr;
            if (tile >= 0)
                mosaicMask = getMosaicMask(mask, tile);

            // openCV uses exceptions rather than error codes to communicate errors so enclose everything in try catch
            try
            {
                // Dimensions of area to perform Blurriness on; whole sensor or just tile
                auto stats = imageData->getStatistics();
                int width = stats.width;
                int height = stats.height;

                // Get the openCV datatype
                int cvType = getCVType(stats.dataType);
                if (cvType < 0)
                    return;

                // Setup  the image matrix
                size_t rowLen = width * stats.bytesPerPixel;
                cv::Mat img = cv::Mat(height, width, cvType, (void *) imageBuffer, rowLen);
                if (img.empty())
                {
                    qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 Unable to process image in openCV").arg(__FUNCTION__);
                    return;
                }

                // Convert colour images to greyscale
                if (img.channels() != 1)
                    cv::cvtColor(img, img, cv::COLOR_BGR2GRAY);

                // Now see if there are restrictions on the image size such as an ROI or a mask
                if (tile >= 0 || roi != QRect())
                {
                    // We are dealing with a ROI within the passed in image
                    cv::Rect cvROI;
                    if (tile >= 0)
                        cvROI = calcROIfromTile(tile, mosaicMask);
                    else
                        cvROI = cv::Rect(roi.x(), roi.y(), roi.width(), roi.height());

                    // Apply the ROI to the image
                    img = img(cvROI);
                    if (img.empty())
                    {
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 Unable to process ROI image in openCV").arg(__FUNCTION__);
                        return;
                    }
                }
                else if (mask)
                {
                    // Check for a ring mask and if set, apply to the image
                    ImageRingMask *ringMask = dynamic_cast<ImageRingMask *>(mask.get());
                    if (ringMask)
                        applyRingMaskToImage(img, width, height, ringMask);
                }

                cv::Mat img_8b, sobelx, sobely, result, abs_grad_x, abs_grad_y;
                cv::Scalar mean, sigma;
                double min;
                // Denoise (blur) the image for better edge detection
                if (denoise)
                    cv::GaussianBlur(img, img, Size(9, 9), 0, 0, BORDER_DEFAULT);

                switch (starMeasure)
                {
                    case Focus::FOCUS_STAR_STDDEV:
                        cv::meanStdDev(img, mean, sigma);
                        result = img.clone();
                        // JEE variance seems better than SD so may need to rename this method
                        *blurriness = sigma.val[0] * sigma.val[0] / mean.val[0];
                        break;
                    case Focus::FOCUS_STAR_SOBEL:
                        // Use a signed matrix otherwise half the negative gradients are lost
                        cv::Sobel(img, sobelx, CV_64F, 1, 0, 5);
                        cv::Sobel(img, sobely, CV_64F, 0, 1, 5);

                        if (m_maxX < 0.0)
                            cv::minMaxIdx(sobelx, &min, &m_maxX);
                        cv::convertScaleAbs(sobelx, abs_grad_x, 255 / m_maxX, 0);
                        if (m_maxY < 0.0)
                            cv::minMaxIdx(sobely, &min, &m_maxY);
                        cv::convertScaleAbs(sobely, abs_grad_y, 255 / m_maxY, 0);

                        cv::addWeighted(abs_grad_x, 0.5, abs_grad_y, 0.5, 0, result);
                        cv::meanStdDev(result, mean, sigma);
                        *blurriness = sigma.val[0] * sigma.val[0] / mean.val[0];
                        break;
                    case Focus::FOCUS_STAR_LAPLASSIAN:
                        cv::Laplacian(img, result, CV_64F, 3);
                        if (m_maxX < 0.0)
                            cv::minMaxIdx(result, &min, &m_maxX);
                        cv::convertScaleAbs(result, result, 255.0 / m_maxX, 0);
                        cv::meanStdDev(result, mean, sigma);
                        *blurriness = mean.val[0] * mean.val[0];
                        break;
                    case Focus::FOCUS_STAR_CANNY:
                        // Canny algorithm works on 8bit images
                        if (m_maxX < 0.0)
                            cv::minMaxIdx(img, &min, &m_maxX);
                        cv::convertScaleAbs(img, img, 255.0 / m_maxX, 0);
                        // The hysteresis variables min=100 and max=3*min seem to work OK
                        cv::Canny(img, result, 100, 300, 5, false);
                        cv::meanStdDev(result, mean, sigma);
                        *blurriness = mean.val[0];
                        break;
                    default:
                        qCDebug(KSTARS_EKOS_FOCUS) << QString("%1 called with unknown star measure %2").arg(__FUNCTION__).arg(starMeasure);
                        break;
                }
            }
            catch (const cv::Exception &ex)
            {
                qCDebug(KSTARS_EKOS_FOCUS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
                *blurriness = INVALID_STAR_MEASURE;
                *weight = 1.0;
            }
        }
        static double constexpr INVALID_STAR_MEASURE = -1.0;

    private:

        double m_maxX = -1.0;
        double m_maxY = -1.0;
};
}
