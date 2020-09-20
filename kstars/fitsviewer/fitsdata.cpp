/***************************************************************************
                          FITSImage.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
    copyright            : (C) 2004 by Jasem Mutlaq
    email                : mutlaqja@ikarustech.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   Some code fragments were adapted from Peter Kirchgessner's FITS plugin*
 *   See http://members.aol.com/pkirchg for more details.                  *
 ***************************************************************************/

#include "fitsdata.h"
#include "fitsbahtinovdetector.h"
#include "fitsthresholddetector.h"
#include "fitsgradientdetector.h"
#include "fitscentroiddetector.h"
#include "fitssepdetector.h"

#include "fpack.h"

#include "kstarsdata.h"
#include "ksutils.h"
#include "kspaths.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "auxiliary/ksnotification.h"

#include <KFormat>
#include <QApplication>
#include <QImage>
#include <QtConcurrent>
#include <QImageReader>

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
#include <wcshdr.h>
#include <wcsfix.h>
#endif

#ifndef KSTARS_LITE
#include "fitshistogram.h"
#endif

#include <cfloat>
#include <cmath>

#include <fits_debug.h>

#define ZOOM_DEFAULT   100.0
#define ZOOM_MIN       10
#define ZOOM_MAX       400
#define ZOOM_LOW_INCR  10
#define ZOOM_HIGH_INCR 50

const QString FITSData::m_TemporaryPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);



FITSData::FITSData(FITSMode fitsMode): m_Mode(fitsMode)
{
    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;
}

FITSData::FITSData(const FITSData * other)
{
    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    this->m_Mode = other->m_Mode;
    this->m_DataType = other->m_DataType;
    this->m_Channels = other->m_Channels;
    memcpy(&stats, &(other->stats), sizeof(stats));
    m_ImageBuffer = new uint8_t[stats.samples_per_channel * m_Channels * stats.bytesPerPixel];
    memcpy(m_ImageBuffer, other->m_ImageBuffer, stats.samples_per_channel * m_Channels * stats.bytesPerPixel);
}

FITSData::~FITSData()
{
    int status = 0;

    clearImageBuffers();

#ifdef HAVE_WCSLIB
    if (m_wcs != nullptr)
        wcsvfree(&m_nwcs, &m_wcs);
#endif

    if (starCenters.count() > 0)
        qDeleteAll(starCenters);

    delete[] wcs_coord;

    if (objList.count() > 0)
        qDeleteAll(objList);

    if (fptr != nullptr)
    {
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        fptr = nullptr;

        if (m_isTemporary && autoRemoveTemporaryFITS)
            QFile::remove(m_Filename);
    }

    qDeleteAll(records);
}

void FITSData::loadCommon(const QString &inFilename)
{
    int status = 0;
    qDeleteAll(starCenters);
    starCenters.clear();

    if (fptr != nullptr)
    {
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        fptr = nullptr;

        // If current file is temporary AND
        // Auto Remove Temporary File is Set AND
        // New filename is different from existing filename
        // THen remove it. We have to check for name since we cannot delete
        // the same filename and try to open it below!
        if (m_isTemporary && autoRemoveTemporaryFITS && inFilename != m_Filename)
            QFile::remove(m_Filename);
    }

    m_Filename = inFilename;
}

bool FITSData::loadFITSFromMemory(const QString &inFilename, void *fits_buffer,
                                  size_t fits_buffer_size, bool silent)
{
    loadCommon(inFilename);
    qCDebug(KSTARS_FITS) << "Reading FITS file buffer (" << KFormat().formatByteSize(fits_buffer_size) << ")";
    return privateLoad(fits_buffer, fits_buffer_size, silent);
}

QFuture<bool> FITSData::loadFITS(const QString &inFilename, bool silent)
{
    loadCommon(inFilename);
    qCInfo(KSTARS_FITS) << "Loading FITS file " << m_Filename;
    QFuture<bool> result = QtConcurrent::run(
                               this, &FITSData::privateLoad, nullptr, 0, silent);

    return result;
}

namespace
{
// Common code for reporting fits read errors. Always returns false.
bool fitsOpenError(int status, const QString &message, bool silent)
{
    char error_status[512];
    fits_report_error(stderr, status);
    fits_get_errstatus(status, error_status);
    QString errMessage = message;
    errMessage.append(i18n(" Error: %1", QString::fromUtf8(error_status)));
    if (!silent)
        KSNotification::error(errMessage, i18n("FITS Open"));
    qCCritical(KSTARS_FITS) << errMessage;
    return false;
}
}

bool FITSData::privateLoad(void *fits_buffer, size_t fits_buffer_size, bool silent)
{
    int status = 0, anynull = 0;
    long naxes[3];
    QString errMessage;

    m_isTemporary = m_Filename.startsWith(m_TemporaryPath);

    if (fits_buffer == nullptr && m_Filename.endsWith(".fz"))
    {
        // Store so we don't lose.
        m_compressedFilename = m_Filename;

        QString uncompressedFile = QDir::tempPath() + QString("/%1").arg(QUuid::createUuid().toString().remove(
                                       QRegularExpression("[-{}]")));
        fpstate	fpvar;
        fp_init (&fpvar);
        if (fp_unpack(m_Filename.toLatin1().data(), uncompressedFile.toLatin1().data(), fpvar) < 0)
        {
            errMessage = i18n("Failed to unpack compressed fits");
            if (!silent)
                KSNotification::error(errMessage, i18n("FITS Open"));
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
        }

        // Remove compressed .fz if it was temporary
        if (m_isTemporary && autoRemoveTemporaryFITS)
            QFile::remove(m_Filename);

        m_Filename = uncompressedFile;
        m_isTemporary = true;
        m_isCompressed = true;
    }

    if (fits_buffer == nullptr)
    {
        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        if (fits_open_diskfile(&fptr, m_Filename.toLatin1(), READONLY, &status))
            return fitsOpenError(status, i18n("Error opening fits file %1", m_Filename), silent);
        else
            stats.size = QFile(m_Filename).size();
    }
    else
    {
        // Read the FITS file from a memory buffer.
        void *temp_buffer = fits_buffer;
        size_t temp_size = fits_buffer_size;
        if (fits_open_memfile(&fptr, m_Filename.toLatin1().data(), READONLY,
                              &temp_buffer, &temp_size, 0, nullptr, &status))
            return fitsOpenError(status, i18n("Error reading fits buffer."), silent);
        else
            stats.size = fits_buffer_size;
    }

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
        return fitsOpenError(status, i18n("Could not locate image HDU."), silent);

    if (fits_get_img_param(fptr, 3, &(stats.bitpix), &(stats.ndim), naxes, &status))
        return fitsOpenError(status, i18n("FITS file open error (fits_get_img_param)."), silent);

    if (stats.ndim < 2)
    {
        errMessage = i18n("1D FITS images are not supported in KStars.");
        if (!silent)
            KSNotification::error(errMessage, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << errMessage;
        return false;
    }

    switch (stats.bitpix)
    {
        case BYTE_IMG:
            m_DataType           = TBYTE;
            stats.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_DataType           = TUSHORT;
            stats.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_DataType           = TUSHORT;
            stats.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_DataType           = TULONG;
            stats.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_DataType           = TULONG;
            stats.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_DataType           = TFLOAT;
            stats.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_DataType           = TLONGLONG;
            stats.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_DataType           = TDOUBLE;
            stats.bytesPerPixel = sizeof(double);
            break;
        default:
            errMessage = i18n("Bit depth %1 is not supported.", stats.bitpix);
            if (!silent)
                KSNotification::error(errMessage, i18n("FITS Open"));
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
    }

    if (stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        errMessage = i18n("Image has invalid dimensions %1x%2", naxes[0], naxes[1]);
        if (!silent)
            KSNotification::error(errMessage, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << errMessage;
        return false;
    }

    stats.width               = naxes[0];
    stats.height              = naxes[1];
    stats.samples_per_channel = stats.width * stats.height;

    clearImageBuffers();

    m_Channels = naxes[2];

    // Channels always set to #1 if we are not required to process 3D Cubes
    // Or if mode is not FITS_NORMAL (guide, focus..etc)
    if (m_Mode != FITS_NORMAL || !Options::auto3DCube())
        m_Channels = 1;

    m_ImageBufferSize = stats.samples_per_channel * m_Channels * stats.bytesPerPixel;
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        qCWarning(KSTARS_FITS) << "FITSData: Not enough memory for image_buffer channel. Requested: "
                               << m_ImageBufferSize << " bytes.";
        clearImageBuffers();
        return false;
    }

    rotCounter     = 0;
    flipHCounter   = 0;
    flipVCounter   = 0;
    long nelements = stats.samples_per_channel * m_Channels;

    if (fits_read_img(fptr, m_DataType, 1, nelements, nullptr, m_ImageBuffer, &anynull, &status))
        return fitsOpenError(status, i18n("Error reading image."), silent);

    parseHeader();

    // Get UTC date time
    QVariant value;
    if (getRecordValue("DATE-OBS", value) && value.isValid())
        m_DateTime = KStarsDateTime(value.toDateTime());

    if (Options::autoDebayer() && checkDebayer())
    {
        //m_BayerBuffer = m_ImageBuffer;
        if (debayer())
            calculateStats();
    }
    else
        calculateStats();

    WCSLoaded = false;

    if (m_Mode == FITS_NORMAL || m_Mode == FITS_ALIGN)
        checkForWCS();

    starsSearched = false;

    return true;
}

bool FITSData::saveImage(const QString &newFilename)
{
    if (newFilename == m_Filename)
        return true;

    const QString ext = QFileInfo(newFilename).suffix();

    if (ext == "jpg" || ext == "png")
    {
        double min, max;
        QImage fitsImage = FITSToImage(newFilename);
        getMinMax(&min, &max);

        if (min == max)
        {
            fitsImage.fill(Qt::white);
        }
        else if (channels() == 1)
        {
            fitsImage = QImage(width(), height(), QImage::Format_Indexed8);

            fitsImage.setColorCount(256);
            for (int i = 0; i < 256; i++)
                fitsImage.setColor(i, qRgb(i, i, i));
        }
        else
        {
            fitsImage = QImage(width(), height(), QImage::Format_RGB32);
        }

        double dataMin = stats.mean[0] - stats.stddev[0];
        double dataMax = stats.mean[0] + stats.stddev[0] * 3;

        double bscale = 255. / (dataMax - dataMin);
        double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

        // Long way to do this since we do not want to use templated functions here
        switch (property("dataType").toInt())
        {
            case TBYTE:
                convertToQImage<uint8_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TSHORT:
                convertToQImage<int16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TUSHORT:
                convertToQImage<uint16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TLONG:
                convertToQImage<int32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TULONG:
                convertToQImage<uint32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TFLOAT:
                convertToQImage<float>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TLONGLONG:
                convertToQImage<int64_t>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            case TDOUBLE:
                convertToQImage<double>(dataMin, dataMax, bscale, bzero, fitsImage);
                break;

            default:
                break;
        }

        fitsImage.save(newFilename, ext.toLatin1().constData());

        m_Filename = newFilename;
        qCInfo(KSTARS_FITS) << "Saved image file:" << m_Filename;
        return true;
    }

    if (m_isCompressed)
    {
        KSNotification::error(i18n("Saving compressed files is not supported."));
        return false;
    }

    int status = 0, exttype = 0;
    long nelements;
    fitsfile * new_fptr;

    if (HasDebayer)
    {
        fits_flush_file(fptr, &status);
        /* close current file */
        if (fits_close_file(fptr, &status))
        {
            fits_report_error(stderr, status);
            return status;
        }

        // Skip "!" in the beginning of the new file name
        QString finalFileName(newFilename);

        finalFileName.remove('!');

        // Remove first otherwise copy will fail below if file exists
        QFile::remove(finalFileName);

        if (!QFile::copy(m_Filename, finalFileName))
        {
            qCCritical(KSTARS_FITS()) << "FITS: Failed to copy " << m_Filename << " to " << finalFileName;
            fptr = nullptr;
            return false;
        }

        if (m_isTemporary && autoRemoveTemporaryFITS)
        {
            QFile::remove(m_Filename);
            m_isTemporary = false;
        }

        m_Filename = finalFileName;

        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        fits_open_diskfile(&fptr, m_Filename.toLatin1(), READONLY, &status);
        fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status);

        return true;
    }

    nelements = stats.samples_per_channel * m_Channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    //    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    //    {
    //        fits_report_error(stderr, status);
    //        return status;
    //    }

    if (fits_copy_header(fptr, new_fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    fits_flush_file(fptr, &status);
    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    status = 0;

    fptr = new_fptr;

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, m_DataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(stats.min), "Minimum value", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(stats.max), "Maximum value", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(stats.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(stats.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    QString history =
        QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    int rot = 0, mirror = 0;
    if (rotCounter > 0)
    {
        rot = (90 * rotCounter) % 360;
        if (rot < 0)
            rot += 360;
    }
    if (flipHCounter % 2 != 0 || flipVCounter % 2 != 0)
        mirror = 1;

    if ((rot != 0) || (mirror != 0))
        rotWCSFITS(rot, mirror);

    rotCounter = flipHCounter = flipVCounter = 0;

    if (m_isTemporary && autoRemoveTemporaryFITS)
    {
        QFile::remove(m_Filename);
        m_isTemporary = false;
    }

    m_Filename = newFilename;

    fits_flush_file(fptr, &status);

    qCInfo(KSTARS_FITS) << "Saved FITS file:" << m_Filename;

    return true;
}

void FITSData::clearImageBuffers()
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = nullptr;
    //m_BayerBuffer = nullptr;
}

void FITSData::calculateStats(bool refresh)
{
    // Calculate min max
    calculateMinMax(refresh);

    // Get standard deviation and mean in one run
    switch (m_DataType)
    {
        case TBYTE:
            runningAverageStdDev<uint8_t>();
            break;

        case TSHORT:
            runningAverageStdDev<int16_t>();
            break;

        case TUSHORT:
            runningAverageStdDev<uint16_t>();
            break;

        case TLONG:
            runningAverageStdDev<int32_t>();
            break;

        case TULONG:
            runningAverageStdDev<uint32_t>();
            break;

        case TFLOAT:
            runningAverageStdDev<float>();
            break;

        case TLONGLONG:
            runningAverageStdDev<int64_t>();
            break;

        case TDOUBLE:
            runningAverageStdDev<double>();
            break;

        default:
            return;
    }

    // FIXME That's not really SNR, must implement a proper solution for this value
    stats.SNR = stats.mean[0] / stats.stddev[0];
}

int FITSData::calculateMinMax(bool refresh)
{
    int status, nfound = 0;

    status = 0;

    if ((fptr != nullptr) && !refresh)
    {
        if (fits_read_key_dbl(fptr, "DATAMIN", &(stats.min[0]), nullptr, &status) == 0)
            nfound++;

        if (fits_read_key_dbl(fptr, "DATAMAX", &(stats.max[0]), nullptr, &status) == 0)
            nfound++;

        // If we found both keywords, no need to calculate them, unless they are both zeros
        if (nfound == 2 && !(stats.min[0] == 0 && stats.max[0] == 0))
            return 0;
    }

    stats.min[0] = 1.0E30;
    stats.max[0] = -1.0E30;

    stats.min[1] = 1.0E30;
    stats.max[1] = -1.0E30;

    stats.min[2] = 1.0E30;
    stats.max[2] = -1.0E30;

    switch (m_DataType)
    {
        case TBYTE:
            calculateMinMax<uint8_t>();
            break;

        case TSHORT:
            calculateMinMax<int16_t>();
            break;

        case TUSHORT:
            calculateMinMax<uint16_t>();
            break;

        case TLONG:
            calculateMinMax<int32_t>();
            break;

        case TULONG:
            calculateMinMax<uint32_t>();
            break;

        case TFLOAT:
            calculateMinMax<float>();
            break;

        case TLONGLONG:
            calculateMinMax<int64_t>();
            break;

        case TDOUBLE:
            calculateMinMax<double>();
            break;

        default:
            break;
    }

    //qDebug() << "DATAMIN: " << stats.min << " - DATAMAX: " << stats.max;
    return 0;
}

template <typename T>
QPair<T, T> FITSData::getParitionMinMax(uint32_t start, uint32_t stride)
{
    auto * buffer = reinterpret_cast<T *>(m_ImageBuffer);
    T min = std::numeric_limits<T>::max();
    T max = std::numeric_limits<T>::min();

    uint32_t end = start + stride;

    for (uint32_t i = start; i < end; i++)
    {
        if (buffer[i] < min)
            min = buffer[i];
        else if (buffer[i] > max)
            max = buffer[i];
    }

    return qMakePair(min, max);
}

template <typename T>
void FITSData::calculateMinMax()
{
    T min = std::numeric_limits<T>::max();
    T max = std::numeric_limits<T>::min();

    // Create N threads
    const uint8_t nThreads = 16;

    for (int n = 0; n < m_Channels; n++)
    {
        uint32_t cStart = n * stats.samples_per_channel;

        // Calculate how many elements we process per thread
        uint32_t tStride = stats.samples_per_channel / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + (stats.samples_per_channel - (tStride * nThreads));

        // Start location for inspecting elements
        uint32_t tStart = cStart;

        // List of futures
        QList<QFuture<QPair<T, T>>> futures;

        for (int i = 0; i < nThreads; i++)
        {
            // Run threads
            futures.append(QtConcurrent::run(this, &FITSData::getParitionMinMax<T>, tStart, (i == (nThreads - 1)) ? fStride : tStride));
            tStart += tStride;
        }

        // Now wait for results
        for (int i = 0; i < nThreads; i++)
        {
            QPair<T, T> result = futures[i].result();
            if (result.first < min)
                min = result.first;
            if (result.second > max)
                max = result.second;
        }

        stats.min[n] = min;
        stats.max[n] = max;
    }
}

template <typename T>
QPair<double, double> FITSData::getSquaredSumAndMean(uint32_t start, uint32_t stride)
{
    uint32_t m_n       = 2;
    double m_oldM = 0, m_newM = 0, m_oldS = 0, m_newS = 0;

    auto * buffer = reinterpret_cast<T *>(m_ImageBuffer);
    uint32_t end = start + stride;

    for (uint32_t i = start; i < end; i++)
    {
        m_newM = m_oldM + (buffer[i] - m_oldM) / m_n;
        m_newS = m_oldS + (buffer[i] - m_oldM) * (buffer[i] - m_newM);

        m_oldM = m_newM;
        m_oldS = m_newS;
        m_n++;
    }

    return qMakePair<double, double>(m_newM, m_newS);
}

template <typename T>
void FITSData::runningAverageStdDev()
{
    // Create N threads
    const uint8_t nThreads = 16;

    for (int n = 0; n < m_Channels; n++)
    {
        uint32_t cStart = n * stats.samples_per_channel;

        // Calculate how many elements we process per thread
        uint32_t tStride = stats.samples_per_channel / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + (stats.samples_per_channel - (tStride * nThreads));

        // Start location for inspecting elements
        uint32_t tStart = cStart;

        // List of futures
        QList<QFuture<QPair<double, double>>> futures;

        for (int i = 0; i < nThreads; i++)
        {
            // Run threads
            futures.append(QtConcurrent::run(this, &FITSData::getSquaredSumAndMean<T>, tStart,
                                             (i == (nThreads - 1)) ? fStride : tStride));
            tStart += tStride;
        }

        double mean = 0, squared_sum = 0;

        // Now wait for results
        for (int i = 0; i < nThreads; i++)
        {
            QPair<double, double> result = futures[i].result();
            mean += result.first;
            squared_sum += result.second;
        }

        double variance = squared_sum / stats.samples_per_channel;

        stats.mean[n]   = mean / nThreads;
        stats.stddev[n] = sqrt(variance);
    }
}

QVector<double> FITSData::createGaussianKernel(int size, double sigma)
{
    QVector<double> kernel(size * size);
    kernel.fill(0.0, size * size);

    double kernelSum = 0.0;
    int fOff = (size - 1) / 2;
    double normal = 1.0 / (2.0 * M_PI * sigma * sigma);
    for (int y = -fOff; y <= fOff; y++)
    {
        for (int x = -fOff; x <= fOff; x++)
        {
            double distance = ((y * y) + (x * x)) / (2.0 * sigma * sigma);
            int index = (y + fOff) * size + (x + fOff);
            kernel[index] = normal * qExp(-distance);
            kernelSum += kernel.at(index);
        }
    }
    for (int y = 0; y < size; y++)
    {
        for (int x = 0; x < size; x++)
        {
            int index = y * size + x;
            kernel[index] = kernel.at(index) * 1.0 / kernelSum;
        }
    }

    return kernel;
}

template <typename T>
void FITSData::convolutionFilter(const QVector<double> &kernel, int kernelSize)
{
    T * imagePtr = reinterpret_cast<T *>(m_ImageBuffer);

    // Create variable for pixel data for each kernel
    T gt = 0;

    // This is how much your center pixel is offset from the border of your kernel
    int fOff = (kernelSize - 1) / 2;

    // Start with the pixel that is offset fOff from top and fOff from the left side
    // this is so entire kernel is on your image
    for (int offsetY = 0; offsetY < stats.height; offsetY++)
    {
        for (int offsetX = 0; offsetX < stats.width; offsetX++)
        {
            // reset gray value to 0
            gt = 0;
            // position of the kernel center pixel
            int byteOffset = offsetY * stats.width + offsetX;

            // kernel calculations
            for (int filterY = -fOff; filterY <= fOff; filterY++)
            {
                for (int filterX = -fOff; filterX <= fOff; filterX++)
                {
                    if ((offsetY + filterY) >= 0 && (offsetY + filterY) < stats.height
                            && ((offsetX + filterX) >= 0 && (offsetX + filterX) < stats.width ))
                    {

                        int calcOffset = byteOffset + filterX + filterY * stats.width;
                        int index = (filterY + fOff) * kernelSize + (filterX + fOff);
                        double kernelValue = kernel.at(index);
                        gt += (imagePtr[calcOffset]) * kernelValue;
                    }
                }
            }

            // set new data in the other byte array for your image data
            imagePtr[byteOffset] = gt;
        }
    }
}

template <typename T>
void FITSData::gaussianBlur(int kernelSize, double sigma)
{
    // Size must be an odd number!
    if (kernelSize % 2 == 0)
    {
        kernelSize--;
        qCInfo(KSTARS_FITS) << "Warning, size must be an odd number, correcting size to " << kernelSize;
    }
    // Edge must be a positive number!
    if (kernelSize < 1)
    {
        kernelSize = 1;
    }

    QVector<double> gaussianKernel = createGaussianKernel(kernelSize, sigma);
    convolutionFilter<T>(gaussianKernel, kernelSize);
}

void FITSData::setMinMax(double newMin, double newMax, uint8_t channel)
{
    stats.min[channel] = newMin;
    stats.max[channel] = newMax;
}

bool FITSData::parseHeader()
{
    char * header = nullptr;
    int status = 0, nkeys = 0;

    if (fits_hdr2str(fptr, 0, nullptr, 0, &header, &nkeys, &status))
    {
        fits_report_error(stderr, status);
        free(header);
        return false;
    }

    QString recordList = QString(header);

    for (int i = 0; i < nkeys; i++)
    {
        Record * oneRecord = new Record;
        // Quotes cause issues for simplified below so we're removing them.
        QString record = recordList.mid(i * 80, 80).remove("'");
        QStringList properties = record.split(QRegExp("[=/]"));
        // If it is only a comment
        if (properties.size() == 1)
        {
            oneRecord->key = properties[0].mid(0, 7);
            oneRecord->comment = properties[0].mid(8).simplified();
        }
        else
        {
            oneRecord->key = properties[0].simplified();
            oneRecord->value = properties[1].simplified();
            if (properties.size() > 2)
                oneRecord->comment = properties[2].simplified();

            // Try to guess the value.
            // Test for integer & double. If neither, then leave it as "string".
            bool ok = false;

            // Is it Integer?
            oneRecord->value.toInt(&ok);
            if (ok)
                oneRecord->value.convert(QMetaType::Int);
            else
            {
                // Is it double?
                oneRecord->value.toDouble(&ok);
                if (ok)
                    oneRecord->value.convert(QMetaType::Double);
            }
        }

        records.append(oneRecord);
    }

    free(header);

    return true;
}

bool FITSData::getRecordValue(const QString &key, QVariant &value) const
{
    for (Record * oneRecord : records)
    {
        if (oneRecord->key == key)
        {
            value = oneRecord->value;
            return true;
        }
    }

    return false;
}

int FITSData::findStars(StarAlgorithm algorithm, const QRect &trackingBox)
{
    int count = 0;
    starAlgorithm = algorithm;

    qDeleteAll(starCenters);
    starCenters.clear();

    switch (algorithm)
    {
        case ALGORITHM_SEP:
        {
            if (m_Mode == FITS_NORMAL && trackingBox.isNull() && Options::quickHFR())
            {
                // Just finds stars in the center 25% of the image.
                const int w = getStatistics().width;
                const int h = getStatistics().height;
                QRect middle(static_cast<int>(w * 0.25), static_cast<int>(h * 0.25), w / 2, h / 2);
                count = FITSSEPDetector(this)
                        .configure("radiusIsBoundary", "false") // need this with QRect.
                        .findSources(starCenters, middle);
            }
            else
                count = FITSSEPDetector(this)
                        .findSources(starCenters, trackingBox);
            break;
        }

        case ALGORITHM_GRADIENT:
            count = FITSGradientDetector(this)
                    .findSources(starCenters, trackingBox);
            break;

        case ALGORITHM_CENTROID:
#ifndef KSTARS_LITE
            if (histogram)
                if (!histogram->isConstructed())
                    histogram->constructHistogram();

            count = FITSCentroidDetector(this)
                    .configure("JMINDEX", histogram ? histogram->getJMIndex() : 100)
                    .findSources(starCenters, trackingBox);
#else
            count = FITSCentroidDetector(this)
                    .findSources(starCenters, trackingBox);
#endif
            break;

        case ALGORITHM_THRESHOLD:
            count = FITSThresholdDetector(this)
                    .configure("THRESHOLD_PERCENTAGE", Options::focusThreshold())
                    .findSources(starCenters, trackingBox);
            break;

        case ALGORITHM_BAHTINOV:
            count = FITSBahtinovDetector(this)
                    .configure("NUMBER_OF_AVERAGE_ROWS", Options::focusMultiRowAverage())
                    .findSources(starCenters, trackingBox);
            break;
    }

    starsSearched = true;

    return count;
}

int FITSData::filterStars(const float innerRadius, const float outerRadius)
{
    long const sqDiagonal = this->width() * this->width() / 4 + this->height() * this->height() / 4;
    long const sqInnerRadius = std::lround(sqDiagonal * innerRadius * innerRadius);
    long const sqOuterRadius = std::lround(sqDiagonal * outerRadius * outerRadius);

    starCenters.erase(std::remove_if(starCenters.begin(), starCenters.end(),
                                     [&](Edge * edge)
    {
        long const x = edge->x - this->width() / 2;
        long const y = edge->y - this->height() / 2;
        long const sqRadius = x * x + y * y;
        return sqRadius < sqInnerRadius || sqOuterRadius < sqRadius;
    }), starCenters.end());

    return starCenters.count();
}

double FITSData::getHFR(HFRType type)
{
    // This method is less susceptible to noise
    // Get HFR for the brightest star only, instead of averaging all stars
    // It is more consistent.
    // TODO: Try to test this under using a real CCD.

    if (starCenters.empty())
        return -1;

    if (type == HFR_MAX)
    {
        maxHFRStar   = nullptr;
        int maxVal   = 0;
        int maxIndex = 0;
        for (int i = 0; i < starCenters.count(); i++)
        {
            if (starCenters[i]->HFR > maxVal)
            {
                maxIndex = i;
                maxVal   = starCenters[i]->HFR;
            }
        }

        maxHFRStar = starCenters[maxIndex];
        return static_cast<double>(starCenters[maxIndex]->HFR);
    }

    // We may remove saturated stars from the HFR calculation, if we have enough stars.
    // No real way to tell the scale, so only remove saturated stars with range 0 -> 2**16
    // for non-byte types. Unsigned types and floating types, or really any pixels whose
    // range isn't 0-64 (or 0-255 for bytes) won't have their saturated stars removed.
    const int saturationValue = m_DataType == TBYTE ? 250 : 50000;
    int numSaturated = 0;
    for (auto center : starCenters)
        if (center->val > saturationValue)
            numSaturated++;
    const bool removeSaturatedStars = starCenters.size() - numSaturated > 20;
    if (removeSaturatedStars && numSaturated > 0)
        qCDebug(KSTARS_FITS) << "Removing " << numSaturated << " stars from HFR calculation";

    QVector<double> HFRs;
    for (auto center : starCenters)
    {
        if (removeSaturatedStars && center->val > saturationValue) continue;
        HFRs << center->HFR;
    }
    std::sort(HFRs.begin(), HFRs.end());

    double sum = std::accumulate(HFRs.begin(), HFRs.end(), 0.0);
    double m =  sum / HFRs.size();

    if (HFRs.size() > 3)
    {
        double accum = 0.0;
        std::for_each (HFRs.begin(), HFRs.end(), [&](const double d)
        {
            accum += (d - m) * (d - m);
        });
        double stddev = sqrt(accum / (HFRs.size() - 1));

        // Remove stars over 2 standard deviations away.
        auto end1 = std::remove_if(HFRs.begin(), HFRs.end(), [m, stddev](const double hfr)
        {
            return hfr > (m + stddev * 2);
        });
        auto end2 = std::remove_if(HFRs.begin(), end1, [m, stddev](const double hfr)
        {
            return hfr < (m - stddev * 2);
        });

        // New mean
        sum = std::accumulate(HFRs.begin(), end2, 0.0);
        const int num_remaining = std::distance(HFRs.begin(), end2);
        if (num_remaining > 0) m = sum / num_remaining;
    }

    return m;
}

double FITSData::getHFR(int x, int y)
{
    if (starCenters.empty())
        return -1;

    for (int i = 0; i < starCenters.count(); i++)
    {
        if (std::fabs(starCenters[i]->x - x) <= starCenters[i]->width / 2 &&
                std::fabs(starCenters[i]->y - y) <= starCenters[i]->width / 2)
        {
            return starCenters[i]->HFR;
        }
    }

    return -1;
}

void FITSData::applyFilter(FITSScale type, uint8_t * image, QVector<double> * min, QVector<double> * max)
{
    if (type == FITS_NONE)
        return;

    QVector<double> dataMin(3);
    QVector<double> dataMax(3);

    if (min)
        dataMin = *min;
    if (max)
        dataMax = *max;

    switch (type)
    {
        case FITS_AUTO_STRETCH:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = stats.mean[i] - stats.stddev[i];
                dataMax[i] = stats.mean[i] + stats.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_CONTRAST:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = stats.mean[i] + stats.stddev[i];
                dataMax[i] = stats.mean[i] + stats.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_PASS:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = stats.mean[i];
            }
        }
        break;

        default:
            break;
    }

    switch (m_DataType)
    {
        case TBYTE:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT8_MAX ? UINT8_MAX : dataMax[i];
            }
            applyFilter<uint8_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TSHORT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < INT16_MIN ? INT16_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > INT16_MAX ? INT16_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }

        break;

        case TUSHORT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT16_MAX ? UINT16_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TLONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < INT_MIN ? INT_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > INT_MAX ? INT_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TULONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < 0 ? 0 : dataMin[i];
                dataMax[i] = dataMax[i] > UINT_MAX ? UINT_MAX : dataMax[i];
            }
            applyFilter<uint16_t>(type, image, &dataMin, &dataMax);
        }
        break;

        case TFLOAT:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < FLT_MIN ? FLT_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > FLT_MAX ? FLT_MAX : dataMax[i];
            }
            applyFilter<float>(type, image, &dataMin, &dataMax);
        }
        break;

        case TLONGLONG:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < LLONG_MIN ? LLONG_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > LLONG_MAX ? LLONG_MAX : dataMax[i];
            }

            applyFilter<long>(type, image, &dataMin, &dataMax);
        }
        break;

        case TDOUBLE:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = dataMin[i] < DBL_MIN ? DBL_MIN : dataMin[i];
                dataMax[i] = dataMax[i] > DBL_MAX ? DBL_MAX : dataMax[i];
            }
            applyFilter<double>(type, image, &dataMin, &dataMax);
        }

        break;

        default:
            return;
    }

    if (min != nullptr)
        *min = dataMin;
    if (max != nullptr)
        *max = dataMax;
}

template <typename T>
void FITSData::applyFilter(FITSScale type, uint8_t * targetImage, QVector<double> * targetMin, QVector<double> * targetMax)
{
    bool calcStats = false;
    T * image = nullptr;

    if (targetImage)
        image = reinterpret_cast<T *>(targetImage);
    else
    {
        image     = reinterpret_cast<T *>(m_ImageBuffer);
        calcStats = true;
    }

    T min[3], max[3];
    for (int i = 0; i < 3; i++)
    {
        min[i] = (*targetMin)[i] < std::numeric_limits<T>::min() ? std::numeric_limits<T>::min() : (*targetMin)[i];
        max[i] = (*targetMax)[i] > std::numeric_limits<T>::max() ? std::numeric_limits<T>::max() : (*targetMax)[i];
    }


    // Create N threads
    const uint8_t nThreads = 16;

    uint32_t width  = stats.width;
    uint32_t height = stats.height;

    //QTime timer;
    //timer.start();
    switch (type)
    {
        case FITS_AUTO:
        case FITS_LINEAR:
        case FITS_AUTO_STRETCH:
        case FITS_HIGH_CONTRAST:
        case FITS_LOG:
        case FITS_SQRT:
        case FITS_HIGH_PASS:
        {
            // List of futures
            QList<QFuture<void>> futures;
            QVector<double> coeff(3);

            if (type == FITS_LOG)
            {
                for (int i = 0; i < 3; i++)
                    coeff[i] = max[i] / std::log(1 + max[i]);
            }
            else if (type == FITS_SQRT)
            {
                for (int i = 0; i < 3; i++)
                    coeff[i] = max[i] / sqrt(max[i]);
            }

            for (int n = 0; n < m_Channels; n++)
            {
                if (type == FITS_HIGH_PASS)
                    min[n] = stats.mean[n];

                uint32_t cStart = n * stats.samples_per_channel;

                // Calculate how many elements we process per thread
                uint32_t tStride = stats.samples_per_channel / nThreads;

                // Calculate the final stride since we can have some left over due to division above
                uint32_t fStride = tStride + (stats.samples_per_channel - (tStride * nThreads));

                T * runningBuffer = image + cStart;

                if (type == FITS_LOG)
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              coeff, n](T & a)
                        {
                            a = qBound(min[n], static_cast<T>(round(coeff[n] * std::log(1 + qBound(min[n], a, max[n])))), max[n]);
                        }));

                        runningBuffer += tStride;
                    }
                }
                else if (type == FITS_SQRT)
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              coeff, n](T & a)
                        {
                            a = qBound(min[n], static_cast<T>(round(coeff[n] * a)), max[n]);
                        }));
                    }

                    runningBuffer += tStride;
                }
                else
                {
                    for (int i = 0; i < nThreads; i++)
                    {
                        // Run threads
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max,
                                                              n](T & a)
                        {
                            a = qBound(min[n], a, max[n]);
                        }));
                        runningBuffer += tStride;
                    }
                }
            }

            for (int i = 0; i < nThreads * m_Channels; i++)
                futures[i].waitForFinished();

            if (calcStats)
            {
                for (int i = 0; i < 3; i++)
                {
                    stats.min[i] = min[i];
                    stats.max[i] = max[i];
                }
                //if (type != FITS_AUTO && type != FITS_LINEAR)
                runningAverageStdDev<T>();
                //QtConcurrent::run(this, &FITSData::runningAverageStdDev<T>);
            }
        }
        break;

        case FITS_EQUALIZE:
        {
#ifndef KSTARS_LITE
            if (histogram == nullptr)
                return;

            if (!histogram->isConstructed())
                histogram->constructHistogram();

            T bufferVal                    = 0;
            QVector<uint32_t> cumulativeFreq = histogram->getCumulativeFrequency();

            double coeff = 255.0 / (height * width);
            uint32_t row = 0;
            uint32_t index = 0;

            for (int i = 0; i < m_Channels; i++)
            {
                uint32_t offset = i * stats.samples_per_channel;
                for (uint32_t j = 0; j < height; j++)
                {
                    row = offset + j * width;
                    for (uint32_t k = 0; k < width; k++)
                    {
                        index     = k + row;
                        bufferVal = (image[index] - min[i]) / histogram->getBinWidth(i);

                        if (bufferVal >= cumulativeFreq.size())
                            bufferVal = cumulativeFreq.size() - 1;

                        image[index] = qBound(min[i], static_cast<T>(round(coeff * cumulativeFreq[bufferVal])), max[i]);
                    }
                }
            }
#endif
        }
        if (calcStats)
            calculateStats(true);
        break;

        // Based on http://www.librow.com/articles/article-1
        case FITS_MEDIAN:
        {
            uint8_t BBP      = stats.bytesPerPixel;
            auto * extension = new T[(width + 2) * (height + 2)];
            //   Check memory allocation
            if (!extension)
                return;
            //   Create image extension
            for (uint32_t ch = 0; ch < m_Channels; ch++)
            {
                uint32_t offset = ch * stats.samples_per_channel;
                uint32_t N = width, M = height;

                for (uint32_t i = 0; i < M; ++i)
                {
                    memcpy(extension + (N + 2) * (i + 1) + 1, image + (N * i) + offset, N * BBP);
                    extension[(N + 2) * (i + 1)]     = image[N * i + offset];
                    extension[(N + 2) * (i + 2) - 1] = image[N * (i + 1) - 1 + offset];
                }
                //   Fill first line of image extension
                memcpy(extension, extension + N + 2, (N + 2) * BBP);
                //   Fill last line of image extension
                memcpy(extension + (N + 2) * (M + 1), extension + (N + 2) * M, (N + 2) * BBP);
                //   Call median filter implementation

                N = width + 2;
                M = height + 2;

                //   Move window through all elements of the image
                for (uint32_t m = 1; m < M - 1; ++m)
                    for (uint32_t n = 1; n < N - 1; ++n)
                    {
                        //   Pick up window elements
                        int k = 0;
                        float window[9];

                        memset(&window[0], 0, 9 * sizeof(float));
                        for (uint32_t j = m - 1; j < m + 2; ++j)
                            for (uint32_t i = n - 1; i < n + 2; ++i)
                                window[k++] = extension[j * N + i];
                        //   Order elements (only half of them)
                        for (uint32_t j = 0; j < 5; ++j)
                        {
                            //   Find position of minimum element
                            int mine = j;
                            for (uint32_t l = j + 1; l < 9; ++l)
                                if (window[l] < window[mine])
                                    mine = l;
                            //   Put found minimum element in its place
                            const float temp = window[j];
                            window[j]        = window[mine];
                            window[mine]     = temp;
                        }
                        //   Get result - the middle element
                        image[(m - 1) * (N - 2) + n - 1 + offset] = window[4];
                    }
            }

            //   Free memory
            delete[] extension;

            if (calcStats)
                runningAverageStdDev<T>();
        }
        break;

        case FITS_GAUSSIAN:
            gaussianBlur<T>(Options::focusGaussianKernelSize(), Options::focusGaussianSigma());
            if (calcStats)
                calculateStats(true);
            break;

        case FITS_ROTATE_CW:
            rotFITS<T>(90, 0);
            rotCounter++;
            break;

        case FITS_ROTATE_CCW:
            rotFITS<T>(270, 0);
            rotCounter--;
            break;

        case FITS_FLIP_H:
            rotFITS<T>(0, 1);
            flipHCounter++;
            break;

        case FITS_FLIP_V:
            rotFITS<T>(0, 2);
            flipVCounter++;
            break;

        default:
            break;
    }
}

QList<Edge *> FITSData::getStarCentersInSubFrame(QRect subFrame) const
{
    QList<Edge *> starCentersInSubFrame;
    for (int i = 0; i < starCenters.count(); i++)
    {
        int x = static_cast<int>(starCenters[i]->x);
        int y = static_cast<int>(starCenters[i]->y);
        if(subFrame.contains(x, y))
        {
            starCentersInSubFrame.append(starCenters[i]);
        }
    }
    return starCentersInSubFrame;
}

bool FITSData::checkForWCS()
{
#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB

    int status = 0;
    char * header;
    int nkeyrec, nreject;

    // Free wcs before re-use
    if (m_wcs != nullptr)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
    }

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        lastError = errmsg;
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &m_nwcs, &m_wcs)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_wcs);
        lastError = QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]);
        return false;
    }

    free(header);

    if (m_wcs == nullptr)
    {
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_wcs->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    if ((status = wcsset(m_wcs)) != 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    HasWCS = true;
#endif
#endif
    return HasWCS;
}

bool FITSData::loadWCS()
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)

    if (WCSLoaded)
    {
        qWarning() << "WCS data already loaded";
        return true;
    }

    if (m_wcs != nullptr)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
    }

    qCDebug(KSTARS_FITS) << "Started WCS Data Processing...";

    int status = 0;
    char * header;
    int nkeyrec, nreject, nwcs, stat[2];
    double imgcrd[2], phi = 0, pixcrd[2], theta = 0, world[2];
    int w  = width();
    int h = height();

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        lastError = errmsg;
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &nwcs, &m_wcs)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]);
        return false;
    }

    free(header);

    if (m_wcs == nullptr)
    {
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_wcs->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    if ((status = wcsset(m_wcs)) != 0)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    delete[] wcs_coord;

    wcs_coord = new wcs_point[w * h];

    if (wcs_coord == nullptr)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = "Not enough memory for WCS data!";
        return false;
    }

    wcs_point * p = wcs_coord;

    for (int i = 0; i < h; i++)
    {
        for (int j = 0; j < w; j++)
        {
            pixcrd[0] = j;
            pixcrd[1] = i;

            if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
            {
                lastError = QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]);
            }
            else
            {
                p->ra  = world[0];
                p->dec = world[1];

                p++;
            }
        }
    }

    findObjectsInImage(&world[0], phi, theta, &imgcrd[0], &pixcrd[0], &stat[0]);

    WCSLoaded = true;
    HasWCS = true;

    qCDebug(KSTARS_FITS) << "Finished WCS Data processing...";

    return true;
#else
    return false;
#endif
}

bool FITSData::wcsToPixel(SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    int status = 0;
    int stat[2];
    double imgcrd[2], worldcrd[2], pixcrd[2], phi[2], theta[2];

    if (m_wcs == nullptr)
    {
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    worldcrd[0] = wcsCoord.ra0().Degrees();
    worldcrd[1] = wcsCoord.dec0().Degrees();

    if ((status = wcss2p(m_wcs, 1, 2, &worldcrd[0], &phi[0], &theta[0], &imgcrd[0], &pixcrd[0], &stat[0])) != 0)
    {
        lastError = QString("wcss2p error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    wcsImagePoint.setX(imgcrd[0]);
    wcsImagePoint.setY(imgcrd[1]);

    wcsPixelPoint.setX(pixcrd[0]);
    wcsPixelPoint.setY(pixcrd[1]);

    return true;
#else
    Q_UNUSED(wcsCoord);
    Q_UNUSED(wcsPixelPoint);
    Q_UNUSED(wcsImagePoint);
    return false;
#endif
}

bool FITSData::pixelToWCS(const QPointF &wcsPixelPoint, SkyPoint &wcsCoord)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    int status = 0;
    int stat[2];
    double imgcrd[2], phi, pixcrd[2], theta, world[2];

    if (m_wcs == nullptr)
    {
        lastError = i18n("No world coordinate systems found.");
        return false;
    }

    pixcrd[0] = wcsPixelPoint.x();
    pixcrd[1] = wcsPixelPoint.y();

    if ((status = wcsp2s(m_wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])) != 0)
    {
        lastError = QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }
    else
    {
        wcsCoord.setRA0(world[0] / 15.0);
        wcsCoord.setDec0(world[1]);
    }

    return true;
#else
    Q_UNUSED(wcsPixelPoint);
    Q_UNUSED(wcsCoord);
    return false;
#endif
}

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
void FITSData::findObjectsInImage(double world[], double phi, double theta, double imgcrd[], double pixcrd[],
                                  int stat[])
{
    int w = width();
    int h = height();
    int status = 0;
    char date[64];
    KSNumbers * num = nullptr;

    if (fits_read_keyword(fptr, "DATE-OBS", date, nullptr, &status) == 0)
    {
        QString tsString(date);
        tsString = tsString.remove('\'').trimmed();
        // Add Zulu time to indicate UTC
        tsString += "Z";

        QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

        if (ts.isValid())
            num = new KSNumbers(KStarsDateTime(ts).djd());
    }
    if (num == nullptr)
        num = new KSNumbers(KStarsData::Instance()->ut().djd()); //Set to current time if the above does not work.

    SkyMapComposite * map = KStarsData::Instance()->skyComposite();

    wcs_point * wcs_coord = getWCSCoord();
    if (wcs_coord != nullptr)
    {
        int size = w * h;

        objList.clear();

        SkyPoint p1;
        p1.setRA0(dms(wcs_coord[0].ra));
        p1.setDec0(dms(wcs_coord[0].dec));
        p1.updateCoordsNow(num);
        SkyPoint p2;
        p2.setRA0(dms(wcs_coord[size - 1].ra));
        p2.setDec0(dms(wcs_coord[size - 1].dec));
        p2.updateCoordsNow(num);
        QList<SkyObject *> list = map->findObjectsInArea(p1, p2);

        foreach (SkyObject * object, list)
        {
            int type = object->type();
            if (object->name() == "star" || type == SkyObject::PLANET || type == SkyObject::ASTEROID ||
                    type == SkyObject::COMET || type == SkyObject::SUPERNOVA || type == SkyObject::MOON ||
                    type == SkyObject::SATELLITE)
            {
                //DO NOT DISPLAY, at least for now, because these things move and change.
            }

            int x = -100;
            int y = -100;

            world[0] = object->ra0().Degrees();
            world[1] = object->dec0().Degrees();

            if ((status = wcss2p(m_wcs, 1, 2, &world[0], &phi, &theta, &imgcrd[0], &pixcrd[0], &stat[0])) != 0)
            {
                fprintf(stderr, "wcsp2s ERROR %d: %s.\n", status, wcs_errmsg[status]);
            }
            else
            {
                x = pixcrd[0]; //The X and Y are set to the found position if it does work.
                y = pixcrd[1];
            }

            if (x > 0 && y > 0 && x < w && y < h)
                objList.append(new FITSSkyObject(object, x, y));
        }
    }

    delete (num);
}
#endif

QList<FITSSkyObject *> FITSData::getSkyObjects()
{
    return objList;
}

int FITSData::getFlipVCounter() const
{
    return flipVCounter;
}

void FITSData::setFlipVCounter(int value)
{
    flipVCounter = value;
}

int FITSData::getFlipHCounter() const
{
    return flipHCounter;
}

void FITSData::setFlipHCounter(int value)
{
    flipHCounter = value;
}

int FITSData::getRotCounter() const
{
    return rotCounter;
}

void FITSData::setRotCounter(int value)
{
    rotCounter = value;
}

/* Rotate an image by 90, 180, or 270 degrees, with an optional
 * reflection across the vertical or horizontal axis.
 * verbose generates extra info on stdout.
 * return nullptr if successful or rotated image.
 */
template <typename T>
bool FITSData::rotFITS(int rotate, int mirror)
{
    int ny, nx;
    int x1, y1, x2, y2;
    uint8_t * rotimage = nullptr;
    int offset        = 0;

    if (rotate == 1)
        rotate = 90;
    else if (rotate == 2)
        rotate = 180;
    else if (rotate == 3)
        rotate = 270;
    else if (rotate < 0)
        rotate = rotate + 360;

    nx = stats.width;
    ny = stats.height;

    int BBP = stats.bytesPerPixel;

    /* Allocate buffer for rotated image */
    rotimage = new uint8_t[stats.samples_per_channel * m_Channels * BBP];

    if (rotimage == nullptr)
    {
        qWarning() << "Unable to allocate memory for rotated image buffer!";
        return false;
    }

    auto * rotBuffer = reinterpret_cast<T *>(rotimage);
    auto * buffer    = reinterpret_cast<T *>(m_ImageBuffer);

    /* Mirror image without rotation */
    if (rotate < 45 && rotate > -45)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y1 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
    }

    /* Rotate by 90 degrees */
    else if (rotate >= 45 && rotate < 135)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = x1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }

        stats.width  = ny;
        stats.height = nx;
    }

    /* Rotate by 180 degrees */
    else if (rotate >= 135 && rotate < 225)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2 * nx) + x1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        x2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * nx) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
    }

    /* Rotate by 270 degrees */
    else if (rotate >= 225 && rotate < 315)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = y1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2                                 = nx - x1 - 1;
                        rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                    }
                }
            }
        }

        stats.width  = ny;
        stats.height = nx;
    }

    /* If rotating by more than 315 degrees, assume top-bottom reflection */
    else if (rotate >= 315 && mirror)
    {
        for (int i = 0; i < m_Channels; i++)
        {
            offset = stats.samples_per_channel * i;
            for (y1 = 0; y1 < ny; y1++)
            {
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2                                 = y1;
                    y2                                 = x1;
                    rotBuffer[(y2 * ny) + x2 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
    }

    delete[] m_ImageBuffer;
    m_ImageBuffer = rotimage;

    return true;
}

void FITSData::rotWCSFITS(int angle, int mirror)
{
    int status = 0;
    char comment[100];
    double ctemp1, ctemp2, ctemp3, ctemp4, naxis1, naxis2;
    int WCS_DECIMALS = 6;

    naxis1 = stats.width;
    naxis2 = stats.height;

    if (fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
    {
        // No WCS keywords
        return;
    }

    /* Reset CROTAn and CD matrix if axes have been exchanged */
    if (angle == 90)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA1", ctemp1 + 90.0, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA2", ctemp1 + 90.0, WCS_DECIMALS, comment, &status);
    }

    status = 0;

    /* Negate rotation angle if mirrored */
    if (mirror != 0)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA1", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CROTA2", -ctemp1, WCS_DECIMALS, comment, &status);

        status = 0;

        if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "LTM1_1", -ctemp1, WCS_DECIMALS, comment, &status);

        status = 0;

        if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD1_2", -ctemp1, WCS_DECIMALS, comment, &status);

        if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp1, comment, &status))
            fits_update_key_dbl(fptr, "CD2_1", -ctemp1, WCS_DECIMALS, comment, &status);
    }

    status = 0;

    /* Unbin CRPIX and CD matrix */
    if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status))
    {
        if (ctemp1 != 1.0)
        {
            if (!fits_read_key_dbl(fptr, "LTM2_2", &ctemp2, comment, &status))
                if (ctemp1 == ctemp2)
                {
                    double ltv1 = 0.0;
                    double ltv2 = 0.0;
                    status      = 0;
                    if (!fits_read_key_dbl(fptr, "LTV1", &ltv1, comment, &status))
                        fits_delete_key(fptr, "LTV1", &status);
                    if (!fits_read_key_dbl(fptr, "LTV2", &ltv2, comment, &status))
                        fits_delete_key(fptr, "LTV2", &status);

                    status = 0;

                    if (!fits_read_key_dbl(fptr, "CRPIX1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CRPIX1", (ctemp3 - ltv1) / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CRPIX2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CRPIX2", (ctemp3 - ltv2) / ctemp1, WCS_DECIMALS, comment, &status);

                    status = 0;

                    if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD1_1", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD1_2", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD2_1", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    if (!fits_read_key_dbl(fptr, "CD2_2", &ctemp3, comment, &status))
                        fits_update_key_dbl(fptr, "CD2_2", ctemp3 / ctemp1, WCS_DECIMALS, comment, &status);

                    status = 0;

                    fits_delete_key(fptr, "LTM1_1", &status);
                    fits_delete_key(fptr, "LTM1_2", &status);
                }
        }
    }

    status = 0;

    /* Reset CRPIXn */
    if (!fits_read_key_dbl(fptr, "CRPIX1", &ctemp1, comment, &status) &&
            !fits_read_key_dbl(fptr, "CRPIX2", &ctemp2, comment, &status))
    {
        if (mirror != 0)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CRPIX1", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis2 - ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CRPIX2", naxis1 - ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    status = 0;

    /* Reset CDELTn (degrees per pixel) */
    if (!fits_read_key_dbl(fptr, "CDELT1", &ctemp1, comment, &status) &&
            !fits_read_key_dbl(fptr, "CDELT2", &ctemp2, comment, &status))
    {
        if (mirror != 0)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status);
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    /* Reset CD matrix, if present */
    ctemp1 = 0.0;
    ctemp2 = 0.0;
    ctemp3 = 0.0;
    ctemp4 = 0.0;
    status = 0;
    if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status))
    {
        fits_read_key_dbl(fptr, "CD1_2", &ctemp2, comment, &status);
        fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status);
        fits_read_key_dbl(fptr, "CD2_2", &ctemp4, comment, &status);
        status = 0;
        if (mirror != 0)
        {
            if (angle == 0)
            {
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status);
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status);
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status);
            }
        }
    }

    /* Delete any polynomial solution */
    /* (These could maybe be switched, but I don't want to work them out yet */
    status = 0;
    if (!fits_read_key_dbl(fptr, "CO1_1", &ctemp1, comment, &status))
    {
        int i;
        char keyword[16];

        for (i = 1; i < 13; i++)
        {
            sprintf(keyword, "CO1_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
        for (i = 1; i < 13; i++)
        {
            sprintf(keyword, "CO2_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
    }

}

uint8_t * FITSData::getWritableImageBuffer()
{
    return m_ImageBuffer;
}

uint8_t const * FITSData::getImageBuffer() const
{
    return m_ImageBuffer;
}

void FITSData::setImageBuffer(uint8_t * buffer)
{
    delete[] m_ImageBuffer;
    m_ImageBuffer = buffer;
}

bool FITSData::checkDebayer()
{
    int status = 0;
    char bayerPattern[64], roworder[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    fits_read_keyword(fptr, "ROWORDER", roworder, nullptr, &status);

    if (stats.bitpix != 16 && stats.bitpix != 8)
    {
        KSNotification::error(i18n("Only 8 and 16 bits bayered images supported."), i18n("Debayer error"));
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

    QString order(roworder);
    order = order.remove('\'').trimmed();

    if (order == "BOTTOM-UP" && !(stats.height % 2))
    {
        if (pattern == "RGGB")
            pattern = "GBRG";
        else if (pattern == "GBRG")
            pattern = "RGGB";
        else if (pattern == "GRBG")
            pattern = "BGGR";
        else if (pattern == "BGGR")
            pattern = "GRBG";
        else return false;
    }

    if (pattern == "RGGB")
        debayerParams.filter = DC1394_COLOR_FILTER_RGGB;
    else if (pattern == "GBRG")
        debayerParams.filter = DC1394_COLOR_FILTER_GBRG;
    else if (pattern == "GRBG")
        debayerParams.filter = DC1394_COLOR_FILTER_GRBG;
    else if (pattern == "BGGR")
        debayerParams.filter = DC1394_COLOR_FILTER_BGGR;
    // We return unless we find a valid pattern
    else
    {
        KSNotification::error(i18n("Unsupported bayer pattern %1.", pattern), i18n("Debayer error"));
        return false;
    }

    fits_read_key(fptr, TINT, "XBAYROFF", &debayerParams.offsetX, nullptr, &status);
    fits_read_key(fptr, TINT, "YBAYROFF", &debayerParams.offsetY, nullptr, &status);

    if (debayerParams.offsetX == 1)
    {
        // This may leave odd values in the 0th column if the color filter is not there
        // in the sensor, but otherwise should process the offset correctly.
        // Only offsets of 0 or 1 are implemented in debayer_8bit() and debayer_16bit().
        switch (debayerParams.filter)
        {
            case DC1394_COLOR_FILTER_RGGB:
                debayerParams.filter = DC1394_COLOR_FILTER_GRBG;
                break;
            case DC1394_COLOR_FILTER_GBRG:
                debayerParams.filter = DC1394_COLOR_FILTER_BGGR;
                break;
            case DC1394_COLOR_FILTER_GRBG:
                debayerParams.filter = DC1394_COLOR_FILTER_RGGB;
                break;
            case DC1394_COLOR_FILTER_BGGR:
                debayerParams.filter = DC1394_COLOR_FILTER_GBRG;
                break;
        }
        debayerParams.offsetX = 0;
    }
    if (debayerParams.offsetX != 0 || debayerParams.offsetY > 1 || debayerParams.offsetY < 0)
    {
        KSNotification::error(i18n("Unsupported bayer offsets %1 %2.",
                                   debayerParams.offsetX, debayerParams.offsetY), i18n("Debayer error"));
        return false;
    }

    HasDebayer = true;

    return true;
}

void FITSData::getBayerParams(BayerParams * param)
{
    param->method  = debayerParams.method;
    param->filter  = debayerParams.filter;
    param->offsetX = debayerParams.offsetX;
    param->offsetY = debayerParams.offsetY;
}

void FITSData::setBayerParams(BayerParams * param)
{
    debayerParams.method  = param->method;
    debayerParams.filter  = param->filter;
    debayerParams.offsetX = param->offsetX;
    debayerParams.offsetY = param->offsetY;
}

bool FITSData::debayer()
{
    //    if (m_ImageBuffer == nullptr)
    //    {
    //        int anynull = 0, status = 0;

    //        //m_BayerBuffer = m_ImageBuffer;

    //        if (fits_read_img(fptr, m_DataType, 1, stats.samples_per_channel, nullptr, m_ImageBuffer, &anynull, &status))
    //        {
    //            char errmsg[512];
    //            fits_get_errstatus(status, errmsg);
    //            KSNotification::error(i18n("Error reading image: %1", QString(errmsg)), i18n("Debayer error"));
    //            return false;
    //        }
    //    }

    switch (m_DataType)
    {
        case TBYTE:
            return debayer_8bit();

        case TUSHORT:
            return debayer_16bit();

        default:
            return false;
    }
}

bool FITSData::debayer_8bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint8_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = stats.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height,
                                            debayerParams.filter,
                                            debayerParams.method);

    if (error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        m_Channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        m_ImageBuffer = new uint8_t[rgb_size];

        if (m_ImageBuffer == nullptr)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = bayered_buffer;
    uint8_t * gBuff = bayered_buffer + (stats.width * stats.height);
    uint8_t * bBuff = bayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    m_Channels = (m_Mode == FITS_NORMAL) ? 3 : 1;
    delete[] destinationBuffer;
    return true;
}

bool FITSData::debayer_16bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = stats.samples_per_channel * 3 * stats.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint16_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint16_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = stats.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height,
                 debayerParams.filter,
                 debayerParams.method, 16);

    if (error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        m_Channels = 1;
        delete[] destinationBuffer;
        return false;
    }

    if (m_ImageBufferSize != rgb_size)
    {
        delete[] m_ImageBuffer;
        m_ImageBuffer = new uint8_t[rgb_size];

        if (m_ImageBuffer == nullptr)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
            return false;
        }

        m_ImageBufferSize = rgb_size;
    }

    auto bayered_buffer = reinterpret_cast<uint16_t *>(m_ImageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint16_t * rBuff = bayered_buffer;
    uint16_t * gBuff = bayered_buffer + (stats.width * stats.height);
    uint16_t * bBuff = bayered_buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    m_Channels = (m_Mode == FITS_NORMAL) ? 3 : 1;
    delete[] destinationBuffer;
    return true;
}

double FITSData::getADU() const
{
    double adu = 0;
    for (int i = 0; i < m_Channels; i++)
        adu += stats.mean[i];

    return (adu / static_cast<double>(m_Channels));
}

QString FITSData::getLastError() const
{
    return lastError;
}

bool FITSData::getAutoRemoveTemporaryFITS() const
{
    return autoRemoveTemporaryFITS;
}

void FITSData::setAutoRemoveTemporaryFITS(bool value)
{
    autoRemoveTemporaryFITS = value;
}


template <typename T>
void FITSData::convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image)
{
    const auto * buffer = reinterpret_cast<const T *>(getImageBuffer());
    const T limit   = std::numeric_limits<T>::max();
    T bMin    = dataMin < 0 ? 0 : dataMin;
    T bMax    = dataMax > limit ? limit : dataMax;
    uint16_t w    = width();
    uint16_t h    = height();
    uint32_t size = w * h;
    double val;

    if (channels() == 1)
    {
        /* Fill in pixel values using indexed map, linear scale */
        for (int j = 0; j < h; j++)
        {
            unsigned char * scanLine = image.scanLine(j);

            for (int i = 0; i < w; i++)
            {
                val         = qBound(bMin, buffer[j * w + i], bMax);
                val         = val * scale + zero;
                scanLine[i] = qBound<unsigned char>(0, static_cast<uint8_t>(val), 255);
            }
        }
    }
    else
    {
        double rval = 0, gval = 0, bval = 0;
        QRgb value;
        /* Fill in pixel values using indexed map, linear scale */
        for (int j = 0; j < h; j++)
        {
            auto * scanLine = reinterpret_cast<QRgb *>((image.scanLine(j)));

            for (int i = 0; i < w; i++)
            {
                rval = qBound(bMin, buffer[j * w + i], bMax);
                gval = qBound(bMin, buffer[j * w + i + size], bMax);
                bval = qBound(bMin, buffer[j * w + i + size * 2], bMax);

                value = qRgb(rval * scale + zero, gval * scale + zero, bval * scale + zero);

                scanLine[i] = value;
            }
        }
    }
}

QImage FITSData::FITSToImage(const QString &filename)
{
    QImage fitsImage;
    double min, max;

    FITSData data;

    QFuture<bool> future = data.loadFITS(filename);

    // Wait synchronously
    future.waitForFinished();
    if (future.result() == false)
        return fitsImage;

    data.getMinMax(&min, &max);

    if (min == max)
    {
        fitsImage.fill(Qt::white);
        return fitsImage;
    }

    if (data.channels() == 1)
    {
        fitsImage = QImage(data.width(), data.height(), QImage::Format_Indexed8);

        fitsImage.setColorCount(256);
        for (int i = 0; i < 256; i++)
            fitsImage.setColor(i, qRgb(i, i, i));
    }
    else
    {
        fitsImage = QImage(data.width(), data.height(), QImage::Format_RGB32);
    }

    double dataMin = data.stats.mean[0] - data.stats.stddev[0];
    double dataMax = data.stats.mean[0] + data.stats.stddev[0] * 3;

    double bscale = 255. / (dataMax - dataMin);
    double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

    // Long way to do this since we do not want to use templated functions here
    switch (data.property("dataType").toInt())
    {
        case TBYTE:
            data.convertToQImage<uint8_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TSHORT:
            data.convertToQImage<int16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TUSHORT:
            data.convertToQImage<uint16_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TLONG:
            data.convertToQImage<int32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TULONG:
            data.convertToQImage<uint32_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TFLOAT:
            data.convertToQImage<float>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TLONGLONG:
            data.convertToQImage<int64_t>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        case TDOUBLE:
            data.convertToQImage<double>(dataMin, dataMax, bscale, bzero, fitsImage);
            break;

        default:
            break;
    }

    return fitsImage;
}

bool FITSData::ImageToFITS(const QString &filename, const QString &format, QString &output)
{
    if (QImageReader::supportedImageFormats().contains(format.toLatin1()) == false)
    {
        qCCritical(KSTARS_FITS) << "Failed to convert" << filename << "to FITS since format" << format << "is not supported in Qt";
        return false;
    }

    QImage input;

    if (input.load(filename, format.toLatin1()) == false)
    {
        qCCritical(KSTARS_FITS) << "Failed to open image" << filename;
        return false;
    }

    output = QString(KSPaths::writableLocation(QStandardPaths::TempLocation) + QFileInfo(filename).fileName() + ".fits");

    //This section sets up the FITS File
    fitsfile *fptr = nullptr;
    int status = 0;
    long  fpixel = 1, naxis = input.allGray() ? 2 : 3, nelements, exposure;
    long naxes[3] = { input.width(), input.height(), naxis == 3 ? 3 : 1 };
    char error_status[512] = {0};

    if (fits_create_file(&fptr, QString('!' + output).toLatin1().data(), &status))
    {
        fits_get_errstatus(status, error_status);
        qCCritical(KSTARS_FITS) << "Failed to create FITS file. Error:" << error_status;
        return false;
    }

    if (fits_create_img(fptr, BYTE_IMG, naxis, naxes, &status))
    {
        qCWarning(KSTARS_FITS) << "fits_create_img failed:" << error_status;
        status = 0;
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        return false;
    }

    exposure = 1;
    fits_update_key(fptr, TLONG, "EXPOSURE", &exposure, "Total Exposure Time", &status);

    // Gray image
    if (naxis == 2)
    {
        nelements = naxes[0] * naxes[1];
        if (fits_write_img(fptr, TBYTE, fpixel, nelements, input.bits(), &status))
        {
            fits_get_errstatus(status, error_status);
            qCWarning(KSTARS_FITS) << "fits_write_img GRAY failed:" << error_status;
            status = 0;
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            return false;
        }
    }
    // RGB image, we have to convert from ARGB format to R G B for each plane
    else
    {
        nelements = naxes[0] * naxes[1] * 3;

        uint8_t *srcBuffer = input.bits();
        // ARGB
        uint32_t srcBytes = naxes[0] * naxes[1] * 4 - 4;

        uint8_t *rgbBuffer = new uint8_t[nelements];
        if (rgbBuffer == nullptr)
        {
            qCWarning(KSTARS_FITS) << "Not enough memory for RGB buffer";
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            return false;
        }

        uint8_t *subR = rgbBuffer;
        uint8_t *subG = rgbBuffer + naxes[0] * naxes[1];
        uint8_t *subB = rgbBuffer + naxes[0] * naxes[1] * 2;
        for (uint32_t i = 0; i < srcBytes; i += 4)
        {
            *subB++ = srcBuffer[i];
            *subG++ = srcBuffer[i + 1];
            *subR++ = srcBuffer[i + 2];
        }

        if (fits_write_img(fptr, TBYTE, fpixel, nelements, rgbBuffer, &status))
        {
            fits_get_errstatus(status, error_status);
            qCWarning(KSTARS_FITS) << "fits_write_img RGB failed:" << error_status;
            status = 0;
            fits_flush_file(fptr, &status);
            fits_close_file(fptr, &status);
            delete [] rgbBuffer;
            return false;
        }

        delete [] rgbBuffer;
    }

    if (fits_flush_file(fptr, &status))
    {
        fits_get_errstatus(status, error_status);
        qCWarning(KSTARS_FITS) << "fits_flush_file failed:" << error_status;
        status = 0;
        fits_close_file(fptr, &status);
        return false;
    }

    if (fits_close_file(fptr, &status))
    {
        fits_get_errstatus(status, error_status);
        qCWarning(KSTARS_FITS) << "fits_close_file failed:" << error_status;
        return false;
    }

    return true;
}

#if 0
bool FITSData::injectWCS(const QString &newWCSFile, double orientation, double ra, double dec, double pixscale)
{
    int status = 0, exttype = 0;
    long nelements;
    fitsfile * new_fptr;
    char errMsg[512];

    qCInfo(KSTARS_FITS) << "Creating new WCS file:" << newWCSFile << "with parameters Orientation:" << orientation
                        << "RA:" << ra << "DE:" << dec << "Pixel Scale:" << pixscale;

    nelements = stats.samples_per_channel * m_Channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, QString('!' + newWCSFile).toLatin1(), &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    if (fits_copy_file(fptr, new_fptr, 1, 1, 1, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    status = 0;

    if (m_isTemporary && autoRemoveTemporaryFITS)
    {
        QFile::remove(m_Filename);
        m_isTemporary = false;
        qCDebug(KSTARS_FITS) << "Removing FITS File: " << m_Filename;
    }

    m_Filename = newWCSFile;
    m_isTemporary = true;

    fptr = new_fptr;

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, m_DataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(stats.min), "Minimum value", &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(stats.max), "Maximum value", &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(stats.width), "length of data axis 1", &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(stats.height), "length of data axis 2", &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    fits_update_key(fptr, TDOUBLE, "OBJCTRA", &ra, "Object RA", &status);
    fits_update_key(fptr, TDOUBLE, "OBJCTDEC", &dec, "Object DEC", &status);

    int epoch = 2000;

    fits_update_key(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);

    fits_update_key(fptr, TDOUBLE, "CRVAL1", &ra, "CRVAL1", &status);
    fits_update_key(fptr, TDOUBLE, "CRVAL2", &dec, "CRVAL1", &status);

    char radecsys[8] = "FK5";
    char ctype1[16]  = "RA---TAN";
    char ctype2[16]  = "DEC--TAN";

    fits_update_key(fptr, TSTRING, "RADECSYS", radecsys, "RADECSYS", &status);
    fits_update_key(fptr, TSTRING, "CTYPE1", ctype1, "CTYPE1", &status);
    fits_update_key(fptr, TSTRING, "CTYPE2", ctype2, "CTYPE2", &status);

    double crpix1 = width() / 2.0;
    double crpix2 = height() / 2.0;

    fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "CRPIX1", &status);
    fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "CRPIX2", &status);

    // Arcsecs per Pixel
    double secpix1 = pixscale;
    double secpix2 = pixscale;

    fits_update_key(fptr, TDOUBLE, "SECPIX1", &secpix1, "SECPIX1", &status);
    fits_update_key(fptr, TDOUBLE, "SECPIX2", &secpix2, "SECPIX2", &status);

    double degpix1 = secpix1 / 3600.0;
    double degpix2 = secpix2 / 3600.0;

    fits_update_key(fptr, TDOUBLE, "CDELT1", &degpix1, "CDELT1", &status);
    fits_update_key(fptr, TDOUBLE, "CDELT2", &degpix2, "CDELT2", &status);

    // Rotation is CW, we need to convert it to CCW per CROTA1 definition
    double rotation = 360 - orientation;
    if (rotation > 360)
        rotation -= 360;

    fits_update_key(fptr, TDOUBLE, "CROTA1", &rotation, "CROTA1", &status);
    fits_update_key(fptr, TDOUBLE, "CROTA2", &rotation, "CROTA2", &status);

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    QString history =
        QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        fits_get_errstatus(status, errMsg);
        lastError = QString(errMsg);
        fits_report_error(stderr, status);
        return false;
    }

    fits_flush_file(fptr, &status);

    WCSLoaded = false;

    qCDebug(KSTARS_FITS) << "Finished creating WCS file: " << newWCSFile;

    return true;
}
#endif

bool FITSData::injectWCS(double orientation, double ra, double dec, double pixscale)
{
    int status = 0;

    fits_update_key(fptr, TDOUBLE, "OBJCTRA", &ra, "Object RA", &status);
    fits_update_key(fptr, TDOUBLE, "OBJCTDEC", &dec, "Object DEC", &status);

    int epoch = 2000;

    fits_update_key(fptr, TINT, "EQUINOX", &epoch, "Equinox", &status);

    fits_update_key(fptr, TDOUBLE, "CRVAL1", &ra, "CRVAL1", &status);
    fits_update_key(fptr, TDOUBLE, "CRVAL2", &dec, "CRVAL1", &status);

    char radecsys[8] = "FK5";
    char ctype1[16]  = "RA---TAN";
    char ctype2[16]  = "DEC--TAN";

    fits_update_key(fptr, TSTRING, "RADECSYS", radecsys, "RADECSYS", &status);
    fits_update_key(fptr, TSTRING, "CTYPE1", ctype1, "CTYPE1", &status);
    fits_update_key(fptr, TSTRING, "CTYPE2", ctype2, "CTYPE2", &status);

    double crpix1 = width() / 2.0;
    double crpix2 = height() / 2.0;

    fits_update_key(fptr, TDOUBLE, "CRPIX1", &crpix1, "CRPIX1", &status);
    fits_update_key(fptr, TDOUBLE, "CRPIX2", &crpix2, "CRPIX2", &status);

    // Arcsecs per Pixel
    double secpix1 = pixscale;
    double secpix2 = pixscale;

    fits_update_key(fptr, TDOUBLE, "SECPIX1", &secpix1, "SECPIX1", &status);
    fits_update_key(fptr, TDOUBLE, "SECPIX2", &secpix2, "SECPIX2", &status);

    double degpix1 = secpix1 / 3600.0;
    double degpix2 = secpix2 / 3600.0;

    fits_update_key(fptr, TDOUBLE, "CDELT1", &degpix1, "CDELT1", &status);
    fits_update_key(fptr, TDOUBLE, "CDELT2", &degpix2, "CDELT2", &status);

    // Rotation is CW, we need to convert it to CCW per CROTA1 definition
    double rotation = 360 - orientation;
    if (rotation > 360)
        rotation -= 360;

    fits_update_key(fptr, TDOUBLE, "CROTA1", &rotation, "CROTA1", &status);
    fits_update_key(fptr, TDOUBLE, "CROTA2", &rotation, "CROTA2", &status);

    WCSLoaded = false;

    qCDebug(KSTARS_FITS) << "Finished update WCS info.";

    return true;
}

bool FITSData::contains(const QPointF &point) const
{
    return (point.x() >= 0 && point.y() >= 0 && point.x() <= stats.width && point.y() <= stats.height);
}

void FITSData::saveStatistics(Statistic &other)
{
    other = stats;
}

void FITSData::restoreStatistics(Statistic &other)
{
    stats = other;
}
