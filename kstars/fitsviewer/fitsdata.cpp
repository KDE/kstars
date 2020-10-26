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

#if !defined(KSTARS_LITE) && defined(HAVE_LIBRAW)
#include <libraw/libraw.h>
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
const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw" };


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
    this->m_Statistics.channels = other->m_Statistics.channels;
    memcpy(&m_Statistics, &(other->m_Statistics), sizeof(m_Statistics));
    m_ImageBuffer = new uint8_t[m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel];
    memcpy(m_ImageBuffer, other->m_ImageBuffer,
           m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel);
}

FITSData::~FITSData()
{
    int status = 0;

    m_StarFindFuture.waitForFinished();

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

bool FITSData::loadFromBuffer(const QByteArray &buffer, const QString &extension, bool silent)
{
    loadCommon(QString());
    qCDebug(KSTARS_FITS) << "Reading file buffer (" << KFormat().formatByteSize(buffer.size()) << ")";
    return privateLoad(buffer, extension, silent);
}

QFuture<bool> FITSData::loadFromFile(const QString &inFilename, bool silent)
{
    loadCommon(inFilename);
    QFileInfo info(m_Filename);
    QString extension = info.completeSuffix().toLower();
    qCInfo(KSTARS_FITS) << "Loading file " << m_Filename;
    QFuture<bool> result = QtConcurrent::run(this, &FITSData::privateLoad, QByteArray(), extension, silent);

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

bool FITSData::privateLoad(const QByteArray &buffer, const QString &extension, bool silent)
{
    m_isTemporary = m_Filename.startsWith(m_TemporaryPath);

    if (extension.contains("fit"))
        return loadFITSImage(buffer, extension, silent);
    if (QImageReader::supportedImageFormats().contains(extension.toLatin1()))
        return loadCanonicalImage(buffer, extension, silent);
    else if (RAWFormats.contains(extension))
        return loadRAWImage(buffer, extension, silent);

    return false;
}

bool FITSData::loadFITSImage(const QByteArray &buffer, const QString &extension, bool silent)
{
    int status = 0, anynull = 0;
    long naxes[3];
    QString errMessage;

    if (buffer.isEmpty() && extension.contains(".fz"))
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

    if (buffer.isEmpty())
    {
        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        if (fits_open_diskfile(&fptr, m_Filename.toLatin1(), READONLY, &status))
        {
            return fitsOpenError(status, i18n("Error opening fits file %1", m_Filename), silent);
        }

        m_Statistics.size = QFile(m_Filename).size();
    }
    else
    {
        // Read the FITS file from a memory buffer.
        void *temp_buffer = const_cast<void *>(reinterpret_cast<const void *>(buffer.data()));
        size_t temp_size = buffer.size();
        if (fits_open_memfile(&fptr, m_Filename.toLatin1().data(), READONLY,
                              &temp_buffer, &temp_size, 0, nullptr, &status))
            return fitsOpenError(status, i18n("Error reading fits buffer."), silent);

        m_Statistics.size = temp_size;
    }

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
        return fitsOpenError(status, i18n("Could not locate image HDU."), silent);

    int fitsBitPix = 0;
    if (fits_get_img_param(fptr, 3, &fitsBitPix, &(m_Statistics.ndim), naxes, &status))
        return fitsOpenError(status, i18n("FITS file open error (fits_get_img_param)."), silent);

    if (m_Statistics.ndim < 2)
    {
        errMessage = i18n("1D FITS images are not supported in KStars.");
        if (!silent)
            KSNotification::error(errMessage, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << errMessage;
        return false;
    }

    switch (fitsBitPix)
    {
        case BYTE_IMG:
            m_Statistics.dataType      = TBYTE;
            m_Statistics.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_Statistics.dataType      = TFLOAT;
            m_Statistics.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_Statistics.dataType      = TLONGLONG;
            m_Statistics.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_Statistics.dataType      = TDOUBLE;
            m_Statistics.bytesPerPixel = sizeof(double);
            break;
        default:
            errMessage = i18n("Bit depth %1 is not supported.", fitsBitPix);
            if (!silent)
                KSNotification::error(errMessage, i18n("FITS Open"));
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
    }

    if (m_Statistics.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        errMessage = i18n("Image has invalid dimensions %1x%2", naxes[0], naxes[1]);
        if (!silent)
            KSNotification::error(errMessage, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << errMessage;
        return false;
    }

    m_Statistics.width               = naxes[0];
    m_Statistics.height              = naxes[1];
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;

    clearImageBuffers();

    m_Statistics.channels = naxes[2];

    // Channels always set to #1 if we are not required to process 3D Cubes
    // Or if mode is not FITS_NORMAL (guide, focus..etc)
    if (m_Mode != FITS_NORMAL || !Options::auto3DCube())
        m_Statistics.channels = 1;

    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel;
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
    long nelements = m_Statistics.samples_per_channel * m_Statistics.channels;

    if (fits_read_img(fptr, m_Statistics.dataType, 1, nelements, nullptr, m_ImageBuffer, &anynull, &status))
        return fitsOpenError(status, i18n("Error reading image."), silent);

    parseHeader();

    // Get UTC date time
    QVariant value;
    if (getRecordValue("DATE-OBS", value) && value.isValid())
        m_DateTime = KStarsDateTime(value.toDateTime());

    if (m_Statistics.channels == 1 && Options::autoDebayer() && checkDebayer())
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

bool FITSData::loadCanonicalImage(const QByteArray &buffer, const QString &extension, bool silent)
{
    // TODO need to add error popups as well later on
    Q_UNUSED(silent);
    QString errMessage;
    QImage imageFromFile;
    if (!buffer.isEmpty())
    {
        if(!imageFromFile.loadFromData(reinterpret_cast<const uint8_t*>(buffer.data()), buffer.size(),
                                       extension.toLatin1().constData()))
        {
            qCCritical(KSTARS_FITS) << "Failed to open image.";
            return false;
        }

        m_Statistics.size = buffer.size();
    }
    else
    {
        if(!imageFromFile.load(m_Filename.toLatin1()))
        {
            qCCritical(KSTARS_FITS) << "Failed to open image.";
            return false;
        }

        m_Statistics.size = QFileInfo(m_Filename).size();
    }

    imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    // Note: This will need to be changed.  I think QT only loads 8 bpp images.
    // Also the depth method gives the total bits per pixel in the image not just the bits per
    // pixel in each channel.
    const uint8_t fitsBitPix = 8;
    switch (fitsBitPix)
    {
        case BYTE_IMG:
            m_Statistics.dataType      = TBYTE;
            m_Statistics.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(int32_t);
            break;
        case ULONG_IMG:
            m_Statistics.dataType      = TULONG;
            m_Statistics.bytesPerPixel = sizeof(uint32_t);
            break;
        case FLOAT_IMG:
            m_Statistics.dataType      = TFLOAT;
            m_Statistics.bytesPerPixel = sizeof(float);
            break;
        case LONGLONG_IMG:
            m_Statistics.dataType      = TLONGLONG;
            m_Statistics.bytesPerPixel = sizeof(int64_t);
            break;
        case DOUBLE_IMG:
            m_Statistics.dataType      = TDOUBLE;
            m_Statistics.bytesPerPixel = sizeof(double);
            break;
        default:
            errMessage = QString("Bit depth %1 is not supported.").arg(fitsBitPix);
            QMessageBox::critical(nullptr, "Message", errMessage);
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
    }

    m_Statistics.width = static_cast<uint16_t>(imageFromFile.width());
    m_Statistics.height = static_cast<uint16_t>(imageFromFile.height());
    m_Statistics.channels = 3;
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
    clearImageBuffers();
    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * static_cast<uint16_t>
                        (m_Statistics.bytesPerPixel);
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        qCCritical(KSTARS_FITS) << QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(
                                    m_ImageBufferSize);
        clearImageBuffers();
        return false;
    }

    auto debayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * original_bayered_buffer = reinterpret_cast<uint8_t *>(imageFromFile.bits());

    // Data in RGB32, with bytes in the order of B,G,R,A, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = debayered_buffer;
    uint8_t * gBuff = debayered_buffer + (m_Statistics.width * m_Statistics.height);
    uint8_t * bBuff = debayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

    int imax = m_Statistics.samples_per_channel * 4 - 4;
    for (int i = 0; i <= imax; i += 4)
    {
        *rBuff++ = original_bayered_buffer[i + 2];
        *gBuff++ = original_bayered_buffer[i + 1];
        *bBuff++ = original_bayered_buffer[i + 0];
    }

    calculateStats();
    return true;
}

bool FITSData::loadRAWImage(const QByteArray &buffer, const QString &extension, bool silent)
{
    // TODO need to add error popups as well later on
    Q_UNUSED(silent);
    Q_UNUSED(extension);

#if !defined(KSTARS_LITE) && !defined(HAVE_LIBRAW)
    lastError = i18n("Unable to find dcraw and cjpeg. Please install the required tools to convert CR2/NEF to JPEG.");
    return false;
#else

    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file/buffer
    if (buffer.isEmpty())
    {
        // Open file
        if ((ret = RawProcessor.open_file(m_Filename.toLatin1().constData())) != LIBRAW_SUCCESS)
        {
            lastError = i18n("Cannot open file %1: %2", m_Filename, libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }

        m_Statistics.size = QFileInfo(m_Filename).size();
    }
    // Open Buffer
    else
    {
        if ((ret = RawProcessor.open_buffer(const_cast<void *>(reinterpret_cast<const void *>(buffer.data())),
                                            buffer.size())) != LIBRAW_SUCCESS)
        {
            lastError = i18n("Cannot open buffer: %1", libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }

        m_Statistics.size = buffer.size();
    }

    // Let us unpack the thumbnail
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
        lastError = i18n("Cannot unpack_thumb: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    if ((ret = RawProcessor.dcraw_process()) != LIBRAW_SUCCESS)
    {
        lastError = i18n("Cannot dcraw_process: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    libraw_processed_image_t *image = RawProcessor.dcraw_make_mem_image(&ret);
    if (ret != LIBRAW_SUCCESS)
    {
        lastError = i18n("Cannot load to memory: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    RawProcessor.recycle();

    m_Statistics.bytesPerPixel = image->bits / 8;
    // We only support two types now
    if (m_Statistics.bytesPerPixel == 1)
        m_Statistics.dataType = TBYTE;
    else
        m_Statistics.dataType = TUSHORT;
    m_Statistics.width = image->width;
    m_Statistics.height = image->height;
    m_Statistics.channels = image->colors;
    m_Statistics.samples_per_channel = m_Statistics.width * m_Statistics.height;
    clearImageBuffers();
    m_ImageBufferSize = m_Statistics.samples_per_channel * m_Statistics.channels * m_Statistics.bytesPerPixel;
    m_ImageBuffer = new uint8_t[m_ImageBufferSize];
    if (m_ImageBuffer == nullptr)
    {
        qCCritical(KSTARS_FITS) << QString("FITSData: Not enough memory for image_buffer channel. Requested: %1 bytes ").arg(
                                    m_ImageBufferSize);
        libraw_dcraw_clear_mem(image);
        clearImageBuffers();
        return false;
    }

    auto destination_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto source_buffer = reinterpret_cast<uint8_t *>(image->data);

    // For mono, we memcpy directly
    if (image->colors == 1)
    {
        memcpy(destination_buffer, source_buffer, m_ImageBufferSize);
    }
    else
    {
        // Data in RGB24, with bytes in the order of R,G,B. We copy them copy them into 3 layers for FITS
        uint8_t * rBuff = destination_buffer;
        uint8_t * gBuff = destination_buffer + (m_Statistics.width * m_Statistics.height);
        uint8_t * bBuff = destination_buffer + (m_Statistics.width * m_Statistics.height * 2);

        int imax = m_Statistics.samples_per_channel * 3 - 3;
        for (int i = 0; i <= imax; i += 3)
        {
            *rBuff++ = source_buffer[i + 0];
            *gBuff++ = source_buffer[i + 1];
            *bBuff++ = source_buffer[i + 2];
        }
    }
    libraw_dcraw_clear_mem(image);

    calculateStats();
    return true;
#endif
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

        double dataMin = m_Statistics.mean[0] - m_Statistics.stddev[0];
        double dataMax = m_Statistics.mean[0] + m_Statistics.stddev[0] * 3;

        double bscale = 255. / (dataMax - dataMin);
        double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

        // Long way to do this since we do not want to use templated functions here
        switch (m_Statistics.dataType)
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

    int status = 0;
    long nelements;
    fitsfile * new_fptr;

    if (HasDebayer && m_Filename.isEmpty() == false)
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

    nelements = m_Statistics.samples_per_channel * m_Statistics.channels;

    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, QString("!%1").arg(newFilename).toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    status = 0;

    fptr = new_fptr;

    // Create image
    long naxis = m_Statistics.channels == 1 ? 2 : 3;
    long naxes[3] = {m_Statistics.width, m_Statistics.height, naxis};
    if (fits_create_img(fptr, m_Statistics.dataType, naxis, naxes, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Write Data */
    if (fits_write_img(fptr, m_Statistics.dataType, 1, nelements, m_ImageBuffer, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(m_Statistics.min), "Minimum value", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(m_Statistics.max), "Maximum value", &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    // Skip first 10 standard records and copy the rest.
    for (int i = 10; i < m_HeaderRecords.count(); i++)
    {
        QString key = m_HeaderRecords[i].key;
        const char *comment = m_HeaderRecords[i].comment.toLatin1().constBegin();
        QVariant value = m_HeaderRecords[i].value;

        switch (value.type())
        {
            case QVariant::Int:
            {
                int number = value.toInt();
                fits_write_key(fptr, TINT, key.toLatin1().constData(), &number, comment, &status);
            }
            break;

            case QVariant::Double:
            {
                double number = value.toDouble();
                fits_write_key(fptr, TDOUBLE, key.toLatin1().constData(), &number, comment, &status);
            }
            break;

            case QVariant::String:
            default:
            {
                char valueBuffer[256] = {0};
                strncpy(valueBuffer, value.toString().toLatin1().constData(), 256);
                fits_write_key(fptr, TSTRING, key.toLatin1().constData(), valueBuffer, comment, &status);
            }
        }
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

    if (m_Filename.isEmpty() == false && m_isTemporary && autoRemoveTemporaryFITS)
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
    switch (m_Statistics.dataType)
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
    m_Statistics.SNR = m_Statistics.mean[0] / m_Statistics.stddev[0];
}

int FITSData::calculateMinMax(bool refresh)
{
    int status, nfound = 0;

    status = 0;

    if ((fptr != nullptr) && !refresh)
    {
        if (fits_read_key_dbl(fptr, "DATAMIN", &(m_Statistics.min[0]), nullptr, &status) == 0)
            nfound++;

        if (fits_read_key_dbl(fptr, "DATAMAX", &(m_Statistics.max[0]), nullptr, &status) == 0)
            nfound++;

        // If we found both keywords, no need to calculate them, unless they are both zeros
        if (nfound == 2 && !(m_Statistics.min[0] == 0 && m_Statistics.max[0] == 0))
            return 0;
    }

    m_Statistics.min[0] = 1.0E30;
    m_Statistics.max[0] = -1.0E30;

    m_Statistics.min[1] = 1.0E30;
    m_Statistics.max[1] = -1.0E30;

    m_Statistics.min[2] = 1.0E30;
    m_Statistics.max[2] = -1.0E30;

    switch (m_Statistics.dataType)
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
        min = qMin(buffer[i], min);
        max = qMax(buffer[i], max);
        //        if (buffer[i] < min)
        //            min = buffer[i];
        //        else if (buffer[i] > max)
        //            max = buffer[i];
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

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        uint32_t cStart = n * m_Statistics.samples_per_channel;

        // Calculate how many elements we process per thread
        uint32_t tStride = m_Statistics.samples_per_channel / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + (m_Statistics.samples_per_channel - (tStride * nThreads));

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
            min = qMin(result.first, min);
            max = qMax(result.second, max);
        }

        m_Statistics.min[n] = min;
        m_Statistics.max[n] = max;
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

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        uint32_t cStart = n * m_Statistics.samples_per_channel;

        // Calculate how many elements we process per thread
        uint32_t tStride = m_Statistics.samples_per_channel / nThreads;

        // Calculate the final stride since we can have some left over due to division above
        uint32_t fStride = tStride + (m_Statistics.samples_per_channel - (tStride * nThreads));

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

        double variance = squared_sum / m_Statistics.samples_per_channel;

        m_Statistics.mean[n]   = mean / nThreads;
        m_Statistics.stddev[n] = sqrt(variance);
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
    for (int offsetY = 0; offsetY < m_Statistics.height; offsetY++)
    {
        for (int offsetX = 0; offsetX < m_Statistics.width; offsetX++)
        {
            // reset gray value to 0
            gt = 0;
            // position of the kernel center pixel
            int byteOffset = offsetY * m_Statistics.width + offsetX;

            // kernel calculations
            for (int filterY = -fOff; filterY <= fOff; filterY++)
            {
                for (int filterX = -fOff; filterX <= fOff; filterX++)
                {
                    if ((offsetY + filterY) >= 0 && (offsetY + filterY) < m_Statistics.height
                            && ((offsetX + filterX) >= 0 && (offsetX + filterX) < m_Statistics.width ))
                    {

                        int calcOffset = byteOffset + filterX + filterY * m_Statistics.width;
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
    m_Statistics.min[channel] = newMin;
    m_Statistics.max[channel] = newMax;
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

    m_HeaderRecords.clear();
    QString recordList = QString(header);

    for (int i = 0; i < nkeys; i++)
    {
        Record oneRecord;
        // Quotes cause issues for simplified below so we're removing them.
        QString record = recordList.mid(i * 80, 80).remove("'");
        QStringList properties = record.split(QRegExp("[=/]"));
        // If it is only a comment
        if (properties.size() == 1)
        {
            oneRecord.key = properties[0].mid(0, 7);
            oneRecord.comment = properties[0].mid(8).simplified();
        }
        else
        {
            oneRecord.key = properties[0].simplified();
            oneRecord.value = properties[1].simplified();
            if (properties.size() > 2)
                oneRecord.comment = properties[2].simplified();

            // Try to guess the value.
            // Test for integer & double. If neither, then leave it as "string".
            bool ok = false;

            // Is it Integer?
            oneRecord.value.toInt(&ok);
            if (ok)
                oneRecord.value.convert(QMetaType::Int);
            else
            {
                // Is it double?
                oneRecord.value.toDouble(&ok);
                if (ok)
                    oneRecord.value.convert(QMetaType::Double);
            }
        }

        m_HeaderRecords.append(oneRecord);
    }

    free(header);

    return true;
}

bool FITSData::getRecordValue(const QString &key, QVariant &value) const
{
    auto result = std::find_if(m_HeaderRecords.begin(), m_HeaderRecords.end(), [&key](const Record & oneRecord)
    {
        return (oneRecord.key == key && oneRecord.value.isValid());
    });

    if (result != m_HeaderRecords.end())
    {
        value = (*result).value;
        return true;
    }
    return false;
}

bool FITSData::parseSolution(FITSImage::Solution &solution) const
{
    dms angleValue;
    bool raOK = false, deOK = false, coordOK = false, scaleOK = false;
    QVariant value;

    // Reset all
    solution.fieldWidth = solution.fieldHeight = solution.pixscale = solution.ra = solution.dec = -1;

    // RA
    if (getRecordValue("OBJCTRA", value))
    {
        angleValue = dms::fromString(value.toString(), false);
        solution.ra = angleValue.Degrees();
        raOK = true;
    }
    else if (getRecordValue("RA", value))
    {
        solution.ra = value.toDouble(&raOK);
    }

    // DE
    if (getRecordValue("OBJCTDEC", value))
    {
        angleValue = dms::fromString(value.toString(), true);
        solution.dec = angleValue.Degrees();
        deOK = true;
    }
    else if (getRecordValue("DEC", value))
    {
        solution.dec = value.toDouble(&deOK);
    }

    coordOK = raOK && deOK;

    // PixScale
    double scale = -1;
    if (getRecordValue("SCALE", value))
    {
        scale = value.toDouble();
    }

    double focal_length = -1;
    if (getRecordValue("FOCALLEN", value))
    {
        focal_length = value.toDouble();
    }

    double pixsize1 = -1, pixsize2 = -1;
    // Pixel Size 1
    if (getRecordValue("PIXSIZE1", value))
    {
        pixsize1 = value.toDouble();
    }
    // Pixel Size 2
    if (getRecordValue("PIXSIZE2", value))
    {
        pixsize2 = value.toDouble();
    }

    int binx = 1, biny = 1;
    // Binning X
    if (getRecordValue("XBINNING", value))
    {
        binx = value.toDouble();
    }
    // Binning Y
    if (getRecordValue("YBINNING", value))
    {
        biny = value.toDouble();
    }

    if (pixsize1 > 0 && pixsize2 > 0)
    {
        // If we have scale, then that's it
        if (scale > 0)
        {
            // Arcsecs per pixel
            solution.pixscale = scale;
            // Arcmins
            solution.fieldWidth = (m_Statistics.width * scale) / 60.0;
            // Arcmins, and account for pixel ratio if non-squared.
            solution.fieldHeight = (m_Statistics.height * scale * (pixsize2 / pixsize1)) / 60.0;
        }
        else if (focal_length > 0)
        {
            // Arcmins
            solution.fieldWidth = ((206264.8062470963552 * m_Statistics.width * (pixsize1 / 1000.0)) / (focal_length * binx)) / 60.0;
            // Arsmins
            solution.fieldHeight = ((206264.8062470963552 * m_Statistics.height * (pixsize2 / 1000.0)) / (focal_length * biny)) / 60.0;
            // Arcsecs per pixel
            solution.pixscale = (206264.8062470963552 * (pixsize1 / 1000.0)) / (focal_length * binx);
        }

        scaleOK = true;
    }

    return (coordOK || scaleOK);
}

QFuture<bool> FITSData::findStars(StarAlgorithm algorithm, const QRect &trackingBox)
{
    starAlgorithm = algorithm;
    qDeleteAll(starCenters);
    starCenters.clear();
    starsSearched = true;

    switch (algorithm)
    {
        case ALGORITHM_SEP:
        {
            QPointer<FITSSEPDetector> detector = new FITSSEPDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            if (m_Mode == FITS_NORMAL && trackingBox.isNull())
            {
                if (Options::quickHFR())
                {
                    // need this with QRect.
                    detector->configure("radiusIsBoundary", false);
                    detector->configure("maxStarsCount", 50);
                    //Just finds stars in the center 25% of the image.
                    const int w = getStatistics().width;
                    const int h = getStatistics().height;
                    QRect middle(static_cast<int>(w * 0.25), static_cast<int>(h * 0.25), w / 2, h / 2);
                    return detector->findSources(middle);
                }
            }
            return detector->findSources(trackingBox);
        }

        case ALGORITHM_GRADIENT:
        default:
        {
            QPointer<FITSGradientDetector> detector = new FITSGradientDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            return detector->findSources(trackingBox);
        }

        case ALGORITHM_CENTROID:
        {
#ifndef KSTARS_LITE
            if (histogram)
                if (!histogram->isConstructed())
                    histogram->constructHistogram();

            QPointer<FITSCentroidDetector> detector = new FITSCentroidDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            detector->configure("JMINDEX", histogram ? histogram->getJMIndex() : 100);
            return detector->findSources(trackingBox);
        }
#else
            {
                QPointer<FITSCentroidDetector> detector = new FITSCentroidDetector(this);
                return detector->findSources(starCenters, trackingBox);
            }
#endif

        case ALGORITHM_THRESHOLD:
        {
            QPointer<FITSThresholdDetector> detector = new FITSThresholdDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            detector->configure("THRESHOLD_PERCENTAGE", Options::focusThreshold());
            return detector->findSources(trackingBox);
        }

        case ALGORITHM_BAHTINOV:
        {
            QPointer<FITSBahtinovDetector> detector = new FITSBahtinovDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            detector->configure("NUMBER_OF_AVERAGE_ROWS", Options::focusMultiRowAverage());
            return detector->findSources(trackingBox);
        }
    }
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

    m_SelectedHFRStar = nullptr;

    if (type == HFR_MAX)
    {

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

        m_SelectedHFRStar = starCenters[maxIndex];
        return starCenters[maxIndex]->HFR;
    }
    else if (type == HFR_HIGH)
    {
        // Reject all stars within 10% of border
        int minX = width() / 10;
        int minY = height() / 10;
        int maxX = width() - minX;
        int maxY = height() - minY;
        starCenters.erase(std::remove_if(starCenters.begin(), starCenters.end(), [minX, minY, maxX, maxY](Edge * oneStar)
        {
            return (oneStar->x < minX || oneStar->x > maxX || oneStar->y < minY || oneStar->y > maxY);
        }), starCenters.end());
        // Top 5%
        if (starCenters.empty())
            return -1;

        m_SelectedHFRStar = starCenters[static_cast<int>(starCenters.size() * 0.05)];
        return m_SelectedHFRStar->HFR;
    }
    else if (type == HFR_MEDIAN)
    {
        std::nth_element(starCenters.begin(), starCenters.begin() + starCenters.size() / 2, starCenters.end());
        m_SelectedHFRStar = starCenters[starCenters.size() / 2];
        return m_SelectedHFRStar->HFR;
    }

    // We may remove saturated stars from the HFR calculation, if we have enough stars.
    // No real way to tell the scale, so only remove saturated stars with range 0 -> 2**16
    // for non-byte types. Unsigned types and floating types, or really any pixels whose
    // range isn't 0-64 (or 0-255 for bytes) won't have their saturated stars removed.
    const int saturationValue = m_Statistics.dataType == TBYTE ? 250 : 50000;
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
            m_SelectedHFRStar = starCenters[i];
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
                dataMin[i] = m_Statistics.mean[i] - m_Statistics.stddev[i];
                dataMax[i] = m_Statistics.mean[i] + m_Statistics.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_CONTRAST:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = m_Statistics.mean[i] + m_Statistics.stddev[i];
                dataMax[i] = m_Statistics.mean[i] + m_Statistics.stddev[i] * 3;
            }
        }
        break;

        case FITS_HIGH_PASS:
        {
            for (int i = 0; i < 3; i++)
            {
                dataMin[i] = m_Statistics.mean[i];
            }
        }
        break;

        default:
            break;
    }

    switch (m_Statistics.dataType)
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

    uint32_t width  = m_Statistics.width;
    uint32_t height = m_Statistics.height;

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

            for (int n = 0; n < m_Statistics.channels; n++)
            {
                if (type == FITS_HIGH_PASS)
                    min[n] = m_Statistics.mean[n];

                uint32_t cStart = n * m_Statistics.samples_per_channel;

                // Calculate how many elements we process per thread
                uint32_t tStride = m_Statistics.samples_per_channel / nThreads;

                // Calculate the final stride since we can have some left over due to division above
                uint32_t fStride = tStride + (m_Statistics.samples_per_channel - (tStride * nThreads));

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

            for (int i = 0; i < nThreads * m_Statistics.channels; i++)
                futures[i].waitForFinished();

            if (calcStats)
            {
                for (int i = 0; i < 3; i++)
                {
                    m_Statistics.min[i] = min[i];
                    m_Statistics.max[i] = max[i];
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

            for (int i = 0; i < m_Statistics.channels; i++)
            {
                uint32_t offset = i * m_Statistics.samples_per_channel;
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
            uint8_t BBP      = m_Statistics.bytesPerPixel;
            auto * extension = new T[(width + 2) * (height + 2)];
            //   Check memory allocation
            if (!extension)
                return;
            //   Create image extension
            for (uint32_t ch = 0; ch < m_Statistics.channels; ch++)
            {
                uint32_t offset = ch * m_Statistics.samples_per_channel;
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

    wcs_coord = new FITSImage::wcs_point[w * h];

    if (wcs_coord == nullptr)
    {
        wcsvfree(&m_nwcs, &m_wcs);
        m_wcs = nullptr;
        lastError = "Not enough memory for WCS data!";
        return false;
    }

    FITSImage::wcs_point * p = wcs_coord;

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

    FITSImage::wcs_point * wcs_coord = getWCSCoord();
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

    nx = m_Statistics.width;
    ny = m_Statistics.height;

    int BBP = m_Statistics.bytesPerPixel;

    /* Allocate buffer for rotated image */
    rotimage = new uint8_t[m_Statistics.samples_per_channel * m_Statistics.channels * BBP];

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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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

        m_Statistics.width  = ny;
        m_Statistics.height = nx;
    }

    /* Rotate by 180 degrees */
    else if (rotate >= 135 && rotate < 225)
    {
        if (mirror == 1)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1 * ny) + y1 + offset] = buffer[(y1 * nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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
            for (int i = 0; i < m_Statistics.channels; i++)
            {
                offset = m_Statistics.samples_per_channel * i;
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

        m_Statistics.width  = ny;
        m_Statistics.height = nx;
    }

    /* If rotating by more than 315 degrees, assume top-bottom reflection */
    else if (rotate >= 315 && mirror)
    {
        for (int i = 0; i < m_Statistics.channels; i++)
        {
            offset = m_Statistics.samples_per_channel * i;
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

    naxis1 = m_Statistics.width;
    naxis2 = m_Statistics.height;

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

    if (m_Statistics.dataType != TUSHORT && m_Statistics.dataType != TBYTE)
    {
        KSNotification::error(i18n("Only 8 and 16 bits bayered images supported."), i18n("Debayer error"));
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove('\'').trimmed();

    QString order(roworder);
    order = order.remove('\'').trimmed();

    if (order == "BOTTOM-UP" && !(m_Statistics.height % 2))
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

    //        if (fits_read_img(fptr, stats.dataType, 1, stats.samples_per_channel, nullptr, m_ImageBuffer, &anynull, &status))
    //        {
    //            char errmsg[512];
    //            fits_get_errstatus(status, errmsg);
    //            KSNotification::error(i18n("Error reading image: %1", QString(errmsg)), i18n("Debayer error"));
    //            return false;
    //        }
    //    }

    switch (m_Statistics.dataType)
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

    uint32_t rgb_size = m_Statistics.samples_per_channel * 3 * m_Statistics.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint8_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint8_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = m_Statistics.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += m_Statistics.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, bayer_destination_buffer, m_Statistics.width, ds1394_height,
                                            debayerParams.filter,
                                            debayerParams.method);

    if (error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        m_Statistics.channels = 1;
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
    uint8_t * gBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height);
    uint8_t * bBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

    int imax = m_Statistics.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    m_Statistics.channels = (m_Mode == FITS_NORMAL) ? 3 : 1;
    m_Statistics.dataType = TBYTE;
    delete[] destinationBuffer;
    return true;
}

bool FITSData::debayer_16bit()
{
    dc1394error_t error_code;

    uint32_t rgb_size = m_Statistics.samples_per_channel * 3 * m_Statistics.bytesPerPixel;
    auto * destinationBuffer = new uint8_t[rgb_size];

    auto * bayer_source_buffer      = reinterpret_cast<uint16_t *>(m_ImageBuffer);
    auto * bayer_destination_buffer = reinterpret_cast<uint16_t *>(destinationBuffer);

    if (bayer_destination_buffer == nullptr)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = m_Statistics.height;
    auto dc1394_source = bayer_source_buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += m_Statistics.width;
        ds1394_height--;
    }
    // offsetX == 1 is handled in checkDebayer() and should be 0 here.

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, bayer_destination_buffer, m_Statistics.width, ds1394_height,
                 debayerParams.filter,
                 debayerParams.method, 16);

    if (error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        m_Statistics.channels = 1;
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
    uint16_t * gBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height);
    uint16_t * bBuff = bayered_buffer + (m_Statistics.width * m_Statistics.height * 2);

    int imax = m_Statistics.samples_per_channel * 3 - 3;
    for (int i = 0; i <= imax; i += 3)
    {
        *rBuff++ = bayer_destination_buffer[i];
        *gBuff++ = bayer_destination_buffer[i + 1];
        *bBuff++ = bayer_destination_buffer[i + 2];
    }

    m_Statistics.channels = (m_Mode == FITS_NORMAL) ? 3 : 1;
    m_Statistics.dataType = TUSHORT;
    delete[] destinationBuffer;
    return true;
}

double FITSData::getADU() const
{
    double adu = 0;
    for (int i = 0; i < m_Statistics.channels; i++)
        adu += m_Statistics.mean[i];

    return (adu / static_cast<double>(m_Statistics.channels));
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

    QFuture<bool> future = data.loadFromFile(filename);

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

    double dataMin = data.m_Statistics.mean[0] - data.m_Statistics.stddev[0];
    double dataMax = data.m_Statistics.mean[0] + data.m_Statistics.stddev[0] * 3;

    double bscale = 255. / (dataMax - dataMin);
    double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

    // Long way to do this since we do not want to use templated functions here
    switch (data.m_Statistics.dataType)
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

    nelements = stats.samples_per_channel * stats.channels;

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
    if (fits_write_img(fptr, stats.dataType, 1, nelements, m_ImageBuffer, &status))
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
    return (point.x() >= 0 && point.y() >= 0 && point.x() <= m_Statistics.width && point.y() <= m_Statistics.height);
}

void FITSData::saveStatistics(FITSImage::Statistic &other)
{
    other = m_Statistics;
}

void FITSData::restoreStatistics(FITSImage::Statistic &other)
{
    m_Statistics = other;
}
