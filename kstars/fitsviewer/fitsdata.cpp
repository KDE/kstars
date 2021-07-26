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

QString getTemporaryPath() {
    return QDir(KSPaths::writableLocation(QStandardPaths::TempLocation) + "/" +
        qAppName()).path();
}

const QStringList RAWFormats = { "cr2", "cr3", "crw", "nef", "raf", "dng", "arw", "orf" };

FITSData::FITSData(FITSMode fitsMode): m_Mode(fitsMode)
{
    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    // Reserve 3 channels
    m_CumulativeFrequency.resize(3);
    m_HistogramBinWidth.resize(3);
    m_HistogramFrequency.resize(3);
    m_HistogramIntensity.resize(3);
}

FITSData::FITSData(const QSharedPointer<FITSData> &other)
{
    qRegisterMetaType<FITSMode>("FITSMode");

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX = debayerParams.offsetY = 0;

    m_TemporaryDataFile.setFileTemplate("fits_memory_XXXXXX");

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

    if (m_StarFindFuture.isRunning())
        m_StarFindFuture.waitForFinished();

    clearImageBuffers();

#ifdef HAVE_WCSLIB
    if (m_WCSHandle != nullptr)
        wcsvfree(&m_nwcs, &m_WCSHandle);
#endif

    if (starCenters.count() > 0)
        qDeleteAll(starCenters);

    if (m_SkyObjects.count() > 0)
        qDeleteAll(m_SkyObjects);

    if (fptr != nullptr)
    {
        fits_flush_file(fptr, &status);
        fits_close_file(fptr, &status);
        fptr = nullptr;
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
    }

    m_Filename = inFilename;
}

bool FITSData::loadFromBuffer(const QByteArray &buffer, const QString &extension, const QString &inFilename, bool silent)
{
    loadCommon(inFilename);
    qCDebug(KSTARS_FITS) << "Reading file buffer (" << KFormat().formatByteSize(buffer.size()) << ")";
    return privateLoad(buffer, extension, silent);
}

QFuture<bool> FITSData::loadFromFile(const QString &inFilename, bool silent)
{
    loadCommon(inFilename);
    QFileInfo info(m_Filename);
    QString extension = info.completeSuffix().toLower();
    qCInfo(KSTARS_FITS) << "Loading file " << m_Filename;
    return QtConcurrent::run(this, &FITSData::privateLoad, QByteArray(), extension, silent);
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
    m_isTemporary = m_Filename.startsWith(getTemporaryPath());

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

    m_HistogramConstructed = false;

    if (buffer.isEmpty() && extension.contains(".fz"))
    {
        // Store so we don't lose.
        m_compressedFilename = m_Filename;

        QString uncompressedFile = QDir::tempPath() + QString("/%1").arg(QUuid::createUuid().toString().remove(
                                       QRegularExpression("[-{}]")));
        fpstate fpvar;
        fp_init (&fpvar);
        if (fp_unpack(m_Filename.toLocal8Bit().data(), uncompressedFile.toLocal8Bit().data(), fpvar) < 0)
        {
            errMessage = i18n("Failed to unpack compressed fits");
            if (!silent)
                KSNotification::error(errMessage, i18n("FITS Open"));
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
        }

        m_Filename = uncompressedFile;
        m_isTemporary = true;
        m_isCompressed = true;
    }

    if (buffer.isEmpty())
    {
        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        if (fits_open_diskfile(&fptr, m_Filename.toLocal8Bit(), READONLY, &status))
        {
            recordLastError(status);
            return fitsOpenError(status, i18n("Error opening fits file %1", m_Filename), silent);
        }

        m_Statistics.size = QFile(m_Filename).size();
    }
    else
    {
        // Read the FITS file from a memory buffer.
        void *temp_buffer = const_cast<void *>(reinterpret_cast<const void *>(buffer.data()));
        size_t temp_size = buffer.size();
        if (fits_open_memfile(&fptr, m_Filename.toLocal8Bit().data(), READONLY,
                              &temp_buffer, &temp_size, 0, nullptr, &status))
        {
            recordLastError(status);
            return fitsOpenError(status, i18n("Error reading fits buffer."), silent);
        }

        m_Statistics.size = temp_size;
    }

    if (fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status))
    {
        recordLastError(status);
        return fitsOpenError(status, i18n("Could not locate image HDU."), silent);
    }

    if (fits_get_img_param(fptr, 3, &m_FITSBITPIX, &(m_Statistics.ndim), naxes, &status))
    {
        recordLastError(status);
        return fitsOpenError(status, i18n("FITS file open error (fits_get_img_param)."), silent);
    }

    if (m_Statistics.ndim < 2)
    {
        m_LastError = i18n("1D FITS images are not supported in KStars.");
        if (!silent)
            KSNotification::error(m_LastError, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << m_LastError;
        return false;
    }

    switch (m_FITSBITPIX)
    {
        case BYTE_IMG:
            m_Statistics.dataType      = TBYTE;
            m_Statistics.bytesPerPixel = sizeof(uint8_t);
            break;
        case SHORT_IMG:
            // Read SHORT image as USHORT
            m_FITSBITPIX               = USHORT_IMG;
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(int16_t);
            break;
        case USHORT_IMG:
            m_Statistics.dataType      = TUSHORT;
            m_Statistics.bytesPerPixel = sizeof(uint16_t);
            break;
        case LONG_IMG:
            // Read LONG image as ULONG
            m_FITSBITPIX               = ULONG_IMG;
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
            m_LastError = i18n("Bit depth %1 is not supported.", m_FITSBITPIX);
            if (!silent)
                KSNotification::error(m_LastError, i18n("FITS Open"));
            qCCritical(KSTARS_FITS) << m_LastError;
            return false;
    }

    if (m_Statistics.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        m_LastError = i18n("Image has invalid dimensions %1x%2", naxes[0], naxes[1]);
        if (!silent)
            KSNotification::error(m_LastError, i18n("FITS Open"));
        qCCritical(KSTARS_FITS) << m_LastError;
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
    {
        recordLastError(status);
        return fitsOpenError(status, i18n("Error reading image."), silent);
    }

    parseHeader();

    // Get UTC date time
    QVariant value;
    if (getRecordValue("DATE-OBS", value) && value.isValid())
    {
        QDateTime ts = value.toDateTime();
        m_DateTime = KStarsDateTime(ts.date(), ts.time());
    }

    // Only check for debayed IF the original naxes[2] is 1
    // which is for single channels.
    if (naxes[2] == 1 && m_Statistics.channels == 1 && Options::autoDebayer() && checkDebayer())
    {
        // Save bayer image on disk in case we need to save it later since debayer destorys this data
        if (m_isTemporary && m_TemporaryDataFile.open())
        {
            m_TemporaryDataFile.write(buffer);
            m_TemporaryDataFile.close();
            m_Filename = m_TemporaryDataFile.fileName();
        }

        if (debayer())
            calculateStats();
    }
    else
        calculateStats();

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
        if(!imageFromFile.load(m_Filename.toLocal8Bit()))
        {
            qCCritical(KSTARS_FITS) << "Failed to open image.";
            return false;
        }

        m_Statistics.size = QFileInfo(m_Filename).size();
    }

    //imageFromFile = imageFromFile.convertToFormat(QImage::Format_RGB32);

    // Note: This will need to be changed.  I think QT only loads 8 bpp images.
    // Also the depth method gives the total bits per pixel in the image not just the bits per
    // pixel in each channel.
    const int m_FITSBITPIX = 8;
    switch (m_FITSBITPIX)
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
            errMessage = QString("Bit depth %1 is not supported.").arg(m_FITSBITPIX);
            QMessageBox::critical(nullptr, "Message", errMessage);
            qCCritical(KSTARS_FITS) << errMessage;
            return false;
    }

    m_Statistics.width = static_cast<uint16_t>(imageFromFile.width());
    m_Statistics.height = static_cast<uint16_t>(imageFromFile.height());
    m_Statistics.channels = imageFromFile.isGrayscale() ? 1 : 3;
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

    if (m_Statistics.channels == 1)
    {
        memcpy(m_ImageBuffer, imageFromFile.bits(), m_ImageBufferSize);
    }
    else
    {

        auto debayered_buffer = reinterpret_cast<uint8_t *>(m_ImageBuffer);
        auto * original_bayered_buffer = reinterpret_cast<uint8_t *>(imageFromFile.bits());

        // Data in RGBA, with bytes in the order of B,G,R we need to copy them into 3 layers for FITS
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
    m_LastError = i18n("Unable to find dcraw and cjpeg. Please install the required tools to convert CR2/NEF to JPEG.");
    return false;
#else

    int ret = 0;
    // Creation of image processing object
    LibRaw RawProcessor;

    // Let us open the file/buffer
    if (buffer.isEmpty())
    {
        // Open file
        if ((ret = RawProcessor.open_file(m_Filename.toLocal8Bit().constData())) != LIBRAW_SUCCESS)
        {
            m_LastError = i18n("Cannot open file %1: %2", m_Filename, libraw_strerror(ret));
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
            m_LastError = i18n("Cannot open buffer: %1", libraw_strerror(ret));
            RawProcessor.recycle();
            return false;
        }

        m_Statistics.size = buffer.size();
    }

    // Let us unpack the thumbnail
    if ((ret = RawProcessor.unpack()) != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot unpack_thumb: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    if ((ret = RawProcessor.dcraw_process()) != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot dcraw_process: %1", libraw_strerror(ret));
        RawProcessor.recycle();
        return false;
    }

    libraw_processed_image_t *image = RawProcessor.dcraw_make_mem_image(&ret);
    if (ret != LIBRAW_SUCCESS)
    {
        m_LastError = i18n("Cannot load to memory: %1", libraw_strerror(ret));
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
            recordLastError(status);
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

        m_Filename = finalFileName;

        // Use open diskfile as it does not use extended file names which has problems opening
        // files with [ ] or ( ) in their names.
        fits_open_diskfile(&fptr, m_Filename.toLocal8Bit(), READONLY, &status);
        fits_movabs_hdu(fptr, 1, IMAGE_HDU, &status);

        return true;
    }

    // Read the image back into buffer in case we debyayed
    nelements = m_Statistics.samples_per_channel * m_Statistics.channels;

    /* close current file */
    if (fptr && fits_close_file(fptr, &status))
    {
        recordLastError(status);
        return false;
    }

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, QString("!%1").arg(newFilename).toLocal8Bit(), &status))
    {
        recordLastError(status);
        return status;
    }

    status = 0;

    fptr = new_fptr;

    // Create image
    long naxis = m_Statistics.channels == 1 ? 2 : 3;
    long naxes[3] = {m_Statistics.width, m_Statistics.height, naxis};
    // JM 2020-12-28: Here we to use bitpix values
    if (fits_create_img(fptr, m_FITSBITPIX, naxis, naxes, &status))
    {
        recordLastError(status);
        return false;
    }

    // Here we need to use the actual data type
    if (fits_write_img(fptr, m_Statistics.dataType, 1, nelements, m_ImageBuffer, &status))
    {
        recordLastError(status);
        return false;
    }

    /* Write keywords */

    // Minimum
    if (fits_update_key(fptr, TDOUBLE, "DATAMIN", &(m_Statistics.min), "Minimum value", &status))
    {
        recordLastError(status);
        return false;
    }

    // Maximum
    if (fits_update_key(fptr, TDOUBLE, "DATAMAX", &(m_Statistics.max), "Maximum value", &status))
    {
        recordLastError(status);
        return false;
    }

    // KStars Min, for 3 channels
    fits_write_key(fptr, TDOUBLE, "MIN1", &m_Statistics.min[0], "Min Channel 1", &status);
    if (channels() > 1)
    {
        fits_write_key(fptr, TDOUBLE, "MIN2", &m_Statistics.min[1], "Min Channel 2", &status);
        fits_write_key(fptr, TDOUBLE, "MIN3", &m_Statistics.min[2], "Min Channel 3", &status);
    }

    // KStars max, for 3 channels
    fits_write_key(fptr, TDOUBLE, "MAX1", &m_Statistics.max[0], "Max Channel 1", &status);
    if (channels() > 1)
    {
        fits_write_key(fptr, TDOUBLE, "MAX2", &m_Statistics.max[1], "Max Channel 2", &status);
        fits_write_key(fptr, TDOUBLE, "MAX3", &m_Statistics.max[2], "Max Channel 3", &status);
    }

    // Mean
    if (m_Statistics.mean[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "MEAN1", &m_Statistics.mean[0], "Mean Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "MEAN2", &m_Statistics.mean[1], "Mean Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "MEAN3", &m_Statistics.mean[2], "Mean Channel 3", &status);
        }
    }

    // Median
    if (m_Statistics.median[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "MEDIAN1", &m_Statistics.median[0], "Median Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "MEDIAN2", &m_Statistics.median[1], "Median Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "MEDIAN3", &m_Statistics.median[2], "Median Channel 3", &status);
        }
    }

    // Standard Deviation
    if (m_Statistics.stddev[0] > 0)
    {
        fits_write_key(fptr, TDOUBLE, "STDDEV1", &m_Statistics.stddev[0], "Standard Deviation Channel 1", &status);
        if (channels() > 1)
        {
            fits_write_key(fptr, TDOUBLE, "STDDEV2", &m_Statistics.stddev[1], "Standard Deviation Channel 2", &status);
            fits_write_key(fptr, TDOUBLE, "STDDEV3", &m_Statistics.stddev[2], "Standard Deviation Channel 3", &status);
        }
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
                strncpy(valueBuffer, value.toString().toLatin1().constData(), 256 - 1);
                fits_write_key(fptr, TSTRING, key.toLatin1().constData(), valueBuffer, comment, &status);
            }
        }
    }

    // ISO Date
    if (fits_write_date(fptr, &status))
    {
        recordLastError(status);
        return false;
    }

    QString history =
        QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        recordLastError(status);
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
    calculateMedian(refresh);

    // Try to read mean/median/stddev if in file
    if (refresh == false && fptr)
    {
        int status = 0;
        int nfound = 0;
        if (fits_read_key_dbl(fptr, "MEAN1", &m_Statistics.mean[0], nullptr, &status) == 0)
            nfound++;
        // NB. These could fail if missing, which is OK.
        fits_read_key_dbl(fptr, "MEAN2", & m_Statistics.mean[1], nullptr, &status);
        fits_read_key_dbl(fptr, "MEAN3", &m_Statistics.mean[2], nullptr, &status);

        status = 0;
        if (fits_read_key_dbl(fptr, "STDDEV1", &m_Statistics.stddev[0], nullptr, &status) == 0)
            nfound++;
        // NB. These could fail if missing, which is OK.
        fits_read_key_dbl(fptr, "STDDEV2", &m_Statistics.stddev[1], nullptr, &status);
        fits_read_key_dbl(fptr, "STDDEV3", &m_Statistics.stddev[2], nullptr, &status);

        // If all is OK, we're done
        if (nfound == 2)
            return;
    }

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

void FITSData::calculateMinMax(bool refresh)
{
    int status, nfound = 0;

    status = 0;

    // Only fetch from header if we have a single channel
    // Otherwise, calculate manually.
    if (fptr != nullptr && !refresh)
    {

        status = 0;

        if (fits_read_key_dbl(fptr, "DATAMIN", &(m_Statistics.min[0]), nullptr, &status) == 0)
            nfound++;
        else if (fits_read_key_dbl(fptr, "MIN1", &(m_Statistics.min[0]), nullptr, &status) == 0)
            nfound++;

        // NB. These could fail if missing, which is OK.
        fits_read_key_dbl(fptr, "MIN2", &m_Statistics.min[1], nullptr, &status);
        fits_read_key_dbl(fptr, "MIN3", &m_Statistics.min[2], nullptr, &status);

        status = 0;

        if (fits_read_key_dbl(fptr, "DATAMAX", &(m_Statistics.max[0]), nullptr, &status) == 0)
            nfound++;
        else if (fits_read_key_dbl(fptr, "MAX1", &(m_Statistics.max[0]), nullptr, &status) == 0)
            nfound++;

        // NB. These could fail if missing, which is OK.
        fits_read_key_dbl(fptr, "MAX2", &m_Statistics.max[1], nullptr, &status);
        fits_read_key_dbl(fptr, "MAX3", &m_Statistics.max[2], nullptr, &status);

        // If we found both keywords, no need to calculate them, unless they are both zeros
        if (nfound == 2 && !(m_Statistics.min[0] == 0 && m_Statistics.max[0] == 0))
            return;
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
}

void FITSData::calculateMedian(bool refresh)
{
    int status, nfound = 0;

    status = 0;

    // Only fetch from header if we have a single channel
    // Otherwise, calculate manually.
    if (fptr != nullptr && !refresh)
    {
        status = 0;
        if (fits_read_key_dbl(fptr, "MEDIAN1", &m_Statistics.median[0], nullptr, &status) == 0)
            nfound++;

        // NB. These could fail if missing, which is OK.
        fits_read_key_dbl(fptr, "MEDIAN2", &m_Statistics.median[1], nullptr, &status);
        fits_read_key_dbl(fptr, "MEDIAN3", &m_Statistics.median[2], nullptr, &status);

        if (nfound == 1)
            return;
    }

    m_Statistics.median[RED_CHANNEL] = 0;
    m_Statistics.median[GREEN_CHANNEL] = 0;
    m_Statistics.median[BLUE_CHANNEL] = 0;

    switch (m_Statistics.dataType)
    {
        case TBYTE:
            calculateMedian<uint8_t>();
            break;

        case TSHORT:
            calculateMedian<int16_t>();
            break;

        case TUSHORT:
            calculateMedian<uint16_t>();
            break;

        case TLONG:
            calculateMedian<int32_t>();
            break;

        case TULONG:
            calculateMedian<uint32_t>();
            break;

        case TFLOAT:
            calculateMedian<float>();
            break;

        case TLONGLONG:
            calculateMedian<int64_t>();
            break;

        case TDOUBLE:
            calculateMedian<double>();
            break;

        default:
            break;
    }
}

template <typename T>
void FITSData::calculateMedian()
{
    auto * buffer = reinterpret_cast<T *>(m_ImageBuffer);
    const uint32_t maxMedianSize = 500000;
    uint32_t medianSize = m_Statistics.samples_per_channel;
    uint8_t downsample = 1;
    if (medianSize > maxMedianSize)
    {
        downsample = (static_cast<double>(medianSize) / maxMedianSize) + 0.999;
        medianSize /= downsample;
    }
    std::vector<T> samples;
    samples.reserve(medianSize);

    for (uint8_t n = 0; n < m_Statistics.channels; n++)
    {
        auto *oneChannel = buffer + n * m_Statistics.samples_per_channel;
        for (uint32_t upto = 0; upto < m_Statistics.samples_per_channel; upto += downsample)
            samples.push_back(oneChannel[upto]);
        const uint32_t middle = samples.size() / 2;
        std::nth_element(samples.begin(), samples.begin() + middle, samples.end());
        m_Statistics.median[n] = samples[middle];
    }
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
    if (m_StarFindFuture.isRunning())
        m_StarFindFuture.waitForFinished();

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
                    //Just finds stars in the center 25% of the image.
                    const int w = getStatistics().width;
                    const int h = getStatistics().height;
                    QRect middle(static_cast<int>(w * 0.25), static_cast<int>(h * 0.25), w / 2, h / 2);
                    return detector->findSources(middle);
                }
            }
            m_StarFindFuture = detector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_GRADIENT:
        default:
        {
            QPointer<FITSGradientDetector> detector = new FITSGradientDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            m_StarFindFuture = detector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_CENTROID:
        {
#ifndef KSTARS_LITE
            QPointer<FITSCentroidDetector> detector = new FITSCentroidDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            // We need JMIndex calculated from histogram
            if (!isHistogramConstructed())
                constructHistogram();
            detector->configure("JMINDEX", m_JMIndex);
            m_StarFindFuture = detector->findSources(trackingBox);
            return m_StarFindFuture;
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
            m_StarFindFuture =  detector->findSources(trackingBox);
            return m_StarFindFuture;
        }

        case ALGORITHM_BAHTINOV:
        {
            QPointer<FITSBahtinovDetector> detector = new FITSBahtinovDetector(this);
            detector->setSettings(m_SourceExtractorSettings);
            detector->configure("NUMBER_OF_AVERAGE_ROWS", Options::focusMultiRowAverage());
            m_StarFindFuture = detector->findSources(trackingBox);
            return m_StarFindFuture;
        }
    }
}

int FITSData::filterStars(const float innerRadius, const float outerRadius)
{
    long const sqDiagonal = (long) this->width() * (long) this->width() / 4 + (long) this->height() * (long) this->height() / 4;
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

double FITSData::getEccentricity()
{
    if (starCenters.empty())
        return -1;
    std::vector<float> eccs;
    for (const auto &s : starCenters)
        eccs.push_back(s->ellipticity);
    int middle = eccs.size() / 2;
    std::nth_element(eccs.begin(), eccs.begin() + middle, eccs.end());
    float medianEllipticity = eccs[middle];

    // SEP gives us the ellipticity (flattening) directly.
    // To get the eccentricity, use this formula:
    // e = sqrt(ellipticity * (2 - ellipticity))
    // https://en.wikipedia.org/wiki/Eccentricity_(mathematics)#Ellipses
    const float eccentricity = sqrt(medianEllipticity * (2 - medianEllipticity));
    return eccentricity;
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

    emit dataChanged();
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
            if (!isHistogramConstructed())
                constructHistogram();

            T bufferVal                    = 0;

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
                        bufferVal = (image[index] - min[i]) / m_HistogramBinWidth[i];

                        if (bufferVal >= m_CumulativeFrequency[i].size())
                            bufferVal = m_CumulativeFrequency[i].size() - 1;

                        image[index] = qBound(min[i], static_cast<T>(round(coeff * m_CumulativeFrequency[i][bufferVal])), max[i]);
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
    if (m_WCSHandle != nullptr)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
    }

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        m_LastError = errmsg;
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &m_nwcs, &m_WCSHandle)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_LastError = QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]);
        return false;
    }

    free(header);

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_WCSHandle->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    cdfix(m_WCSHandle);
    if ((status = wcsset(m_WCSHandle)) != 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_LastError = QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        return false;
    }

    HasWCS = true;
#endif
#endif
    return HasWCS;
}

bool FITSData::loadWCS(bool extras)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)

    if (m_WCSState == Success)
    {
        qCWarning(KSTARS_FITS) << "WCS data already loaded";
        return true;
    }

    if (m_WCSHandle != nullptr)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
    }

    qCDebug(KSTARS_FITS) << "Started WCS Data Processing...";

    int status = 0;
    char * header;
    int nkeyrec, nreject, nwcs;

    if (fits_hdr2str(fptr, 1, nullptr, 0, &header, &nkeyrec, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        m_LastError = errmsg;
        m_WCSState = Failure;
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &nwcs, &m_WCSHandle)) != 0)
    {
        free(header);
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_LastError = QString("wcspih ERROR %1: %2.").arg(status).arg(wcshdr_errmsg[status]);
        m_WCSState = Failure;
        return false;
    }

    free(header);

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        m_WCSState = Failure;
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (m_WCSHandle->crpix[0] == 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_LastError = i18n("No world coordinate systems found.");
        m_WCSState = Failure;
        return false;
    }

    cdfix(m_WCSHandle);
    if ((status = wcsset(m_WCSHandle)) != 0)
    {
        wcsvfree(&m_nwcs, &m_WCSHandle);
        m_WCSHandle = nullptr;
        m_LastError = QString("wcsset error %1: %2.").arg(status).arg(wcs_errmsg[status]);
        m_WCSState = Failure;
        return false;
    }

    SkyPoint startPoint;
    SkyPoint endPoint;

    pixelToWCS(QPointF(0, 0), startPoint);
    pixelToWCS(QPointF(width() - 1, height() - 1), endPoint);

    findObjectsInImage(startPoint, endPoint);

    m_WCSState = Success;
    FullWCS = extras;
    HasWCS = true;

    qCDebug(KSTARS_FITS) << "Finished WCS Data processing...";

    return true;
#else
    return false;
#endif
}

bool FITSData::wcsToPixel(const SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint)
{
#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
    int status, stat[NWCSFIX];
    double imgcrd[NWCSFIX], phi, pixcrd[NWCSFIX], theta, worldcrd[NWCSFIX];

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    worldcrd[0] = wcsCoord.ra0().Degrees();
    worldcrd[1] = wcsCoord.dec0().Degrees();

    if ((status = wcss2p(m_WCSHandle, 1, 2, worldcrd, &phi, &theta, imgcrd, pixcrd, stat)) != 0)
    {
        m_LastError = QString("wcss2p error %1: %2.").arg(status).arg(wcs_errmsg[status]);
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
    int status, stat[NWCSFIX];
    double imgcrd[NWCSFIX], phi, pixcrd[NWCSFIX], theta, world[NWCSFIX];

    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    pixcrd[0] = wcsPixelPoint.x();
    pixcrd[1] = wcsPixelPoint.y();

    if ((status = wcsp2s(m_WCSHandle, 1, 2, pixcrd, imgcrd, &phi, &theta, world, stat)) != 0)
    {
        m_LastError = QString("wcsp2s error %1: %2.").arg(status).arg(wcs_errmsg[status]);
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
bool FITSData::findWCSBounds(double &minRA, double &maxRA, double &minDec, double &maxDec)
{
    if (m_WCSHandle == nullptr)
    {
        m_LastError = i18n("No world coordinate systems found.");
        return false;
    }

    maxRA  = -1000;
    minRA  = 1000;
    maxDec = -1000;
    minDec = 1000;

    auto updateMinMax = [&](int x, int y)
    {
        int stat[2];
        double imgcrd[2], phi, pixcrd[2], theta, world[2];

        pixcrd[0] = x;
        pixcrd[1] = y;

        if (wcsp2s(m_WCSHandle, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0]))
            return;

        minRA = std::min(minRA, world[0]);
        maxRA = std::max(maxRA, world[0]);
        minDec = std::min(minDec, world[1]);
        maxDec = std::max(maxDec, world[1]);
    };

    // Find min and max values from edges
    for (int y = 0; y < height(); y++)
    {
        updateMinMax(0, y);
        updateMinMax(width() - 1, y);
    }

    for (int x = 1; x < width() - 1; x++)
    {
        updateMinMax(x, 0);
        updateMinMax(x, height() - 1);
    }

    // Check if either pole is in the image
    SkyPoint NCP(0, 90);
    SkyPoint SCP(0, -90);
    QPointF pixelPoint, imagePoint, pPoint;
    if (wcsToPixel(NCP, pPoint, imagePoint))
    {
        if (pPoint.x() > 0 && pPoint.x() < width() && pPoint.y() > 0 && pPoint.y() < height())
        {
            maxDec = 90;
        }
    }
    if (wcsToPixel(SCP, pPoint, imagePoint))
    {
        if (pPoint.x() > 0 && pPoint.x() < width() && pPoint.y() > 0 && pPoint.y() < height())
        {
            minDec = -90;
        }
    }
    return true;
}
#endif

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
void FITSData::findObjectsInImage(SkyPoint startPoint, SkyPoint endPoint)
{
    if (KStarsData::Instance() == nullptr)
        return;

    int w = width();
    int h = height();
    QVariant date;
    KSNumbers * num = nullptr;

    if (getRecordValue("DATE-OBS", date))
    {
        QString tsString(date.toString());
        tsString = tsString.remove('\'').trimmed();
        // Add Zulu time to indicate UTC
        tsString += "Z";

        QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

        if (ts.isValid())
            num = new KSNumbers(KStarsDateTime(ts).djd());
    }

    //Set to current time if the above does not work.
    if (num == nullptr)
        num = new KSNumbers(KStarsData::Instance()->ut().djd());

    startPoint.updateCoordsNow(num);
    endPoint.updateCoordsNow(num);

    m_SkyObjects.clear();

    QList<SkyObject *> list = KStarsData::Instance()->skyComposite()->findObjectsInArea(startPoint, endPoint);
    list.erase(std::remove_if(list.begin(), list.end(), [](SkyObject * oneObject)
    {
        int type = oneObject->type();
        return (type == SkyObject::STAR || type == SkyObject::PLANET || type == SkyObject::ASTEROID ||
                type == SkyObject::COMET || type == SkyObject::SUPERNOVA || type == SkyObject::MOON ||
                type == SkyObject::SATELLITE);
    }), list.end());

    double world[2], phi, theta, imgcrd[2], pixcrd[2];
    int stat[2];
    for (auto &object : list)
    {
        world[0] = object->ra0().Degrees();
        world[1] = object->dec0().Degrees();

        if (wcss2p(m_WCSHandle, 1, 2, &world[0], &phi, &theta, &imgcrd[0], &pixcrd[0], &stat[0]) == 0)
        {
            //The X and Y are set to the found position if it does work.
            int x = pixcrd[0];
            int y = pixcrd[1];
            if (x > 0 && y > 0 && x < w && y < h)
                m_SkyObjects.append(new FITSSkyObject(object, x, y));
        }
    }

    delete (num);
}
#endif

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

bool FITSData::debayer(bool reload)
{
    if (reload)
    {
        int anynull = 0, status = 0;

        if (fits_read_img(fptr, m_Statistics.dataType, 1, m_Statistics.samples_per_channel, nullptr, m_ImageBuffer,
                          &anynull, &status))
        {
            //                char errmsg[512];
            //                fits_get_errstatus(status, errmsg);
            //                KSNotification::error(i18n("Error reading image: %1", QString(errmsg)), i18n("Debayer error"));
            return false;
        }
    }

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
    uint8_t * destinationBuffer = nullptr;

    try
    {
        destinationBuffer = new uint8_t[rgb_size];
    }
    catch (const std::bad_alloc &e)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what()), i18n("Debayer error"));
        return false;
    }

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
        try
        {
            m_ImageBuffer = new uint8_t[rgb_size];
        }
        catch (const std::bad_alloc &e)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what()), i18n("Debayer error"));
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
    uint8_t *destinationBuffer = nullptr;
    try
    {
        destinationBuffer = new uint8_t[rgb_size];
    }
    catch (const std::bad_alloc &e)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what()), i18n("Debayer error"));
        return false;
    }

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
        try
        {
            m_ImageBuffer = new uint8_t[rgb_size];
        }
        catch (const std::bad_alloc &e)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer: %1", e.what()), i18n("Debayer error"));
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
    return m_LastError;
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

    output = QDir(getTemporaryPath()).filePath(filename + ".fits");

    //This section sets up the FITS File
    fitsfile *fptr = nullptr;
    int status = 0;
    long  fpixel = 1, naxis = input.allGray() ? 2 : 3, nelements, exposure;
    long naxes[3] = { input.width(), input.height(), naxis == 3 ? 3 : 1 };
    char error_status[512] = {0};

    if (fits_create_file(&fptr, QString('!' + output).toLocal8Bit().data(), &status))
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

bool FITSData::injectWCS(double orientation, double ra, double dec, double pixscale, bool eastToTheRight)
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
    double secpix1 = eastToTheRight ? pixscale : -pixscale;
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

    m_WCSState = Idle;

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

    emit dataChanged();
}

void FITSData::constructHistogram()
{
    switch (m_Statistics.dataType)
    {
        case TBYTE:
            constructHistogramInternal<uint8_t>();
            break;

        case TSHORT:
            constructHistogramInternal<int16_t>();
            break;

        case TUSHORT:
            constructHistogramInternal<uint16_t>();
            break;

        case TLONG:
            constructHistogramInternal<int32_t>();
            break;

        case TULONG:
            constructHistogramInternal<uint32_t>();
            break;

        case TFLOAT:
            constructHistogramInternal<float>();
            break;

        case TLONGLONG:
            constructHistogramInternal<int64_t>();
            break;

        case TDOUBLE:
            constructHistogramInternal<double>();
            break;

        default:
            break;
    }
}

template <typename T> void FITSData::constructHistogramInternal()
{
    auto * const buffer = reinterpret_cast<T const *>(m_ImageBuffer);
    uint32_t samples = m_Statistics.width * m_Statistics.height;
    const uint32_t sampleBy = samples > 500000 ? samples / 500000 : 1;

    m_HistogramBinCount = qMax(0., qMin(m_Statistics.max[0] - m_Statistics.min[0], 256.0));
    if (m_HistogramBinCount <= 0)
        m_HistogramBinCount = 256;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        m_HistogramIntensity[n].fill(0, m_HistogramBinCount + 1);
        m_HistogramFrequency[n].fill(0, m_HistogramBinCount + 1);
        m_CumulativeFrequency[n].fill(0, m_HistogramBinCount + 1);
        m_HistogramBinWidth[n] = qMax(1.0, (m_Statistics.max[n] - m_Statistics.min[n]) / (m_HistogramBinCount - 1));
    }

    QVector<QFuture<void>> futures;

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            for (int i = 0; i < m_HistogramBinCount; i++)
                m_HistogramIntensity[n][i] = m_Statistics.min[n] + (m_HistogramBinWidth[n] * i);
        }));
    }

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t offset = n * samples;

            for (uint32_t i = 0; i < samples; i += sampleBy)
            {
                int32_t id = qMax(static_cast<T>(0), qMin(static_cast<T>(m_HistogramBinCount),
                                  static_cast<T>(rint((buffer[i + offset] - m_Statistics.min[n]) / m_HistogramBinWidth[n]))));
                m_HistogramFrequency[n][id] += sampleBy;
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            uint32_t accumulator = 0;
            for (int i = 0; i < m_HistogramBinCount; i++)
            {
                accumulator += m_HistogramFrequency[n][i];
                m_CumulativeFrequency[n].replace(i, accumulator);
            }
        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();

    futures.clear();

#if 0
    for (int n = 0; n < m_Statistics.channels; n++)
    {
        futures.append(QtConcurrent::run([ = ]()
        {
            double median[3] = {0};
            //const bool cutoffSpikes = ui->hideSaturated->isChecked();
            const uint32_t halfCumulative = m_CumulativeFrequency[n][m_HistogramBinCount - 1] / 2;

            // Find which bin contains the median.
            int median_bin = -1;
            for (int i = 0; i < m_HistogramBinCount; i++)
            {
                if (m_CumulativeFrequency[n][i] > halfCumulative)
                {
                    median_bin = i;
                    break;
                }
            }

            if (median_bin >= 0)
            {
                // The number of items in the median bin
                const uint32_t median_bin_size = m_HistogramFrequency[n][median_bin] / sampleBy;
                if (median_bin_size > 0)
                {
                    // The median is this element inside the sorted median_bin;
                    const uint32_t samples_before_median_bin = median_bin == 0 ? 0 : m_CumulativeFrequency[n][median_bin - 1];
                    uint32_t median_position = (halfCumulative - samples_before_median_bin) / sampleBy;

                    if (median_position >= median_bin_size)
                        median_position = median_bin_size - 1;
                    if (median_position >= 0 && median_position < median_bin_size)
                    {
                        // Fill a vector with the values in the median bin (sampled by sampleBy).
                        std::vector<T> median_bin_samples(median_bin_size);
                        uint32_t upto = 0;
                        const uint32_t offset = n * samples;
                        for (uint32_t i = 0; i < samples; i += sampleBy)
                        {
                            if (upto >= median_bin_size) break;
                            const int32_t id = rint((buffer[i + offset] - m_Statistics.min[n]) / m_HistogramBinWidth[n]);
                            if (id == median_bin)
                                median_bin_samples[upto++] = buffer[i + offset];
                        }
                        // Get the Nth value using N = the median position.
                        if (upto > 0)
                        {
                            if (median_position >= upto) median_position = upto - 1;
                            std::nth_element(median_bin_samples.begin(), median_bin_samples.begin() + median_position,
                                             median_bin_samples.begin() + upto);
                            median[n] = median_bin_samples[median_position];
                        }
                    }
                }
            }

            setMedian(median[n], n);

        }));
    }

    for (QFuture<void> future : futures)
        future.waitForFinished();
#endif

    // Custom index to indicate the overall contrast of the image
    if (m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount / 4] > 0)
        m_JMIndex = m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount / 8] / static_cast<double>
                    (m_CumulativeFrequency[RED_CHANNEL][m_HistogramBinCount /
                            4]);
    else
        m_JMIndex = 1;

    qCDebug(KSTARS_FITS) << "FITHistogram: JMIndex " << m_JMIndex;

    m_HistogramConstructed = true;
    emit histogramReady();
}

void FITSData::recordLastError(int errorCode)
{
    char fitsErrorMessage[512] = {0};
    fits_get_errstatus(errorCode, fitsErrorMessage);
    m_LastError = fitsErrorMessage;
}

double FITSData::getAverageMean() const
{
    if (m_Statistics.channels == 1)
        return m_Statistics.mean[0];
    else
        return (m_Statistics.mean[0] + m_Statistics.mean[1] + m_Statistics.mean[2]) / 3.0;
}

double FITSData::getAverageMedian() const
{
    if (m_Statistics.channels == 1)
        return m_Statistics.median[0];
    else
        return (m_Statistics.median[0] + m_Statistics.median[1] + m_Statistics.median[2]) / 3.0;
}

double FITSData::getAverageStdDev() const
{
    if (m_Statistics.channels == 1)
        return m_Statistics.stddev[0];
    else
        return (m_Statistics.stddev[0] + m_Statistics.stddev[1] + m_Statistics.stddev[2]) / 3.0;
}
