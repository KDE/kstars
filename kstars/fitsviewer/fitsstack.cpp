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
 * - Works with SolverUtils for plate solving and alignment
 * - Outputs stacked images through getStackImage() and FITS buffer access
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
 * - Alignment: addAlignMasterWCS, calcWarpMatrix(), solverDone()
 * - Stacking logic: stack(), stackn(), stackSubs(), stackSubsSigmaClipping()
 * - Post-processing: postProcessImage(), wienerDeconvolution()
 * - PSF utilities: calculatePSF()
 * - Stack management: setupRunningStack(), updateRunningStack(), tidyUpInitialStack()
 */

FITSStack::FITSStack(FITSData *parent, StackChannel channel, StackData params)
    : QObject(parent)
{
    m_Data = parent;
    m_Channel = channel;
    m_StackData = params;
}

FITSStack::~FITSStack()
{
    tidyUpInitalStack();
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

void FITSStack::setInitalStackDone(bool done)
{
    m_InitialStackDone = done;
}

// Setup the image data structure for later processing
void FITSStack::setupNextSub(const LiveStackFile &sub)
{
    StackImageData imageData;
    imageData.sub = sub;
    imageData.image = cv::Mat();
    imageData.status = PLATESOLVE_IN_PROGRESS;
    imageData.isCalibrated = false;
    imageData.isCorrected = false;
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

        if (m_StackData.rejectTrailedSubs)
        {
            // Downsample (box-filter average) before the elongation check. Debayer
            // interpolation leaves pixel-scale directional artifacts on every star
            // regardless of tracking quality, which raises the "round star" baseline
            // enough to bury real trailing; averaging 2x2 blocks smooths that out while
            // preserving the multi-pixel-scale shape a genuine trail has.
            cv::Mat trailCheckImage;
            cv::resize(newImage, trailCheckImage, cv::Size(), 0.5, 0.5, cv::INTER_AREA);
            qWarning() << "DIAGTRAIL about to call detectStarTrailing, image size" << newImage.cols << newImage.rows
                       << "channels" << newImage.channels() << "downsampled to" << trailCheckImage.cols
                       << trailCheckImage.rows;
            QElapsedTimer trailTimer;
            trailTimer.start();
            double medianElongation = -1.0;
            int numSources = 0;
            const bool trailOk = FITSData::detectStarTrailing(trailCheckImage, medianElongation, numSources);
            qWarning() << "DIAGTRAIL detectStarTrailing returned" << trailOk << "elongation=" << medianElongation
                       << "numSources=" << numSources << "elapsedMs=" << trailTimer.elapsed();
            if (trailOk &&
                    medianElongation > m_StackData.maxStarElongation)
            {
                qCDebug(KSTARS_FITS) << QString("Sub rejected: median star elongation %1 (from %2 sources) exceeds "
                                                 "threshold %3 - likely trailing/tracking error")
                                     .arg(medianElongation).arg(numSources).arg(m_StackData.maxStarElongation);
                m_StackImageData.last().status = TRAILING_REJECTED;
                return false;
            }
        }

        snr = m_StackData.calcSNR ? FITSData::calcStackSNR(newImage) : 0.0;
        m_MaxSubSNR = std::max(m_MaxSubSNR, snr);
        m_MinSubSNR = (m_MinSubSNR > 0.0) ? std::min(m_MinSubSNR, snr) : snr;
        int subs = m_StackImageData.size();
        if (getInitialStackDone())
            subs += m_RunningStackImageData.numSubs;
        m_MeanSubSNR = ((m_MeanSubSNR * (subs - 1)) + snr) / subs;

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

void FITSStack::addAlignMasterWCS(const QSharedPointer<wcsprm> &wcs)
{
    m_AlignMasterWCS = wcs;
    setWCSStackImage(m_AlignMasterWCS);
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

        if (m_StackData.downscale != StackDownscale::NONE)
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
    if (m_StackData.downscale == StackDownscale::X2)
        factor = 2.0;
    else if (m_StackData.downscale == StackDownscale::X3)
        factor = 3.0;
    else if (m_StackData.downscale == StackDownscale::X4)
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
bool FITSStack::solverDone(const wcsprm * wcsHandle, const bool timedOut, const bool success, const double hfr,
                           const int numStars)
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

    if (ok)
        m_StackImageData.last().status = OK;
    // Don't clobber a more specific failure addSub() may have already recorded
    // (e.g. TRAILING_REJECTED) with the generic PLATESOLVE_FAILED bucket.
    else if (m_StackImageData.last().status == PLATESOLVE_IN_PROGRESS)
        m_StackImageData.last().status = PLATESOLVE_FAILED;
}

// Perform the initial stack
bool FITSStack::stack()
{
    qWarning() << "DIAGSTACKFN stack() ENTRY (before try block)";
    try
    {
        QElapsedTimer timer;
        timer.start();
        int numSubs = m_StackImageData.size();
        qWarning() << "DIAGSTACKFN stack() entered, numSubs=" << numSubs;

        for(int i = 0; i < numSubs; i++)
        {
            qWarning() << "DIAGSTACKFN loop i=" << i << "status=" << m_StackImageData[i].status;
            // Ignore any bad subs
            if (m_StackImageData[i].status != OK)
                continue;

            // Signal the Wait Stack stage complete (waiting for enough subs to stack) to Stack Monitor
            QVector<LiveStackFile> subs { m_StackImageData[i].sub };
            QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::WaitStack,
                                                LSStatus::LSStatusOK) };
            Q_EMIT updateStackMon(subs, infos);

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

            // Correct sub (remove hot/cold pixels)
            if (!m_StackImageData[i].isCorrected)
            {
                if (correctSub(m_StackImageData[i].sub, m_StackImageData[i].image))
                    m_StackImageData[i].isCorrected = true;
                else
                {
                    m_StackImageData[i].status = CORRECTION_FAILED;
                    continue;
                }
            }

            if (m_StackData.alignMethod == StackAlignMethod::NONE || m_AlignMasterWCS.isNull())
                // No alignment needed (or not setup) so skip this stage
                m_StackImageData[i].isAligned = true;
            else if (!m_StackImageData[i].isAligned)
            {
                // Align this image to the reference image
                cv::Mat warp, warpedImage;
                bool ok = calcWarpMatrix(m_AlignMasterWCS.get(), m_StackImageData[i].wcsprm, warp);
                if (!ok)
                    m_StackImageData[i].status = ALIGNMENT_FAILED;
                else
                {
                    // Use LINEAR interpolation. LANCZOS4 theoretically should be better but gives edge artifacts
                    cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp,
                                        m_StackImageData[i].image.size(), cv::INTER_LINEAR);
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
                QVector<LiveStackFile> subs { m_StackImageData[i].sub };
                Q_EMIT updateStackMon(subs, infos);
            }
        }
        // Stack the aligned subs
        float totalWeight = 0.0;
        cv::Mat hitMap;
        stackSubs(true, totalWeight, hitMap, m_StackedImage32F);

        if (m_StackData.numInMem <= m_StackImageData.size())
        {
            // We've completed the initial stack so perform post processing such as sharpening / denoising
            cv::Mat finalImage = postProcessImage(m_StackedImage32F);
            finalImage.copyTo(m_StackedImageFinal);
            // Move to incremental stacking as new subs arrive
            setupRunningStack(m_StackImageData.size(), totalWeight, hitMap);
        }
        else
            // Still more subs to stack so skip post-processing which is time consuming
            m_StackedImage32F.copyTo(m_StackedImageFinal);

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
            QVector<LiveStackFile> subs { m_StackImageData[i].sub };
            QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::WaitStack,
                                                LSStatus::LSStatusOK) };
            Q_EMIT updateStackMon(subs, infos);

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

            // Remove hot/cold pixels. Don't fail the sub if this step fails
            if (!m_StackImageData[i].isCorrected)
            {
                if (correctSub(m_StackImageData[i].sub, m_StackImageData[i].image))
                    m_StackImageData[i].isCorrected = true;
                else
                {
                    m_StackImageData[i].status = CORRECTION_FAILED;
                    continue;
                }
            }

            // Alignment stage
            cv::Mat warp, warpedImage;
            if (m_StackData.alignMethod == StackAlignMethod::NONE)
                // No alignment needed so skip this stage
                m_StackImageData[i].isAligned = true;
            else
            {
                bool ok = calcWarpMatrix(m_AlignMasterWCS.get(), m_StackImageData[i].wcsprm, warp);
                if (!ok)
                    m_StackImageData[i].status = ALIGNMENT_FAILED;
                else
                {
                    // Use LINEAR interpolation. LANCZOS4 theoretically should be better but gives edge artifacts
                    cv::warpPerspective(m_StackImageData[i].image, warpedImage, warp,
                                        m_StackImageData[i].image.size(), cv::INTER_LINEAR);
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
                QVector<LiveStackFile> subs { m_StackImageData[i].sub };
                Q_EMIT updateStackMon(subs, infos);
            }
        }
        // Stack the aligned subs
        float totalWeight = m_RunningStackImageData.totalWeight;
        cv::Mat hitMap = m_RunningStackImageData.hitMap;
        if (stackSubs(false, totalWeight, hitMap, m_StackedImage32F))
        {
            // Perform any post stacking processing such as sharpening / denoising
            cv::Mat finalImage = postProcessImage(m_StackedImage32F);

            finalImage.copyTo(m_StackedImageFinal);
        }

        updateRunningStack(m_StackImageData.size(), totalWeight, hitMap);
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
        std::vector<cv::Point2d> points1, points2;
        int gridSize = 5;

        for (int i = 0; i < gridSize; i++)
        {
            for (int j = 0; j < gridSize; j++)
            {
                // Calculate pixel coordinates (0-based)
                double px = (double)i * (m_Width - 1.0) / (gridSize - 1);
                double py = (double)j * (m_Height - 1.0) / (gridSize - 1);

                double imgcrd[2], phi, theta, world[2], pixcrd[2];
                int status, stat[2];

                // Pixel (Ref) -> World
                double pix1_fits[2] = { px + 1.0, py + 1.0 };
                status = wcsp2s(wcs1, 1, 2, pix1_fits, imgcrd, &phi, &theta, world, stat);
                if (status != 0)
                {
                    qCDebug(KSTARS_FITS) << QString("WCS wcsp2s error %1: %2").arg(status).arg(wcs_errmsg[status]);
                    continue;
                }

                // World -> Pixel (Sub)
                status = wcss2p(wcs2, 1, 2, world, &phi, &theta, imgcrd, pixcrd, stat);
                if (status != 0)
                {
                    qCDebug(KSTARS_FITS) << QString("WCS wcss2p error %1: %2").arg(status).arg(wcs_errmsg[status]);
                    continue;
                }

                points1.push_back(cv::Point2d(px, py));
                points2.push_back(cv::Point2d(pixcrd[0] - 1.0, pixcrd[1] - 1.0));
            }
        }

        unsigned int minPoints = (gridSize * gridSize / 2) + 1;
        if (points1.size() < minPoints)
        {
            qCDebug(KSTARS_FITS) << QString("Not enough datapoints [%1] to align image").arg(points1.size());
            return false;
        }

        // Use estimateAffinePartial2D for a rigid (Rotation + Translation) transform
        // This is much more stable for Alt-Az stacking than Homography.
        cv::Mat inliers;
        cv::Mat affine = cv::estimateAffinePartial2D(points2, points1, inliers, cv::RANSAC, 1.0);
        if (affine.empty())
        {
            qCDebug(KSTARS_FITS) << "openCV estimateAffinePartial2D matrix empty";
            return false;
        }

        double inlierRatio = (double)cv::countNonZero(inliers) / points1.size();
        if (inlierRatio < 0.8)
        {
            qCDebug(KSTARS_FITS) << "openCV estimateAffinePartial2D points do not form a consistent rigid body.";
            return false;
        }

        // Reject an implausibly large pure translation — a wrong-target sub, or a
        // meridian flip that wasn't re-centered. Neither check above catches this:
        // a translation shifts all 25 correspondence points by the same amount, so
        // they stay perfectly mutually consistent (100% RANSAC inliers), and the
        // determinant check below only looks at the rotation/scale block, not the
        // translation column — a rotation (including a full 180° meridian-flip
        // rotation) is already handled correctly by both checks regardless of
        // angle, since a pure rotation's determinant stays ~1.0 either way; it's
        // specifically translation magnitude that neither check constrains.
        // Past half the frame's smaller dimension, the two subs share too little
        // real overlap to be worth combining.
        const double tx = affine.at<double>(0, 2);
        const double ty = affine.at<double>(1, 2);
        const double translationMagnitude = std::sqrt(tx * tx + ty * ty);
        const double maxTranslation = std::min(m_Width, m_Height) * 0.5;
        if (translationMagnitude > maxTranslation)
        {
            qCDebug(KSTARS_FITS) << QString("Sub-frame translation too large (%1 px, limit %2 px), sub rejected")
                                  .arg(translationMagnitude).arg(maxTranslation);
            return false;
        }

        // Convert the 2x3 Affine matrix to a 3x3 Homography matrix
        warp = cv::Mat::eye(3, 3, CV_64F);
        affine.copyTo(warp.rowRange(0, 2));

        // Extract the Top-left 2x2 matrix to check for distortion
        // The determinant of the rotation/scale part tells us the area scaling factor
        cv::Mat linearPart = warp.rowRange(0, 2).colRange(0, 2);
        double det = cv::determinant(linearPart);

        // Apply a tolerance check to det
        // Ideal is 1.0. 0.9975 to 1.0025 allows for small changes.
        double tolerance = 0.0025;
        if (std::abs(det - 1.0) > tolerance)
        {
            qCDebug(KSTARS_FITS) << QString("Sub-frame distortion too high (Det: %1), sub rejected").arg(det);
            return false;
        }

        // If we are downscaling the image we need to adjust the warp matrix which is calculated from the un-downscaled images
        if (m_StackData.downscale != StackDownscale::NONE)
        {
            double scale = 1.0 / getDownscaleFactor();
            cv::Mat S = cv::Mat(cv::Matx33d(scale, 0,     0,
                                             0,     scale, 0,
                                             0,     0,     1));
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
void FITSStack::decomposeWarpMatrix(const cv::Mat &warp, const cv::Size &imageSize, double &dx, double &dy,
                                    double &rotationDeg)
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
    double tx = warp.at<double>(0, 2);
    double ty = warp.at<double>(1, 2);

    cv::Point2d center(imageSize.width / 2.0, imageSize.height / 2.0);

    // The effective translation relative to the center
    cv::Matx22d R(a, warp.at<double>(0, 1), warp.at<double>(1, 0), warp.at<double>(1, 1));
    cv::Point2d newCenter = R * center + cv::Point2d(tx, ty);
    cv::Point2d delta = newCenter - center;

    dx = delta.x;
    dy = delta.y;
}

// Calibrate the passed in sub with an associated Dark (if available) and / or Flat (if available)
bool FITSStack::calibrateSub(const LiveStackFile &subFile, cv::Mat &sub)
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
    QVector<LiveStackFile> subs { subFile };
    QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::Calibrated,
                                        (ok) ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData) };
    Q_EMIT updateStackMon(subs, infos);
    return ok;
}

// Correct sub by removing hot/cold pixels
bool FITSStack::correctSub(const LiveStackFile &subFile, cv::Mat &sub)
{
    bool ok = false;
    bool workToDo = m_StackData.hotPixels || m_StackData.coldPixels;
    int hotCount = 0, coldCount = 0;
    try
    {
        if (workToDo)
        {
            const int kernelSize = 3;       // median kernel
            double threshold = 25.0;

            // Process per channel
            std::vector<cv::Mat> channels;
            cv::split(sub, channels);

            for (auto &channel : channels)
            {
                cv::Mat median;
                cv::medianBlur(channel, median, kernelSize);

                cv::Mat diff;
                cv::subtract(channel, median, diff, cv::noArray(), channel.type());

                // Get the threshold to define a defective pixel
                cv::Mat absDiff;
                cv::absdiff(channel, median, absDiff);

                // Flatten diff to 1D
                cv::Mat diffFlat = absDiff.reshape(0, 1);
                std::vector<ushort> v;
                diffFlat.copyTo(v);

                if (!v.empty())
                {
                    // Compute median of diff (MAD)
                    std::nth_element(v.begin(), v.begin() + v.size() / 2, v.end());
                    double mad = v[v.size() / 2];
                    threshold =  std::max(mad * 5.0, 1.0); // 5-sigma threshold, min=1
                }

                cv::Mat hotMask, coldMask;
                if (m_StackData.hotPixels)
                {
                    // Hot pixels: pixel > median + threshold
                    cv::compare(diff, threshold, hotMask, cv::CMP_GT);
                    hotCount += cv::countNonZero(hotMask);
                }

                if (m_StackData.coldPixels)
                {
                    // Cold pixels: pixel < median - threshold
                    cv::compare(diff, -threshold, coldMask, cv::CMP_LT);
                    coldCount += cv::countNonZero(coldMask);
                    if (!coldMask.empty())
                    {
                        // Apply an isolation mask to reduce the chance of removing structure
                        cv::Mat dilated;
                        cv::dilate(coldMask, dilated, cv::Mat());
                        coldMask &= (dilated == coldMask);
                    }
                }

                // Combine the masks
                cv::Mat mask = hotMask.empty() ? coldMask : coldMask.empty() ? hotMask : (hotMask | coldMask);

                // Replace hot/cold pixels with local median
                median.copyTo(channel, mask);
            }
            cv::merge(channels, sub);
        }
        ok = true;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        ok = false;
    }
    // Signal the Correction stage complete to Stack Monitor
    QVariantMap extraData;
    extraData.insert("hotpix", hotCount);
    extraData.insert("coldpix", coldCount);
    QVector<LiveStackFile> subs { subFile };
    LSStatus status = (!ok) ? LSStatus::LSStatusError : (workToDo) ? LSStatus::LSStatusOK : LSStatus::LSStatusNA;
    QVector<LiveStackStageInfo> infos { LiveStackStageInfo::fromNow(-1, LSStage::Correction, status, extraData) };
    Q_EMIT updateStackMon(subs, infos);
    return ok;
}

// Stack the vector of subs
bool FITSStack::stackSubs(const bool initial, float &totalWeight, cv::Mat &hitMap, cv::Mat &stack)
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
        cv::Mat origHitMap;

        if (m_StackData.normalization == StackNormalization::LINEAR)
        {
            origHitMap = hitMap.clone();
            normalizeSubs(initial, weights, hitMap, stack);
        }

        if (m_StackData.stackingMethod == StackingMethod::SIGMA ||
                m_StackData.stackingMethod == StackingMethod::WINDSOR)
        {
            // Sigma clipping (standard or Windsorized
            if (initial)
                stack = stackSubsSigmaClipping(weights);
            else
                stack = stacknSubsSigmaClipping(weights);
        }
        else if (m_StackData.stackingMethod == StackingMethod::IMAGEMM)
        {
            if (initial)
                stack = stackSubsImageMM(weights, m_StackData);
            else
                stack = stacknSubsImageMM(weights, m_StackData);
        }
        else // Average stacking
        {
            // Add the pixels weighted per sub based on user setting. Then divide by the total weight
            // If its an initial stack then just use the subs, if not then include the existing partial stack
            int start = 0;
            if (initial)
            {
                start = 1;
                totalWeight = weights[0];
                stack = m_StackImageData[0].image.clone();
            }
            else if (m_StackData.normalization == StackNormalization::LINEAR && !origHitMap.empty())
            {
                std::vector<cv::Mat> chs;
                cv::split(stack, chs);
                for (auto &c : chs)
                    cv::multiply(c, origHitMap, c, 1.0, CV_32F);
                cv::merge(chs, stack);
            }
            else
                stack *= totalWeight;

            // Stacking loop
            for (int sub = start; sub < m_StackImageData.size(); sub++)
            {
                cv::Mat &currentSub = m_StackImageData[sub].image;
                float w = weights[sub];

                // Add the sub to the stack
                if (m_StackData.weighting == StackFrameWeighting::EQUAL)
                    cv::add(stack, currentSub, stack);
                else
                {
                    cv::Mat temp;
                    cv::multiply(currentSub, w, temp, 1.0, CV_32F);
                    cv::add(stack, temp, stack);
                }
                totalWeight += w;
            }

            // Rescale the new stack
            if (m_StackData.normalization == StackNormalization::LINEAR && !hitMap.empty())
            {
                std::vector<cv::Mat> channels;
                cv::split(stack, channels);
                for (auto &ch : channels)
                    cv::divide(ch, hitMap, ch);
                cv::merge(channels, stack);
            }
            else if (totalWeight > 0.0)
                // Global average
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
        QVector<LiveStackFile> subs;
        QVector<LiveStackStageInfo> infos;
        for (int sub = 0; sub < m_StackImageData.size(); sub++)
        {
            QVariantMap extraData;
            extraData.insert("weight", weights[sub]);
            subs << m_StackImageData[sub].sub;
            infos << LiveStackStageInfo::fromNow(-1, LSStage::Stacked,
                                                 ok ? LSStatus::LSStatusOK : LSStatus::LSStatusError, extraData);
        }
        Q_EMIT updateStackMon(subs, infos);
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
            case StackFrameWeighting::EQUAL:
                weights[i] = 1.0;
                break;
            case StackFrameWeighting::HFR:
                if (m_StackImageData[i].hfr > 0.0)
                    weights[i] = 1.0 / m_StackImageData[i].hfr;
                else
                    weights[i] = 1.0;
                break;
            case StackFrameWeighting::NUM_STARS:
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

// Control routine to normalize subs before stacking
void FITSStack::normalizeSubs(const bool initial, const QVector<float> &weights, cv::Mat &hitMap, cv::Mat &stack)
{
    try
    {
        cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));

        int start = 0;
        if (initial)
        {
            start = 1;
            stack = m_StackImageData[0].image.clone();

            // Get the mask for the stack
            cv::Mat mask = getBinaryMask(stack);

            // Update hitMap using the mask - note no need to use erosion because the alignment master isn't warped
            hitMap = cv::Mat::zeros(stack.size(), CV_32F);
            cv::multiply(mask, weights[0], mask);
            cv::add(hitMap, mask, hitMap);
        }

        // Get the mask for the stack - we'll use stack starting with alignment master
        cv::Mat stackMask = getBinaryMask(stack);

        // Convert mask to CV_8U for later processing
        stackMask.convertTo(stackMask, CV_8U, 255.0);

        for (int sub = start; sub < m_StackImageData.size(); sub++)
        {
            // Generate the mask (slightly shrunken to hide edge artifacts)
            cv::Mat subMask = getBinaryMask(m_StackImageData[sub].image);

            // Convert mask to CV_8U for later processing
            cv::Mat subMask8U;
            subMask.convertTo(subMask8U, CV_8U, 255.0);

            // Erode the mask by 1-2 pixels to remove interpolation edges
            // This removes the horizontal/vertical lines at the boundaries
            cv::Mat element = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3, 3));
            cv::erode(subMask8U, subMask8U, element);

            // Normalize the current image
            linearNormalization(m_StackImageData[sub].image, subMask8U, stack, stackMask);

            // Update the hitMap
            subMask8U.convertTo(subMask, CV_32F, 1.0 / 255.0);
            cv::multiply(subMask, weights[sub], subMask);
            cv::add(hitMap, subMask, hitMap);
        }
        return;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
    }
}

// Generate a binary mask, 1 where there's data and 0 otherwise
cv::Mat FITSStack::getBinaryMask(const cv::Mat &img)
{
    try
    {
        cv::Mat mask;
        if (img.channels() > 1)
            cv::cvtColor(img, mask, cv::COLOR_BGR2GRAY);
        else
            mask = img.clone();

        // 1. Threshold to find data area
        cv::threshold(mask, mask, 0.00001, 1.0, cv::THRESH_BINARY);
        return mask;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
        return cv::Mat();
    }
}

// Normalize the brightness of the sub to the master
void FITSStack::linearNormalization(cv::Mat &sub, const cv::Mat &subMask, const cv::Mat &ref, const cv::Mat &refMask)
{
    try
    {
        if (sub.empty() || ref.empty() || sub.size() != ref.size())
            return;

        cv::Mat overlapMask;

        // Calculate mask overlap
        cv::bitwise_and(subMask, refMask, overlapMask);

        // Safety: If overlap is less than 5% of the sensor, normalization is unreliable.
        if (cv::countNonZero(overlapMask) < (sub.total() * 0.05))
            return;

        // Process per channel
        int channels = sub.channels();
        std::vector<cv::Mat> subChannels, refChannels;
        cv::split(sub, subChannels);
        cv::split(ref, refChannels);

        for (int i = 0; i < channels; i++)
        {
            cv::Scalar muSub, sigSub, muRef, sigRef;

            // Calculate Mean/StdDev using the 1-channel 8U overlap mask
            cv::meanStdDev(subChannels[i], muSub, sigSub, overlapMask);
            cv::meanStdDev(refChannels[i], muRef, sigRef, overlapMask);

            float scale = 1.0f;
            if (sigSub[0] > 0.0001f)
                scale = static_cast<float>(sigRef[0] / sigSub[0]);

            scale = std::max(0.1f, std::min(scale, 10.0f));
            float offset = static_cast<float>(muRef[0] - (scale * muSub[0]));

            // Apply transformation (New = Old * scale + offset)
            subChannels[i].convertTo(subChannels[i], -1, scale, offset);

            // Clean the edges using the 1-channel 8U sub mask
            // This prevents the offset from turning black edges gray.
            subChannels[i].setTo(cv::Scalar(0), ~subMask);
        }
        cv::merge(subChannels, sub);
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
    }
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
        cv::Mat finalImage = cv::Mat::zeros(rows, cols, CV_32FC(m_Channels));
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
                                      [](const cv::Mat & mat)
        {
            return mat.isContinuous();
        }) &&
        std::all_of(m_StackImageData.begin(), m_StackImageData.end(),
                    [](const StackImageData & data)
        {
            return data.image.isContinuous();
        });
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

            float * finalImagePtr = finalImage.ptr<float>(0);
            QVector<cv::Vec4f *> sigmaClipPtr(m_Channels);
            for (int ch = 0; ch < m_Channels; ch++)
                sigmaClipPtr[ch] = m_SigmaClip32FC4[ch].ptr<cv::Vec4f>(0);

            // Setup the function for parallel processing to handle a chunk of pixels
            auto processPixelChunk = [&](const QPair<int, int> &chunk)
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

                        if (m_StackData.stackingMethod == StackingMethod::WINDSOR)
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

    for (int ch = 0; ch < m_Channels; ch++)
    {
        std::vector<float> validValues;
        std::vector<float> validWeights;
        validValues.reserve(numImages);
        validWeights.reserve(numImages);

        for (int image = 0; image < numImages; image++)
        {
            float val = imagesPtrs[image][x * m_Channels + ch];
            if (val > 0.0001f) // Only collect real data
            {
                validValues.push_back(val);
                validWeights.push_back(weights[image]);
            }
        }

        // Handle cases with no data (or only 1-2 frames of data)
        if (validValues.empty())
        {
            finalImagePtr[x * m_Channels + ch] = 0.0f;
            sigmaClipPtr[ch][x] = cv::Vec4f(0, 0, 0, 0);
            continue;
        }

        float pixelValue = 0.0;

        // Use our filtered 'validValues' for all statistical math
        if (m_StackData.stackingMethod == StackingMethod::WINDSOR)
        {
            float median = Mathematics::RobustStatistics::ComputeLocation(
                               Mathematics::RobustStatistics::LOCATION_MEDIAN, validValues);
            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                                              Mathematics::RobustStatistics::SCALE_VARIANCE, validValues));

            float lower = std::max(0.0f, static_cast<float>(median - (stddev * m_StackData.windsorCutoff)));
            float upper = median + (stddev * m_StackData.windsorCutoff);

            for (size_t i = 0; i < validValues.size(); i++)
            {
                if (validValues[i] < lower) validValues[i] = lower;
                else if (validValues[i] > upper) validValues[i] = upper;
            }
        }

        float median = Mathematics::RobustStatistics::ComputeLocation(
                           Mathematics::RobustStatistics::LOCATION_MEDIAN, validValues);

        float sum = 0.0, weightSum = 0.0, lower = -1.0, upper = -1.0;

        if (validValues.size() <= 3)
            pixelValue = median;
        else
        {
            auto const stddev = std::sqrt(Mathematics::RobustStatistics::ComputeScale(
                                              Mathematics::RobustStatistics::SCALE_VARIANCE, validValues));

            lower = std::max(0.0f, static_cast<float>(median - (stddev * m_StackData.lowSigma)));
            upper = median + (stddev * m_StackData.highSigma);

            for (size_t i = 0; i < validValues.size(); i++)
            {
                // Reject outliers (planes, satellites, hot pixels)
                if (validValues[i] < lower || validValues[i] > upper)
                    continue;

                sum += validValues[i] * validWeights[i];
                weightSum += validWeights[i];
            }

            if (weightSum > 0.0)
                pixelValue = sum / weightSum;
            else
                pixelValue = median;
        }

        // Store intermediate results for incremental stacking (stacknSubs)
        cv::Vec4f sigmaClip;
        sigmaClip[0] = lower;
        sigmaClip[1] = upper;
        sigmaClip[2] = sum;
        sigmaClip[3] = weightSum;
        sigmaClipPtr[ch][x] = sigmaClip;

        // Store the result in our 32-bit float buffer
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
                                      [](const cv::Mat & mat)
        {
            return mat.isContinuous();
        }) &&
        std::all_of(m_StackImageData.begin(), m_StackImageData.end(),
                    [](const StackImageData & data)
        {
            return data.image.isContinuous();
        });
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
                        if (pixel > 0.0001f)
                        {
                            if (lower < 0.0 || (pixel >= lower && pixel <= upper))
                            {
                                sum += pixel * weights[image];
                                weightSum += weights[image];
                            }
                        }
                    }

                    // Update image pixel with new value
                    if (weightSum > 0.0f)
                    {
                        finalImagePtr[x * m_Channels + ch] = sum / weightSum;

                        // Save the new intermediate results for next time
                        sigmaClip[2] = sum;
                        sigmaClip[3] = weightSum;
                        sigmaClipPtr[ch][x] = sigmaClip;
                    }
                    else
                        // If no data exists for this pixel, keep it black
                        finalImagePtr[x * m_Channels + ch] = 0.0f;
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
cv::Mat FITSStack::stackSubsImageMM(const QVector<float> &weights, const StackData &lsd)
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
cv::Mat FITSStack::stacknSubsImageMM(const QVector<float> &weights, const StackData &lsd)
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
                               const QVector<float> &weights, const StackData &lsd, bool incremental)
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
                imageMMPixelwiseUpdate(latentChannels[c], std::vector<cv::Mat> {acc.first},
                                       std::vector<cv::Mat> {acc.second}, (float)lsd.kappa);

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
                cv::filter2D(latentChannel, Fi_x, -1, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);

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

                cv::filter2D(wi_y, FiT_wi_y, -1, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
                cv::filter2D(wi_Fix, FiT_wi_Fix, -1, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);

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
            cv::filter2D(latent, Fi_x, -1, psf, cv::Point(-1, -1), 0, cv::BORDER_REPLICATE);
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

void FITSStack::setWCSStackImage(const QSharedPointer<wcsprm> &wcs)
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
    if ((status = wcssub(1, wcs.get(), 0x0, 0x0, m_WCSStackImage)) != 0)
    {
        qCDebug(KSTARS_FITS) << QString("%1 wcssub error processing %2").arg(__FUNCTION__).arg(status)
                             .arg(wcs_errmsg[status]);
        delete m_WCSStackImage;
        m_WCSStackImage = nullptr;
        return;
    }

    // If the stacked image is downscaled, adjust CRPIX and CDELT
    if (m_StackData.downscale != StackDownscale::NONE)
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

        cv::Mat finalImage, gradCorrect;

        // Firstly perform gradient correction (if requested)
        if (m_StackData.postProcessing.gradientAmt <= 0.0)
            gradCorrect = image32F;
        else
            gradCorrect = gradientCorrection(image32F, m_StackData.postProcessing.gradientAmt);
        \
        // Next perform deconvolution (if requested). Calculate psf then use this for deconvolution
        cv::Mat deconvolvedImage = gradCorrect;
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
                    deconvolvedImage = deconvolved;
            }
        }

        cv::Mat sharpenedImage;

        // Sharpen using Unsharp Mask - openCV functions work on mono and colour images
        double sharpenAmount = m_StackData.postProcessing.sharpenAmt;
        if (sharpenAmount <= 0.0)
            sharpenedImage = deconvolvedImage;
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

            cv::GaussianBlur(deconvolvedImage, blurredImage, cv::Size(sharpenKernal, sharpenKernal), sharpenSigma);
            cv::addWeighted(deconvolvedImage, 1.0 + sharpenAmount, blurredImage, -sharpenAmount, 0, sharpenedImage);
        }

        // Denoise
        float denoiseAmount = m_StackData.postProcessing.denoiseAmt;
        if (denoiseAmount <= 0.0)
            finalImage = sharpenedImage;
        else
        {
            std::vector<cv::Mat> channels;
            cv::split(sharpenedImage, channels);

            // Map the user-facing [0,1] strength to a significance multiplier in units
            // of the layer's own sigma, rather than an absolute value — this is what
            // makes denoiseAmt mean the same thing regardless of the input's pixel-value
            // scale (raw ADU counts, a normalized [0,1] image, an 8-bit preview, ...).
            const float kSigma = 0.5f + denoiseAmount * 4.5f; // ~0.5 sigma (mild) .. 5 sigma (aggressive)
            const bool soft = (m_StackData.postProcessing.denoiseMethod == DenoiseMethod::SOFT);

            for (auto &ch : channels)
            {
                CV_Assert(ch.type() == CV_32F);

                cv::Mat low1, low2, low3;
                cv::GaussianBlur(ch, low1, cv::Size(3, 3), 0.8);
                cv::GaussianBlur(low1, low2, cv::Size(5, 5), 1.6);
                cv::GaussianBlur(low2, low3, cv::Size(9, 9), 3.2);

                cv::Mat d1 = ch - low1;
                cv::Mat d2 = low1 - low2;
                cv::Mat d3 = low2 - low3;

                const float t1 = kSigma * robustSigma(d1);
                const float t2 = kSigma * robustSigma(d2);

                cv::Mat d1_shrink, d2_shrink;
                if (soft)
                {
                    // Donoho-style shrinkage: sign(x)*max(|x|-t, 0), computed without an
                    // explicit sign extraction via the identity
                    // soft(x,t) = max(x-t, 0) - max(-x-t, 0). Every coefficient above
                    // threshold is pulled toward zero by t rather than passed through
                    // unchanged, which avoids hard thresholding's blotchy, hard-edged look.
                    cv::Mat pos1, neg1, pos2, neg2;
                    cv::max(d1 - t1, 0.0f, pos1);
                    cv::max(-d1 - t1, 0.0f, neg1);
                    d1_shrink = pos1 - neg1;

                    cv::max(d2 - t2, 0.0f, pos2);
                    cv::max(-d2 - t2, 0.0f, neg2);
                    d2_shrink = pos2 - neg2;
                }
                else
                {
                    // Hard threshold: binary keep-above/zero-below.
                    cv::Mat mask1, mask2;
                    cv::compare(cv::abs(d1), t1, mask1, cv::CmpTypes::CMP_GT);
                    cv::compare(cv::abs(d2), t2, mask2, cv::CmpTypes::CMP_GT);

                    // cv::Mat::copyTo(mask) only writes pixels where the mask is non-zero
                    // — it does NOT zero the rest of a freshly allocated destination, it
                    // leaves them as uninitialized memory. Below-threshold coefficients
                    // must become exactly zero, so d1_shrink/d2_shrink must be explicitly
                    // zeroed first, or most of the image ends up mixed with garbage
                    // instead of being denoised.
                    d1_shrink = cv::Mat::zeros(d1.size(), d1.type());
                    d2_shrink = cv::Mat::zeros(d2.size(), d2.type());
                    d1.copyTo(d1_shrink, mask1);
                    d2.copyTo(d2_shrink, mask2);
                }

                ch = low3 + d3 + d2_shrink + d1_shrink;
            }
            cv::merge(channels, finalImage);
        }

        // Chroma-only noise reduction — separate opt-in from the per-channel denoise
        // above, off by default (see StackPPData::chromaDenoiseAmt).
        const double chromaAmt = m_StackData.postProcessing.chromaDenoiseAmt;
        if (chromaAmt > 0.0 && finalImage.channels() == 3)
            finalImage = chromaDenoise(finalImage, chromaAmt);

        // Convert the image back to float before returning
        cv::Mat returnImage;
        finalImage.convertTo(returnImage, CV_32F);
        return returnImage;
    }
    catch (const cv::Exception &ex)
    {
        QString s1 = ex.what();
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(s1).arg(__FUNCTION__);
        return cv::Mat();
    }
}

float FITSStack::robustSigma(const cv::Mat &d)
{
    // Row-strided VIEW (no copy, no averaging) to bound the sort below to a few
    // hundred thousand samples on a full-resolution image, while every sampled value
    // stays a genuine, unmodified pixel — averaging (e.g. a naive resize) would bias
    // the estimate low, since averaging independent noise samples reduces their
    // apparent amplitude.
    const int stride = std::max(1, d.rows / 750);
    const cv::Mat view(d.rows / stride, d.cols, d.type(), d.data, d.step[0] * stride);
    const cv::Mat absView = cv::abs(view);
    cv::Mat flat = absView.reshape(1, 1);
    cv::Mat sorted;
    cv::sort(flat, sorted, cv::SORT_ASCENDING);
    return 1.4826f * sorted.at<float>(0, sorted.cols / 2);
}

cv::Mat FITSStack::chromaDenoise(const cv::Mat &image, double amt)
{
    std::vector<cv::Mat> bgr;
    cv::split(image, bgr);
    const cv::Mat &B = bgr[0], &G = bgr[1], &R = bgr[2];

    // Luma/color-difference decomposition, deliberately not cv::COLOR_BGR2YCrCb:
    // OpenCV's YCrCb conversion bakes in a fixed midpoint offset (0.5 for float
    // input) that only round-trips correctly for [0,1]-normalized data, but this
    // runs on the pipeline's native linear image, which can be at raw ADU scale
    // (tens of thousands). Cr = R-Y / Cb = B-Y needs no offset at all, so it stays
    // exact at any scale.
    cv::Mat Y = 0.114f * B + 0.587f * G + 0.299f * R;
    cv::Mat Cr = R - Y;
    cv::Mat Cb = B - Y;

    // [0,1] strength -> a 1-8px Gaussian blur radius on the chroma planes only.
    const double sigma = 1.0 + std::clamp(amt, 0.0, 1.0) * 7.0;
    cv::GaussianBlur(Cr, Cr, cv::Size(0, 0), sigma);
    cv::GaussianBlur(Cb, Cb, cv::Size(0, 0), sigma);

    cv::Mat newR = Cr + Y;
    cv::Mat newB = Cb + Y;
    cv::Mat newG = (Y - 0.299f * newR - 0.114f * newB) / 0.587f;

    cv::Mat result;
    std::vector<cv::Mat> outChannels { newB, newG, newR };
    cv::merge(outChannels, result);
    return result;
}

// Performs Automatic Gradient Removal using a dual-pass
// Pass1: Division for vignetting
// Pass2: Subtraction for sky glow
cv::Mat FITSStack::gradientCorrection(const cv::Mat& image, const double strength)
{
    QElapsedTimer timer;
    timer.start();

    try
    {
        if (image.empty())
            return image;

        const int targetWidth = 200;
        cv::Size smallSize(targetWidth, std::max(1, (int)(image.rows * ((double)targetWidth / image.cols))));

        std::vector<cv::Mat> channels;
        cv::split(image, channels);

        // Multi-thread per channel
        QtConcurrent::blockingMap(channels, [&](cv::Mat & channel)
        {
            cv::Scalar initial_mu, initial_sigma;
            cv::meanStdDev(channel, initial_mu, initial_sigma);
            float originalBackgroundLevel = (float)initial_mu[0];

            for (int pass = 1; pass <= 2; pass++)
            {
                cv::Mat smallChan;
                cv::resize(channel, smallChan, smallSize, 0, 0, cv::INTER_AREA);

                cv::Scalar g_mu, g_sigma;
                cv::meanStdDev(smallChan, g_mu, g_sigma);

                // Masking
                cv::Mat starMask, blurred;
                cv::GaussianBlur(smallChan, blurred, cv::Size(5, 5), 0);
                float threshMult = (pass == 1) ? 0.6f : 0.2f;
                cv::threshold(smallChan - blurred, starMask, g_sigma[0] * threshMult, 255, cv::THRESH_BINARY);
                starMask.convertTo(starMask, CV_8U);

                int dilateSize = (pass == 1) ? 7 : 13;
                cv::dilate(starMask, starMask, cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(dilateSize, dilateSize)));

                // Protected Sampling
                int marginX = (int)(smallSize.width * 0.05);
                int marginY = (int)(smallSize.height * 0.05);
                int step = (pass == 1) ? 18 : 12;

                int cx1 = smallSize.width * 0.35, cx2 = smallSize.width * 0.65;
                int cy1 = smallSize.height * 0.35, cy2 = smallSize.height * 0.65;

                std::vector<cv::Point3f> points;
                for (int y = marginY; y < smallSize.height - marginY; y += step)
                {
                    const uchar* maskRow = starMask.ptr<uchar>(y);
                    for (int x = marginX; x < smallSize.width - marginX; x += step)
                    {
                        if (maskRow[x] > 0 || (x > cx1 && x < cx2 && y > cy1 && y < cy2))
                            continue;

                        cv::Rect roi = cv::Rect(x - 2, y - 2, 5, 5) & cv::Rect(0, 0, smallSize.width, smallSize.height);
                        double t_mu = cv::mean(smallChan(roi))[0];

                        if (t_mu < g_mu[0] + g_sigma[0] * 0.15)
                            points.push_back(cv::Point3f((float)x, (float)y, (float)t_mu));
                    }
                }

                if (points.size() < 10)
                    break;

                // TPS SOLVER
                int N = (int)points.size();
                int M = N + 3;
                cv::Mat A = cv::Mat::zeros(M, M, CV_64F);
                cv::Mat B_vec = cv::Mat::zeros(M, 1, CV_64F);
                double lambda = (pass == 1) ? 0.1 : 0.05;

                for (int i = 0; i < N; i++)
                {
                    for (int j = 0; j < N; j++)
                    {
                        double dx = points[i].x - points[j].x;
                        double dy = points[i].y - points[j].y;
                        double r2 = dx * dx + dy * dy;
                        if (r2 > 1e-6)
                            // Optimized TPS: 0.5 * r2 * log(r2) avoids sqrt()
                            A.at<double>(i, j) = 0.5 * r2 * std::log(r2);
                        if (i == j)
                            A.at<double>(i, j) += lambda;
                    }
                    A.at<double>(i, N) = points[i].x;
                    A.at<double>(i, N + 1) = points[i].y;
                    A.at<double>(i, N + 2) = 1.0;
                    A.at<double>(N, i) = points[i].x;
                    A.at<double>(N + 1, i) = points[i].y;
                    A.at<double>(N + 2, i) = 1.0;
                    B_vec.at<double>(i, 0) = points[i].z;
                }

                cv::Mat weights;
                if (!cv::solve(A, B_vec, weights, cv::DECOMP_LU))
                    break;

                // Reconstruct Model (Optimized loop)
                cv::Mat modelSmall = cv::Mat::zeros(smallSize, CV_32F);
                const double wN = weights.at<double>(N, 0);
                const double wN1 = weights.at<double>(N + 1, 0);
                const double wN2 = weights.at<double>(N + 2, 0);

                for (int y = 0; y < modelSmall.rows; y++)
                {
                    float* row = modelSmall.ptr<float>(y);
                    for (int x = 0; x < modelSmall.cols; x++)
                    {
                        double val = 0;
                        for (int i = 0; i < N; i++)
                        {
                            double dx = x - points[i].x;
                            double dy = y - points[i].y;
                            double r2 = dx * dx + dy * dy;
                            if (r2 > 1e-6)
                                val += weights.at<double>(i, 0) * (0.5 * r2 * std::log(r2));
                        }
                        val += wN * x + wN1 * y + wN2;
                        row[x] = (float)val;
                    }
                }

                cv::GaussianBlur(modelSmall, modelSmall, cv::Size(11, 11), 0);
                cv::Mat modelFull;
                cv::resize(modelSmall, modelFull, channel.size(), 0, 0, cv::INTER_CUBIC);

                // Apply the correction
                if (pass == 1)
                {
                    cv::Scalar model_mu = cv::mean(modelFull);
                    cv::Mat divCorrection = (1.0f - (float)strength) + (modelFull / (float)model_mu[0] * (float)strength);
                    cv::max(divCorrection, 0.1f, divCorrection);
                    channel /= divCorrection;
                }
                else
                {
                    channel -= (modelFull * (float)strength);
                    cv::Scalar mid_mu = cv::mean(channel);
                    channel += (float)mid_mu[0] * (float)strength;
                }
            }

            // Anchor to original baseline
            cv::Scalar f_mu;
            cv::meanStdDev(channel, f_mu, cv::noArray());
            channel += (originalBackgroundLevel - (float)f_mu[0]);
            cv::max(channel, 0.0f, channel);
        });

        cv::Mat result;
        cv::merge(channels, result);
        qCDebug(KSTARS_FITS) << QString("Gradient removal in %1 ms").arg(timer.elapsed());
        return result;
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS) << QString("openCV exception %1 called from %2").arg(ex.what()).arg(__FUNCTION__);
        return image;
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
        q0.copyTo(tmp);
        q3.copyTo(q0);
        tmp.copyTo(q3);
        q1.copyTo(tmp);
        q2.copyTo(q1);
        tmp.copyTo(q2);

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

void FITSStack::redoPostProcessStack(const StackPPData &ppParams)
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
        finalImage.copyTo(m_StackedImageFinal);
    }
}

// We're done with the original stack so tidy up and keep data necessary to add individual
// subs to the interim stack as they arrive
void FITSStack::setupRunningStack(const int numSubs, const float totalWeight, const cv::Mat &hitMap)
{
    setInitalStackDone(true);
    m_RunningStackImageData.numSubs = numSubs;
    m_RunningStackImageData.ref_hfr = 0;
    m_RunningStackImageData.ref_numStars = 0;
    m_RunningStackImageData.totalWeight = totalWeight;
    if (m_StackData.normalization == StackNormalization::LINEAR && !hitMap.empty())
        m_RunningStackImageData.hitMap = hitMap.clone();

    // Initialize latent for incremental ImageMM
    if (!m_StackedImage32F.empty())
        m_RunningStackImageData.imageMMState.latent = m_StackedImage32F.clone();
    else
        m_RunningStackImageData.imageMMState.latent = cv::Mat::zeros(
                m_StackImageData[0].image.size(), m_StackImageData[0].image.type());

    if (m_StackData.stackingMethod == StackingMethod::IMAGEMM)
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
    tidyUpInitalStack();
}

void FITSStack::updateRunningStack(const int numSubs, const float totalWeight, const cv::Mat &hitMap)
{
    try
    {
        // Update running stack metadata
        m_RunningStackImageData.numSubs += numSubs;
        m_RunningStackImageData.totalWeight = totalWeight;
        if (m_StackData.normalization == StackNormalization::LINEAR && !hitMap.empty())
            m_RunningStackImageData.hitMap = hitMap.clone();

        if (m_StackData.stackingMethod == StackingMethod::IMAGEMM)
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
        tidyUpInitalStack();
    }
    catch (const cv::Exception &ex)
    {
        qCDebug(KSTARS_FITS)
                << QString("OpenCV exception %1 in %2").arg(ex.what()).arg(__FUNCTION__);
    }
}

// Release FITS and openCV memory used in original stack
void FITSStack::tidyUpInitalStack()
{
    for (int i = 0; i < m_StackImageData.size(); i++)
    {
        if (m_StackImageData[i].wcsprm != nullptr)
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
    m_RunningStackImageData.imageMMState = {};
    m_RunningStackImageData.hitMap.release();
}
