/*
    SPDX-FileCopyrightText: 2026 Jasem Mutlaq <mutlaqja@ikarustech.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "masterbuilder.h"
#include "fitsviewer/fitsdata.h"

#include <fitsio.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <cmath>

bool MasterBuilder::loadFrame(const QString &path, cv::Mat &outFrame, double &outMedian, QString &error)
{
    FITSData data(FITS_CALIBRATE);
    QFuture<bool> future = data.loadFromFile(path);
    future.waitForFinished();
    if (!future.result())
    {
        error = QString("Failed to load %1").arg(path);
        return false;
    }

    int cvType = -1;
    switch (data.dataType())
    {
        case TBYTE:
            cvType = CV_MAKETYPE(CV_8U, data.channels());
            break;
        case TUSHORT:
            cvType = CV_MAKETYPE(CV_16U, data.channels());
            break;
        case TFLOAT:
            cvType = CV_MAKETYPE(CV_32F, data.channels());
            break;
        case TDOUBLE:
            cvType = CV_MAKETYPE(CV_64F, data.channels());
            break;
        default:
            error = QString("%1 has an unsupported bit depth for master-building").arg(path);
            return false;
    }

    // FITSData stores multi-channel data plane-by-plane (R plane, then G, then B), not
    // interleaved like a normal cv::Mat — so wrap each channel as its own single-channel
    // Mat and merge, rather than constructing a multi-channel Mat directly over the buffer.
    const int channels = data.channels();
    const int width = data.width();
    const int height = data.height();
    const auto *buffer = data.getImageBuffer();
    const size_t planeBytes = static_cast<size_t>(width) * height * data.getBytesPerPixel();

    std::vector<cv::Mat> planes;
    for (int c = 0; c < channels; c++)
    {
        cv::Mat plane(height, width, CV_MAKETYPE(CV_MAT_DEPTH(cvType), 1),
                      const_cast<uint8_t *>(buffer) + c * planeBytes);
        cv::Mat plane32f;
        plane.convertTo(plane32f, CV_32F);
        planes.push_back(plane32f);
    }
    cv::merge(planes, outFrame);

    outMedian = data.getAverageMedian();
    return true;
}

bool MasterBuilder::readExptime(const QString &path, double &outExptime, QString &error)
{
    fitsfile *fptr = nullptr;
    int status = 0;
    QByteArray pathBytes = path.toLocal8Bit();

    if (fits_open_diskfile(&fptr, pathBytes.constData(), READONLY, &status))
    {
        char errStatus[FLEN_STATUS] = {0};
        fits_get_errstatus(status, errStatus);
        error = QString("Failed to open %1: %2").arg(path).arg(errStatus);
        return false;
    }

    double exptime = 0.0;
    if (fits_read_key(fptr, TDOUBLE, "EXPTIME", &exptime, nullptr, &status))
    {
        char errStatus[FLEN_STATUS] = {0};
        fits_get_errstatus(status, errStatus);
        error = QString("%1 has no readable EXPTIME header: %2").arg(path).arg(errStatus);
        status = 0;
        fits_close_file(fptr, &status);
        return false;
    }

    fits_close_file(fptr, &status);
    outExptime = exptime;
    return true;
}

cv::Mat MasterBuilder::combineSigmaClip(const std::vector<cv::Mat> &frames, double lowSigma, double highSigma)
{
    const cv::Mat &first = frames.front();
    cv::Mat sum = cv::Mat::zeros(first.size(), first.type());
    cv::Mat sumSq = cv::Mat::zeros(first.size(), first.type());

    for (const auto &frame : frames)
    {
        cv::accumulate(frame, sum);
        cv::accumulateSquare(frame, sumSq);
    }

    const double n = static_cast<double>(frames.size());
    cv::Mat mean = sum / n;
    cv::Mat variance = sumSq / n - mean.mul(mean);
    cv::max(variance, 0.0, variance);
    cv::Mat stddev;
    cv::sqrt(variance, stddev);

    if (frames.size() < 3)
        return mean;

    cv::Mat lowThresh = mean - lowSigma * stddev;
    cv::Mat highThresh = mean + highSigma * stddev;

    cv::Mat maskedSum = cv::Mat::zeros(first.size(), first.type());
    cv::Mat maskedCount = cv::Mat::zeros(first.size(), first.type());

    for (const auto &frame : frames)
    {
        cv::Mat keep;
        cv::bitwise_and(frame >= lowThresh, frame <= highThresh, keep);
        cv::Mat keepF;
        keep.convertTo(keepF, first.type(), 1.0 / 255.0);

        cv::Mat contribution;
        cv::multiply(frame, keepF, contribution);
        maskedSum += contribution;
        maskedCount += keepF;
    }

    cv::Mat combined;
    cv::divide(maskedSum, maskedCount, combined);

    // Where every frame got rejected at a pixel (maskedCount == 0, division above leaves
    // NaN/inf), fall back to the unclipped mean rather than propagate garbage.
    cv::Mat noSurvivors = (maskedCount == 0);
    mean.copyTo(combined, noSurvivors);

    return combined;
}

bool MasterBuilder::build(const QString &dir, Type type, cv::Mat &outMaster, QString &error,
                          double lowSigma, double highSigma, const QString &subtractPath,
                          double matchExptime, double exptimeTolerance, int *outUsedCount)
{
    QDir directory(dir);
    if (!directory.exists())
    {
        error = QString("Directory %1 does not exist").arg(dir);
        return false;
    }

    QStringList files;
    for (const auto &info : directory.entryInfoList(QDir::Files, QDir::Name))
    {
        if (FITSData::readableFilename(info.absoluteFilePath()))
            files << info.absoluteFilePath();
    }

    if (files.isEmpty())
    {
        error = QString("No FITS-loadable files found in %1").arg(dir);
        return false;
    }

    // Exposure-time filtering, e.g. for a shared calibration folder that mixes multiple
    // exposure lengths (light-darks and flat-darks alike) under one undifferentiated
    // "Dark Frame" IMAGETYP — see the header comment on this parameter.
    if (matchExptime >= 0.0)
    {
        QStringList matched;
        int checked = 0;
        for (const auto &file : files)
        {
            double exptime = 0.0;
            QString readError;
            if (!readExptime(file, exptime, readError))
            {
                error = readError;
                return false;
            }
            checked++;
            if (std::abs(exptime - matchExptime) <= exptimeTolerance)
                matched << file;
        }

        if (matched.isEmpty())
        {
            error = QString("No frames in %1 have EXPTIME within %2s of %3s (checked %4 files)")
                    .arg(dir).arg(exptimeTolerance).arg(matchExptime).arg(checked);
            return false;
        }

        files = matched;
    }

    if (outUsedCount)
        *outUsedCount = files.size();

    cv::Mat subtractFrame;
    if (!subtractPath.isEmpty())
    {
        double unusedMedian = 0.0;
        if (!loadFrame(subtractPath, subtractFrame, unusedMedian, error))
            return false;
    }

    std::vector<cv::Mat> frames;
    QVector<double> medians;
    for (const auto &file : files)
    {
        cv::Mat frame;
        double median = 0.0;
        if (!loadFrame(file, frame, median, error))
            return false;

        if (!frames.empty() && (frame.size() != frames.front().size() || frame.channels() != frames.front().channels()))
        {
            error = QString("%1 has different dimensions/channels than the rest of %2 — skipping folder").arg(file).arg(dir);
            return false;
        }

        if (!subtractFrame.empty())
        {
            if (frame.size() != subtractFrame.size() || frame.channels() != subtractFrame.channels())
            {
                error = QString("%1 has different dimensions/channels than %2 — cannot subtract")
                        .arg(file).arg(subtractPath);
                return false;
            }
            frame -= subtractFrame;
            // Re-derive the central-tendency estimate post-subtraction for the FLAT
            // normalization step below. Uses the mean rather than a true median here
            // (loadFrame()'s median came from FITSData's stats, computed pre-subtraction
            // and now stale) — a reasonable simplification for a flat's tightly-clustered,
            // largely outlier-free histogram.
            median = cv::mean(frame)[0];
        }

        // Flats need per-frame normalization to correct for illumination drift between
        // subs (e.g. sky brightness changing during a twilight flat sequence) — bias/dark
        // subs don't move between exposures, so this is FLAT-only. FITSStack::addMaster's
        // flat branch re-normalizes by median again anyway, so the exact output scale here
        // doesn't matter downstream — only that inter-frame drift is corrected before the
        // sigma-clip combine below, which otherwise would misfire on real, illumination-
        // driven per-frame differences rather than genuine per-pixel outliers.
        if (type == Type::FLAT)
        {
            if (median <= 0.0)
            {
                error = QString("%1 has zero/negative median — cannot normalize as a flat").arg(file);
                return false;
            }
            frame /= median;
        }

        frames.push_back(frame);
        medians.push_back(median);
    }

    outMaster = combineSigmaClip(frames, lowSigma, highSigma);
    return true;
}

bool MasterBuilder::buildAndSave(const QString &dir, Type type, const QString &outputPath, QString &error,
                                 double lowSigma, double highSigma, const QString &subtractPath,
                                 double matchExptime, double exptimeTolerance)
{
    cv::Mat master;
    int usedCount = 0;
    if (!build(dir, type, master, error, lowSigma, highSigma, subtractPath, matchExptime, exptimeTolerance, &usedCount))
        return false;

    const int width = master.cols;
    const int height = master.rows;
    const int channels = master.channels();

    fitsfile *fptr = nullptr;
    int status = 0;
    long naxis = (channels == 1) ? 2 : 3;
    long naxes[3] = { width, height, channels };
    char errStatus[FLEN_STATUS] = {0};

    // fits_create_diskfile() passes the filename straight to fopen() (that's the whole
    // point of using it over fits_create_file() — no extended-filename-syntax surprises
    // if outputPath ever contains brackets/colons), which means the "!" clobber-prefix
    // convention does NOT apply here — remove any existing file explicitly instead.
    QFile::remove(outputPath);
    QByteArray pathBytes = outputPath.toLocal8Bit();
    if (fits_create_diskfile(&fptr, pathBytes.constData(), &status))
    {
        fits_get_errstatus(status, errStatus);
        error = QString("Failed to create %1: %2").arg(outputPath).arg(errStatus);
        return false;
    }

    if (fits_create_img(fptr, FLOAT_IMG, naxis, naxes, &status))
    {
        fits_get_errstatus(status, errStatus);
        error = QString("Failed to create image in %1: %2").arg(outputPath).arg(errStatus);
        status = 0;
        fits_close_file(fptr, &status);
        return false;
    }

    const char *imageType = (type == Type::BIAS) ? "BIAS" : (type == Type::DARK) ? "DARK" : "FLAT";
    fits_write_key(fptr, TSTRING, "IMAGETYP", (void *)imageType, (char *)"Master calibration frame type", &status);
    fits_write_key(fptr, TINT, "NCOMBINE", &usedCount, (char *)"Number of subs combined", &status);

    if (channels == 3)
    {
        std::vector<cv::Mat> split(3);
        cv::split(master, split);
        const size_t total = static_cast<size_t>(width) * height;
        std::vector<float> planar(total * 3);
        for (int c = 0; c < 3; c++)
            memcpy(planar.data() + c * total, split[c].ptr<float>(), total * sizeof(float));
        fits_write_img(fptr, TFLOAT, 1, total * 3, planar.data(), &status);
    }
    else
    {
        cv::Mat cont = master.isContinuous() ? master : master.clone();
        fits_write_img(fptr, TFLOAT, 1, static_cast<long>(width) * height, cont.data, &status);
    }

    fits_close_file(fptr, &status);

    if (status != 0)
    {
        fits_get_errstatus(status, errStatus);
        error = QString("Failed to write %1: %2").arg(outputPath).arg(errStatus);
        return false;
    }

    return true;
}
