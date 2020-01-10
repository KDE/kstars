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

#include "sep/sep.h"
#include "fpack.h"

#include "kstarsdata.h"
#include "ksutils.h"
#include "kspaths.h"
#include "Options.h"
#include "skymapcomposite.h"
#include "auxiliary/ksnotification.h"

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

const int MINIMUM_ROWS_PER_CENTER = 3;
const QString FITSData::m_TemporaryPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);

#define DIFFUSE_THRESHOLD 0.15

#define MAX_EDGE_LIMIT     10000
#define LOW_EDGE_CUTOFF_1  50
#define LOW_EDGE_CUTOFF_2  10
#define MINIMUM_EDGE_LIMIT 2

bool greaterThan(Edge * s1, Edge * s2)
{
    //return s1->width > s2->width;
    return s1->sum > s2->sum;
}

FITSData::FITSData(FITSMode fitsMode): m_Mode(fitsMode)
{
    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;
}

FITSData::FITSData(const FITSData * other)
{
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
    qCInfo(KSTARS_FITS) << "Reading FITS file buffer ";
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

        QString uncompressedFile = QDir::tempPath() + QString("/%1").arg(QUuid::createUuid().toString().remove(QRegularExpression("[-{}]")));
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

int FITSData::saveFITS(const QString &newFilename)
{
    if (newFilename == m_Filename)
        return 0;

    if (m_isCompressed)
    {
        KSNotification::error(i18n("Saving compressed files is not supported."));
        return -1;
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
            return -1;
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

        return 0;
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
        return status;
    }

    fits_flush_file(fptr, &status);
    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    status = 0;

    fptr = new_fptr;

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Write Data */
    if (fits_write_img(fptr, m_DataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(stats.min), "Minimum value", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(stats.max), "Maximum value", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // NAXIS1
    if (fits_update_key(fptr, TUSHORT, "NAXIS1", &(stats.width), "length of data axis 1", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // NAXIS2
    if (fits_update_key(fptr, TUSHORT, "NAXIS2", &(stats.height), "length of data axis 2", &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    QString history =
        QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
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

    return status;
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

    if (refresh && markStars)
        // Let's try to find star positions again after transformation
        starsSearched = false;
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
            futures.append(QtConcurrent::run(this, &FITSData::getSquaredSumAndMean<T>, tStart, (i == (nThreads - 1)) ? fStride : tStride));
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

bool FITSData::checkCollision(Edge * s1, Edge * s2)
{
    int dis; //distance

    int diff_x = s1->x - s2->x;
    int diff_y = s1->y - s2->y;

    dis = std::abs(sqrt(diff_x * diff_x + diff_y * diff_y));
    dis -= s1->width / 2;
    dis -= s2->width / 2;

    if (dis <= 0) //collision
        return true;

    //no collision
    return false;
}

int FITSData::findCannyStar(FITSData * data, const QRect &boundary)
{
    switch (data->property("dataType").toInt())
    {
        case TBYTE:
            return FITSData::findCannyStar<uint8_t>(data, boundary);

        case TSHORT:
            return FITSData::findCannyStar<int16_t>(data, boundary);

        case TUSHORT:
            return FITSData::findCannyStar<uint16_t>(data, boundary);

        case TLONG:
            return FITSData::findCannyStar<int32_t>(data, boundary);

        case TULONG:
            return FITSData::findCannyStar<uint16_t>(data, boundary);

        case TFLOAT:
            return FITSData::findCannyStar<float>(data, boundary);

        case TLONGLONG:
            return FITSData::findCannyStar<int64_t>(data, boundary);

        case TDOUBLE:
            return FITSData::findCannyStar<double>(data, boundary);

        default:
            break;
    }

    return 0;
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
            count = findSEPStars(trackingBox);
            break;

        case ALGORITHM_GRADIENT:
            count = findCannyStar(this, trackingBox);
            break;

        case ALGORITHM_CENTROID:
            count = findCentroid(trackingBox);
            break;

        case ALGORITHM_THRESHOLD:
            count = findOneStar(trackingBox);
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

template <typename T>
int FITSData::findCannyStar(FITSData * data, const QRect &boundary)
{
    int subX = qMax(0, boundary.isNull() ? 0 : boundary.x());
    int subY = qMax(0, boundary.isNull() ? 0 : boundary.y());
    int subW = (boundary.isNull() ? data->width() : boundary.width());
    int subH = (boundary.isNull() ? data->height() : boundary.height());

    int BBP = data->getBytesPerPixel();

    uint16_t dataWidth = data->width();

    // #1 Find offsets
    uint32_t size   = subW * subH;
    uint32_t offset = subX + subY * dataWidth;

    // #2 Create new buffer
    auto * buffer = new uint8_t[size * BBP];
    // If there is no offset, copy whole buffer in one go
    if (offset == 0)
        memcpy(buffer, data->getImageBuffer(), size * BBP);
    else
    {
        uint8_t * dataPtr     = buffer;
        uint8_t * origDataPtr = data->getImageBuffer();
        uint32_t lineOffset  = 0;
        // Copy data line by line
        for (int height = subY; height < (subY + subH); height++)
        {
            lineOffset = (subX + height * dataWidth) * BBP;
            memcpy(dataPtr, origDataPtr + lineOffset, subW * BBP);
            dataPtr += (subW * BBP);
        }
    }

    // #3 Create new FITSData to hold it
    auto * boundedImage                      = new FITSData();
    boundedImage->stats.width               = subW;
    boundedImage->stats.height              = subH;
    boundedImage->stats.bitpix              = data->stats.bitpix;
    boundedImage->stats.bytesPerPixel       = data->stats.bytesPerPixel;
    boundedImage->stats.samples_per_channel = size;
    boundedImage->stats.ndim                = 2;

    boundedImage->setProperty("dataType", data->property("dataType"));

    // #4 Set image buffer and calculate stats.
    boundedImage->setImageBuffer(buffer);

    boundedImage->calculateStats(true);

    // #5 Apply Median + High Contrast filter to remove noise and move data to non-linear domain
    boundedImage->applyFilter(FITS_MEDIAN);
    boundedImage->applyFilter(FITS_HIGH_CONTRAST);

    // #6 Perform Sobel to find gradients and their directions
    QVector<float> gradients;
    QVector<float> directions;

    // TODO Must trace neighbours and assign IDs to each shape so that they can be centered massed
    // and discarded whenever necessary. It won't work on noisy images unless this is done.
    boundedImage->sobel<T>(gradients, directions);

    QVector<int> ids(gradients.size());

    int maxID = boundedImage->partition(subW, subH, gradients, ids);

    // Not needed anymore
    delete boundedImage;

    if (maxID == 0)
        return 0;

    typedef struct
    {
        float massX     = 0;
        float massY     = 0;
        float totalMass = 0;
    } massInfo;

    QMap<int, massInfo> masses;

    // #7 Calculate center of mass for all detected regions
    for (int y = 0; y < subH; y++)
    {
        for (int x = 0; x < subW; x++)
        {
            int index = x + y * subW;

            int regionID = ids[index];
            if (regionID > 0)
            {
                float pixel = gradients[index];

                masses[regionID].totalMass += pixel;
                masses[regionID].massX += x * pixel;
                masses[regionID].massY += y * pixel;
            }
        }
    }

    // Compare multiple masses, and only select the highest total mass one as the desired star
    int maxRegionID       = 1;
    int maxTotalMass      = masses[1].totalMass;
    double totalMassRatio = 1e6;
    for (auto key : masses.keys())
    {
        massInfo oneMass = masses.value(key);
        if (oneMass.totalMass > maxTotalMass)
        {
            totalMassRatio = oneMass.totalMass / maxTotalMass;
            maxTotalMass   = oneMass.totalMass;
            maxRegionID    = key;
        }
    }

    // If image has many regions and there is no significant relative center of mass then it's just noise and no stars
    // are probably there above a useful threshold.
    if (maxID > 10 && totalMassRatio < 1.5)
        return 0;

    auto * center  = new Edge;
    center->width = -1;
    center->x     = masses[maxRegionID].massX / masses[maxRegionID].totalMass + 0.5;
    center->y     = masses[maxRegionID].massY / masses[maxRegionID].totalMass + 0.5;
    center->HFR   = 1;

    // Maximum Radius
    int maxR = qMin(subW - 1, subH - 1) / 2;

    for (int r = maxR; r > 1; r--)
    {
        int pass = 0;

        for (float theta = 0; theta < 2 * M_PI; theta += (2 * M_PI) / 36.0)
        {
            int testX = center->x + std::cos(theta) * r;
            int testY = center->y + std::sin(theta) * r;

            // if out of bound, break;
            if (testX < 0 || testX >= subW || testY < 0 || testY >= subH)
                break;

            if (gradients[testX + testY * subW] > 0)
                //if (thresholded[testX + testY * subW] > 0)
            {
                if (++pass >= 24)
                {
                    center->width = r * 2;
                    // Break of outer loop
                    r = 0;
                    break;
                }
            }
        }
    }

    qCDebug(KSTARS_FITS) << "FITS: Weighted Center is X: " << center->x << " Y: " << center->y << " Width: " << center->width;

    // If no stars were detected
    if (center->width == -1)
    {
        delete center;
        return 0;
    }

    // 30% fuzzy
    //center->width += center->width*0.3 * (running_threshold / threshold);

    double FSum = 0, HF = 0, TF = 0;
    const double resolution = 1.0 / 20.0;

    int cen_y = qRound(center->y);

    double rightEdge = center->x + center->width / 2.0;
    double leftEdge  = center->x - center->width / 2.0;

    QVector<double> subPixels;
    subPixels.reserve(center->width / resolution);

    const T * origBuffer = reinterpret_cast<T *>(data->getImageBuffer()) + offset;

    for (double x = leftEdge; x <= rightEdge; x += resolution)
    {
        double slice = resolution * (origBuffer[static_cast<int>(floor(x)) + cen_y * dataWidth]);
        FSum += slice;
        subPixels.append(slice);
    }

    // Half flux
    HF = FSum / 2.0;

    int subPixelCenter = (center->width / resolution) / 2;

    // Start from center
    TF            = subPixels[subPixelCenter];
    double lastTF = TF;
    // Integrate flux along radius axis until we reach half flux
    //for (double k=resolution; k < (center->width/(2*resolution)); k += resolution)
    for (int k = 1; k < subPixelCenter; k++)
    {
        TF += subPixels[subPixelCenter + k];
        TF += subPixels[subPixelCenter - k];

        if (TF >= HF)
        {
            // We overpassed HF, let's calculate from last TF how much until we reach HF

            // #1 Accurate calculation, but very sensitive to small variations of flux
            center->HFR = (k - 1 + ((HF - lastTF) / (TF - lastTF)) * 2) * resolution;

            // #2 Less accurate calculation, but stable against small variations of flux
            //center->HFR = (k - 1) * resolution;
            break;
        }

        lastTF = TF;
    }

    // Correct center for subX and subY
    center->x += subX;
    center->y += subY;

    data->appendStar(center);

    qCDebug(KSTARS_FITS) << "Flux: " << FSum << " Half-Flux: " << HF << " HFR: " << center->HFR;

    return 1;
}

int FITSData::findOneStar(const QRect &boundary)
{
    switch (m_DataType)
    {
        case TBYTE:
            return findOneStar<uint8_t>(boundary);

        case TSHORT:
            return findOneStar<int16_t>(boundary);

        case TUSHORT:
            return findOneStar<uint16_t>(boundary);

        case TLONG:
            return findOneStar<int32_t>(boundary);

        case TULONG:
            return findOneStar<uint32_t>(boundary);

        case TFLOAT:
            return findOneStar<float>(boundary);

        case TLONGLONG:
            return findOneStar<int64_t>(boundary);

        case TDOUBLE:
            return findOneStar<double>(boundary);

        default:
            break;
    }

    return 0;
}

template <typename T>
int FITSData::findOneStar(const QRect &boundary)
{
    if (boundary.isEmpty())
        return -1;

    int subX = boundary.x();
    int subY = boundary.y();
    int subW = subX + boundary.width();
    int subH = subY + boundary.height();

    float massX = 0, massY = 0, totalMass = 0;

    auto * buffer = reinterpret_cast<T *>(m_ImageBuffer);

    // TODO replace magic number with something more useful to understand
    double threshold = stats.mean[0] * Options::focusThreshold() / 100.0;

    for (int y = subY; y < subH; y++)
    {
        for (int x = subX; x < subW; x++)
        {
            T pixel = buffer[x + y * stats.width];
            if (pixel > threshold)
            {
                totalMass += pixel;
                massX += x * pixel;
                massY += y * pixel;
            }
        }
    }

    qCDebug(KSTARS_FITS) << "FITS: Weighted Center is X: " << massX / totalMass << " Y: " << massY / totalMass;

    auto * center  = new Edge;
    center->width = -1;
    center->x     = massX / totalMass + 0.5;
    center->y     = massY / totalMass + 0.5;
    center->HFR   = 1;

    // Maximum Radius
    int maxR = qMin(subW - 1, subH - 1) / 2;

    // Critical threshold
    double critical_threshold = threshold * 0.7;
    double running_threshold  = threshold;

    while (running_threshold >= critical_threshold)
    {
        for (int r = maxR; r > 1; r--)
        {
            int pass = 0;

            for (float theta = 0; theta < 2 * M_PI; theta += (2 * M_PI) / 10.0)
            {
                int testX = center->x + std::cos(theta) * r;
                int testY = center->y + std::sin(theta) * r;

                // if out of bound, break;
                if (testX < subX || testX > subW || testY < subY || testY > subH)
                    break;

                if (buffer[testX + testY * stats.width] > running_threshold)
                    pass++;
            }

            //qDebug() << "Testing for radius " << r << " passes # " << pass << " @ threshold " << running_threshold;
            //if (pass >= 6)
            if (pass >= 5)
            {
                center->width = r * 2;
                break;
            }
        }

        if (center->width > 0)
            break;

        // Increase threshold fuzziness by 10%
        running_threshold -= running_threshold * 0.1;
    }

    // If no stars were detected
    if (center->width == -1)
    {
        delete center;
        return 0;
    }

    // 30% fuzzy
    //center->width += center->width*0.3 * (running_threshold / threshold);

    starCenters.append(center);

    double FSum = 0, HF = 0, TF = 0, min = stats.min[0];
    const double resolution = 1.0 / 20.0;

    int cen_y = qRound(center->y);

    double rightEdge = center->x + center->width / 2.0;
    double leftEdge  = center->x - center->width / 2.0;

    QVector<double> subPixels;
    subPixels.reserve(center->width / resolution);

    for (double x = leftEdge; x <= rightEdge; x += resolution)
    {
        //subPixels[x] = resolution * (image_buffer[static_cast<int>(floor(x)) + cen_y * stats.width] - min);
        double slice = resolution * (buffer[static_cast<int>(floor(x)) + cen_y * stats.width] - min);
        FSum += slice;
        subPixels.append(slice);
    }

    // Half flux
    HF = FSum / 2.0;

    //double subPixelCenter = center->x - fmod(center->x,resolution);
    int subPixelCenter = (center->width / resolution) / 2;

    // Start from center
    TF            = subPixels[subPixelCenter];
    double lastTF = TF;
    // Integrate flux along radius axis until we reach half flux
    //for (double k=resolution; k < (center->width/(2*resolution)); k += resolution)
    for (int k = 1; k < subPixelCenter; k++)
    {
        TF += subPixels[subPixelCenter + k];
        TF += subPixels[subPixelCenter - k];

        if (TF >= HF)
        {
            // We have two ways to calculate HFR. The first is the correct method but it can get quite variable within 10% due to random fluctuations of the measured star.
            // The second method is not truly HFR but is much more resistant to noise.

            // #1 Approximate HFR, accurate and reliable but quite variable to small changes in star flux
            center->HFR = (k - 1 + ((HF - lastTF) / (TF - lastTF)) * 2) * resolution;

            // #2 Not exactly HFR, but much more stable
            //center->HFR = (k*resolution) * (HF/TF);
            break;
        }

        lastTF = TF;
    }

    return 1;
}

/*** Find center of stars and calculate Half Flux Radius */
int FITSData::findCentroid(const QRect &boundary, int initStdDev, int minEdgeWidth)
{
    switch (m_DataType)
    {
        case TBYTE:
            return findCentroid<uint8_t>(boundary, initStdDev, minEdgeWidth);

        case TSHORT:
            return findCentroid<int16_t>(boundary, initStdDev, minEdgeWidth);

        case TUSHORT:
            return findCentroid<uint16_t>(boundary, initStdDev, minEdgeWidth);

        case TLONG:
            return findCentroid<int32_t>(boundary, initStdDev, minEdgeWidth);

        case TULONG:
            return findCentroid<uint32_t>(boundary, initStdDev, minEdgeWidth);

        case TFLOAT:
            return findCentroid<float>(boundary, initStdDev, minEdgeWidth);

        case TLONGLONG:
            return findCentroid<int64_t>(boundary, initStdDev, minEdgeWidth);

        case TDOUBLE:
            return findCentroid<double>(boundary, initStdDev, minEdgeWidth);

        default:
            return -1;
    }
}

template <typename T>
int FITSData::findCentroid(const QRect &boundary, int initStdDev, int minEdgeWidth)
{
    double threshold = 0, sum = 0, avg = 0, min = 0;
    int starDiameter     = 0;
    int pixVal           = 0;
    int minimumEdgeCount = MINIMUM_EDGE_LIMIT;

    auto * buffer = reinterpret_cast<T *>(m_ImageBuffer);

    double JMIndex = 100;
#ifndef KSTARS_LITE
    if (histogram)
    {
        if (!histogram->isConstructed())
            histogram->constructHistogram();
        JMIndex = histogram->getJMIndex();
    }
#endif

    float dispersion_ratio = 1.5;

    QList<Edge *> edges;

    if (JMIndex < DIFFUSE_THRESHOLD)
    {
        minEdgeWidth     = JMIndex * 35 + 1;
        minimumEdgeCount = minEdgeWidth - 1;
    }
    else
    {
        minEdgeWidth     = 6;
        minimumEdgeCount = 4;
    }

    while (initStdDev >= 1)
    {
        minEdgeWidth--;
        minimumEdgeCount--;

        minEdgeWidth     = qMax(3, minEdgeWidth);
        minimumEdgeCount = qMax(3, minimumEdgeCount);

        if (JMIndex < DIFFUSE_THRESHOLD)
        {
            // Taking the average out seems to have better result for noisy images
            threshold = stats.max[0] - stats.mean[0] * ((MINIMUM_STDVAR - initStdDev) * 0.5 + 1);

            min = stats.min[0];
            if (threshold - min < 0)
            {
                threshold = stats.mean[0] * ((MINIMUM_STDVAR - initStdDev) * 0.5 + 1);
                min       = 0;
            }

            dispersion_ratio = 1.4 - (MINIMUM_STDVAR - initStdDev) * 0.08;
        }
        else
        {
            threshold = stats.mean[0] + stats.stddev[0] * initStdDev * (0.3 - (MINIMUM_STDVAR - initStdDev) * 0.05);
            min       = stats.min[0];
            // Ratio between centeroid center and edge
            dispersion_ratio = 1.8 - (MINIMUM_STDVAR - initStdDev) * 0.2;
        }

        qCDebug(KSTARS_FITS) << "SNR: " << stats.SNR;
        qCDebug(KSTARS_FITS) << "The threshold level is " << threshold << "(actual " << threshold - min
                             << ")  minimum edge width" << minEdgeWidth << " minimum edge limit " << minimumEdgeCount;

        threshold -= min;

        int subX, subY, subW, subH;

        if (boundary.isNull())
        {
            if (m_Mode == FITS_GUIDE || m_Mode == FITS_FOCUS)
            {
                // Only consider the central 70%
                subX = round(stats.width * 0.15);
                subY = round(stats.height * 0.15);
                subW = stats.width - subX;
                subH = stats.height - subY;
            }
            else
            {
                // Consider the complete area 100%
                subX = 0;
                subY = 0;
                subW = stats.width;
                subH = stats.height;
            }
        }
        else
        {
            subX = boundary.x();
            subY = boundary.y();
            subW = subX + boundary.width();
            subH = subY + boundary.height();
        }

        // Detect "edges" that are above threshold
        for (int i = subY; i < subH; i++)
        {
            starDiameter = 0;

            for (int j = subX; j < subW; j++)
            {
                pixVal = buffer[j + (i * stats.width)] - min;

                // If pixel value > threshold, let's get its weighted average
                if (pixVal >= threshold)
                {
                    avg += j * pixVal;
                    sum += pixVal;
                    starDiameter++;
                }
                // Value < threshold but avg exists
                else if (sum > 0)
                {
                    // We found a potential centroid edge
                    if (starDiameter >= minEdgeWidth)
                    {
                        float center = avg / sum + 0.5;
                        if (center > 0)
                        {
                            int i_center = std::floor(center);

                            // Check if center is 10% or more brighter than edge, if not skip
                            if (((buffer[i_center + (i * stats.width)] - min) /
                                    (buffer[i_center + (i * stats.width) - starDiameter / 2] - min) >=
                                    dispersion_ratio) &&
                                    ((buffer[i_center + (i * stats.width)] - min) /
                                     (buffer[i_center + (i * stats.width) + starDiameter / 2] - min) >=
                                     dispersion_ratio))
                            {
                                qCDebug(KSTARS_FITS)
                                        << "Edge center is " << buffer[i_center + (i * stats.width)] - min
                                        << " Edge is " << buffer[i_center + (i * stats.width) - starDiameter / 2] - min
                                        << " and ratio is "
                                        << ((buffer[i_center + (i * stats.width)] - min) /
                                            (buffer[i_center + (i * stats.width) - starDiameter / 2] - min))
                                        << " located at X: " << center << " Y: " << i + 0.5;

                                auto * newEdge = new Edge();

                                newEdge->x       = center;
                                newEdge->y       = i + 0.5;
                                newEdge->scanned = 0;
                                newEdge->val     = buffer[i_center + (i * stats.width)] - min;
                                newEdge->width   = starDiameter;
                                newEdge->HFR     = 0;
                                newEdge->sum     = sum;

                                edges.append(newEdge);
                            }
                        }
                    }

                    // Reset
                    avg = sum = starDiameter = 0;
                }
            }
        }

        qCDebug(KSTARS_FITS) << "Total number of edges found is: " << edges.count();

        // In case of hot pixels
        if (edges.count() == 1 && initStdDev > 1)
        {
            initStdDev--;
            continue;
        }

        if (edges.count() >= MAX_EDGE_LIMIT)
        {
            qCWarning(KSTARS_FITS) << "Too many edges, aborting... " << edges.count();
            qDeleteAll(edges);
            return -1;
        }

        if (edges.count() >= minimumEdgeCount)
            break;

        qDeleteAll(edges);
        edges.clear();
        initStdDev--;
    }

    int cen_count = 0;
    int cen_x     = 0;
    int cen_y     = 0;
    int cen_v     = 0;
    int cen_w     = 0;
    int width_sum = 0;

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), greaterThan);

    // Now, let's scan the edges and find the maximum centroid vertically
    for (int i = 0; i < edges.count(); i++)
    {
        qCDebug(KSTARS_FITS) << "# " << i << " Edge at (" << edges[i]->x << "," << edges[i]->y << ") With a value of "
                             << edges[i]->val << " and width of " << edges[i]->width << " pixels. with sum " << edges[i]->sum;

        // If edge scanned already, skip
        if (edges[i]->scanned == 1)
        {
            qCDebug(KSTARS_FITS) << "Skipping check for center " << i << " because it was already counted";
            continue;
        }

        qCDebug(KSTARS_FITS) << "Investigating edge # " << i << " now ...";

        // Get X, Y, and Val of edge
        cen_x = edges[i]->x;
        cen_y = edges[i]->y;
        cen_v = edges[i]->sum;
        cen_w = edges[i]->width;

        float avg_x = 0;
        float avg_y = 0;

        sum       = 0;
        cen_count = 0;

        // Now let's compare to other edges until we hit a maxima
        for (int j = 0; j < edges.count(); j++)
        {
            if (edges[j]->scanned)
                continue;

            if (checkCollision(edges[j], edges[i]))
            {
                if (edges[j]->sum >= cen_v)
                {
                    cen_v = edges[j]->sum;
                    cen_w = edges[j]->width;
                }

                edges[j]->scanned = 1;
                cen_count++;

                avg_x += edges[j]->x * edges[j]->val;
                avg_y += edges[j]->y * edges[j]->val;
                sum += edges[j]->val;

                continue;
            }
        }

        int cen_limit = (MINIMUM_ROWS_PER_CENTER - (MINIMUM_STDVAR - initStdDev));

        if (edges.count() < LOW_EDGE_CUTOFF_1)
        {
            if (edges.count() < LOW_EDGE_CUTOFF_2)
                cen_limit = 1;
            else
                cen_limit = 2;
        }

        qCDebug(KSTARS_FITS) << "center_count: " << cen_count << " and initstdDev= " << initStdDev << " and limit is "
                             << cen_limit;

        if (cen_limit < 1)
            continue;

        // If centroid count is within acceptable range
        //if (cen_limit >= 2 && cen_count >= cen_limit)
        if (cen_count >= cen_limit)
        {
            // We detected a centroid, let's init it
            auto * rCenter = new Edge();

            rCenter->x = avg_x / sum;
            rCenter->y = avg_y / sum;
            width_sum += rCenter->width;
            rCenter->width = cen_w;

            qCDebug(KSTARS_FITS) << "Found a real center with number with (" << rCenter->x << "," << rCenter->y << ")";

            // Calculate Total Flux From Center, Half Flux, Full Summation
            double TF   = 0;
            double HF   = 0;
            double FSum = 0;

            cen_x = (int)std::floor(rCenter->x);
            cen_y = (int)std::floor(rCenter->y);

            if (cen_x < 0 || cen_x > stats.width || cen_y < 0 || cen_y > stats.height)
            {
                delete rCenter;
                continue;
            }

            // Complete sum along the radius
            //for (int k=0; k < rCenter->width; k++)
            for (int k = rCenter->width / 2; k >= -(rCenter->width / 2); k--)
            {
                FSum += buffer[cen_x - k + (cen_y * stats.width)] - min;
                //qDebug() << image_buffer[cen_x-k+(cen_y*stats.width)] - min;
            }

            // Half flux
            HF = FSum / 2.0;

            // Total flux starting from center
            TF = buffer[cen_y * stats.width + cen_x] - min;

            int pixelCounter = 1;

            // Integrate flux along radius axis until we reach half flux
            for (int k = 1; k < rCenter->width / 2; k++)
            {
                if (TF >= HF)
                {
                    qCDebug(KSTARS_FITS) << "Stopping at TF " << TF << " after #" << k << " pixels.";
                    break;
                }

                TF += buffer[cen_y * stats.width + cen_x + k] - min;
                TF += buffer[cen_y * stats.width + cen_x - k] - min;

                pixelCounter++;
            }

            // Calculate weighted Half Flux Radius
            rCenter->HFR = pixelCounter * (HF / TF);
            // Store full flux
            rCenter->val = FSum;

            qCDebug(KSTARS_FITS) << "HFR for this center is " << rCenter->HFR << " pixels and the total flux is " << FSum;

            starCenters.append(rCenter);
        }
    }

    if (starCenters.count() > 1 && m_Mode != FITS_FOCUS)
    {
        float width_avg = (float)width_sum / starCenters.count();
        float lsum = 0, sdev = 0;

        for (auto &center : starCenters)
            lsum += (center->width - width_avg) * (center->width - width_avg);

        sdev = (std::sqrt(lsum / (starCenters.count() - 1))) * 4;

        // Reject stars > 4 * stddev
        foreach (Edge * center, starCenters)
            if (center->width > sdev)
                starCenters.removeOne(center);

        //foreach(Edge *center, starCenters)
        //qDebug() << center->x << "," << center->y << "," << center->width << "," << center->val << endl;
    }

    // Release memory
    qDeleteAll(edges);

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

    QVector<double> HFRs;
    for (auto center : starCenters)
        HFRs << center->HFR;
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
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max, coeff, n](T & a)
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
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max, coeff, n](T & a)
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
                        futures.append(QtConcurrent::map(runningBuffer, (runningBuffer + ((i == (nThreads - 1)) ? fStride : tStride)), [min, max, n](T & a)
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

void FITSData::getCenterSelection(int * x, int * y)
{
    if (starCenters.count() == 0)
        return;

    auto * pEdge  = new Edge();
    pEdge->x     = *x;
    pEdge->y     = *y;
    pEdge->width = 1;

    foreach (Edge * center, starCenters)
        if (checkCollision(pEdge, center))
        {
            *x = static_cast<int>(center->x);
            *y = static_cast<int>(center->y);
            break;
        }

    delete (pEdge);
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

FITSSkyObject::FITSSkyObject(SkyObject * object, int xPos, int yPos) : QObject()
{
    skyObjectStored = object;
    xLoc            = xPos;
    yLoc            = yPos;
}

SkyObject * FITSSkyObject::skyObject()
{
    return skyObjectStored;
}

int FITSSkyObject::x()
{
    return xLoc;
}

int FITSSkyObject::y()
{
    return yLoc;
}

void FITSSkyObject::setX(int xPos)
{
    xLoc = xPos;
}

void FITSSkyObject::setY(int yPos)
{
    yLoc = yPos;
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

uint8_t * FITSData::getImageBuffer()
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
    char bayerPattern[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, nullptr, &status))
        return false;

    if (stats.bitpix != 16 && stats.bitpix != 8)
    {
        KSNotification::error(i18n("Only 8 and 16 bits bayered images supported."), i18n("Debayer error"));
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

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

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height, debayerParams.filter,
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

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, stats.width, ds1394_height, debayerParams.filter,
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

/* CannyDetector, Implementation of Canny edge detector in Qt/C++.
 * Copyright (C) 2015  Gonzalo Exequiel Pedone
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Email   : hipersayan DOT x AT gmail DOT com
 * Web-Site: https://github.com/hipersayanX/CannyDetector
 */

template <typename T>
void FITSData::sobel(QVector<float> &gradient, QVector<float> &direction)
{
    //int size = image.width() * image.height();
    gradient.resize(stats.samples_per_channel);
    direction.resize(stats.samples_per_channel);

    for (int y = 0; y < stats.height; y++)
    {
        size_t yOffset    = y * stats.width;
        const T * grayLine = reinterpret_cast<T *>(m_ImageBuffer) + yOffset;

        const T * grayLine_m1 = y < 1 ? grayLine : grayLine - stats.width;
        const T * grayLine_p1 = y >= stats.height - 1 ? grayLine : grayLine + stats.width;

        float * gradientLine  = gradient.data() + yOffset;
        float * directionLine = direction.data() + yOffset;

        for (int x = 0; x < stats.width; x++)
        {
            int x_m1 = x < 1 ? x : x - 1;
            int x_p1 = x >= stats.width - 1 ? x : x + 1;

            int gradX = grayLine_m1[x_p1] + 2 * grayLine[x_p1] + grayLine_p1[x_p1] - grayLine_m1[x_m1] -
                        2 * grayLine[x_m1] - grayLine_p1[x_m1];

            int gradY = grayLine_m1[x_m1] + 2 * grayLine_m1[x] + grayLine_m1[x_p1] - grayLine_p1[x_m1] -
                        2 * grayLine_p1[x] - grayLine_p1[x_p1];

            gradientLine[x] = qAbs(gradX) + qAbs(gradY);

            /* Gradient directions are classified in 4 possible cases
             *
             * dir 0
             *
             * x x x
             * - - -
             * x x x
             *
             * dir 1
             *
             * x x /
             * x / x
             * / x x
             *
             * dir 2
             *
             * \ x x
             * x \ x
             * x x \
             *
             * dir 3
             *
             * x | x
             * x | x
             * x | x
             */
            if (gradX == 0 && gradY == 0)
                directionLine[x] = 0;
            else if (gradX == 0)
                directionLine[x] = 3;
            else
            {
                qreal a = 180. * atan(qreal(gradY) / gradX) / M_PI;

                if (a >= -22.5 && a < 22.5)
                    directionLine[x] = 0;
                else if (a >= 22.5 && a < 67.5)
                    directionLine[x] = 2;
                else if (a >= -67.5 && a < -22.5)
                    directionLine[x] = 1;
                else
                    directionLine[x] = 3;
            }
        }
    }
}

int FITSData::partition(int width, int height, QVector<float> &gradient, QVector<int> &ids)
{
    int id = 0;

    for (int y = 1; y < height - 1; y++)
    {
        for (int x = 1; x < width - 1; x++)
        {
            int index = x + y * width;
            float val = gradient[index];
            if (val > 0 && ids[index] == 0)
            {
                trace(width, height, ++id, gradient, ids, x, y);
            }
        }
    }

    // Return max id
    return id;
}

void FITSData::trace(int width, int height, int id, QVector<float> &image, QVector<int> &ids, int x, int y)
{
    int yOffset      = y * width;
    float * cannyLine = image.data() + yOffset;
    int * idLine      = ids.data() + yOffset;

    if (idLine[x] != 0)
        return;

    idLine[x] = id;

    for (int j = -1; j < 2; j++)
    {
        int nextY = y + j;

        if (nextY < 0 || nextY >= height)
            continue;

        float * cannyLineNext = cannyLine + j * width;

        for (int i = -1; i < 2; i++)
        {
            int nextX = x + i;

            if (i == j || nextX < 0 || nextX >= width)
                continue;

            if (cannyLineNext[nextX] > 0)
            {
                // Trace neighbors.
                trace(width, height, id, image, ids, nextX, nextY);
            }
        }
    }
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
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
    auto * buffer = (T *)getImageBuffer();
#pragma GCC diagnostic pop
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
                scanLine[i] = qBound<unsigned char>(0, (unsigned char)val, 255);
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
        qCCritical(KSTARS_FITS) << "Failed to create FITS file. Error:" << status;
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

int FITSData::findSEPStars(const QRect &boundary)
{
    int x = 0, y = 0, w = stats.width, h = stats.height, maxRadius = 50;

    if (!boundary.isNull())
    {
        x = boundary.x();
        y = boundary.y();
        w = boundary.width();
        h = boundary.height();
        maxRadius = w;
    }

    auto * data = new float[w * h];

    switch (stats.bitpix)
    {
        case BYTE_IMG:
            getFloatBuffer<uint8_t>(data, x, y, w, h);
            break;
        case SHORT_IMG:
            getFloatBuffer<int16_t>(data, x, y, w, h);
            break;
        case USHORT_IMG:
            getFloatBuffer<uint16_t>(data, x, y, w, h);
            break;
        case LONG_IMG:
            getFloatBuffer<int32_t>(data, x, y, w, h);
            break;
        case ULONG_IMG:
            getFloatBuffer<uint32_t>(data, x, y, w, h);
            break;
        case FLOAT_IMG:
            delete [] data;
            data = reinterpret_cast<float *>(m_ImageBuffer);
            break;
        case LONGLONG_IMG:
            getFloatBuffer<int64_t>(data, x, y, w, h);
            break;
        case DOUBLE_IMG:
            getFloatBuffer<double>(data, x, y, w, h);
            break;
        default:
            delete [] data;
            return -1;
    }

    float * imback = nullptr;
    double * flux = nullptr, *fluxerr = nullptr, *area = nullptr;
    short * flag = nullptr;
    short flux_flag = 0;
    int status = 0;
    sep_bkg * bkg = nullptr;
    sep_catalog * catalog = nullptr;
    float conv[] = {1, 2, 1, 2, 4, 2, 1, 2, 1};
    double flux_fractions[2] = {0};
    double requested_frac[2] = { 0.5, 0.99 };
    QList<Edge *> edges;

    // #0 Create SEP Image structure
    sep_image im = {data, nullptr, nullptr, SEP_TFLOAT, 0, 0, w, h, 0.0, SEP_NOISE_NONE, 1.0, 0.0};

    // #1 Background estimate
    status = sep_background(&im, 64, 64, 3, 3, 0.0, &bkg);
    if (status != 0) goto exit;

    // #2 Background evaluation
    imback = (float *)malloc((w * h) * sizeof(float));
    status = sep_bkg_array(bkg, imback, SEP_TFLOAT);
    if (status != 0) goto exit;

    // #3 Background subtraction
    status = sep_bkg_subarray(bkg, im.data, im.dtype);
    if (status != 0) goto exit;

    // #4 Source Extraction
    // Note that we set deblend_cont = 1.0 to turn off deblending.
    status = sep_extract(&im, 2 * bkg->globalrms, SEP_THRESH_ABS, 10, conv, 3, 3, SEP_FILTER_CONV, 32, 1.0, 1, 1.0, &catalog);
    if (status != 0) goto exit;

    // TODO
    // Must detect edge detection
    // Must limit to brightest 100 (by flux) centers
    // Should probably use ellipse to draw instead of simple circle?
    // Useful for galaxies and also elenogated stars.
    for (int i = 0; i < catalog->nobj; i++)
    {
        double flux = catalog->flux[i];
        // Get HFR
        sep_flux_radius(&im, catalog->x[i], catalog->y[i], maxRadius, 5, 0, &flux, requested_frac, 2, flux_fractions, &flux_flag);

        auto * center = new Edge();
        center->x = catalog->x[i] + x + 0.5;
        center->y = catalog->y[i] + y + 0.5;
        center->val = catalog->peak[i];
        center->sum = flux;
        center->HFR = center->width = flux_fractions[0];
        if (flux_fractions[1] < maxRadius)
            center->width = flux_fractions[1] * 2;
        edges.append(center);
    }

    // Let's sort edges, starting with widest
    std::sort(edges.begin(), edges.end(), [](const Edge * edge1, const Edge * edge2) -> bool { return edge1->width > edge2->width;});

    // Take only the first 100 stars
    {
        int starCount = qMin(100, edges.count());
        for (int i = 0; i < starCount; i++)
            starCenters.append(edges[i]);
    }

    edges.clear();

    qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << "#" << "#X" << "#Y" << "#Flux" << "#Width" << "#HFR";
    for (int i = 0; i < starCenters.count(); i++)
        qCDebug(KSTARS_FITS) << qSetFieldWidth(10) << i << starCenters[i]->x << starCenters[i]->y
                             << starCenters[i]->sum << starCenters[i]->width << starCenters[i]->HFR;

exit:
    if (stats.bitpix != FLOAT_IMG)
        delete [] data;
    sep_bkg_free(bkg);
    sep_catalog_free(catalog);
    free(imback);
    free(flux);
    free(fluxerr);
    free(area);
    free(flag);

    if (status != 0)
    {
        char errorMessage[512];
        sep_get_errmsg(status, errorMessage);
        qCritical(KSTARS_FITS) << errorMessage;
        return -1;
    }

    return starCenters.count();
}

template <typename T>
void FITSData::getFloatBuffer(float * buffer, int x, int y, int w, int h)
{
    auto * rawBuffer = reinterpret_cast<T *>(m_ImageBuffer);

    float * floatPtr = buffer;

    int x2 = x + w;
    int y2 = y + h;

    for (int y1 = y; y1 < y2; y1++)
    {
        int offset = y1 * stats.width;
        for (int x1 = x; x1 < x2; x1++)
        {
            *floatPtr++ = rawBuffer[offset + x1];
        }
    }
}

void FITSData::saveStatistics(Statistic &other)
{
    other = stats;
}

void FITSData::restoreStatistics(Statistic &other)
{
    stats = other;
}
