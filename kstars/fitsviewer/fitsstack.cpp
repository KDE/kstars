/*
    SPDX-FileCopyrightText: 2025 John Evans <john.e.evans.email@googlemail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include <QtConcurrent>

#include "fitsstack.h"
#include "fitsdata.h"
#include <fits_debug.h>
#include "fitscommon.h"
#include "ekos/auxiliary/solverutils.h"
#include "kstars.h"
#include "../auxiliary/robuststatistics.h"

#include <wcshdr.h>
#include <fitsio.h>

FITSStack::FITSStack(FITSData *parent, LiveStackData params) : QObject(parent)
{
    m_Data = parent;
    m_StackData = params;
}

FITSStack::~FITSStack()
{
    tidyUpInitalStack(nullptr);
    tidyUpRunningStack();
    if (m_WCSStackImage)
    {
        wcsfree(m_WCSStackImage);
        m_WCSStackImage = nullptr;
    }
}

void FITSStack::setStackInProgress(bool inProgress)
{
    m_StackInProgress = inProgress;
}

void FITSStack::resetStackedImage()
{
    m_StackedBuffer.reset();
}

void FITSStack::setInitalStackDone(bool done)
{
    m_InitialStackDone = done;
}

void FITSStack::setBayerPattern(const QString pattern, const int offsetX, const int offsetY)
{
    m_BayerPattern = pattern;
    m_BayerOffsetX = offsetX;
    m_BayerOffsetY = offsetY;
}

// Setup the image data structure for later processing
void FITSStack::setupNextSub()
{
    StackImageData imageData;
    imageData.image = cv::Mat();
    imageData.status = PLATESOLVE_IN_PROGRESS;
    imageData.isCalibrated = false;
    imageData.isAligned = false;
    imageData.wcsprm = nullptr;
    imageData.hfr = -1;
    imageData.numStars = 0;
    m_StackImageData.push_back(imageData);
}

bool FITSStack::addSub(void * imageBuffer, const int cvType, const int width, const int height, const int bytesPerPixel)
{
    try
    {
        int channels = CV_MAT_CN(cvType);
        int depth = CV_MAT_DEPTH(cvType);

        cv::Mat image;
        if (channels == 3)
        {
            // Colour image. The image buffer is in planar format (all red pixels, then all green, etc.
            // openCV wants data interleaved R1G1B1R2G2B2R3....
            switch (depth)
            {
                case CV_8U:
                    image = convertToCV(reinterpret_cast<uint8_t *>(imageBuffer), width, height);
                    break;
                case CV_16U:
                    image = convertToCV(reinterpret_cast<uint16_t *>(imageBuffer), width, height);
                    break;
                case CV_32F:
                    image = convertToCV(reinterpret_cast<float *>(imageBuffer), width, height);
                    break;
                case CV_64F:
                    image = convertToCV(reinterpret_cast<double *>(imageBuffer), width, height);
                    break;
                default:
                    qCDebug(KSTARS_FITS) << QString("%1 Unsupported openCV datatype %2").arg(__FUNCTION__).arg(cvType);
                    return false;
            }
        }
        else
        {
            // Mono so we can just load up the image
            size_t rowLen = width * bytesPerPixel * channels;
            image = cv::Mat(height, width, cvType, imageBuffer, rowLen);
        }
        if (image.empty())
        {
            qCDebug(KSTARS_FITS) << QString("%1 Unable to process image in openCV").arg(__FUNCTION__);
            return false;
        }

        // Convert Mat to float and downscale if required
        cv::Mat newImage;
        if (!convertMat(image, newImage))
            return false;

        // Check the image is the correct shape
        if (!checkSub(newImage.cols, newImage.rows, bytesPerPixel, channels))
            return false;

        double snr = getSNR(newImage);
        if (snr > 0.0)
        {
            m_MaxSubSNR = std::max(m_MaxSubSNR, snr);
            m_MinSubSNR = (m_MinSubSNR > 0.0) ? std::min(m_MinSubSNR, snr) : snr;
            int subs = m_StackImageData.size();
            if (getInitialStackDone())
                subs += m_RunningStackImageData.numSubs;
            m_MeanSubSNR = ((m_MeanSubSNR * (subs - 1)) + snr) / subs;
        }

        m_StackImageData.last().image = newImage;
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }
    return false;
}

void FITSStack::addMaster(const bool dark, void * imageBuffer, const int width, const int height,
                          const int bytesPerPixel, const int cvType)
{
    try
    {
        if (dark)
            m_MasterDark.release();
        else
            m_MasterFlat.release();

        int channels = CV_MAT_CN(cvType);

        cv::Mat image;
        if (channels == 3)
        {
            // Colour image. The image buffer is in planar format (all red pixels, then all green, etc.
            // openCV wants data interleaved R1G1B1R2G2B2R3....
            int depth = CV_MAT_DEPTH(cvType);
            switch (depth)
            {
                case CV_8U:
                    image = convertToCV(reinterpret_cast<uint8_t *>(imageBuffer), width, height);
                    break;
                case CV_16U:
                    image = convertToCV(reinterpret_cast<uint16_t *>(imageBuffer), width, height);
                    break;
                case CV_32F:
                    image = convertToCV(reinterpret_cast<float *>(imageBuffer), width, height);
                    break;
                case CV_64F:
                    image = convertToCV(reinterpret_cast<double *>(imageBuffer), width, height);
                    break;
                default:
                    qCDebug(KSTARS_FITS) << QString("%1 Unsupported openCV datatype %2").arg(__FUNCTION__).arg(cvType);
                    return;
            }
        }
        else
        {
            // Mono so we can just load up the image
            size_t rowLen = width * bytesPerPixel * channels;
            image = cv::Mat(height, width, cvType, imageBuffer, rowLen);
        }
        if (image.empty())
        {
            qCDebug(KSTARS_FITS) << QString("%1 Unable to process master file").arg(__FUNCTION__);
            return;
        }

        // Convert Mat to float and downscale if required
        cv::Mat imageClone;
        if (!convertMat(image, imageClone))
            return;

        // Check the image is the correct shape - pass 0 for bytesPerPixel to skip check
        // This allows masters to be different datatypes to subs
        if (!checkSub(imageClone.cols, imageClone.rows, 0, channels))
            return;

        if (dark)
        {
            m_MasterDark = imageClone;

            // If the dark has been normalised to 0-1 then we need to increase values so they match the subs
            double minVal, maxVal;
            cv::minMaxLoc(m_MasterDark, &minVal, &maxVal);
            if (maxVal <= 1.0)
            {
                if (m_BytesPerPixel == 1)
                    m_MasterDark *= 255;
                else if (m_BytesPerPixel == 2)
                    m_MasterDark *= 65535;
            }
        }
        else
        {
            m_MasterFlat = imageClone;

            // Scale the flat down using the median value (note that this also takes care of normalised flats 0-1
            std::vector<cv::Mat> channels;
            cv::split(m_MasterFlat, channels);

            for (unsigned int c = 0; c < channels.size(); c++)
            {
                std::vector<float> values;
                values.assign((float*)channels[c].data, (float*)channels[c].data + channels[c].total());

                float median = Mathematics::RobustStatistics::ComputeLocation(
                                                Mathematics::RobustStatistics::LOCATION_MEDIAN, values);

                if (median <= 0.0f)
                    qCDebug(KSTARS_FITS) << QString("%1 Unable to calculate median of Master flat channel %2")
                                                .arg(__FUNCTION__).arg(c);
                else
                {
                    channels[c] /= median;
                    // Make sure no zero or very small values that will later give problems when dividing by the flat
                    cv::max(channels[c], 0.1f, channels[c]);
                }
            }
            cv::merge(channels, m_MasterFlat);
        }
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }
}

// Converts the input Mat to float - our standard internal type
// If required, downscales the Mat (for faster processing)
bool FITSStack::convertMat(const cv::Mat &input, cv::Mat &output)
{
    try
    {
        cv::Mat image;
        // Convert the Mat to float type for upcoming calcs. This is our standard internal processing type
        input.convertTo(output, CV_MAKETYPE(CV_32F, input.channels()));

        if (m_StackData.downscale != LS_DOWNSCALE_NONE)
        {
            // Downscale image (if required). Less data = faster...
            double downscaleFactor = getDownscaleFactor();

            cv::Mat downsizedImage;
            int newWidth = output.cols / downscaleFactor;
            int newHeight = output.rows / downscaleFactor;
            cv::resize(output, downsizedImage, cv::Size(newWidth, newHeight), 0, 0, cv::INTER_AREA);
            output = downsizedImage;
        }
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
}

double FITSStack::getDownscaleFactor()
{
    double factor = 1.0;
    if (m_StackData.downscale == LS_DOWNSCALE_2X)
        factor = 2.0;
    else if (m_StackData.downscale == LS_DOWNSCALE_3X)
        factor = 3.0;
    else if (m_StackData.downscale == LS_DOWNSCALE_4X)
        factor = 4.0;
    return factor;
}

// Check that the passed in sub or master is the same shape as the others
bool FITSStack::checkSub(const int width, const int height, const int bytesPerPixel, const int channels)
{
    try
    {
        if (m_Width == 0)
            m_Width = width;
        else if (m_Width != width)
        {
            qCDebug(KSTARS_FITS) << QString("%1 Images have inconsistent widths").arg(__FUNCTION__);
            return false;
        }

        if (m_Height == 0)
            m_Height = height;
        else if (m_Height != height)
        {
            qCDebug(KSTARS_FITS) << QString("%1 Images have inconsistent heights").arg(__FUNCTION__);
            return false;
        }

        if (m_Channels == 0)
            m_Channels = channels;
        else if (m_Channels != channels)
        {
            qCDebug(KSTARS_FITS) << QString("%1 Images have inconsistent channels").arg(__FUNCTION__);
            return false;
        }

        if (bytesPerPixel > 0)
        {
            // Skip the check if bytesPerPixel set to 0, e.g. to allow master flat to be different to subs
            if (m_BytesPerPixel == 0)
                m_BytesPerPixel = bytesPerPixel;
            else if (m_BytesPerPixel != bytesPerPixel)
            {
                qCDebug(KSTARS_FITS) << QString("%1 Images have inconsistent bytes per pixel").arg(__FUNCTION__);
                return false;
            }
        }

        // Now setup the target CVTYPE for use in stacking calculations - use 32bit floating
        if (m_CVType == 0)
            m_CVType = CV_MAKETYPE(CV_32F, channels);
        else if (m_CVType != CV_MAKETYPE(CV_32F, channels))
        {
            qCDebug(KSTARS_FITS) << QString("%1 Images have inconsistent CVTypes").arg(__FUNCTION__);
            return false;
        }
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }
    return false;
}

// Update plate solving status
bool FITSStack::solverDone(const wcsprm * wcsHandle, const bool timedOut, const bool success, const double hfr, const int numStars)
{
    if (m_StackImageData.size() <= 0)
    {
        // This shouldn't happen
        qCDebug(KSTARS_FITS) << "Solver done called but no m_StackImageData";
        return false;
    }

    if (timedOut || !success)
    {
        m_StackImageData.last().status = PLATESOLVE_FAILED;
        return false;
    }

    m_StackImageData.last().status = OK;

    // Take a deep copy of the WCS state for alignment purposes
    struct wcsprm * wcsCopy = new struct wcsprm;
    wcsCopy->flag = -1; // Allocate space
    int status = 0;
    if ((status = wcssub(1, wcsHandle, 0x0, 0x0, wcsCopy)) != 0)
    {
        m_StackImageData.last().status = PLATESOLVE_FAILED;
        qCDebug(KSTARS_FITS) << QString("wcssub error processing %1 %2").arg(status).arg(wcs_errmsg[status]);
        return false;
    }
    else if ((status = wcsset(wcsCopy)) != 0)
    {
        m_StackImageData.last().status = PLATESOLVE_FAILED;
        qCDebug(KSTARS_FITS) << QString("wcsset error processing %1 %2").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    m_StackImageData.last().wcsprm = wcsCopy;
    m_StackImageData.last().hfr = hfr;
    m_StackImageData.last().numStars = numStars;
    return true;
}

// Couldn't add an image to be stacked for some reason so complete the admin needed
void FITSStack::addSubFailed()
{
    if (m_StackImageData.size() <= 0)
    {
        // This shouldn't happen
        qCDebug(KSTARS_FITS) << "addSubFailed called but no m_StackImageData";
        return;
    }

    m_StackImageData.last().status = PLATESOLVE_FAILED;
}

// Perform the initial stack
bool FITSStack::stack()
{
    try
    {
        for(int i = 0; i < m_StackImageData.size(); i++)
        {
            // Ignore any bad subs
            if (m_StackImageData[i].status != OK)
                continue;

            // Calibrate sub
            if (!m_StackImageData[i].isCalibrated)
            {
                if (calibrateSub(m_StackImageData[i].image))
                    m_StackImageData[i].isCalibrated = true;
                else
                {
                    m_StackImageData[i].status = CALIBRATION_FAILED;
                    continue;
                }
            }

            if (m_InitialStackRef < 0)
            {
                // First image ise reference  thto which others are aligned
                m_InitialStackRef = i;
                m_StackImageData[i].isAligned = true;
                setWCSStackImage(m_StackImageData[i].wcsprm);
            }
            else if (!m_StackImageData[i].isAligned)
            {
                // Align this image to the reference image
                cv::Mat warp, warpedImage;
                if (!calcWarpMatrix(m_StackImageData[m_InitialStackRef].wcsprm, m_StackImageData[i].wcsprm, warp))
                    m_StackImageData[i].status = ALIGNMENT_FAILED;
                else
                {
                    cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp,
                                        m_StackImageData[i].image.size(), cv::INTER_LANCZOS4);
                    m_StackImageData[i].image = warpedImage;
                    m_StackImageData[i].isAligned = true;
                }
            }
        }
        // Stack the aligned subs
        float totalWeight = 0.0;
        stackSubs(true, totalWeight, m_StackedImage32F);

        if (m_StackData.numInMem <= m_StackImageData.size())
        {
            // We've completed the initial stack so perform post processing such as sharpening / denoising
            cv::Mat finalImage = postProcessImage(m_StackedImage32F);
            m_StackSNR = getSNR(finalImage);
            convertMatToFITS(finalImage);
            // Move to incremental stacking as new subs arrive
            setupRunningStack(m_StackImageData[m_InitialStackRef].wcsprm, m_StackImageData.size(), totalWeight);
        }
        else
        {
            // Still more subs to stack so skip post-processing which is time consuming
            m_StackSNR = getSNR(m_StackedImage32F);
            convertMatToFITS(m_StackedImage32F);
        }

        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
}

// Add 'n' new images to pre-existing stack
bool FITSStack::stackn()
{
    try
    {
        for(int i = 0; i < m_StackImageData.size(); i++)
        {
            // Ignore any bad subs
            if (m_StackImageData[i].status != OK)
                continue;

            // Calibrate sub
            if (!m_StackImageData[i].isCalibrated)
            {
                if (calibrateSub(m_StackImageData[i].image))
                    m_StackImageData[i].isCalibrated = true;
                else
                {
                    m_StackImageData[i].status = CALIBRATION_FAILED;
                    continue;
                }
            }

            // Align this image to the reference image
            cv::Mat warp, warpedImage;
            if (!calcWarpMatrix(m_RunningStackImageData.ref_wcsprm, m_StackImageData[i].wcsprm, warp))
                m_StackImageData[i].status = ALIGNMENT_FAILED;
            else
            {
                cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp, m_StackImageData[i].image.size(),
                                    cv::INTER_LANCZOS4);
                m_StackImageData[i].image = warpedImage;
                m_StackImageData[i].isAligned = true;
            }
        }
        // Stack the aligned subs
        float totalWeight = m_RunningStackImageData.totalWeight;
        if (stackSubs(false, totalWeight, m_StackedImage32F))
        {
            // Perform any post stacking processing such as sharpening / denoising
            cv::Mat finalImage = postProcessImage(m_StackedImage32F);
            m_StackSNR = getSNR(finalImage);
            convertMatToFITS(finalImage);
        }
        updateRunningStack(m_StackImageData.size(), totalWeight);
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
    return true;
}

// Calculate the warp matrix to align image2 to image1
bool FITSStack::calcWarpMatrix(struct wcsprm * wcs1, struct wcsprm * wcs2, cv::Mat &warp)
{
    try
    {
        double X = m_Width - 1.0;
        double Y = m_Height - 1.0;

        // Define corners and centre of image 1 in pixels
        std::vector<cv::Point2d> corners1;
        corners1.push_back(cv::Point2d(0.0, 0.0));
        corners1.push_back(cv::Point2d(X, 0.0));
        corners1.push_back(cv::Point2d(X, Y));
        corners1.push_back(cv::Point2d(0.0, Y));
        corners1.push_back(cv::Point2d(X / 2.0, Y / 2.0));

        // Convert pix points to world coordinates of image 1
        double imgcrd[2], phi, theta, world[2], pixcrd[2];
        int status, stat[2];
        std::vector<cv::Point2d> worldCoords1;
        for (unsigned int i = 0; i < corners1.size(); i++)
        {
            pixcrd[0] = corners1[i].x;
            pixcrd[1] = corners1[i].y;
            if ((status = wcsp2s(wcs1, 1, 2, pixcrd, imgcrd, &phi, &theta, world, stat)) != 0)
                qCDebug(KSTARS_FITS) << QString("WCS wcsp2s error %1: %2").arg(status).arg(wcs_errmsg[status]);
            worldCoords1.push_back(cv::Point2d(world[0], world[1]));
        }

        // Convert world coordinates to pixel coordinates in image 2
        std::vector<cv::Point2d> corners2;
        for (unsigned int i = 0; i < worldCoords1.size(); i++)
        {
            world[0] = worldCoords1[i].x;
            world[1] = worldCoords1[i].y;
            if ((status = wcss2p(wcs2, 1, 2, world, &phi, &theta, imgcrd, pixcrd, stat)) != 0)
                qCDebug(KSTARS_FITS) << QString("WCS wcss2p error %1: %2").arg(status).arg(wcs_errmsg[status]);
            corners2.push_back(cv::Point2d(pixcrd[0], pixcrd[1]));
        }

        // Compute the homography matrix using OpenCV to go from image 2 to image 1 (reference)
        warp = cv::findHomography(corners2, corners1, 0);
        if (warp.empty())
        {
            qCDebug(KSTARS_FITS) << QString("openCV findHomography warp matrix empty");
            return false;
        }

        // If we are downscaling the image we need to adjust the warp matrix which is calculated from the un-downscaled images
        if (m_StackData.downscale != LS_DOWNSCALE_NONE)
        {
            double scale = 1.0 / getDownscaleFactor();
            cv::Mat S = (cv::Mat_<double>(3,3) <<
                         scale, 0,     0,
                         0,     scale, 0,
                         0,     0,     1 );
            cv::Mat S_inv;
            cv::invert(S, S_inv);
            warp = S * warp * S_inv;
        }
        // Uncomment to display warp matrix - useless for debugging alignment issues
        cv::Ptr<cv::Formatter> fmt = cv::Formatter::get(cv::Formatter::FMT_DEFAULT);
        std::cout << fmt->format(warp) << std::endl;
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
}

// Calibrate the passed in sub with an associated Dark (if available) and / or Flat (if available)
bool FITSStack::calibrateSub(cv::Mat &sub)
{
    try
    {
        if (sub.empty())
            return false;

        // Dark subtraction (make sure no negative pixels)
        if (!m_MasterDark.empty())
        {
            sub -= m_MasterDark;
            cv::max(sub, 0.0f, sub);
        }

        // Flat calibration
        if (!m_MasterFlat.empty())
            sub /= m_MasterFlat;
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }
    return false;
}

// Stack the vector of subs
bool FITSStack::stackSubs(const bool initial, float &totalWeight, cv::Mat &stack)
{
    try
    {
        // Remove any bad subs so m_StackImageData just contains good data
        for (int i = m_StackImageData.size() - 1; i >= 0; i--)
        {
            if (m_StackImageData[i].status != OK)
                m_StackImageData.remove(i);
        }

        if (m_StackImageData.size() <= 0)
            return false;

        QVector<float> weights = getWeights();

        if (m_StackData.rejection == LS_STACKING_REJ_SIGMA || m_StackData.rejection == LS_STACKING_REJ_WINDSOR)
        {
            if (initial)
                stack = stackSubsSigmaClipping(weights);
            else
                stack = stacknSubsSigmaClipping(weights);
        }
        else
        {
            // Add the pixels weighted per sub based on user setting. Then divide by the total weight
            // If its an initial stack then just use the subs, if not then include the existing partial stack
            if (initial)
            {
                totalWeight = 0.0;
                stack = cv::Mat::zeros(m_StackImageData[0].image.rows, m_StackImageData[0].image.cols, m_CVType);
            }
            else
            {
                totalWeight = m_RunningStackImageData.totalWeight;
                stack = m_StackedImage32F * totalWeight;
            }

            for (int sub = 0; sub < m_StackImageData.size(); sub++)
            {
                stack += m_StackImageData[sub].image * weights[sub];
                totalWeight += weights[sub];
            }
            stack /= totalWeight;
        }
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
}

// Get the weight for each sub for the stacking process
QVector<float> FITSStack::getWeights()
{
    QVector<float> weights(m_StackImageData.size());

    for (int i = 0; i < weights.size(); i++)
    {
        switch (m_StackData.weighting)
        {
            case LS_STACKING_EQUAL:
                weights[i] = 1.0;
                break;
            case LS_STACKING_HFR:
                if (m_StackImageData[i].hfr > 0.0)
                    weights[i] = 1.0 / m_StackImageData[i].hfr;
                else
                    weights[i] = 1.0;
                break;
            case LS_STACKING_NUM_STARS:
                if (m_StackImageData[i].numStars > 0)
                    weights[i] = m_StackImageData[i].numStars;
                else
                    weights[i] = 1.0;
                break;
            default:
                qCDebug(KSTARS_FITS) << QString("Error calculating weights in %1").arg(__FUNCTION__);
                weights[i] = 1.0;
        }
    }
    return weights;
}

// Function to stack subs using standard or Windsorized Sigma Clipping
// Uses parallel processing to increase speed
cv::Mat FITSStack::stackSubsSigmaClipping(const QVector<float> &weights)
{
    try
    {
        if (m_StackImageData.size() != weights.size())
        {
            qCDebug(KSTARS_FITS) << QString("Inconsistent subs and weights in %1").arg(__FUNCTION__);
            return cv::Mat();
        }

        int rows = m_StackImageData[0].image.rows;
        int cols = m_StackImageData[0].image.cols;
        int numImages = m_StackImageData.size();
        cv::Mat finalImage = cv::Mat::zeros(rows, cols, m_CVType);
        float *finalImagePtr;

        // Setup structure for each channel for future sigma clipping
        m_SigmaClip32FC4.clear();
        m_SigmaClip32FC4.resize(m_Channels);
        for (int ch = 0; ch < m_Channels; ch++)
            m_SigmaClip32FC4[ch] = cv::Mat::zeros(rows, cols, CV_32FC4);
        QVector<cv::Vec4f *> sigmaClipPtr(m_Channels);

        // If all subs are continuous so we can treat as 1D arrays to speed things up
        bool continuous = finalImage.isContinuous() &&
                          std::all_of(m_SigmaClip32FC4.begin(), m_SigmaClip32FC4.end(),
                                      [](const cv::Mat &mat) { return mat.isContinuous(); }) &&
                          std::all_of(m_StackImageData.begin(), m_StackImageData.end(),
                                      [](const StackImageData &data) {return data.image.isContinuous(); });
        if (continuous)
        {
            // We can flatten the 2D image to 1D for efficiency and also use parallel processing
            cols *= rows;
            rows = 1;

            // Chunk up for available threads. Tried multipliers of 1, 2, 3, 4, 6, 8. Not a big difference by 2 was best
            const int chunkSize = std::max(1, cols / (QThread::idealThreadCount() * 2));

            QVector<QPair<int, int>> pixelChunks;
            for (int start = 0; start < cols; start += chunkSize)
            {
                int end = std::min(start + chunkSize, cols);
                pixelChunks.append(qMakePair(start, end));
            }

            qCDebug(KSTARS_FITS) << QString("Starting sigma clipping: %1 chunks on %2 threads")
                                                .arg(pixelChunks.size()).arg(QThread::idealThreadCount());

            // Get pointers once (since rows=1)
            std::vector<const float *> imagesPtrs(numImages);
            for (int i = 0; i < numImages; i++)
                imagesPtrs[i] = m_StackImageData[i].image.ptr<float>(0);

            float* finalImagePtr = finalImage.ptr<float>(0);
            QVector<cv::Vec4f *> sigmaClipPtr(m_Channels);
            for (int ch = 0; ch < m_Channels; ch++)
                sigmaClipPtr[ch] = m_SigmaClip32FC4[ch].ptr<cv::Vec4f>(0);

            // Setup the function for parallel processing to handle a chunk of pixels
            auto processPixelChunk = [&](const QPair<int, int>& chunk)
            {
                for (int x = chunk.first; x < chunk.second; x++)
                {
                    // Cancellation check every once per 100 iterations
                    if ((x - chunk.first) % 100 == 0 && QThread::currentThread()->isInterruptionRequested())
                        return;

                    // Process the pixel
                    stackSigmaClipPixel(x, imagesPtrs, finalImagePtr, sigmaClipPtr, weights);
                }
            };

            QtConcurrent::blockingMap(pixelChunks, processPixelChunk);
        }
        else
        {
            qCDebug(KSTARS_FITS) << QString("Starting single thread sigma clipping");

            std::vector<float> values(numImages);

            // Process each pixel position
            std::vector<const float *> imagesPtrs(numImages);
            for (int y = 0; y < rows; y++)
            {
                // Update ptrs for current y
                for (int i = 0; i < numImages; i++)
                    imagesPtrs[i] = m_StackImageData[i].image.ptr<float>(y);

                finalImagePtr = finalImage.ptr<float>(y);
                for (int ch = 0; ch < m_Channels; ch++)
                    sigmaClipPtr[ch] = m_SigmaClip32FC4[ch].ptr<cv::Vec4f>(y);

                for (int x = 0; x < cols; x++)
                {
                    // Process the pixel
                    stackSigmaClipPixel(x, imagesPtrs, finalImagePtr, sigmaClipPtr, weights);

                    // Collect values for this pixel/channel from all images
                    for (int ch = 0; ch < m_Channels; ch++)
                    {
                        for (int image = 0; image < numImages; image++)
                            values[image] = imagesPtrs[image][x * m_Channels + ch];

                        float pixelValue = 0.0;

                        if (m_StackData.rejection == LS_STACKING_REJ_WINDSOR)
                        {
                            // Winsorize the data
                            float median = Mathematics::RobustStatistics::ComputeLocation(
                                                    Mathematics::RobustStatistics::LOCATION_MEDIAN, values);
                            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                                                    Mathematics::RobustStatistics::SCALE_VARIANCE, values));

                            float lower = std::max(0.0, median - (stddev * m_StackData.windsorCutoff));
                            float upper = median + (stddev * m_StackData.windsorCutoff);

                            for (unsigned int i = 0; i < values.size(); i++)
                            {
                                if (values[i] < lower)
                                    values[i] = lower;
                                else if (values[i] > upper)
                                    values[i] = upper;
                            }
                        }

                        // Now process the data
                        float median = Mathematics::RobustStatistics::ComputeLocation(
                                                    Mathematics::RobustStatistics::LOCATION_MEDIAN, values);

                        float sum = 0.0, weightSum = 0.0, lower = -1.0, upper = -1.0;
                        if (values.size() <= 3)
                            // For small samples just use median
                            pixelValue = median;
                        else
                        {
                            // Sigma clipping
                            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                                                    Mathematics::RobustStatistics::SCALE_VARIANCE, values));

                            // Get the lower and upper bounds
                            lower = std::max(0.0, median - (stddev * m_StackData.lowSigma));
                            upper = median + (stddev * m_StackData.highSigma);

                            for (unsigned int i = 0; i < values.size(); i++)
                            {
                                if (values[i] < lower || values[i] > upper)
                                    continue;

                                sum += values[i] * weights[i];
                                weightSum += weights[i];
                            }

                            if (weightSum > 0.0)
                                pixelValue = sum / weightSum;
                            else
                                pixelValue = median;
                        }
                        // Store intermediate calcs from this process, necessary for processing new subs
                        cv::Vec4f sigmaClip;
                        sigmaClip[0] = lower;
                        sigmaClip[1] = upper;
                        sigmaClip[2] = sum;
                        sigmaClip[3] = weightSum;
                        sigmaClipPtr[ch][x] = sigmaClip;

                        // Update the pixel/channel with the calculated value
                        finalImagePtr[x * m_Channels + ch] = pixelValue;
                    }
                }
            }
        }
        return finalImage;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return cv::Mat();
    }
}

// This function does the pixel level sigma clipping and Winsorization
void FITSStack::stackSigmaClipPixel(int x, const std::vector<const float *> &imagesPtrs, float* finalImagePtr,
                                    const QVector<cv::Vec4f *> &sigmaClipPtr, const QVector<float> &weights)
{
    int numImages = imagesPtrs.size();
    std::vector<float> values(numImages);
    for (int ch = 0; ch < m_Channels; ch++)
    {
        for (int image = 0; image < numImages; image++)
            values[image] = imagesPtrs[image][x * m_Channels + ch];

        float pixelValue = 0.0;

        if (m_StackData.rejection == LS_STACKING_REJ_WINDSOR)
        {
            // Winsorize the data
            float median = Mathematics::RobustStatistics::ComputeLocation(
                Mathematics::RobustStatistics::LOCATION_MEDIAN, values);
            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                Mathematics::RobustStatistics::SCALE_VARIANCE, values));

            float lower = std::max(0.0, median - (stddev * m_StackData.windsorCutoff));
            float upper = median + (stddev * m_StackData.windsorCutoff);

            for (unsigned int i = 0; i < values.size(); i++)
            {
                if (values[i] < lower)
                    values[i] = lower;
                else if (values[i] > upper)
                    values[i] = upper;
            }
        }

        // Now process the data
        float median = Mathematics::RobustStatistics::ComputeLocation(
            Mathematics::RobustStatistics::LOCATION_MEDIAN, values);

        float sum = 0.0, weightSum = 0.0, lower = -1.0, upper = -1.0;
        if (values.size() <= 3)
            // For small samples just use median
            pixelValue = median;
        else
        {
            // Sigma clipping
            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                Mathematics::RobustStatistics::SCALE_VARIANCE, values));

            // Get the lower and upper bounds
            lower = std::max(0.0, median - (stddev * m_StackData.lowSigma));
            upper = median + (stddev * m_StackData.highSigma);

            for (unsigned int i = 0; i < values.size(); i++)
            {
                if (values[i] < lower || values[i] > upper)
                    continue;

                sum += values[i] * weights[i];
                weightSum += weights[i];
            }

            if (weightSum > 0.0)
                pixelValue = sum / weightSum;
            else
                pixelValue = median;
        }

        // Store intermediate calcs from this process
        cv::Vec4f sigmaClip;
        sigmaClip[0] = lower;
        sigmaClip[1] = upper;
        sigmaClip[2] = sum;
        sigmaClip[3] = weightSum;
        sigmaClipPtr[ch][x] = sigmaClip;

        // Update the pixel/channel with the calculated value
        finalImagePtr[x * m_Channels + ch] = pixelValue;
    }
}

// Function to stack n subs to an existing stack using Sigma Clipping
cv::Mat FITSStack::stacknSubsSigmaClipping(const QVector<float> &weights)
{
    try
    {
        int rows = m_StackImageData[0].image.rows;
        int cols = m_StackImageData[0].image.cols;
        int numImages = m_StackImageData.size();
        cv::Mat finalImage = m_StackedImage32F;
        float *finalImagePtr;
        QVector<cv::Vec4f *> sigmaClipPtr(m_Channels);

        if (m_StackImageData.size() != weights.size())
        {
            qCDebug(KSTARS_FITS) << QString("Inconsistent subs and weights in %1").arg(__FUNCTION__);
            return finalImage;
        }

        // If all images are continuous so we can treat as 1D arrays to speed things up
        bool continuous = finalImage.isContinuous() &&
                          std::all_of(m_SigmaClip32FC4.begin(), m_SigmaClip32FC4.end(),
                                      [](const cv::Mat& mat) { return mat.isContinuous(); }) &&
                          std::all_of(m_StackImageData.begin(), m_StackImageData.end(),
                                      [](const StackImageData &data) {return data.image.isContinuous(); });
        if (continuous)
        {
            cols *= rows;
            rows = 1;
        }

        // Process each pixel position
        std::vector<const float *> imagesPtrs(numImages);
        for (int y = 0; y < rows; y++)
        {
            // Update pointers for current y
            for (int i = 0; i < numImages; i++)
                imagesPtrs[i] = m_StackImageData[i].image.ptr<float>(y);

            finalImagePtr = finalImage.ptr<float>(y);
            for (int ch = 0; ch < m_Channels; ch++)
                sigmaClipPtr[ch] = m_SigmaClip32FC4[ch].ptr<cv::Vec4f>(y);

            for (int x = 0; x < cols; x++)
            {
                for (int ch = 0; ch < m_Channels; ch++)
                {
                    // Get the sigma clip data from the current pixel/channel
                    cv::Vec4f sigmaClip = sigmaClipPtr[ch][x];
                    float lower = sigmaClip[0];
                    float upper = sigmaClip[1];
                    float sum = sigmaClip[2];
                    float weightSum = sigmaClip[3];

                    // Process each image
                    for (int image = 0; image < numImages; image++)
                    {
                        float pixel = imagesPtrs[image][x * m_Channels + ch];
                        if (lower < 0.0 || (pixel >= lower && pixel <= upper))
                        {
                            sum += pixel * weights[image];
                            weightSum += weights[image];
                        }
                    }

                    // Update image pixel with new value
                    finalImagePtr[x * m_Channels + ch] = sum / weightSum;

                    // Save the new intermediate results for next time
                    sigmaClip[2] = sum;
                    sigmaClip[3] = weightSum;
                    sigmaClipPtr[ch][x] = sigmaClip;
                }
            }
        }
        return finalImage;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return m_StackedImage32F;
    }
}

void FITSStack::setWCSStackImage(const struct wcsprm *wcs)
{
    if (!wcs)
        return;

    if (m_WCSStackImage != nullptr)
    {
        wcsfree(m_WCSStackImage);
        delete m_WCSStackImage;
        m_WCSStackImage = nullptr;
    }

    m_WCSStackImage = new struct wcsprm;
    m_WCSStackImage->flag = -1;

    // Deep copy the original WCS structure
    int status = 0;
    if ((status = wcssub(1, wcs, 0x0, 0x0, m_WCSStackImage)) != 0)
    {
        qCDebug(KSTARS_FITS) << QString("%1 wcssub error processing %2").arg(__FUNCTION__).arg(status)
                                    .arg(wcs_errmsg[status]);
        delete m_WCSStackImage;
        m_WCSStackImage = nullptr;
        return;
    }

    // If the stacked image is downscaled, adjust CRPIX and CDELT
    if (m_StackData.downscale != LS_DOWNSCALE_NONE)
    {
        double downscale = getDownscaleFactor();

        m_WCSStackImage->cdelt[0] *= downscale;
        m_WCSStackImage->cdelt[1] *= downscale;

        m_WCSStackImage->crpix[0] /= downscale;
        m_WCSStackImage->crpix[1] /= downscale;
    }

    if ((status = wcsset(m_WCSStackImage)) != 0)
    {
        qCDebug(KSTARS_FITS) << QString("%1 wcsset error processing %2").arg(__FUNCTION__).arg(status)
                                            .arg(wcs_errmsg[status]);
        delete m_WCSStackImage;
        m_WCSStackImage = nullptr;
        return;
    }
}

cv::Mat FITSStack::postProcessImage(const cv::Mat &image32F)
{
    try
    {
        cv::Mat finalImage;
        // Firstly perform deconvolution (if requested). Calculate psf then use this for deconvolution
        cv::Mat image;
        if (m_StackData.postProcessing.deconvAmt > 0.0)
        {
            cv::Mat greyImage32F, deconvolved;
            int channels = image32F.channels();
            if (channels == 1)
                greyImage32F = image32F;
            else
                cv::cvtColor(image32F, greyImage32F, cv::COLOR_BGR2GRAY);

            cv::Mat psf = calculatePSF(greyImage32F);
            if (!psf.empty())
            {
                deconvolved = wienerDeconvolution(image32F, psf);
                if (!deconvolved.empty())
                    deconvolved.convertTo(image, CV_MAKETYPE(CV_16U, channels));
            }
        }

        if (image.empty())
            // Convert from 32F to 16U as following functions require 16U.
        {
            // First, find the range of the float data
            double minVal, maxVal;
            cv::minMaxLoc(image32F, &minVal, &maxVal);

            // Then scale to use full 16-bit range
            double scale = 65535.0 / maxVal;
            image32F.convertTo(image, CV_16U, scale);

            //image32F.convertTo(image, CV_MAKETYPE(CV_16U, 1));
        }

        cv::Mat sharpenedImage;

        // Sharpen using Unsharp Mask - openCV functions work on mono and colour images
        double sharpenAmount = m_StackData.postProcessing.sharpenAmt;
        if (sharpenAmount <= 0.0)
            sharpenedImage = image;
        else
        {
            cv::Mat blurredImage;
            int sharpenKernal = m_StackData.postProcessing.sharpenKernal;
            double sharpenSigma = m_StackData.postProcessing.sharpenSigma;

            // Ensure kernel size is odd and positive
            if (sharpenKernal < 3)
                sharpenKernal = 3;
            else if (sharpenKernal % 2 == 0)
                sharpenKernal++;

            cv::GaussianBlur(image, blurredImage, cv::Size(sharpenKernal, sharpenKernal), sharpenSigma);
            cv::addWeighted(image, 1.0 + sharpenAmount, blurredImage, -sharpenAmount, 0, sharpenedImage);
        }

        // Denoise
        float denoiseAmount = m_StackData.postProcessing.denoiseAmt;
        if (denoiseAmount <= 0.0)
            finalImage = sharpenedImage;
        else
        {
            // cv::fastNlMeansDenoising works on single channel images in 16bit
            // cv::fastNlMeansDenoisingColored works on colour images but only 8bit
            // So denoise per channel at 16bit
            std::vector<float> amount;
            amount.push_back(denoiseAmount);
            std::vector<cv::Mat> channels;
            cv::split(sharpenedImage, channels);

            for (auto& channel : channels)
            {
                cv::Mat denoisedChannel;
                cv::fastNlMeansDenoising(channel, denoisedChannel, amount, 7, 21, cv::NORM_L1);
                channel = denoisedChannel;
            }
            cv::merge(channels, finalImage);
        }
        return finalImage;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return cv::Mat();
    }
}

// Calculate psf for deconvolution. There are 2 methods:
// 1. Just create an idealised gaussian based on the user's input sigma
// 2. Calculate from stars in the image.
// At the moment 2. isn't working properly so just use 1.
cv::Mat FITSStack::calculatePSF(const cv::Mat &image, int patchSize)
{
    try
    {
        cv::Mat psf;
        QList<Edge *> starCenters = m_Data->getStarCenters();
        if (starCenters.size() <= 0)
        {
            // Create 1D Gaussian kernel, then make it 2D - note this is normalised
            double sigma = m_StackData.postProcessing.PSFSigma;
            cv::Mat kernel1D = cv::getGaussianKernel(patchSize, sigma, CV_32F);
            cv::Mat psf = kernel1D * kernel1D.t();
            return psf;
        }

        QVector<cv::Mat> starPatches;
        int halfPatch = patchSize / 2;

        for (int i = 0; i < starCenters.size(); i++)
        {
            bool keepStar = true;

            // Ignore stars near the edge of the image
            float minx = starCenters[i]->x - halfPatch;
            float maxx = starCenters[i]->x + halfPatch;
            float miny = starCenters[i]->y - halfPatch;
            float maxy = starCenters[i]->y + halfPatch;

            if (minx < 0 || miny < 0 || maxx >= image.cols || maxy >= image.rows)
                continue;

            // Ignore stars near each other as they'll create a complicated PSF
            for (int j = 0; j < starCenters.size(); j++)
            {
                if (i == j)
                    continue;
                if (starCenters[j]->x >= minx && starCenters[j]->x <= maxx &&
                    starCenters[j]->y >= miny && starCenters[j]->y <= maxy)
                {
                    // Star j lies in star i's patch so ignore star i
                    keepStar = false;
                    break;
                }
            }

            if (keepStar)
            {
                cv::Rect roi(minx, miny, patchSize, patchSize);
                cv::Mat patch = image(roi).clone();
                // Normalise the patch so we're adding together stars of similar brightness
                cv::Scalar patchSum = cv::sum(patch);
                patch /= patchSum[0];
                starPatches.push_back(patch);
            }

            // Limit the number of star patches
            if (starPatches.size() >= 20)
                break;
        }

        if (starPatches.empty())
            qCDebug(KSTARS_FITS) << QString("No valid stars for PSF estimation in %1").arg(__FUNCTION__);
        else
        {
            cv::Mat psf = cv::Mat::zeros(patchSize, patchSize, CV_32F);
            for (const auto &patch : starPatches)
                psf += patch;

            // Normalise PSF to unit energy
            cv::Scalar psfSum = cv::sum(psf);
            psf /= psfSum[0];
        }
        return psf;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return cv::Mat();
    }
}

// Wiener deconvolution assumes Gaussian noise and can be calculated using a single pass.
// Lucy-Richardson deconvolution assumes Poisson noise and needs to be done iteratively.
// For now we'll try Wiener
cv::Mat FITSStack::wienerDeconvolution(const cv::Mat &image, const cv::Mat &psf)
{
    try
    {
        if (image.type() != m_CVType || psf.type() != CV_MAKETYPE(CV_32F, 1))
            return image;

        // Pad the image to the optimum size for FFT
        cv::Mat imagePadded;
        int m = cv::getOptimalDFTSize(image.rows);
        int n = cv::getOptimalDFTSize(image.cols);
        cv::copyMakeBorder(image, imagePadded, 0, m - image.rows, 0, n - image.cols,
                                                                cv::BORDER_CONSTANT, cv::Scalar::all(0));

        // At the end, scale back to original range if needed
        // Centre the PSF in an image of the same size as imagePadded
        cv::Mat psfPadded = cv::Mat::zeros(imagePadded.size(), CV_32F);
        cv::Rect psfROI((psfPadded.cols - psf.cols) / 2, (psfPadded.rows - psf.rows) / 2, psf.cols, psf.rows);
        psf.copyTo(psfPadded(psfROI));

        // Shift PSF so zero frequency is at corners (fftshift)
        int cx = psfPadded.cols / 2;
        int cy = psfPadded.rows / 2;

        // Create quadrants
        cv::Mat q0(psfPadded, cv::Rect(0, 0, cx, cy)); // Top-Left
        cv::Mat q1(psfPadded, cv::Rect(cx, 0, cx, cy)); // Top-Right
        cv::Mat q2(psfPadded, cv::Rect(0, cy, cx, cy)); // Bottom-Left
        cv::Mat q3(psfPadded, cv::Rect(cx, cy, cx, cy)); // Bottom-Right

        // Swap diagonally opposite quadrants (0<->3, 1<->2)
        cv::Mat tmp;
        q0.copyTo(tmp); q3.copyTo(q0); tmp.copyTo(q3);
        q1.copyTo(tmp); q2.copyTo(q1); tmp.copyTo(q2);

        // Split into channels: 1 for mono, 3 for colour
        std::vector<cv::Mat> channels;
        cv::split(imagePadded, channels);
        std::vector<cv::Mat> deconChannels(channels.size());

        // FFT the PSF
        cv::Mat psfFFT;
        cv::dft(psfPadded, psfFFT, cv::DFT_COMPLEX_OUTPUT);

        // Compute |PSF|² = PSF* × PSF (complex conjugate multiplication)
        cv::Mat psfPower;
        cv::mulSpectrums(psfFFT, psfFFT, psfPower, 0, true);

        // Denominator: |PSF|² + NSR
        cv::Mat denomReal, denomImag;
        cv::Mat psfPowerChannels[2];
        cv::split(psfPower, psfPowerChannels);

        // Loop through the channels applying the Wiener filter
        for (int i = 0; i < m_Channels; i++)
        {
            // Take FFTs
            cv::Mat channelFFT;
            cv::dft(channels[i], channelFFT, cv::DFT_COMPLEX_OUTPUT);

            // Estimate noise variance using MAD
            // Flatten and sort for median
            cv::Mat channelFlat = channels[i].reshape(1, channels[i].total());
            cv::Mat channelSorted;
            channelFlat.copyTo(channelSorted);
            cv::sort(channelSorted, channelSorted, cv::SORT_ASCENDING);
            float median = channelSorted.at<float>(channelSorted.total() / 2);

            // MAD calculation - fix the absdiff operation
            cv::Mat absDiff;
            cv::absdiff(channelFlat, cv::Scalar(median), absDiff);
            cv::sort(absDiff, absDiff, cv::SORT_ASCENDING);
            float mad = std::max(absDiff.at<float>(absDiff.total() * 0.75), 1e-6f);
            float noiseVariance = std::pow(1.4826f * mad, 2.0f);

            // Calculate signal variance
            cv::Scalar channelMean, channelStddev;
            cv::meanStdDev(channels[i], channelMean, channelStddev);
            float totalVariance = channelStddev[0] * channelStddev[0];
            float signalVariance = std::max(totalVariance - noiseVariance, 1e-6f);

            // Calculate the Noise to Signal ratio
            float NSR = noiseVariance / signalVariance;
            NSR = std::max(NSR, 1e-6f);

            // Apply Wiener filter: H* × G / (|H|² + NSR)
            // Add NSR to real part only
            denomReal = psfPowerChannels[0] + NSR;
            denomImag = psfPowerChannels[1];  // Should be near zero for |PSF|²

            // Protect against division by zero / very small numbers
            cv::Mat mask = denomReal < 1e-10f;
            denomReal.setTo(1e-10f, mask);

            // Numerator: PSF* × Image_FFT (this part is correct)
            cv::Mat numerator;
            cv::mulSpectrums(psfFFT, channelFFT, numerator, 0, true);

            // Split numerator into real and imaginary parts so we can do proper complex division
            cv::Mat numChannels[2];
            cv::split(numerator, numChannels);
            cv::Mat numReal = numChannels[0];
            cv::Mat numImag = numChannels[1];

            cv::Mat wienerReal, wienerImag;
            cv::divide(numReal, denomReal, wienerReal);
            cv::divide(numImag, denomReal, wienerImag);

            // Merge back into complex result
            std::vector<cv::Mat> wienerChannels = {wienerReal, wienerImag};
            cv::Mat wienerResult;
            cv::merge(wienerChannels, wienerResult);

            // Inverse FFT to get deconvolved image
            cv::dft(wienerResult, deconChannels[i], cv::DFT_INVERSE | cv::DFT_REAL_OUTPUT | cv::DFT_SCALE);
        }
        // Merge channels back
        cv::Mat mergedResult;
        cv::merge(deconChannels, mergedResult);

        // Rotate by 180 degrees - necessary because of the original PSF fftshift.
        cv::rotate(mergedResult, mergedResult, cv::ROTATE_180);

        // Extract the original image region (remove padding)
        cv::Rect originalROI(0, 0, image.cols, image.rows);
        cv::Mat result = mergedResult(originalROI).clone();

        // Blend deconv result with the original
        result = (m_StackData.postProcessing.deconvAmt * result) + ((1 - m_StackData.postProcessing.deconvAmt) * image);
        return result;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return image;
    }
}

void FITSStack::redoPostProcessStack(const LiveStackPPData &ppParams)
{
    if (m_StackedImage32F.empty())
        // Nothing to do if there is no image to operate on
        return;

    // Get the current user options for post processing
    m_StackData.postProcessing = ppParams;

    cv::Mat finalImage = postProcessImage(m_StackedImage32F);
    m_StackSNR = getSNR(finalImage);
    convertMatToFITS(finalImage);
    emit stackChanged();
}

struct wcsprm * FITSStack::getWCSRef()
{
    struct wcsprm * ref = nullptr;
    if (getInitialStackDone())
        ref = m_RunningStackImageData.ref_wcsprm;
    else if (m_StackImageData.size() > m_InitialStackRef)
        ref = m_StackImageData[m_InitialStackRef].wcsprm;
    return ref;
}

// This converts the float cv::Mat to TUSHORT for display
// Keeping to float format would be more accurate but use twice
// the memory for little benefit.
bool FITSStack::convertMatToFITS(const cv::Mat &inImage)
{
    try
    {
        // Check if the image is valid
        if (inImage.empty())
            return false;

        int width = inImage.size().width;
        int height = inImage.size().height;
        int channels = inImage.channels();

        cv::Mat image;
        if(inImage.depth() == CV_16U)
            image = inImage;
        else
            inImage.convertTo(image, CV_MAKETYPE(CV_16U, channels));

        //This section sets up the FITS File
        fitsfile *fptr = nullptr;
        int status = 0;
        long fpixel = 1, nelements;
        long naxis = (channels == 1) ? 2 : 3;
        long naxes[3] = { width, height, channels };
        char error_status[512] = { 0 };
        void* fits_buffer = nullptr;
        size_t fits_buffer_size = 0;

        if (fits_create_memfile(&fptr, &fits_buffer, &fits_buffer_size, 4096, realloc, &status))
        {
            fits_get_errstatus(status, error_status);
            qCDebug(KSTARS_FITS()) << "fits_create_memfile failed " << error_status;
            return false;
        }

        if (fits_create_img(fptr, USHORT_IMG, naxis, naxes, &status))
        {
            fits_get_errstatus(status, error_status);
            qCDebug(KSTARS_FITS) << "fits_create_img failed " << error_status;
            status = 0;
            fits_close_file(fptr, &status);
            free(fits_buffer);
            return false;
        }

        if (channels == 3)
        {
            // Colour image so firstly add bayer FITS keywords
            QByteArray ba = m_BayerPattern.toUtf8();
            const char* bayerPattern = ba.constData();
            const char* comment = "Bayer color pattern";

            if (fits_write_key(fptr, TSTRING, "BAYERPAT", (void*)bayerPattern, (char*)comment, &status))
            {
                fits_get_errstatus(status, error_status);
                qCDebug(KSTARS_FITS) << "fits_write_key BAYERPAT failed:" << error_status;
                status = 0;
            }

            comment = "X offset of Bayer array";
            if (fits_write_key(fptr, TINT, "XBAYROFF", &m_BayerOffsetX, (char*)comment, &status))
            {
                fits_get_errstatus(status, error_status);
                qCDebug(KSTARS_FITS) << "fits_write_key XBAYROFF failed:" << error_status;
                status = 0;
            }

            comment = "Y offset of Bayer arra";
            if (fits_write_key(fptr, TINT, "YBAYROFF", &m_BayerOffsetY, (char*)comment, &status))
            {
                fits_get_errstatus(status, error_status);
                qCDebug(KSTARS_FITS) << "fits_write_key YBAYROFF failed:" << error_status;
                status = 0;
            }

            // Colour images need to be converted from interleaved R1G1B1R2G2B2R3...
            // format to planar RRRRR.. GGGGG.. BBBBB.. format for display
            int totalPixels = width * height;

            std::vector<cv::Mat> splitChannels(3);
            cv::split(image, splitChannels);

            // Allocate planar buffer to hold R, G, B planes consecutively
            std::vector<uint16_t> planarBuffer(totalPixels * 3);

            auto* planarPtr = planarBuffer.data();

            // Copy each channel data into planar buffer
            memcpy(planarPtr, splitChannels[0].data, totalPixels * sizeof(uint16_t));
            memcpy(planarPtr + totalPixels, splitChannels[1].data, totalPixels * sizeof(uint16_t));
            memcpy(planarPtr + 2 * totalPixels, splitChannels[2].data, totalPixels * sizeof(uint16_t));

            nelements = totalPixels * 3;
            if (fits_write_img(fptr, TUSHORT, fpixel, nelements, planarPtr, &status))
            {
                fits_get_errstatus(status, error_status);
                qCDebug(KSTARS_FITS) << "fits_write_img failed " << status;
                status = 0;
                fits_close_file(fptr, &status);
                free(fits_buffer);
                return false;
            }
        }        
        else
        {
            // Mono image so we can just write it out
            nelements = width * height * channels;

            cv::Mat contImage;
            if (image.isContinuous())
                contImage = image;
            else
                contImage = image.clone();

            if (fits_write_img(fptr, TUSHORT, fpixel, nelements, contImage.data, &status))
            {
                fits_get_errstatus(status, error_status);
                qCDebug(KSTARS_FITS) << "fits_write_img failed " << status;
                status = 0;
                fits_close_file(fptr, &status);
                free(fits_buffer);
                return false;
            }
        }

        if (fits_flush_file(fptr, &status))
        {
            fits_get_errstatus(status, error_status);
            qCDebug(KSTARS_FITS) << "fits_flush_file failed:" << error_status;
            status = 0;
            fits_close_file(fptr, &status);
            free(fits_buffer);
            return false;
        }

        if (fits_close_file(fptr, &status))
        {
            fits_get_errstatus(status, error_status);
            qCDebug(KSTARS_FITS) << "fits_close_file failed:" << error_status;
            free(fits_buffer);
            return false;
        }

        m_StackedBuffer.reset(new QByteArray(reinterpret_cast<char *>(fits_buffer), fits_buffer_size));
        free(fits_buffer);
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }
    return false;
}

// Calculate the SNR of the passed in image.
double FITSStack::getSNR(const cv::Mat &image)
{
    double snr = 0.0;
    try
    {
        if (image.empty())
            return snr;

        // Split into channels: 1 for mono, 3 for colour
        std::vector<cv::Mat> channels;
        cv::split(image, channels);

        // Get a ROI in the centre of the image to use for the signal region
        cv::Rect roi = cv::Rect(image.cols/4, image.rows/4, image.cols/2, image.rows/2);

        int count = 0;
        for (const auto &channel : channels)
        {
            cv::Mat channelRoi = channel(roi);
            cv::Scalar mean, stdDev;
            cv::meanStdDev(channelRoi, mean, stdDev);
            if (stdDev.val[0] > 1e-06)
            {
                snr += mean.val[0] / stdDev.val[0];
                count++;
            }
        }
        if (count > 0)
            snr /= count;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        snr = 0.0;
    }
    return snr;
}

// We're done with the original stack so tidy up and keep data necessary to add individual
// subs to the interim stack as they arrive
void FITSStack::setupRunningStack(struct wcsprm * refWCS, const int numSubs, const float totalWeight)
{
    setInitalStackDone(true);
    m_RunningStackImageData.numSubs = numSubs;
    m_RunningStackImageData.ref_wcsprm = refWCS;
    m_RunningStackImageData.ref_hfr = 0;
    m_RunningStackImageData.ref_numStars = 0;
    m_RunningStackImageData.totalWeight = totalWeight;
    tidyUpInitalStack(refWCS);
}

void FITSStack::updateRunningStack(const int numSubs, const float totalWeight)
{
    m_RunningStackImageData.numSubs += numSubs;
    m_RunningStackImageData.totalWeight = totalWeight;
    tidyUpInitalStack(nullptr);
}

// Release FITS and openCV memory used in original stack
void FITSStack::tidyUpInitalStack(struct wcsprm * refWCS)
{
    for (int i = 0; i < m_StackImageData.size(); i++)
    {
        if (m_StackImageData[i].wcsprm != nullptr && m_StackImageData[i].wcsprm != refWCS)
        {
            // Don't free up the reference WCS as we'll need that for later processing
            wcsfree(m_StackImageData[i].wcsprm);
            free(m_StackImageData[i].wcsprm);
            m_StackImageData[i].wcsprm = nullptr;
        }
        m_StackImageData[i].image.release();
    }
    m_StackImageData.clear();
}

// Release FITS and openCV memory used in the running stack
void FITSStack::tidyUpRunningStack()
{
    if (m_RunningStackImageData.ref_wcsprm != nullptr)
    {
        wcsfree(m_RunningStackImageData.ref_wcsprm);
        free(m_RunningStackImageData.ref_wcsprm);
        m_RunningStackImageData.ref_wcsprm = nullptr;
    }
}
