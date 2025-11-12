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

/**
 * @file fitsstack.cpp
 * @brief Implementation of the FITSStack class used in the KStars Live Stacker module.
 *
 * This file implements the logic for live stacking of FITS images in real time. It supports
 * frame-by-frame processing, calibration, alignment, integration, and optional post-processing.
 *
 * ### Core Responsibilities:
 * - Stack initialization upon first frame or reset
 * - Memory-efficient accumulation of subs using OpenCV
 * - Plate-solve-based alignment using WCS transformations and OpenCV warping
 * - Optional calibration using user-supplied master dark and/or flat frames
 * - Incremental stacking updates as new frames are added from the watched directory
 * - Post-processing (Wiener deconvolution, unsharp mask, denoising) after stacking
 * - Management of metadata (e.g. WCS headers, image size/type consistency, SNR tracking)
 *
 * ### Integration Points:
 * - Receives FITS frames from FITSDirWatcher or FITSViewer via addSub()
 * - Emits stackChanged() signal whenever a new stack is generated
 * - Works with SolverUtils for plate solving and alignment
 * - Outputs stacked images through getStackedImage() and FITS buffer access
 *
 * ### Stack Lifecycle:
 * 1. Initial stack is built from a fixed-size chunk of frames
 * 2. Once the initial stack is complete, subsequent frames are incrementally added
 * 3. Stacking continues live as new subs appear in the monitored folder
 * 4. When stacking completes (or pauses), the result can be post-processed and saved
 *
 * ### File Overview:
 * - Image consistency checks: checkSub(), convertMat(), convertToCV()
 * - Calibration: calibrateSub(), addMaster()
 * - Alignment: calcWarpMatrix(), solverDone()
 * - Stacking logic: stack(), stackn(), stackSubs(), stackSubsSigmaClipping()
 * - Post-processing: postProcessImage(), wienerDeconvolution()
 * - SNR and PSF utilities: getSNR(), calculatePSF()
 * - Stack management: setupRunningStack(), updateRunningStack(), tidyUpInitialStack()
 */

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

    m_RunningStackImageData.imageMMState.accumNum.release();
    m_RunningStackImageData.imageMMState.accumDen.release();
    m_RunningStackImageData.imageMMState.latent.release();
    for (int i = 0; i < m_RunningStackImageData.runningSubs.size(); i++)
    {
        m_RunningStackImageData.runningSubs[i].image.release();
        m_RunningStackImageData.runningSubs[i].psfKernel.release();
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
void FITSStack::setupNextSub(const QString &sub)
{
    StackImageData imageData;
    imageData.sub = sub;
    imageData.image = cv::Mat();
    imageData.status = PLATESOLVE_IN_PROGRESS;
    imageData.isCalibrated = false;
    imageData.isAligned = false;
    imageData.wcsprm = nullptr;
    imageData.hfr = -1;
    imageData.numStars = 0;
    m_StackImageData.push_back(imageData);
}

bool FITSStack::addSub(void * imageBuffer, const int cvType, const int width, const int height,
                       const int bytesPerPixel, double &snr)
{
    snr = -1;
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

        snr = getSNR(newImage);
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
            m_MasterFlatInv.release();

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
            // Scale the flat down using the median value (note that this also takes care of normalised flats 0-1
            std::vector<cv::Mat> channels;
            cv::split(imageClone, channels);

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
            cv::merge(channels, imageClone);
            // Store the inverse of the flat so we can then multiply it with the sub because
            // multiply is faster than divide in openCV
            cv::divide(1.0f, imageClone, m_MasterFlatInv);
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

void FITSStack::addSubStatus(const bool ok)
{
    if (m_StackImageData.size() <= 0)
    {
        // This shouldn't happen
        qCDebug(KSTARS_FITS) << "addSubStatus called but no m_StackImageData";
        return;
    }

    (ok) ? m_StackImageData.last().status = OK : m_StackImageData.last().status = PLATESOLVE_FAILED;
}

// Perform the initial stack
bool FITSStack::stack()
{
    try
    {
        QElapsedTimer timer;
        timer.start();
        int numSubs = m_StackImageData.size();

        for(int i = 0; i < numSubs; i++)
        {
            // Ignore any bad subs
            if (m_StackImageData[i].status != OK)
                continue;

            // Signal the Wait Stack stage complete (waiting for enough subs to stack) to Stack Monitor
            QVector<QString> subs { m_StackImageData[i].sub };
            QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::WaitStack,
                                                                            LSStatus::LSStatusOK) };
            emit updateStackMon(subs, infos);

            // Calibrate sub
            if (!m_StackImageData[i].isCalibrated)
            {
                if (calibrateSub(m_StackImageData[i].sub, m_StackImageData[i].image))
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
            if (m_StackData.alignMethod == LS_ALIGNMENT_NONE)
                // No alignment needed so skip this stage
                m_StackImageData[i].isAligned = true;
            else if (!m_StackImageData[i].isAligned)
            {
                // Align this image to the reference image
                cv::Mat warp, warpedImage;
                bool ok = calcWarpMatrix(m_StackImageData[m_InitialStackRef].wcsprm, m_StackImageData[i].wcsprm, warp);
                if (!ok)
                    m_StackImageData[i].status = ALIGNMENT_FAILED;
                else
                {
                    cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp,
                                        m_StackImageData[i].image.size(), cv::INTER_LANCZOS4);
                    m_StackImageData[i].image = warpedImage;
                    m_StackImageData[i].isAligned = true;
                }

                // Signal the Alignment stage complete to Stack Monitor
                double dx, dy, rotationDeg;
                QVariantMap extraData;
                decomposeWarpMatrix(warp, m_StackImageData[i].image.size(), dx, dy, rotationDeg);
                extraData.insert("dx", dx);
                extraData.insert("dy", dy);
                extraData.insert("rotation", rotationDeg);
                QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::Aligned,
                                                    ok ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData) };
                QVector<QString> subs { m_StackImageData[i].sub };
                emit updateStackMon(subs, infos);
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

        qCDebug(KSTARS_FITS) << QString("Stacked %1 subs in %2 ms").arg(numSubs).arg(timer.elapsed());
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
        QElapsedTimer timer;
        timer.start();
        int numSubs = m_StackImageData.size();

        for(int i = 0; i < numSubs; i++)
        {
            // Ignore any bad subs
            if (m_StackImageData[i].status != OK)
                continue;

            // Signal the Wait Stack stage complete (waiting for enough subs to stack) to Stack Monitor
            QVector<QString> subs { m_StackImageData[i].sub };
            QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::WaitStack,
                                                                            LSStatus::LSStatusOK) };
            emit updateStackMon(subs, infos);

            // Calibrate sub
            if (!m_StackImageData[i].isCalibrated)
            {
                if (calibrateSub(m_StackImageData[i].sub, m_StackImageData[i].image))
                    m_StackImageData[i].isCalibrated = true;
                else
                {
                    m_StackImageData[i].status = CALIBRATION_FAILED;
                    continue;
                }
            }

            // Alignment stage
            cv::Mat warp, warpedImage;
            if (m_StackData.alignMethod == LS_ALIGNMENT_NONE)
                // No alignment needed so skip this stage
                m_StackImageData[i].isAligned = true;
            else
            {
                bool ok = calcWarpMatrix(m_RunningStackImageData.ref_wcsprm, m_StackImageData[i].wcsprm, warp);
                if (!ok)
                    m_StackImageData[i].status = ALIGNMENT_FAILED;
                else
                {
                    cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp,
                                        m_StackImageData[i].image.size(), cv::INTER_LANCZOS4);
                    m_StackImageData[i].image = warpedImage;
                    m_StackImageData[i].isAligned = true;
                }

                // Signal the Alignment stage complete to Stack Monitor
                double dx, dy, rotationDeg;
                QVariantMap extraData;
                decomposeWarpMatrix(warp, m_StackImageData[i].image.size(), dx, dy, rotationDeg);
                extraData.insert("dx", dx);
                extraData.insert("dy", dy);
                extraData.insert("rotation", rotationDeg);
                QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::Aligned,
                                                    ok ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData) };
                QVector<QString> subs { m_StackImageData[i].sub };
                emit updateStackMon(subs, infos);
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
        qCDebug(KSTARS_FITS) << QString("Stacked %1 subs in %2 ms").arg(numSubs).arg(timer.elapsed());
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
        //cv::Ptr<cv::Formatter> fmt = cv::Formatter::get(cv::Formatter::FMT_DEFAULT);
        //std::cout << fmt->format(warp) << std::endl;
        return true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return false;
    }
}

// Extract useful information from the warp matrix for use by the Monitor
// One complexity is that the translation elements need to be adjusted as openCV rotates
// about the top left but its more intuitive to display results for a rotation about the
// image center.
void FITSStack::decomposeWarpMatrix(const cv::Mat &warp, const cv::Size &imageSize, double &dx, double &dy, double &rotationDeg)
{
    dx = dy = rotationDeg = 0.0;
    if (warp.rows != 3 || warp.cols != 3)
    {
        qCDebug(KSTARS_FITS) << QString("Invalid warp matrix in %1").arg(__FUNCTION__);
        return;
    }

    // Rotation
    const double a = warp.at<double>(0, 0);
    const double b = warp.at<double>(0, 1);
    const double rotationRad = std::atan2(b, a);
    rotationDeg = rotationRad * 180.0 / M_PI;

    // Adjust translation to be relative to image center - openCV warps about top left
    double tx = warp.at<double>(0,2);
    double ty = warp.at<double>(1,2);

    cv::Point2d center(imageSize.width/2.0, imageSize.height/2.0);

    // The effective translation relative to the center
    cv::Matx22d R(a, warp.at<double>(0,1), warp.at<double>(1,0), warp.at<double>(1,1));
    cv::Point2d newCenter = R * center + cv::Point2d(tx, ty);
    cv::Point2d delta = newCenter - center;

    dx = delta.x;
    dy = delta.y;
}

// Calibrate the passed in sub with an associated Dark (if available) and / or Flat (if available)
bool FITSStack::calibrateSub(const QString &subname, cv::Mat &sub)
{
    bool ok = false;
    int dark = -1, flat = -1;
    try
    {
        if (sub.empty())
            return false;

        // Dark subtraction (make sure no negative pixels)
        if (!m_MasterDark.empty())
        {
            cv::subtract(sub, m_MasterDark, sub);
            cv::max(sub, 0.0f, sub);
            dark = 0;
        }

        // Flat calibration
        if (!m_MasterFlatInv.empty())
        {
            sub = sub.mul(m_MasterFlatInv);
            flat = 0;
        }
        ok = true;
    }
    catch (const cv::Exception &ex)
    {
        dark = flat = 1;
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
    }

    // Signal the Calibrated stage complete to Stack Monitor
    QVariantMap extraData;
    extraData.insert("dark", dark);
    extraData.insert("flat", flat);
    QVector<QString> subs { subname };
    QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::Calibrated,
                                        (ok) ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData) };
    emit updateStackMon(subs, infos);
    return ok;
}

// Stack the vector of subs
bool FITSStack::stackSubs(const bool initial, float &totalWeight, cv::Mat &stack)
{
    bool ok = false;
    QVector<float> weights;
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

        weights = getWeights();

        if (m_StackData.stackingMethod == LS_STACKING_SIGMA || m_StackData.stackingMethod == LS_STACKING_WINDSOR)
        {
            // Sigma clipping (standard or Windsorized
            if (initial)
                stack = stackSubsSigmaClipping(weights);
            else
                stack = stacknSubsSigmaClipping(weights);
        }
        else if (m_StackData.stackingMethod == LS_STACKING_IMAGEMM)
        {
            // ImageMM method
            if (initial)
                stack = stackSubsImageMM(weights, m_StackData);
            else
                stack = stacknSubsImageMM(weights, m_StackData);
        }
        else
        {
            // Add the pixels weighted per sub based on user setting. Then divide by the total weight
            // If its an initial stack then just use the subs, if not then include the existing partial stack
            int start;
            if (initial)
            {
                totalWeight = weights[0];
                stack = m_StackImageData[0].image;
                start = 1;
            }
            else
            {
                totalWeight = m_RunningStackImageData.totalWeight;
                stack = m_StackedImage32F * totalWeight;
                start = 0;
            }

            cv::Mat temp;
            for (int sub = start; sub < m_StackImageData.size(); sub++)
            {
                if (m_StackData.weighting == LS_STACKING_EQUAL)
                    // No need to multiply by 1 for equal weighting
                    cv::add(stack, m_StackImageData[sub].image, stack);
                else
                {
                    cv::multiply(m_StackImageData[sub].image, weights[sub], temp, 1.0, m_CVType);
                    cv::add(stack, temp, stack);
                }
                //stack += m_StackImageData[sub].image * weights[sub];
                totalWeight += weights[sub];
            }
            cv::multiply(stack, 1.0 / totalWeight, stack, 1.0, m_CVType);
        }
        ok = true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        ok = false;
    }

    // Signal the Stacking stage complete to Stack Monitor
    if (m_StackImageData.size() > 0)
    {
        QVector<QString> subs;
        QVector<LiveStackStageInfo> infos;
        for (int sub = 0; sub < m_StackImageData.size(); sub++)
        {
            QVariantMap extraData;
            extraData.insert("weight", weights[sub]);
            subs << m_StackImageData[sub].sub;
            infos << LiveStackStageInfo::fromNow(-1, LSStage::Stacked,
                                                 ok ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData);
        }
        emit updateStackMon(subs, infos);
    }
    return ok;
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
        QElapsedTimer timer;
        timer.start();

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

                        if (m_StackData.stackingMethod == LS_STACKING_WINDSOR)
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
        qCDebug(KSTARS_FITS) << QString("Sigma clipping completed in %1 ms").arg(timer.elapsed());
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

        if (m_StackData.stackingMethod == LS_STACKING_WINDSOR)
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

/**
 * Run full ImageMM stacking on the current subframe set.
 *
 * This function performs a complete ImageMM (Iterative Multiplicative Model) stacking
 * over all available subframes. It first builds a combined list of subframes and weights
 * (including any running history), then calls the main ImageMM core solver to produce
 * a new stacked latent image.
 *
 * The function resets the latent state and sigma estimate before starting, so each call
 * performs a multi-frame refinement without reusing any previous iterative state.
 */
cv::Mat FITSStack::stackSubsImageMM(const QVector<float> &weights, const LiveStackData &lsd)
{
    try
    {
        QVector<float> allWeights;
        QVector<StackImageData> allSubs;
        if (!imageMMBuildAllSubs(weights, allSubs, allWeights))
            return m_StackedImage32F;

        cv::Mat latent = m_StackedImage32F;
        double sigma = 0.0;
        bool incremental = false;
        return imageMMCore(allSubs, latent, sigma, allWeights, lsd, incremental);
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
        return m_StackedImage32F;
    }
}

/**
 * Incrementally update the ImageMM stack using new subframes.
 *
 * This variant of ImageMM stacking continues from the previous latent
 * image and sigma state stored in `m_RunningStackImageData.imageMMState`.
 * It reuses the existing latent estimate (`latent`) and noise model (`sigma`)
 * to efficiently refine the current stack when new subframes arrive.
 *
 * The method merges the current running subframes with the new ones,
 * builds the combined data set and weight vector via `imageMMBuildAllSubs()`,
 * and then calls `imageMMCore()` in incremental mode.
 */
cv::Mat FITSStack::stacknSubsImageMM(const QVector<float> &weights, const LiveStackData &lsd)
{
    try
    {
        QVector<float> allWeights;
        QVector<StackImageData> allSubs;
        if (!imageMMBuildAllSubs(weights, allSubs, allWeights))
            return m_StackedImage32F;

        cv::Mat latent = m_RunningStackImageData.imageMMState.latent;
        double sigma = m_RunningStackImageData.imageMMState.sigma;
        const bool incremental = true;
        cv::Mat result = imageMMCore(allSubs, latent, sigma, allWeights, lsd, incremental);
        m_RunningStackImageData.imageMMState.latent = result;
        m_RunningStackImageData.imageMMState.sigma = sigma;
        return result;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return m_StackedImage32F;
    }
}

/**
 * Build a complete list of subframes and corresponding weights for ImageMM stacking.
 *
 * This function merges the currently running set of stacked subframes with any new pending subframes.
 * It ensures each subframe has a valid PSF kernel (creating a default Gaussian kernel if missing) and
 * produces normalized weights across all subframes.
 *
 * Specifically, the function:
 *  - Starts from the currently accumulated subframes in `m_RunningStackImageData`.
 *  - Appends all new subframes from `m_StackImageData` along with their associated weights.
 *  - Ensures PSF kernels exist for each subframe (building one from HFR if needed).
 *  - Normalizes the combined weights
 */
bool FITSStack::imageMMBuildAllSubs(const QVector<float> &newWeights, QVector<FITSStack::StackImageData> &allSubs,
                                    QVector<float> &allWeights)
{
    try
    {
        // Add historical subs
        auto &run = m_RunningStackImageData;
        allSubs = run.runningSubs;
        for (int i = 0; i < allSubs.size(); i++)
            allWeights.push_back(allSubs[i].weight);

        // Add new subs
        if (m_StackImageData.size() != newWeights.size())
        {
            qCDebug(KSTARS_FITS) << QString("Inconsistent new subs and weights in %1").arg(__FUNCTION__);
            return false;
        }

        for (int i = 0; i < m_StackImageData.size(); i++)
        {
            auto &sub = m_StackImageData[i];

            // Ensure PSF is built
            if (sub.psfKernel.empty())
            {
                if (sub.hfr > 0)
                    sub.psfKernel = buildPSFFromHFR(sub.hfr);
                else
                {
                    cv::Mat g = cv::getGaussianKernel(9, 1.5, CV_32F);
                    sub.psfKernel = g * g.t();
                }
            }

            // Default weight (if not already set)
            if (sub.weight <= 0.0f)
                sub.weight = newWeights[i];

            allSubs.append(sub);
            allWeights.append(newWeights[i]);
        }
        // Normalise weights
        float sumW = std::accumulate(allWeights.begin(), allWeights.end(), 0.0f);
        if (sumW <= 0.0)
            std::fill(allWeights.begin(), allWeights.end(), 1.0f / allWeights.size());
        else
        {
            for (float &w : allWeights)
                w /= sumW;
        }
        return true;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return false;
    }
}

/**
 * Robust Image Stacking via Majorization–Minimization (ImageMM)
 *
 * Implements **Algorithm 3** from Sukurdeep et al. (2025), AJ 170, Article 233:
 * “ImageMM: Robust Astronomical Image Stacking via MM Optimization”.
 *
 * Link: https://iopscience.iop.org/article/10.3847/1538-3881/adfb72
 *
 * Algorithm 3 summary (from the paper, adapted):
 *
 *  1. Input: aligned frames {yₜ}, initial latent x⁰, weights wₜ, parameters κ, α, ε
 *  2. For k = 0, 1, 2, … until convergence:
 *     a. Compute residuals: rₜ = yₜ – xᵏ
 *     b. Estimate global scale σ (e.g. via MAD of residuals) (Eq. 11)
 *     c. Compute robust weights: wₜ(p) = 1 / (1 + (rₜ(p)/σ)²)  (Eq. 10–12)
 *     d. Compute numerator N(p) = Σₜ wₜ(p) · yₜ(p), denominator D(p) = Σₜ wₜ(p) · xᵏ(p)
 *     e. Ratio u(p) = N(p) / (D(p) + ε)
 *     f. Clip u(p) to [1/κ, κ]
 *     g. Update latent: xᵏ⁺¹(p) = xᵏ(p) · u(p)
 *     h. (Optional relaxation): xᵏ⁺¹ ← (1−α)xᵏ + α xᵏ⁺¹
 *     i. Enforce non-negativity: xᵏ⁺¹(p) ≥ 0
 *     j. Check convergence: if ‖xᵏ⁺¹ – xᵏ‖ / ‖xᵏ‖ < ε then stop
 *  3. Output: final latent x̂ = xᵏ
 *
 * This implementation:
 *  • Uses global σ per iteration (step 2b)
 *  • Uses Cauchy weighting (step 2c)
 *  • Multiplies via QtConcurrent (step 2g)
 *  • Implements convergence test (step 2j)
 *  • Supports multi-channel (RGB) stacking
 *  • Parallel over frames processing
 *  • Parallel over pixel processing of final image (tiles)
 */
cv::Mat FITSStack::imageMMCore(QVector<StackImageData> &subs, cv::Mat &latent, double &sigma,
                               const QVector<float> &weights, const LiveStackData &lsd, bool incremental)
{
    try
    {
        const float convergenceTest = 1e-3;
        const int pixelSample = 4;           // Sample every nth row & column - for speed
        const int frameSample = 4;           // How many frames to sample - for speed
        const float psfLearningRate = 0.05f; // Small step size
        const double sigmaBlend = 0.25;      // 0 - 1. Higher = smoother updates

        qCDebug(KSTARS_FITS) << QString("Running %1ImageMM: iterations=%2 kappa=%3 alpha=%4 sigmaScale=%5 PSFUpdate=%6")
                                    .arg(incremental ? "Incremental" : "Initial").arg(lsd.iterations).arg(lsd.kappa)
                                    .arg(lsd.alpha).arg(lsd.sigma).arg(lsd.PSFUpdate);

        const int n = subs.size();
        if (n == 0)
        {
            qCDebug(KSTARS_FITS) << QString("No data to stack in %1").arg(__FUNCTION__);
            return m_StackedImage32F;
        }

        if (n != weights.size())
        {
            qCDebug(KSTARS_FITS) << QString("Inconsistent subs and weights in %1").arg(__FUNCTION__);
            return m_StackedImage32F;
        }

        // Initialize latent (if required)
        imageMMInitializeLatent(latent, subs, weights);

        // Split the subs into channels for later processing
        std::vector<std::vector<cv::Mat>> subsChannels;
        subsChannels.reserve(n);
        for (int i = 0; i < n; i++)
        {
            std::vector<cv::Mat> tempChannels;
            cv::split(subs[i].image, tempChannels);
            subsChannels.push_back(tempChannels);
        }

        cv::Mat prevLatent = latent.clone();
        double prevSigma = 0.0;

        // Outer loop for iterations (or until convergence)
        for (int iter = 0; iter < lsd.iterations; iter++)
        {
            // Debug
            cv::Scalar mn, sd;
            cv::meanStdDev(latent, mn, sd);
            qCDebug(KSTARS_FITS) << QString("%1 iter %2 mean=%3 std=%4").arg(__FUNCTION__).arg(iter).arg(mn[0])
                                        .arg(sd[0]);

            // Get an estimate of sigma across all subs / channels
            sigma = imageMMEstimateSigma(subs, latent, pixelSample, frameSample, lsd.sigma, prevSigma, sigmaBlend);

            // Split latent into channels
            std::vector<cv::Mat> latentChannels;
            cv::split(latent, latentChannels);

            // Per-channel loop
            for (uint c = 0; c < latentChannels.size(); c++)
            {
                std::pair<cv::Mat, cv::Mat> acc = imageMMAccumulateChannel(subs, subsChannels, latentChannels[c],
                                                                           weights, sigma, c);
                // Step 2e–2g: multiplicative update
                imageMMPixelwiseUpdate(latentChannels[c], std::vector<cv::Mat>{acc.first},
                                       std::vector<cv::Mat>{acc.second}, (float)lsd.kappa);

                // debug
                cv::Scalar mc, sc;
                cv::meanStdDev(latentChannels[c], mc, sc);
                qCDebug(KSTARS_FITS) << QString("%1: channel %2 iter %3 mean=%4 std=%5").arg(__FUNCTION__).arg(c)
                                            .arg(iter).arg(mc[0]).arg(sc[0]);
            } // end channel loop

            // Merge channels back into latent
            cv::merge(latentChannels, latent);

            // Update the PSFs
            if ((lsd.PSFUpdate > 0) && ((iter + 1) % lsd.PSFUpdate == 0))
                imageMMRefinePSFs(subs, latent, psfLearningRate);

            // Step 2h: relaxation damping
            if (lsd.alpha < 1.0f)
                latent = (1 - lsd.alpha) * prevLatent + lsd.alpha * latent;

            // Step 2j: convergence check
            double relChange = imageMMComputeRelChange(latent, prevLatent);
            if (relChange >= convergenceTest)
                qCDebug(KSTARS_FITS) << QString("Converging (iter=%1) Δ=%2").arg(iter).arg(relChange, 0, 'e', 4);
            else
            {
                qCDebug(KSTARS_FITS) << QString("Converged (iter=%1) Δ=%2").arg(iter).arg(relChange, 0, 'e', 4);
                break;
            }

            // Step 2i: non-negativity
            cv::threshold(latent, latent, 0.0, 0.0, cv::THRESH_TOZERO);

            prevLatent = latent.clone();
        } // end iterations loop
        return latent;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return m_StackedImage32F;
    }
}

// Initialize latent image if empty, using weighted mean of subs.
void FITSStack::imageMMInitializeLatent(cv::Mat &latent, const QVector<StackImageData> &subs,
                                        const QVector<float> &weights)
{
    const int n = subs.size();
    if (n > 0 && latent.empty())
    {
        latent = weights[0] * subs[0].image;
        for (int i = 1; i < n; i++)
            latent += weights[i] * subs[i].image;
    }
}

/**
 * Estimate the global noise scale (σ) of the ImageMM model using the Median Absolute Deviation (MAD) of residuals.
 *
 * This function computes a robust estimate of the per-pixel residual variance between each subframe and the current
 * latent image. The estimate is based on the median absolute deviation (MAD), which is robust.
 *
 * The computation works as follows:
 *  - Select a subset of frames (`frameSample`) from the available subframes.
 *  - For each selected frame, compute residuals as |subframe - latent|.
 *  - Uniformly subsample residuals by `pixelSample` to reduce computation.
 *  - Compute the median of all residual samples.
 *  - Convert MAD to a Gaussian-equivalent σ estimate via `σ = 1.4826 * MAD * sigmaScale`.
 *  - Blend the new σ with the previous estimate (`prevSigma`) using `sigmaBlend`.
 */
double FITSStack::imageMMEstimateSigma(const QVector<StackImageData> &subs, const cv::Mat &latent, int pixelSample,
                                       int frameSample, double sigmaScale, double prevSigma, double sigmaBlend)
{
    try
    {
        const int n = subs.size();
        if (n == 0)
            return prevSigma > 0 ? prevSigma : 1.0;

        std::vector<float> residualSamples;
        residualSamples.reserve(latent.total() / (pixelSample * pixelSample) * std::min(n, frameSample));

        // Collect sample data
        int sampleCount = std::min(n, frameSample);
        for (int t = 0; t < sampleCount; t++)
        {
            const cv::Mat &frame = subs[t].image;
            cv::Mat absr;
            cv::absdiff(frame, latent, absr);

            for (int y = 0; y < absr.rows; y += pixelSample)
            {
                const float *row = absr.ptr<float>(y);
                for (int x = 0; x < absr.cols; x += pixelSample)
                    residualSamples.push_back(row[x]);
            }
        }

        if (residualSamples.empty())
            residualSamples.push_back(1e-6f);

        // Compute median of residuals (robust location)
        const size_t mid = residualSamples.size() / 2;
        std::nth_element(residualSamples.begin(), residualSamples.begin() + mid, residualSamples.end());
        const double medianResidual = residualSamples[mid];

        // Compute absolute deviations from that median
        for (float &v : residualSamples)
            v = std::abs(v - static_cast<float>(medianResidual));

        // Median of deviations (MAD)
        std::nth_element(residualSamples.begin(), residualSamples.begin() + mid, residualSamples.end());
        const double mad = residualSamples[mid];

        // Convert MAD → σ (Eq. 11) and apply the user defined sigmaScale
        const double sigmaNew = std::max(1e-6, 1.4826 * mad * sigmaScale);

        // Blend with previous estimate
        double sigma = (prevSigma > 0.0) ? sigmaBlend * prevSigma + (1.0 - sigmaBlend) * sigmaNew : sigmaNew;

        qCDebug(KSTARS_FITS)
            << QString("%1: medianResidual=%2 mad=%3 sigmaNew=%4 blended=%5").arg(__FUNCTION__)
                   .arg(medianResidual, 0, 'f', 4).arg(mad, 0, 'f', 4).arg(sigmaNew, 0, 'f', 4).arg(sigma, 0, 'f', 4);
        return sigma;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return prevSigma > 0 ? prevSigma : 1.0;
    }
}        

/**
 * Accumulate per-subframe contributions for one color channel in the ImageMM iteration.
 *
 * This function performs Step 2c–2d of the ImageMM algorithm:
 * computing the forward and backward model accumulations for a single color channel across all registered subframes.
 *
 * Each subframe contributes to two accumulators:
 *  - Numerator (accumNum):  Σ Fᵀ (w ⊙ y)
 *  - Denominator (accumDen): Σ Fᵀ (w ⊙ F·x)
 *
 * where:
 *   - F is the convolution operator (PSF for the subframe),
 *   - Fᵀ is its transpose (implemented by convolving again with the PSF),
 *   - x is the current latent (merged) estimate,
 *   - y is the observed subframe channel,
 *   - w is a robust weight defined as:
 *     w = 1 / (1 + (r² / σ²))
 *     with residual r = (y − F·x),
 *   - σ controls robustness to outliers.
 *
 * Each subframe’s scalar weight (e.g. SNR or exposure-based) is multiplied into the per-pixel weights `w`.
 *
 * Parallelized across subframes using with per-thread partial results accumulated using a mutex.
 */
std::pair<cv::Mat, cv::Mat> FITSStack::imageMMAccumulateChannel(const QVector<StackImageData> &subs,
                                const std::vector<std::vector<cv::Mat>> &subsChannels, const cv::Mat &latentChannel,
                                const QVector<float> &normWeights, double sigma, int channelIndex)
{
    try
    {
        const int n = subs.size();
        if (n <= 0)
            return {cv::Mat(), cv::Mat()};

        int numThreads = std::min(n, QThreadPool::globalInstance()->maxThreadCount());
        qCDebug(KSTARS_FITS) << QString("%1 Channel %2: running per-frame parallel map on upto %3 threads")
                                    .arg(__FUNCTION__).arg(channelIndex).arg(numThreads);

        // Since we're processing per channel the num and den need to be single channel
        const cv::Size imageSize = latentChannel.size();
        int depth = CV_MAT_DEPTH(m_CVType);
        cv::Mat accumNum = cv::Mat::zeros(imageSize, CV_MAKETYPE(depth, 1));
        cv::Mat accumDen = cv::Mat::zeros(imageSize, CV_MAKETYPE(depth, 1));
        QMutex accumLock;

        QVector<int> subIndices(n);
        std::iota(subIndices.begin(), subIndices.end(), 0);
        const char *func = __FUNCTION__;

        // Step 2c–2d in parallel: per-frame contributions
        QtConcurrent::blockingMap(subIndices, [&](int t)
        {
            try
            {
                const auto &s = subs[t];
                const auto &psf = s.psfKernel;

                // Forward model Fi_x = Fi * x
                cv::Mat Fi_x;
                cv::filter2D(latentChannel, Fi_x, -1, psf, cv::Point(-1,-1), 0, cv::BORDER_REPLICATE);

                // Residual and robust weight
                cv::Mat r, rsq, wi;
                cv::subtract(subsChannels[t][channelIndex], Fi_x, r);
                cv::multiply(r, r, rsq);
                wi = 1.0f / (1.0f + rsq / (sigma * sigma));

                // Subframe scalar weight
                const float subScalar = (normWeights.size() == n) ? normWeights[t] : s.weight;
                wi *= subScalar;

                // Build Fiᵀ(w·y) and Fiᵀ(w·Fi·x)
                cv::Mat wi_y, wi_Fix, FiT_wi_y, FiT_wi_Fix;
                cv::multiply(wi, subsChannels[t][channelIndex], wi_y);
                cv::multiply(wi, Fi_x, wi_Fix);

                cv::filter2D(wi_y, FiT_wi_y, -1, psf, cv::Point(-1,-1), 0, cv::BORDER_REPLICATE);
                cv::filter2D(wi_Fix, FiT_wi_Fix, -1, psf, cv::Point(-1,-1), 0, cv::BORDER_REPLICATE);

                // Thread-safe accumulation
                QMutexLocker lock(&accumLock);
                accumNum += FiT_wi_y;
                accumDen += FiT_wi_Fix;
            }
            catch (const cv::Exception &ex)
            {
                qCDebug(KSTARS_FITS) << QString("OpenCV exception in %1: %2").arg(func).arg(ex.what());
            }
        });
        return {accumNum, accumDen};
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception in %1: %2").arg(__FUNCTION__).arg(ex.what());
        return {cv::Mat(), cv::Mat()};
    }
}

/**
 * Perform a multiplicative pixel-wise update to the latent image channel.
 *
 * This function applies the multiplicative update step of the ImageMM algorithm to a single latent channel
 * (e.g. R, G, or B). Each pixel in the latent image is updated by a multiplicative factor *u* computed from the
 * ratio of accumulated numerators (`Fiᵀ·w·y`) to denominators (`Fiᵀ·w·Fi·x`) across all subframes.
 *
 * The update rule for each pixel is:
 * x_new(y, x) = x_old(y, x) * clamp( num / (den + ε), 1/kappa, kappa )
 *
 * where:
 * - `num` = Σₜ Fiᵀₜ(wₜ · yₜ)
 * - `den` = Σₜ Fiᵀₜ(wₜ · Fiₜ · x)
 * - `ε`   = small stabilizer (1e-8)
 *
 * This step ensures stability and prevents excessive multiplicative jumps by clamping the update factor `u`
 * between `1/kappa` and `kappa`.
 *
 * NOTE: it seems to be a bit softer to apply the clamping in log space so this is now implemented.
 *
 * Parallelism is achieved using QtConcurrent by partitioning the image into blocks
 */
void FITSStack::imageMMPixelwiseUpdate(cv::Mat &channel, const std::vector<cv::Mat> &FiT_wi_y,
                                       const std::vector<cv::Mat> &FiT_wi_Fix, float kappa)
{
    const int height = channel.rows;
    const int width = channel.cols;
    const int n = static_cast<int>(FiT_wi_y.size());

    // Partition rows into work blocks
    const int numChunks = std::max(1, QThread::idealThreadCount() * 2);
    const int chunkRows = std::max(1, height / numChunks);

    qCDebug(KSTARS_FITS) << QString("Starting ImageMM update: %1 chunks on %2 threads")
                                .arg(numChunks).arg(QThread::idealThreadCount());

    QVector<int> rowBlocks;
    for (int y = 0; y < height; y += chunkRows)
        rowBlocks.append(y);

    // Add small stabilizer to denominator
    const float denomBeta = 1e-8f;

    auto processBlock = [&](int yStart)
    {
        int yEnd = std::min(yStart + chunkRows, height);
        for (int y = yStart; y < yEnd; ++y)
        {
            float *outRow = channel.ptr<float>(y);
            for (int x = 0; x < width; ++x)
            {
                float num = 0.0f, den = 0.0f;
                for (int t = 0; t < n; ++t)
                {
                    num += FiT_wi_y[t].at<float>(y, x);
                    den += FiT_wi_Fix[t].at<float>(y, x);
                }

                // Use log-domain damping for stability as it seems a bit softer
                float u = num / std::max(den, denomBeta);
                float logu = std::log(std::max(u, denomBeta));
                logu = std::clamp(logu, -std::log(kappa), std::log(kappa));
                outRow[x] *= std::exp(logu);
            }
        }
    };
    QtConcurrent::blockingMap(rowBlocks, processBlock);
}

/**
 * Refine per-subframe PSFs using gradient-based optimization.
 *
 * This function performs a simple iterative refinement of each subframe's point spread function (PSF) based on the
 * current latent (model) image. For each subframe:
 *  - The latent image is convolved with the current PSF estimate (`Fi_x`).
 *  - The gradient of the reconstruction error (`Fi_x - sub.image`) is computed.
 *  - The PSF is updated via `imageMMUpdatePSF()`.
 *
 * The idea is to slightly reshape each PSF kernel so that, when convolved with the latent image, it better
 * reproduces the observed subframe.
 */
void FITSStack::imageMMRefinePSFs(QVector<StackImageData> &subs, const cv::Mat &latent, float learningRate)
{
    try
    {
        for (int t = 0; t < subs.size(); ++t)
        {
            cv::Mat &psf = subs[t].psfKernel;
            if (psf.empty())
                continue;

            cv::Mat Fi_x, grad;
            cv::filter2D(latent, Fi_x, -1, psf, cv::Point(-1,-1), 0, cv::BORDER_REPLICATE);
            cv::subtract(Fi_x, subs[t].image, grad);

            imageMMUpdatePSF(psf, grad, learningRate);
        }
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
    }
}

/**
 * Apply a simple gradient descent based update to the PSF kernel.
 *
 * This function performs a single optimization step on the point spread function (PSF) used in the ImageMM model.
 * The update is applied element-wise as:
 * psf ← psf − η · ∇L(psf)
 * where:
 *   - η is the learning rate (`lr`),
 *   - ∇L(psf) is the gradient of the current loss with respect to the PSF (`grad`).
 *
 * Negative values are clamped to zero after the update to preserve a physically valid (non-negative) kernel, and
 * the PSF is then renormalized to maintain flux conservation:
 * psf ← psf / Σ(psf)
 */
inline void FITSStack::imageMMUpdatePSF(cv::Mat &psf, const cv::Mat &grad, float lr)
{
    try
    {
        const int rows = psf.rows, cols = psf.cols;
        for (int y = 0; y < rows; ++y)
        {
            float *p_psf = psf.ptr<float>(y);
            const float *p_g = grad.ptr<float>(y);
            for (int x = 0; x < cols; ++x)
            {
                // Single-step descent
                p_psf[x] -= lr * p_g[x];
                if (p_psf[x] < 0.0f)
                    p_psf[x] = 0.0f;
            }
        }

        // Renormalize kernel to maintain flux conservation
        double sumVal = cv::sum(psf)[0];
        if (sumVal > 1e-8)
            psf /= sumVal;
        return;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
    }
}

/**
 * Compute relative change between two images.
 *
 * This function measures how much an updated image `a` differs from a reference image `b` using the L2 (Euclidean)
 * norm. It is typically used within the ImageMM iterative optimization loop to determine convergence between
 * successive updates.
 *
 * The relative change is defined as:
 *      rel_change = ||a - b||_2 / (||b||_2 + 1e-8)
 *
 * A small epsilon (1e-8) is added to the denominator to prevent div by zero.
 */
double FITSStack::imageMMComputeRelChange(const cv::Mat &a, const cv::Mat &b)
{
    try
    {
        double num = cv::norm(a - b, cv::NORM_L2);
        double den = cv::norm(b, cv::NORM_L2) + 1e-8;
        return num / den;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return 0.0;
    }
}

/**
 * Build a synthetic 2D Gaussian PSF kernel from a given HFR value.
 *
 * This function generates a normalized Gaussian point spread function (PSF) whose width corresponds to the specified
 * half-flux radius (HFR), expressed in pixels. The PSF is commonly used in ImageMM routines for convolution,
 * deconvolution, or as an initial estimate of the stellar profile.
 *
 * The conversion assumes an approximate relationship:
 *      σ ≈ HFR / 1.177
 * which relates the Gaussian standard deviation (σ) to the half-flux radius. The kernel size is chosen to cover
 * roughly ±3σ and is enforced to be odd.
 */
cv::Mat FITSStack::buildPSFFromHFR(const double hfr)
{
    try
    {
        // Sanity clamp
        if (!std::isfinite(hfr) || hfr <= 0.1 || hfr > 20.0)
            return cv::Mat();

        // Convert HFR -> Gaussian sigma
        double sigma = hfr / 1.177;
        sigma = std::clamp(sigma, 0.5, 5.0);

        // Kernel size: roughly ±3σ (odd)
        int ksize = std::max(7, int(6 * sigma) | 1);

        // 1D Gaussian -> 2D kernel
        cv::Mat g1d = cv::getGaussianKernel(ksize, sigma, CV_MAT_TYPE(m_CVType));
        cv::Mat psf = g1d * g1d.t();

        // Normalize to sum = 1
        psf /= cv::sum(psf)[0];

        qCDebug(KSTARS_FITS) << QString("%1: HFR=%2 px -> σ=%3 (ksize=%4x%4)").arg(__FUNCTION__).arg(hfr, 0, 'f', 2)
                                    .arg(sigma, 0, 'f', 2).arg(ksize);

        return psf;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
        return cv::Mat();
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
        if (!m_StackData.postProcessing.postProcess)
            return image32F;

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
        {
            // Convert from 32F to 16U as following functions require 16U.
            // Subs could have values out of range - due to processing
            // Darks won't be out of range so preserve photometry by not scaling
            double minVal, maxVal;
            cv::minMaxLoc(image32F, &minVal, &maxVal);

            if (maxVal <= 65535.0)
                image32F.convertTo(image, CV_16U);
            else
            {
                double scale = 65535.0 / maxVal;
                image32F.convertTo(image, CV_16U, scale);
            }
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
        QList<Edge *> starCenters;
        if (m_Data)
            starCenters = m_Data->getStarCenters();

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
    // Get the current user options for post processing
    m_StackData.postProcessing = ppParams;

    if (getStackInProgress())
    {
        qCDebug(KSTARS_FITS) << QString("Request to Reprocess Post Processing ignored because stacking operation in flight");
        return;
    }

    if (!m_StackedImage32F.empty())
    {
        cv::Mat finalImage = postProcessImage(m_StackedImage32F);
        m_StackSNR = getSNR(finalImage);
        convertMatToFITS(finalImage);
    }
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

    // Initialize latent for incremental ImageMM
    if (!m_StackedImage32F.empty())
        m_RunningStackImageData.imageMMState.latent = m_StackedImage32F.clone();
    else
        m_RunningStackImageData.imageMMState.latent = cv::Mat::zeros(
            m_StackImageData[0].image.size(), m_StackImageData[0].image.type());

    if (m_StackData.stackingMethod == LS_STACKING_IMAGEMM)
    {
        // Copy subs to running buffer for ImageMM
        m_RunningStackImageData.runningSubs.clear();
        for (int i = 0; i < numSubs; ++i)
        {
            StackImageData sub;
            sub.image = m_StackImageData[i].image;
            sub.psfKernel = m_StackImageData[i].psfKernel.empty()
                                   ? buildPSFFromHFR(m_StackImageData[i].hfr)
                                   : m_StackImageData[i].psfKernel;
            sub.weight = m_StackImageData[i].weight;
            m_RunningStackImageData.runningSubs.append(sub);
        }
    }

    // Now it’s safe to free the old data
    tidyUpInitalStack(refWCS);
}

void FITSStack::updateRunningStack(const int numSubs, const float totalWeight)
{
    try
    {
        // Update running stack metadata
        m_RunningStackImageData.numSubs += numSubs;
        m_RunningStackImageData.totalWeight = totalWeight;

        if (m_StackData.stackingMethod == LS_STACKING_IMAGEMM)
        {
            // Merge new subs from m_StackImageData into runningSubs
            for (auto &newSub : m_StackImageData)
            {
                // Ensure PSF kernel is valid
                if (newSub.psfKernel.empty())
                {
                    if (newSub.hfr > 0)
                        newSub.psfKernel = buildPSFFromHFR(newSub.hfr);
                    else
                    {
                        cv::Mat g = cv::getGaussianKernel(9, 1.5, CV_MAT_TYPE(m_CVType));
                        newSub.psfKernel = g * g.t();
                    }
                }

                // Append to running buffer
                m_RunningStackImageData.runningSubs.append(newSub);
            }

            // Trim history if too many old subs
            int excess = m_RunningStackImageData.runningSubs.size() - m_StackData.numInMem;
            if (excess > 0)
                m_RunningStackImageData.runningSubs.remove(0, excess);
        }

        // Free any unnecessary references to old FITS buffers
        tidyUpInitalStack(nullptr);
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS)
            << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
    }
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
        m_StackImageData[i].psfKernel.release();
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

    // Reset ImageMM state
    m_RunningStackImageData.imageMMState = {};
}
