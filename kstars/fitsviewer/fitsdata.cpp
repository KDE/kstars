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
#include "skymapcomposite.h"
#include "kstarsdata.h"

#include <cmath>
#include <cstdlib>
#include <climits>
#include <float.h>

#include <QApplication>
#include <QStringList>
#include <QLocale>
#include <QFile>
#include <QTime>
#include <QProgressDialog>

#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
#include <wcshdr.h>
#include <wcsfix.h>
#endif
#endif

#include <KLocalizedString>

#include "auxiliary/ksnotification.h"
#include "ksutils.h"
#include "Options.h"

#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN	10
#define ZOOM_MAX	400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

const int MINIMUM_ROWS_PER_CENTER=3;

#define DIFFUSE_THRESHOLD       0.15

#define MAX_EDGE_LIMIT  10000
#define LOW_EDGE_CUTOFF_1   50
#define LOW_EDGE_CUTOFF_2   10
#define MINIMUM_EDGE_LIMIT  2
#define SMALL_SCALE_SQUARE  256

bool greaterThan(Edge *s1, Edge *s2)
{
    //return s1->width > s2->width;
    return s1->sum > s2->sum;
}

FITSData::FITSData(FITSMode fitsMode)
{
    channels = 0;
    wcs_coord    = NULL;
    fptr = NULL;
    maxHFRStar = NULL;
    tempFile  = false;
    starsSearched = false;
    HasWCS = false;
    HasDebayer=false;
    mode = fitsMode;
    channels=1;

    stats.bitpix=8;
    stats.ndim = 2;
    stats.bytesPerPixel=1;

    debayerParams.method  = DC1394_BAYER_METHOD_NEAREST;
    debayerParams.filter  = DC1394_COLOR_FILTER_RGGB;
    debayerParams.offsetX  = debayerParams.offsetY = 0;
}

FITSData::~FITSData()
{
    int status=0;

    clearImageBuffers();

    if (starCenters.count() > 0)
        qDeleteAll(starCenters);

    delete[] wcs_coord;

    if (objList.count() > 0)
        qDeleteAll(objList);

    if (fptr)
    {
        fits_close_file(fptr, &status);

        if (tempFile)
            QFile::remove(filename);

    }
}

bool FITSData::loadFITS (const QString &inFilename, bool silent)
{
    int status=0, anynull=0;
    long naxes[3];
    char error_status[512];
    QString errMessage;

    qDeleteAll(starCenters);
    starCenters.clear();

    if (fptr)
    {
        fits_close_file(fptr, &status);

        if (tempFile)
            QFile::remove(filename);
    }

    filename = inFilename;

    if (filename.startsWith("/tmp/") || filename.contains("/Temp"))
        tempFile = true;
    else
        tempFile = false;

    if (fits_open_image(&fptr, filename.toLatin1(), READONLY, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        errMessage = i18n("Could not open file %1. Error %2", filename, QString::fromUtf8(error_status));
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
    }

    if (fits_get_img_param(fptr, 3, &(stats.bitpix), &(stats.ndim), naxes, &status))
    {
        fits_report_error(stderr, status);
        fits_get_errstatus(status, error_status);
        errMessage = i18n("FITS file open error (fits_get_img_param): %1", QString::fromUtf8(error_status));
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
    }

    if (stats.ndim < 2)
    {
        errMessage = i18n("1D FITS images are not supported in KStars.");
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
    }

    switch (stats.bitpix)
    {
    case BYTE_IMG:
        data_type = TBYTE;
        stats.bytesPerPixel = sizeof(uint8_t);
        break;
    case SHORT_IMG:
        // Read SHORT image as USHORT
        data_type = TUSHORT;
        stats.bytesPerPixel = sizeof(int16_t);
        break;
    case USHORT_IMG:
        data_type = TUSHORT;
        stats.bytesPerPixel = sizeof(uint16_t);
        break;
    case LONG_IMG:
        // Read LONG image as ULONG
        data_type = TULONG;
        stats.bytesPerPixel = sizeof(int32_t);
        break;
    case ULONG_IMG:
        data_type = TULONG;
        stats.bytesPerPixel = sizeof(uint32_t);
        break;
    case FLOAT_IMG:
        data_type = TFLOAT;
        stats.bytesPerPixel = sizeof(float);
        break;
    case LONGLONG_IMG:
        data_type = TLONGLONG;
        stats.bytesPerPixel = sizeof(int64_t);
        break;
    case DOUBLE_IMG:
        data_type = TDOUBLE;
        stats.bytesPerPixel = sizeof(double);
    default:
        errMessage = i18n("Bit depth %1 is not supported.", stats.bitpix);
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
        break;
    }

    if (stats.ndim < 3)
        naxes[2] = 1;

    if (naxes[0] == 0 || naxes[1] == 0)
    {
        errMessage = i18n("Image has invalid dimensions %1x%2", naxes[0], naxes[1]);
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
    }

    stats.width  = naxes[0];
    stats.height = naxes[1];
    stats.samples_per_channel   = stats.width*stats.height;

    clearImageBuffers();

    channels = naxes[2];

    // Channels always set to #1 if we are not required to process 3D Cubes
    if (Options::auto3DCube() == false)
        channels = 1;

    //image_buffer = new float[stats.samples_per_channel * channels];
    imageBuffer = new uint8_t[stats.samples_per_channel * channels * stats.bytesPerPixel];
    //if (image_buffer == NULL)
    if (imageBuffer == NULL)
    {
        qDebug() << "FITSData: Not enough memory for image_buffer channel. Requested: " << stats.samples_per_channel * channels * stats.bytesPerPixel << " bytes.";
        clearImageBuffers();
        return false;
    }

    rotCounter=0;
    flipHCounter=0;
    flipVCounter=0;
    long nelements = stats.samples_per_channel * channels;

    if (fits_read_img(fptr, data_type, 1, nelements, 0, imageBuffer, &anynull, &status))
    {
        char errmsg[512];
        fits_get_errstatus(status, errmsg);
        errMessage = i18n("Error reading image: %1", QString(errmsg));
        if (silent == false)
            KSNotification::error(errMessage, i18n("FITS Open"));
        fits_report_error(stderr, status);
        if (Options::fITSLogging())
            qDebug() << errMessage;
        return false;
    }

    calculateStats();

    if (Options::autoDebayerFITS() && checkDebayer())
    {
        bayerBuffer = imageBuffer;
        debayer();
    }

    starsSearched = false;

    return true;

}

int FITSData::saveFITS( const QString &newFilename )
{
    if (newFilename == filename)
        return 0;

    int status=0, exttype=0;
    long nelements;
    fitsfile *new_fptr;

    if (HasDebayer)
    {
        /* close current file */
        if (fits_close_file(fptr, &status))
        {
            fits_report_error(stderr, status);
            return status;
        }

        // Skip "!" in the beginning of the new file name
        QFile::copy(filename, newFilename.mid(1));

        if (tempFile)
        {
            QFile::remove(filename);
            tempFile = false;
        }

        filename = newFilename;

        fits_open_image(&fptr, filename.toLatin1(), READONLY, &status);

        return 0;
    }

    nelements = stats.samples_per_channel * channels;

    /* Create a new File, overwriting existing*/
    if (fits_create_file(&new_fptr, newFilename.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    if (fits_copy_file(fptr, new_fptr, 1, 1, 1, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* close current file */
    if (fits_close_file(fptr, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    status=0;

    if (tempFile)
    {
        QFile::remove(filename);
        tempFile = false;
    }

    filename = newFilename;

    fptr = new_fptr;

    if (fits_movabs_hdu(fptr, 1, &exttype, &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    /* Write Data */
    if (fits_write_img(fptr, data_type, 1, nelements, imageBuffer, &status))
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

    QString history = QString("Modified by KStars on %1").arg(QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss"));
    // History
    if (fits_write_history(fptr, history.toLatin1(), &status))
    {
        fits_report_error(stderr, status);
        return status;
    }

    int rot=0, mirror=0;
    if (rotCounter > 0)
    {
        rot = (90 * rotCounter) % 360;
        if (rot < 0)
            rot += 360;
    }
    if (flipHCounter %2 !=0 || flipVCounter % 2 != 0)
        mirror = 1;

    if (rot || mirror)
        rotWCSFITS(rot, mirror);

    rotCounter=flipHCounter=flipVCounter=0;

    return status;
}

void FITSData::clearImageBuffers()
{
    delete[] imageBuffer;
    imageBuffer=NULL;
    bayerBuffer=NULL;
}

void FITSData::calculateStats(bool refresh)
{
    // Calculate min max
    calculateMinMax(refresh);

    // Get standard deviation and mean in one run
    switch (data_type)
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

    stats.SNR = stats.mean[0] / stats.stddev[0];

    if (refresh && markStars)
        // Let's try to find star positions again after transformation
        starsSearched = false;

}
int FITSData::calculateMinMax(bool refresh)
{
    int status, nfound=0;

    status = 0;

    if (fptr && refresh == false)
    {
        if (fits_read_key_dbl(fptr, "DATAMIN", &(stats.min[0]), NULL, &status) ==0)
            nfound++;

        if (fits_read_key_dbl(fptr, "DATAMAX", &(stats.max[0]), NULL, &status) == 0)
            nfound++;

        // If we found both keywords, no need to calculate them, unless they are both zeros
        if (nfound == 2 && !(stats.min[0] == 0 && stats.max[0] ==0))
            return 0;
    }

    stats.min[0]= 1.0E30;
    stats.max[0]= -1.0E30;

    stats.min[1]= 1.0E30;
    stats.max[1]= -1.0E30;

    stats.min[2]= 1.0E30;
    stats.max[2]= -1.0E30;

    switch (data_type)
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

template<typename T> void FITSData::calculateMinMax()
{
    T *buffer = reinterpret_cast<T*>(imageBuffer);

    if (channels == 1)
    {
        for (unsigned int i=0; i < stats.samples_per_channel; i++)
        {
            if (buffer[i] < stats.min[0]) stats.min[0] = buffer[i];
            else if (buffer[i] > stats.max[0]) stats.max[0] = buffer[i];
        }
    }
    else
    {
        int g_offset = stats.samples_per_channel;
        int b_offset = stats.samples_per_channel*2;

        for (unsigned int i=0; i < stats.samples_per_channel; i++)
        {
            if (buffer[i] < stats.min[0])
                stats.min[0] = buffer[i];
            else if (buffer[i] > stats.max[0])
                stats.max[0] = buffer[i];

            if (buffer[i+g_offset] < stats.min[1])
                stats.min[1] = buffer[i+g_offset];
            else if (buffer[i+g_offset] > stats.max[1])
                stats.max[1] = buffer[i+g_offset];

            if (buffer[i+b_offset] < stats.min[2])
                stats.min[2] = buffer[i+b_offset];
            else if (buffer[i+b_offset] > stats.max[2])
                stats.max[2] = buffer[i+b_offset];
        }
    }
}

template<typename T> void FITSData::runningAverageStdDev()
{
    T *buffer = reinterpret_cast<T*>(imageBuffer);

    int m_n = 2;
    double m_oldM=0, m_newM=0, m_oldS=0, m_newS=0;
    m_oldM = m_newM = buffer[0];

    for (unsigned int i=1; i < stats.samples_per_channel; i++)
    {
        m_newM = m_oldM + (buffer[i] - m_oldM)/m_n;
        m_newS = m_oldS + (buffer[i] - m_oldM) * (buffer[i] - m_newM);

        m_oldM = m_newM;
        m_oldS = m_newS;
        m_n++;
    }

    double variance = m_newS / (m_n -2);

    stats.mean[0] = m_newM;
    stats.stddev[0]  = sqrt(variance);
}

void FITSData::setMinMax(double newMin,  double newMax, uint8_t channel)
{
    stats.min[channel] = newMin;
    stats.max[channel] = newMax;
}

int FITSData::getFITSRecord(QString &recordList, int &nkeys)
{
    char *header;
    int status=0;

    if (fits_hdr2str(fptr, 0, NULL, 0, &header, &nkeys, &status))
    {
        fits_report_error(stderr, status);
        free(header);
        return -1;
    }

    recordList = QString(header);

    free(header);

    return 0;
}

bool FITSData::checkCollision(Edge* s1, Edge*s2)
{
    int dis; //distance

    int diff_x=s1->x - s2->x;
    int diff_y=s1->y - s2->y;

    dis = std::abs( sqrt( diff_x*diff_x + diff_y*diff_y));
    dis -= s1->width/2;
    dis -= s2->width/2;

    if (dis<=0) //collision
        return true;

    //no collision
    return false;
}

int FITSData::findCannyStar(FITSData *data, const QRect &boundary)
{
    switch (data->getDataType())
    {
        case TBYTE:
            return FITSData::findCannyStar<uint8_t>(data, boundary);
            break;

        case TSHORT:
            return FITSData::findCannyStar<int16_t>(data, boundary);
            break;

        case TUSHORT:
            return FITSData::findCannyStar<uint16_t>(data, boundary);
            break;

        case TLONG:
            return FITSData::findCannyStar<int32_t>(data, boundary);
            break;

        case TULONG:
            return FITSData::findCannyStar<uint16_t>(data, boundary);
            break;

        case TFLOAT:
            return FITSData::findCannyStar<float>(data, boundary);
            break;

        case TLONGLONG:
            return FITSData::findCannyStar<int64_t>(data, boundary);
            break;

        case TDOUBLE:
        return FITSData::findCannyStar<double>(data, boundary);
        break;

        default:
        return 0;
    }

    return 0;
}

template<typename T> int FITSData::findCannyStar(FITSData *data, const QRect &boundary)
{
    int subX = qMax(0, boundary.isNull() ? 0 : boundary.x());
    int subY = qMax(0, boundary.isNull() ? 0 : boundary.y());
    int subW = (boundary.isNull() ? data->getWidth() : boundary.width());
    int subH = (boundary.isNull() ? data->getHeight(): boundary.height());

    int BBP =  data->getBytesPerPixel();

    uint16_t dataWidth = data->getWidth();

    // #1 Find offsets
    uint32_t size   = subW * subH;
    uint32_t offset = subX + subY * dataWidth;

    // #2 Create new buffer
    uint8_t *buffer = new uint8_t[size*BBP];
    // If there is no offset, copy whole buffer in one go
    if (offset == 0)
        memcpy(buffer, data->getImageBuffer(), size*BBP);
    else
    {
        uint8_t *dataPtr     = buffer;
        uint8_t *origDataPtr = data->getImageBuffer();
        uint32_t lineOffset  = 0;
        // Copy data line by line
        for (int height=subY; height < (subY+subH); height++)
        {
            lineOffset = (subX + height * dataWidth) * BBP;
            memcpy(dataPtr, origDataPtr + lineOffset, subW * BBP);
            dataPtr += (subW * BBP);
        }
    }

    // #3 Create new FITSData to hold it
    FITSData *boundedImage = new FITSData();
    boundedImage->stats.width = subW;
    boundedImage->stats.height = subH;
    boundedImage->stats.bitpix = data->stats.bitpix;
    boundedImage->stats.bytesPerPixel = data->stats.bytesPerPixel;
    boundedImage->stats.samples_per_channel = size;
    boundedImage->stats.ndim = 2;

    boundedImage->setDataType(data->getDataType());

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

    //QVector<float> thresholded = boundedImage->threshold(boundedImage->stats.mean[0], boundedImage->stats.max[0], gradients);

    // Not needed anymore
    delete boundedImage;

    if (maxID == 0)
        return 0;

    typedef struct
    {
        float massX=0;
        float massY=0;
        float totalMass=0;
    } massInfo;

    QMap<int, massInfo> masses;

    // #7 Calculate center of mass for all detected regions
    for (int y=0; y < subH; y++)
    {
        for (int x=0; x < subW; x++)
        {
            int index = x+y*subW;

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
    int maxRegionID=1;
    int maxTotalMass=masses[1].totalMass;
    double totalMassRatio=1e6;
    for (auto key : masses.keys())
    {
        massInfo oneMass = masses.value(key);
        if (oneMass.totalMass > maxTotalMass)
        {
            totalMassRatio = oneMass.totalMass / maxTotalMass;
            maxTotalMass = oneMass.totalMass;
            maxRegionID = key;
        }
    }

    // If image has many regions and there is no significant relative center of mass then it's just noise and no stars
    // are probably there above a useful threshold.
    if (maxID > 10 && totalMassRatio < 1.5)
        return 0;

    Edge *center = new Edge;
    center->width = -1;
    center->x     = masses[maxRegionID].massX / masses[maxRegionID].totalMass + 0.5;
    center->y     = masses[maxRegionID].massY / masses[maxRegionID].totalMass + 0.5;
    center->HFR   = 1;

    // Maximum Radius
    int maxR = qMin(subW-1, subH-1) / 2;

    for (int r=maxR; r > 1; r--)
    {
        int pass=0;

        for (float theta=0; theta < 2*M_PI; theta += (2*M_PI)/36.0)
        {
            int testX = center->x + cos(theta) * r;
            int testY = center->y + sin(theta) * r;

            // if out of bound, break;
            if (testX < 0 || testX >= subW || testY < 0 || testY >= subH)
                break;

            if (gradients[testX + testY * subW] > 0)
                //if (thresholded[testX + testY * subW] > 0)
            {
                if (++pass >= 24)
                {
                    center->width = r*2;
                    // Break of outer loop
                    r=0;
                    break;
                }
            }
        }
    }

    if (Options::fITSLogging())
        qDebug() << "FITS: Weighted Center is X: " << center->x << " Y: " << center->y << " Width: " << center->width;

    // If no stars were detected
    if (center->width == -1)
    {
        delete center;
        return 0;
    }

    // 30% fuzzy
    //center->width += center->width*0.3 * (running_threshold / threshold);

    double FSum=0, HF=0, TF=0;
    const double resolution = 1.0/20.0;

    int cen_y = round(center->y);

    double rightEdge = center->x + center->width / 2.0;
    double leftEdge  = center->x - center->width / 2.0;

    QVector<double> subPixels;
    subPixels.reserve(center->width / resolution);

    const T *origBuffer = reinterpret_cast<T*>(data->getImageBuffer()) + offset;

    QDebug deb = qDebug();

    for (int i=0; i < subW; i++)
        deb << origBuffer[i + cen_y * dataWidth] << ",";

    for (double x=leftEdge; x <= rightEdge; x += resolution)
    {
        double slice = resolution * (origBuffer[static_cast<int>(floor(x)) + cen_y * dataWidth]);
        FSum += slice;
        subPixels.append(slice);
    }

    // Half flux
    HF = FSum / 2.0;

    int subPixelCenter = (center->width / resolution) / 2;

    // Start from center
    TF = subPixels[subPixelCenter];
    double lastTF = TF;
    // Integrate flux along radius axis until we reach half flux
    //for (double k=resolution; k < (center->width/(2*resolution)); k += resolution)
    for (int k=1; k < subPixelCenter; k ++)
    {
        TF += subPixels[subPixelCenter+k];
        TF += subPixels[subPixelCenter-k];

        if (TF >= HF)
        {
            // We overpassed HF, let's calculate from last TF how much until we reach HF

            // #1 Accurate calculation, but very sensitive to small variations of flux
            center->HFR = (k - 1 + ( (HF-lastTF)/(TF-lastTF) )*2 ) * resolution;

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

    if (Options::fITSLogging())
        qDebug() << "Flux: " << FSum << " Half-Flux: " << HF << " HFR: " << center->HFR;

    return 1;
}

int FITSData::findOneStar(const QRectF &boundary)
{
    switch (data_type)
    {
        case TBYTE:
            return findOneStar<uint8_t>(boundary);
            break;

        case TSHORT:
            return findOneStar<int16_t>(boundary);
            break;

        case TUSHORT:
            return findOneStar<uint16_t>(boundary);
            break;

        case TLONG:
            return findOneStar<int32_t>(boundary);
            break;

        case TULONG:
            return findOneStar<uint32_t>(boundary);
            break;

        case TFLOAT:
            return findOneStar<float>(boundary);
            break;

        case TLONGLONG:
            return findOneStar<int64_t>(boundary);
            break;

        case TDOUBLE:
            return findOneStar<double>(boundary);
        break;

        default:
        break;
    }

    return 0;
}

template<typename T> int FITSData::findOneStar(const QRectF &boundary)
{
    int subX = boundary.x();
    int subY = boundary.y();
    int subW = subX + boundary.width();
    int subH = subY + boundary.height();

    float massX=0, massY=0, totalMass=0;

    T* buffer = reinterpret_cast<T*>(imageBuffer);

    // TODO replace magic number with something more useful to understand
    double threshold = stats.mean[0] * Options::focusThreshold()/100.0;

    for (int y=subY; y < subH; y++)
    {
        for (int x=subX; x < subW; x++)
        {
            T pixel = buffer[x+y*stats.width];
            if (pixel > threshold)
            {
                totalMass += pixel;
                massX     += x * pixel;
                massY     += y * pixel;
            }
        }
    }

    qDebug() << "Weighted Center is X: " << massX/totalMass << " Y: " << massY/totalMass;

    Edge *center = new Edge;
    center->width = -1;
    center->x     = massX/totalMass + 0.5;
    center->y     = massY/totalMass + 0.5;
    center->HFR   = 1;

    // Maximum Radius
    int maxR = qMin(subW-1, subH-1) / 2;

    // Critical threshold
    double critical_threshold = threshold * 0.7;
    double running_threshold = threshold;

    while (running_threshold >= critical_threshold)
    {
        for (int r=maxR; r > 1; r--)
        {
            int pass=0;

            for (float theta=0; theta < 2*M_PI; theta += (2*M_PI)/10.0)
            {
                int testX = center->x + cos(theta) * r;
                int testY = center->y + sin(theta) * r;

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
                center->width = r*2;
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
        return 0;

    // 30% fuzzy
    //center->width += center->width*0.3 * (running_threshold / threshold);

    starCenters.append(center);

    double FSum=0, HF=0, TF=0, min = stats.min[0];
    const double resolution = 1.0/20.0;

    int cen_y = round(center->y);

    double rightEdge = center->x + center->width / 2.0;
    double leftEdge  = center->x - center->width / 2.0;

    QVector<double> subPixels;
    subPixels.reserve(center->width / resolution);

    for (double x=leftEdge; x <= rightEdge; x += resolution)
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
    TF = subPixels[subPixelCenter];
    double lastTF = TF;
    // Integrate flux along radius axis until we reach half flux
    //for (double k=resolution; k < (center->width/(2*resolution)); k += resolution)
    for (int k=1; k < subPixelCenter; k ++)
    {
        TF += subPixels[subPixelCenter+k];
        TF += subPixels[subPixelCenter-k];

        if (TF >= HF)
        {
            // We have two ways to calculate HFR. The first is the correct method but it can get quite variable within 10% due to random fluctuations of the measured star.
            // The second method is not truly HFR but is much more resistant to noise.


            // #1 Approximate HFR, accurate and reliable but quite variable to small changes in star flux
            center->HFR = (k - 1 + ( (HF-lastTF)/(TF-lastTF) )*2 ) * resolution;

            // #2 Not exactly HFR, but much more stable
            //center->HFR = (k*resolution) * (HF/TF);
            break;
        }

        lastTF = TF;
    }

    return 1;
}

/*** Find center of stars and calculate Half Flux Radius */
void FITSData::findCentroid(const QRectF &boundary, int initStdDev, int minEdgeWidth)
{
    switch (data_type)
    {
        case TBYTE:
            findCentroid<uint8_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TSHORT:
            findCentroid<int16_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TUSHORT:
            findCentroid<uint16_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TLONG:
            findCentroid<int32_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TULONG:
            findCentroid<uint32_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TFLOAT:
            findCentroid<float>(boundary, initStdDev, minEdgeWidth);
            break;

        case TLONGLONG:
            findCentroid<int64_t>(boundary, initStdDev, minEdgeWidth);
            break;

        case TDOUBLE:
            findCentroid<double>(boundary, initStdDev, minEdgeWidth);
        break;

        default:
        return;
    }
}

template<typename T> void FITSData::findCentroid(const QRectF &boundary, int initStdDev, int minEdgeWidth)
{    
    double threshold=0,sum=0,avg=0,min=0;
    int starDiameter=0;
    int pixVal=0;
    int minimumEdgeCount = MINIMUM_EDGE_LIMIT;

    T *buffer = reinterpret_cast<T*>(imageBuffer);

    double JMIndex = 100;
    #ifndef KSTARS_LITE
    if (histogram)
        JMIndex = histogram->getJMIndex();
    #endif

    float dispersion_ratio=1.5;

    QList<Edge*> edges;

    if (JMIndex < DIFFUSE_THRESHOLD)
    {
        minEdgeWidth = JMIndex*35+1;
        minimumEdgeCount=minEdgeWidth-1;
    }
    else
    {
        minEdgeWidth =6;
        minimumEdgeCount=4;
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
            threshold = stats.max[0] - stats.mean[0] * ( (MINIMUM_STDVAR - initStdDev)*0.5 +1);

            min =stats.min[0];
            if (threshold-min < 0)
            {
                threshold=stats.mean[0]* ( (MINIMUM_STDVAR - initStdDev)*0.5 +1);
                min=0;
            }

            dispersion_ratio=1.4 - (MINIMUM_STDVAR - initStdDev)*0.08;
        }
        else
        {
            threshold = stats.mean[0]+stats.stddev[0]*initStdDev*(0.3 - (MINIMUM_STDVAR - initStdDev) * 0.05);
            min = stats.min[0];
            // Ratio between centeroid center and edge
            dispersion_ratio=1.8 - (MINIMUM_STDVAR - initStdDev)*0.2;
        }

        if (Options::fITSLogging())
        {
            qDebug() << "SNR: " << stats.SNR;
            qDebug() << "The threshold level is " << threshold << "(actual " << threshold-min << ")  minimum edge width" << minEdgeWidth << " minimum edge limit " << minimumEdgeCount;
        }

        threshold -= min;

        int subX, subY, subW, subH;

        if (boundary.isNull())
        {
            if (mode == FITS_GUIDE || mode == FITS_FOCUS)
            {
                subX = round(stats.width/15.0);
                subY = round(stats.height/15.0);
                subW = stats.width - subX;
                subH = stats.height - subY;
            }
            else
            {
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
        for (int i=subY; i < subH; i++)
        {
            starDiameter = 0;

            for(int j=subX; j < subW; j++)
            {
                pixVal = buffer[j+(i*stats.width)] - min;

                // If pixel value > threshold, let's get its weighted average
                if ( pixVal >= threshold )
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
                        float center = avg/sum + 0.5;
                        if (center > 0)
                        {
                            int i_center = floor(center);

                            // Check if center is 10% or more brighter than edge, if not skip
                            if ( ((buffer[i_center+(i*stats.width)]-min) / (buffer[i_center+(i*stats.width)-starDiameter/2]-min) >= dispersion_ratio) &&
                                 ((buffer[i_center+(i*stats.width)]-min) / (buffer[i_center+(i*stats.width)+starDiameter/2]-min) >= dispersion_ratio))
                            {
                                if (Options::fITSLogging())
                                {
                                    qDebug() << "Edge center is " << buffer[i_center+(i*stats.width)]-min << " Edge is " << buffer[i_center+(i*stats.width)-starDiameter/2]-min
                                             << " and ratio is " << ((buffer[i_center+(i*stats.width)]-min) / (buffer[i_center+(i*stats.width)-starDiameter/2]-min))
                                            << " located at X: " << center << " Y: " << i+0.5;
                                }

                                Edge *newEdge = new Edge();

                                newEdge->x          = center;
                                newEdge->y          = i + 0.5;
                                newEdge->scanned    = 0;
                                newEdge->val        = buffer[i_center+(i*stats.width)] - min;
                                newEdge->width      = starDiameter;
                                newEdge->HFR        = 0;
                                newEdge->sum        = sum;

                                edges.append(newEdge);

                            }
                        }
                    }

                    // Reset
                    avg= sum = starDiameter=0;
                }
            }
        }

        if (Options::fITSLogging())
            qDebug() << "Total number of edges found is: " << edges.count();

        // In case of hot pixels
        if (edges.count() == 1 && initStdDev > 1)
        {
            initStdDev--;
            continue;
        }

        if (edges.count() >= MAX_EDGE_LIMIT)
        {
            if (Options::fITSLogging())
                qDebug() << "Too many edges, aborting... " << edges.count();

            qDeleteAll(edges);
            return;
        }

        if (edges.count() >= minimumEdgeCount)
            break;

        qDeleteAll(edges);
        edges.clear();
        initStdDev--;
    }

    int cen_count=0;
    int cen_x=0;
    int cen_y=0;
    int cen_v=0;
    int cen_w=0;
    int width_sum=0;

    // Let's sort edges, starting with widest
    qSort(edges.begin(), edges.end(), greaterThan);

    // Now, let's scan the edges and find the maximum centroid vertically
    for (int i=0; i < edges.count(); i++)
    {
        if (Options::fITSLogging())
        {
            qDebug() << "# " << i << " Edge at (" << edges[i]->x << "," << edges[i]->y << ") With a value of " << edges[i]->val  << " and width of "
                     << edges[i]->width << " pixels. with sum " << edges[i]->sum;
        }

        // If edge scanned already, skip
        if (edges[i]->scanned == 1)
        {
            if (Options::fITSLogging())
                qDebug() << "Skipping check for center " << i << " because it was already counted";
            continue;
        }

        if (Options::fITSLogging())
            qDebug() << "Invetigating edge # " << i << " now ...";

        // Get X, Y, and Val of edge
        cen_x = edges[i]->x;
        cen_y = edges[i]->y;
        cen_v = edges[i]->sum;
        cen_w = edges[i]->width;

        float avg_x = 0;
        float avg_y = 0;

        sum = 0;
        cen_count=0;

        // Now let's compare to other edges until we hit a maxima
        for (int j=0; j < edges.count();j++)
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

        if (Options::fITSLogging())
            qDebug() << "center_count: " << cen_count << " and initstdDev= " << initStdDev << " and limit is " << cen_limit;

        if (cen_limit < 1)
            continue;

        // If centroid count is within acceptable range
        //if (cen_limit >= 2 && cen_count >= cen_limit)
        if (cen_count >= cen_limit)
        {
            // We detected a centroid, let's init it
            Edge *rCenter = new Edge();

            rCenter->x = avg_x/sum;
            rCenter->y = avg_y/sum;
            width_sum += rCenter->width;
            rCenter->width = cen_w;

            if (Options::fITSLogging())
                qDebug() << "Found a real center with number with (" << rCenter->x << "," << rCenter->y << ")";

            // Calculate Total Flux From Center, Half Flux, Full Summation
            double TF=0;
            double HF=0;
            double FSum=0;

            cen_x = (int) floor(rCenter->x);
            cen_y = (int) floor(rCenter->y);

            if (cen_x < 0 || cen_x > stats.width || cen_y < 0 || cen_y > stats.height)
                continue;


            // Complete sum along the radius
            //for (int k=0; k < rCenter->width; k++)
            for (int k=rCenter->width/2; k >= -(rCenter->width/2) ; k--)
            {
                FSum += buffer[cen_x-k+(cen_y*stats.width)] - min;
                //qDebug() << image_buffer[cen_x-k+(cen_y*stats.width)] - min;
            }

            // Half flux
            HF = FSum / 2.0;

            // Total flux starting from center
            TF = buffer[cen_y * stats.width + cen_x] - min;

            int pixelCounter = 1;

            // Integrate flux along radius axis until we reach half flux
            for (int k=1; k < rCenter->width/2; k++)
            {
                if (TF >= HF)
                {
                    if (Options::fITSLogging())
                        qDebug() << "Stopping at TF " << TF << " after #" << k << " pixels.";
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

            if (Options::fITSLogging())
                qDebug() << "HFR for this center is " << rCenter->HFR << " pixels and the total flux is " << FSum;

            starCenters.append(rCenter);
        }
    }

    if (starCenters.count() > 1 && mode != FITS_FOCUS)
    {
        float width_avg = width_sum / starCenters.count();

        float lsum =0, sdev=0;

        foreach(Edge *center, starCenters)
            lsum += (center->width - width_avg) * (center->width - width_avg);

        sdev = (sqrt(lsum/(starCenters.count() - 1))) * 4;

        // Reject stars > 4 * stddev
        foreach(Edge *center, starCenters)
            if (center->width > sdev)
                starCenters.removeOne(center);

        //foreach(Edge *center, starCenters)
        //qDebug() << center->x << "," << center->y << "," << center->width << "," << center->val << endl;

    }

    // Release memory
    qDeleteAll(edges);

}

double FITSData::getHFR(HFRType type)
{
    // This method is less susceptible to noise
    // Get HFR for the brightest star only, instead of averaging all stars
    // It is more consistent.
    // TODO: Try to test this under using a real CCD.

    if (starCenters.size() == 0)
        return -1;

    if (type == HFR_MAX)
    {
        maxHFRStar = NULL;
        int maxVal=0;
        int maxIndex=0;
        for (int i=0; i < starCenters.count() ; i++)
        {
            if (starCenters[i]->val > maxVal)
            {
                maxIndex=i;
                maxVal = starCenters[i]->val;

            }
        }

        maxHFRStar = starCenters[maxIndex];
        return starCenters[maxIndex]->HFR;
    }

    double FSum=0;
    double avgHFR=0;

    // Weighted average HFR
    for (int i=0; i < starCenters.count() ; i++)
    {
        avgHFR += starCenters[i]->val * starCenters[i]->HFR;
        FSum   += starCenters[i]->val;
    }

    if (FSum != 0)
    {
        //qDebug() << "Average HFR is " << avgHFR / FSum << endl;
        return (avgHFR / FSum);
    }
    else
        return -1;

}

double FITSData::getHFR(int x, int y)
{
    if (starCenters.size() == 0)
        return -1;

    for (int i=0; i < starCenters.count() ; i++)
    {
        if (fabs(starCenters[i]->x-x) <= starCenters[i]->width/2 && fabs(starCenters[i]->y-y) <= starCenters[i]->width/2)
        {
            return starCenters[i]->HFR;
        }
    }

    return -1;
}

void FITSData::applyFilter(FITSScale type, uint8_t *image, float *min, float *max)
{
    if (type == FITS_NONE)
        return;

    float dataMin=stats.min[0], dataMax=stats.max[0];

    if (min && *min != -1)
        dataMin = *min;
    if (max && *max != -1)
        dataMax = *max;

    switch (type)
    {
    case FITS_AUTO_STRETCH:
    {
        dataMin = stats.mean[0] - stats.stddev[0];
        dataMax = stats.mean[0] + stats.stddev[0] * 3;

        //dataMin = stats.min[0] + stats.stddev[0];
        //dataMax = stats.mean[0]*2;
        //dataMin = 0;
        //dataMax = pow(2, stats.bitpix) - 1;
    }
        break;

    case FITS_HIGH_CONTRAST:
    {
        dataMin = stats.mean[0] + stats.stddev[0];
        dataMax = stats.mean[0] + stats.stddev[0] * 3;
    }
        break;

    case FITS_HIGH_PASS:
    {
        dataMin = stats.mean[0];
    }
        break;


    default:
        break;
    }

    switch (data_type)
    {
    case TBYTE:
    {
        dataMin = dataMin < 0 ? 0 : dataMin;
        dataMax = dataMax > UINT8_MAX ? UINT8_MAX : dataMax;
        applyFilter<uint8_t>(type, image, dataMin, dataMax);
    }
        break;

    case TSHORT:
    {
        dataMin = dataMin < INT16_MIN ? INT16_MIN : dataMin;
        dataMax = dataMax > INT16_MAX ? INT16_MAX : dataMax;
        applyFilter<uint16_t>(type, image, dataMin, dataMax);
    }

        break;

    case TUSHORT:
    {
        dataMin = dataMin < 0 ? 0 : dataMin;
        dataMax = dataMax > UINT16_MAX ? UINT16_MAX : dataMax;
        applyFilter<uint16_t>(type, image, dataMin, dataMax);
    }
        break;

    case TLONG:
    {
        dataMin = dataMin < INT_MIN ? INT_MIN : dataMin;
        dataMax = dataMax > INT_MAX ? INT_MAX : dataMax;
        applyFilter<uint16_t>(type, image, dataMin, dataMax);
    }
        break;

    case TULONG:
    {
        dataMin = dataMin < 0 ? 0 : dataMin;
        dataMax = dataMax > UINT_MAX ? UINT_MAX : dataMax;
        applyFilter<uint16_t>(type, image, dataMin, dataMax);
    }
        break;

    case TFLOAT:
    {
        dataMin = dataMin < FLT_MIN ? FLT_MIN : dataMin;
        dataMax = dataMax > FLT_MAX ? FLT_MAX : dataMax;
        applyFilter<float>(type, image, dataMin, dataMax);
    }
        break;

    case TLONGLONG:
    {
        dataMin = dataMin < LLONG_MIN ? LLONG_MIN : dataMin;
        dataMax = dataMax > LLONG_MAX ? LLONG_MAX : dataMax;
        applyFilter<long>(type, image, dataMin, dataMax);
    }
        break;

    case TDOUBLE:
    {
        dataMin = dataMin < DBL_MIN ? DBL_MIN : dataMin;
        dataMax = dataMax > DBL_MAX ? DBL_MAX : dataMax;
        applyFilter<double>(type, image, dataMin, dataMax);
    }

        break;

    default:
        return;
    }


    if (min)
        *min = dataMin;
    if (max)
        *max = dataMax;
}

template<typename T> void FITSData::applyFilter(FITSScale type, uint8_t *targetImage, float image_min, float image_max)
{
    int offset=0, row=0;
    double coeff=0;
    bool calcStats = false;

    T bufferVal =0, val= 0;

    T *image = NULL;

    if (targetImage)
        image = reinterpret_cast<T*>(targetImage);
    else
    {
        image = reinterpret_cast<T*>(imageBuffer);
        calcStats = true;
    }

    int width = stats.width;
    int height = stats.height;

    T min = image_min, max = image_max;

    int size = stats.samples_per_channel;
    int index=0;

    switch (type)
    {
    case FITS_AUTO:
    case FITS_LINEAR:
    {
        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    bufferVal = image[index];
                    if (bufferVal < min) bufferVal = min;
                    else if (bufferVal > max) bufferVal = max;
                    image[index] = bufferVal;
                }
            }
        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
        }

    }
        break;

    case FITS_LOG:
    {
        coeff = max / log(1 + max);

        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    bufferVal = image[index];
                    if (bufferVal < min) bufferVal = min;
                    else if (bufferVal > max) bufferVal = max;
                    val = (coeff * log(1 + qBound(min, image[index], max)));
                    image[index] = qBound(min, val, max);
                }
            }

        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
            runningAverageStdDev<T>();
        }
    }
        break;

    case FITS_SQRT:
    {
        coeff = max / sqrt(max);

        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    val = (int) (coeff * sqrt(qBound(min, image[index], max)));
                    image[index] = val;
                }
            }
        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
            runningAverageStdDev<T>();
        }
    }
        break;

    case FITS_AUTO_STRETCH:
    {
        /*double alpha = (pow(2, stats.bitpix) - 1) / (stats.max[0]-stats.min[0]);
        double beta  = (-1 * stats.min[0]) * alpha;

        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    //image[index] = qBound(min, image[index], max);
                    image[index] = static_cast<T>((alpha * image[index] + beta));
                }
            }
        }*/

        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                    image[k + row] = qBound(min, image[k + row], max);
            }
        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
            runningAverageStdDev<T>();
        }
    }
        break;

    case FITS_HIGH_CONTRAST:
    {
        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    image[index] = qBound(min, image[index], max);
                }
            }
        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
            runningAverageStdDev<T>();
        }

    }
        break;

    case FITS_EQUALIZE:
    {
        #ifndef KSTARS_LITE
        if (histogram == NULL)
            return;
        QVector<double> cumulativeFreq = histogram->getCumulativeFrequency();
        coeff = 255.0 / (height * width);

        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    bufferVal = (int) (image[index] - min) / histogram->getBinWidth();

                    if (bufferVal >= cumulativeFreq.size())
                        bufferVal = cumulativeFreq.size()-1;

                    val = (int) (coeff * cumulativeFreq[bufferVal]);

                    image[index] = val;
                }
            }
        }
        #endif
    }
        if (calcStats)
            calculateStats(true);
        break;

    case FITS_HIGH_PASS:
    {
        min = stats.mean[0];
        for (int i=0; i < channels; i++)
        {
            offset = i*size;
            for (int j=0; j < height; j++)
            {
                row = offset + j * width;
                for (int k=0; k < width; k++)
                {
                    index=k + row;
                    image[index] = qBound(min, image[index], max);
                }
            }
        }

        if (calcStats)
        {
            stats.min[0] = min;
            stats.max[0] = max;
            runningAverageStdDev<T>();
        }
    }
        break;

        // Based on http://www.librow.com/articles/article-1
    case FITS_MEDIAN:
    {
        int BBP = stats.bytesPerPixel;
        T* extension = new T[(width + 2) * (height + 2)];
        //   Check memory allocation
        if (!extension)
            return;
        //   Create image extension
        for (int ch=0; ch < channels; ch++)
        {
            offset = ch*size;
            int N=width,M=height;

            for (int i = 0; i < M; ++i)
            {
                memcpy(extension + (N + 2) * (i + 1) + 1, image + (N * i) + offset, N * BBP);
                extension[(N + 2) * (i + 1)] = image[N * i + offset];
                extension[(N + 2) * (i + 2) - 1] = image[N * (i + 1) - 1 + offset];
            }
            //   Fill first line of image extension
            memcpy(extension, extension + N + 2, (N + 2) * BBP);
            //   Fill last line of image extension
            memcpy(extension + (N + 2) * (M + 1), extension + (N + 2) * M, (N + 2) * BBP);
            //   Call median filter implementation

            N=width+2;
            M=height+2;

            //   Move window through all elements of the image
            for (int m = 1; m < M - 1; ++m)
                for (int n = 1; n < N - 1; ++n)
                {
                    //   Pick up window elements
                    int k = 0;
                    float window[9];
                    for (int j = m - 1; j < m + 2; ++j)
                        for (int i = n - 1; i < n + 2; ++i)
                            window[k++] = extension[j * N + i];
                    //   Order elements (only half of them)
                    for (int j = 0; j < 5; ++j)
                    {
                        //   Find position of minimum element
                        int mine = j;
                        for (int l = j + 1; l < 9; ++l)
                            if (window[l] < window[mine])
                                mine = l;
                        //   Put found minimum element in its place
                        const float temp = window[j];
                        window[j] = window[mine];
                        window[mine] = temp;
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

    case FITS_CUSTOM:
    default:
        return;
        break;
    }
}

int FITSData::findStars(const QRectF &boundary, bool force)
{
    //if (histogram == NULL)
    //return -1;

    if (starsSearched == false || force)
    {
        qDeleteAll(starCenters);
        starCenters.clear();
        findCentroid(boundary);
        getHFR();
    }

    starsSearched = true;

    return starCenters.count();

}

void FITSData::getCenterSelection(int *x, int *y)
{
    if (starCenters.count() == 0)
        return;

    Edge *pEdge = new Edge();
    pEdge->x = *x;
    pEdge->y = *y;
    pEdge->width = 1;

    foreach(Edge *center, starCenters)
        if (checkCollision(pEdge, center))
        {
            *x = center->x;
            *y = center->y;
            break;
        }

    delete (pEdge);
}

bool FITSData::checkWCS()
{
#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB

    int status=0;
    char *header;
    int nkeyrec, nreject, nwcs, stat[2];
    double imgcrd[2], phi, pixcrd[2], theta, world[2];
    struct wcsprm *wcs=0;
    int width=getWidth();
    int height=getHeight();

    if (fits_hdr2str(fptr, 1, NULL, 0, &header, &nkeyrec, &status))
    {
        fits_report_error(stderr, status);
        return false;
    }

    if ((status = wcspih(header, nkeyrec, WCSHDR_all, -3, &nreject, &nwcs, &wcs)))
    {
        free(header);
        fprintf(stderr, "wcspih ERROR %d: %s.\n", status, wcshdr_errmsg[status]);
        return false;
    }

    free(header);

    if (wcs == 0)
    {
        //fprintf(stderr, "No world coordinate systems found.\n");
        return false;
    }

    // FIXME: Call above goes through EVEN if no WCS is present, so we're adding this to return for now.
    if (wcs->crpix[0] == 0)
        return false;

    if ((status = wcsset(wcs)))
    {
        fprintf(stderr, "wcsset ERROR %d: %s.\n", status, wcs_errmsg[status]);
        return false;
    }

    delete[] wcs_coord;

    wcs_coord = new wcs_point[width*height];

    wcs_point *p = wcs_coord;

    for (int i=0; i < height; i++)
    {
        for (int j=0; j < width; j++)
        {
            pixcrd[0]=j;
            pixcrd[1]=i;

            if ((status = wcsp2s(wcs, 1, 2, &pixcrd[0], &imgcrd[0], &phi, &theta, &world[0], &stat[0])))
            {
                fprintf(stderr, "wcsp2s ERROR %d: %s.\n", status,  wcs_errmsg[status]);
            }
            else
            {
                p->ra  = world[0];
                p->dec = world[1];

                p++;
            }
        }
    }

    findObjectsInImage(wcs, &world[0], phi, theta, &imgcrd[0], &pixcrd[0], &stat[0]);

    HasWCS = true;
    return HasWCS;
#endif
#endif

    return false;
}

#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
void FITSData::findObjectsInImage(struct wcsprm *wcs, double world[], double phi, double theta, double imgcrd[], double pixcrd[], int stat[])
{
    int width=getWidth();
    int height=getHeight();
    int status=0;

    char date[64];
    KSNumbers *num = NULL;

    if (fits_read_keyword(fptr, "DATE-OBS", date, NULL, &status) == 0)
    {
        QString tsString(date);
        tsString = tsString.remove("'").trimmed();

        QDateTime ts = QDateTime::fromString(tsString, Qt::ISODate);

        if (ts.isValid())
            num = new KSNumbers(KStarsDateTime(ts).djd());
    }
    if (num == NULL)
        num=new KSNumbers(KStarsData::Instance()->ut().djd());//Set to current time if the above does not work.

    SkyMapComposite *map=KStarsData::Instance()->skyComposite();

    wcs_point * wcs_coord = getWCSCoord();
    if (wcs_coord)
    {
        int size=width*height;

        objList.clear();

        SkyPoint p1;
        p1.setRA0(dms(wcs_coord[0].ra));
        p1.setDec0(dms(wcs_coord[0].dec));
        p1.updateCoordsNow(num);
        SkyPoint p2;
        p2.setRA0(dms(wcs_coord[size-1].ra));
        p2.setDec0(dms(wcs_coord[size-1].dec));
        p2.updateCoordsNow(num);
        QList<SkyObject*> list= map->findObjectsInArea( p1, p2 );

        foreach(SkyObject *object, list)
        {

            int type=object->type();
            if(object->name()=="star"||type==SkyObject::PLANET||type==SkyObject::ASTEROID||type==SkyObject::COMET||type==SkyObject::SUPERNOVA||type==SkyObject::MOON||type==SkyObject::SATELLITE){
                //DO NOT DISPLAY, at least for now, becaus these things move and change.
            }

            int x=-100;
            int y=-100;

            world[0]=object->ra0().Degrees();
            world[1]=object->dec0().Degrees();

            if ((status = wcss2p(wcs, 1, 2, &world[0],  &phi, &theta, &imgcrd[0], &pixcrd[0], &stat[0])))
            {
                fprintf(stderr, "wcsp2s ERROR %d: %s.\n", status,  wcs_errmsg[status]);
            }
            else
            {
                x = pixcrd[0];//The X and Y are set to the found position if it does work.
                y = pixcrd[1];
            }

            if(x>0&&y>0&&x<width&&y<height)
                objList.append(new FITSSkyObject(object,x,y));
        }
    }

    delete (num);
}
#endif
#endif

QList<FITSSkyObject *> FITSData::getSkyObjects(){
    return objList;
}


FITSSkyObject::FITSSkyObject(SkyObject *object, int xPos, int yPos) : QObject()
{
    skyObjectStored=object;
    xLoc=xPos;
    yLoc=yPos;
}

SkyObject *FITSSkyObject::skyObject(){
    return skyObjectStored;
}

int FITSSkyObject::x(){
    return xLoc;
}

int FITSSkyObject::y(){
    return yLoc;
}

void FITSSkyObject::setX(int xPos){
    xLoc=xPos;
}

void FITSSkyObject::setY(int yPos){
    yLoc=yPos;
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
 * return NULL if successful or rotated image.
 */
template<typename T>  bool FITSData::rotFITS (int rotate, int mirror)
{
    int ny, nx;
    int x1, y1, x2, y2;
    uint8_t *rotimage = NULL;
    int offset=0;

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
    rotimage = new uint8_t[stats.samples_per_channel*channels*BBP];

    if (rotimage == NULL)
    {
        qWarning() << "Unable to allocate memory for rotated image buffer!";
        return false;
    }

    T * rotBuffer = reinterpret_cast<T*>(rotimage);
    T * buffer    = reinterpret_cast<T*>(imageBuffer);

    /* Mirror image without rotation */
    if (rotate < 45 && rotate > -45)
    {
        if (mirror == 1)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1*nx) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }

        }
        else if (mirror == 2)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2*nx) + x1 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }

        }
        else
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y1*nx) + x1 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
    }

    /* Rotate by 90 degrees */
    else if (rotate >= 45 && rotate < 135)
    {
        if (mirror == 1)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2 = nx - x1 - 1;
                        rotBuffer[(y2*ny) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                    }
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1*ny) + y1 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2 = x1;
                        rotBuffer[(y2*ny) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                    }
                }
            }
        }

        stats.width = ny;
        stats.height = nx;
    }

    /* Rotate by 180 degrees */
    else if (rotate >= 135 && rotate < 225)
    {
        if (mirror == 1)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(y2*nx) + x1 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = nx - x1 - 1;
                    for (y1 = 0; y1 < ny; y1++)
                        rotBuffer[(y1*nx) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
        else
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    y2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        x2 = nx - x1 - 1;
                        rotBuffer[(y2*nx) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
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
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    for (x1 = 0; x1 < nx; x1++)
                        rotBuffer[(x1*ny) + y1 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
        else if (mirror == 2)
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = ny - y1 - 1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2 = nx - x1 - 1;
                        rotBuffer[(y2*ny) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                    }
                }
            }
        }
        else
        {
            for (int i=0; i < channels; i++)
            {
                offset = stats.samples_per_channel * i;
                for (y1 = 0; y1 < ny; y1++)
                {
                    x2 = y1;
                    for (x1 = 0; x1 < nx; x1++)
                    {
                        y2 = nx - x1 - 1;
                        rotBuffer[(y2*ny) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                    }
                }
            }
        }

        stats.width = ny;
        stats.height = nx;
    }

    /* If rotating by more than 315 degrees, assume top-bottom reflection */
    else if (rotate >= 315 && mirror)
    {
        for (int i=0; i < channels; i++)
        {
            offset = stats.samples_per_channel * i;
            for (y1 = 0; y1 < ny; y1++)
            {
                for (x1 = 0; x1 < nx; x1++)
                {
                    x2 = y1;
                    y2 = x1;
                    rotBuffer[(y2*ny) + x2 + offset] = buffer[(y1*nx) + x1 + offset];
                }
            }
        }
    }

    delete[] imageBuffer;
    imageBuffer = rotimage;

    return true;
}

/*
QVariant FITSData::getFITSHeaderValue(const QString &keyword){
    QVariant property;
    int status=0;
    char comment[100];
    fits_read_key_dbl(fptr, keyword, &property, comment, &status );
    return property;
}
*/

void FITSData::rotWCSFITS (int angle, int mirror)
{
    int status=0;
    char comment[100];
    double ctemp1, ctemp2, ctemp3, ctemp4, naxis1, naxis2;
    int WCS_DECIMALS=6;

    naxis1=stats.width;
    naxis2=stats.height;

    if (fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status ))
    {
        // No WCS keywords
        return;
    }

    /* Reset CROTAn and CD matrix if axes have been exchanged */
    if (angle == 90)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CROTA1", ctemp1+90.0, WCS_DECIMALS, comment, &status );

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CROTA2", ctemp1+90.0, WCS_DECIMALS, comment, &status );
    }

    status=0;

    /* Negate rotation angle if mirrored */
    if (mirror)
    {
        if (!fits_read_key_dbl(fptr, "CROTA1", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CROTA1", -ctemp1, WCS_DECIMALS, comment, &status );

        if (!fits_read_key_dbl(fptr, "CROTA2", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CROTA2", -ctemp1, WCS_DECIMALS, comment, &status );

        status=0;

        if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "LTM1_1", -ctemp1, WCS_DECIMALS, comment, &status );

        status=0;

        if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status );

        if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CD1_2", -ctemp1, WCS_DECIMALS, comment, &status );

        if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp1, comment, &status ))
            fits_update_key_dbl(fptr, "CD2_1", -ctemp1, WCS_DECIMALS, comment, &status );
    }

    status=0;

    /* Unbin CRPIX and CD matrix */
    if (!fits_read_key_dbl(fptr, "LTM1_1", &ctemp1, comment, &status ))
    {
        if (ctemp1 != 1.0)
        {
            if (!fits_read_key_dbl(fptr, "LTM2_2", &ctemp2, comment, &status ))
                if (ctemp1 == ctemp2)
                {
                    double ltv1 = 0.0;
                    double ltv2 = 0.0;
                    status=0;
                    if (!fits_read_key_dbl(fptr, "LTV1", &ltv1, comment, &status))
                        fits_delete_key(fptr, "LTV1", &status);
                    if (!fits_read_key_dbl(fptr, "LTV2", &ltv2, comment, &status))
                        fits_delete_key(fptr, "LTV2", &status);

                    status=0;

                    if (!fits_read_key_dbl(fptr, "CRPIX1", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CRPIX1", (ctemp3-ltv1)/ctemp1, WCS_DECIMALS, comment, &status );

                    if (!fits_read_key_dbl(fptr, "CRPIX2", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CRPIX2", (ctemp3-ltv2)/ctemp1, WCS_DECIMALS, comment, &status );

                    status=0;

                    if (!fits_read_key_dbl(fptr, "CD1_1", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CD1_1", ctemp3/ctemp1, WCS_DECIMALS, comment, &status );

                    if (!fits_read_key_dbl(fptr, "CD1_2", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CD1_2", ctemp3/ctemp1, WCS_DECIMALS, comment, &status );

                    if (!fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CD2_1", ctemp3/ctemp1, WCS_DECIMALS, comment, &status );

                    if (!fits_read_key_dbl(fptr, "CD2_2", &ctemp3, comment, &status ))
                        fits_update_key_dbl(fptr, "CD2_2", ctemp3/ctemp1, WCS_DECIMALS, comment, &status );

                    status=0;

                    fits_delete_key(fptr, "LTM1_1", &status);
                    fits_delete_key(fptr, "LTM1_2", &status);
                }
        }
    }

    status=0;

    /* Reset CRPIXn */
    if ( !fits_read_key_dbl(fptr, "CRPIX1", &ctemp1, comment, &status ) && !fits_read_key_dbl(fptr, "CRPIX2", &ctemp2, comment, &status ) )
    {
        if (mirror)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CRPIX1", naxis1-ctemp1, WCS_DECIMALS, comment, &status );
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2-ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", naxis1-ctemp1, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", naxis2-ctemp2, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis2-ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", ctemp1, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CRPIX1", naxis1-ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", naxis2-ctemp2, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CRPIX1", ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CRPIX2", naxis1-ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
    }

    status=0;

    /* Reset CDELTn (degrees per pixel) */
    if ( !fits_read_key_dbl(fptr, "CDELT1", &ctemp1, comment, &status ) && !fits_read_key_dbl(fptr, "CDELT2", &ctemp2, comment, &status ) )
    {
        if (mirror)
        {
            if (angle == 0)
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status );
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status );

            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
        else
        {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", ctemp1, WCS_DECIMALS, comment, &status );

            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CDELT1", -ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", -ctemp2, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CDELT1", ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CDELT2", -ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
    }

    /* Reset CD matrix, if present */
    ctemp1 = 0.0;
    ctemp2 = 0.0;
    ctemp3 = 0.0;
    ctemp4 = 0.0;
    status=0;
    if ( !fits_read_key_dbl(fptr, "CD1_1", &ctemp1, comment, &status ) )
    {
        fits_read_key_dbl(fptr, "CD1_2", &ctemp2, comment, &status );
        fits_read_key_dbl(fptr, "CD2_1", &ctemp3, comment, &status );
        fits_read_key_dbl(fptr, "CD2_2", &ctemp4, comment, &status );
        status=0;
        if (mirror)
        {
            if (angle == 0)
            {
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status );

            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
        else {
            if (angle == 90)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp4, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", -ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", ctemp1, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 180)
            {
                fits_update_key_dbl(fptr, "CD1_1", -ctemp1, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", -ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", -ctemp4, WCS_DECIMALS, comment, &status );
            }
            else if (angle == 270)
            {
                fits_update_key_dbl(fptr, "CD1_1", ctemp4, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD1_2", ctemp3, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_1", -ctemp2, WCS_DECIMALS, comment, &status );
                fits_update_key_dbl(fptr, "CD2_2", -ctemp1, WCS_DECIMALS, comment, &status );
            }
        }
    }

    /* Delete any polynomial solution */
    /* (These could maybe be switched, but I don't want to work them out yet */
    status=0;
    if ( !fits_read_key_dbl(fptr, "CO1_1", &ctemp1, comment, &status ) )
    {
        int i;
        char keyword[16];

        for (i = 1; i < 13; i++)
        {
            sprintf (keyword,"CO1_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
        for (i = 1; i < 13; i++)
        {
            sprintf (keyword,"CO2_%d", i);
            fits_delete_key(fptr, keyword, &status);
        }
    }

    return;
}

uint8_t *FITSData::getImageBuffer()
{
    return imageBuffer;
}

void FITSData::setImageBuffer(uint8_t *buffer)
{
    delete[] imageBuffer;
    imageBuffer = buffer;
}

bool FITSData::checkDebayer()
{
    int status=0;
    char bayerPattern[64];

    // Let's search for BAYERPAT keyword, if it's not found we return as there is no bayer pattern in this image
    if (fits_read_keyword(fptr, "BAYERPAT", bayerPattern, NULL, &status))
        return false;

    if (stats.bitpix != 16 && stats.bitpix != 8)
    {
        KSNotification::error(i18n("Only 8 and 16 bits bayered images supported."), i18n("Debayer error"));
        return false;
    }
    QString pattern(bayerPattern);
    pattern = pattern.remove("'").trimmed();

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

    fits_read_key(fptr, TINT, "XBAYROFF", &debayerParams.offsetX, NULL, &status);
    fits_read_key(fptr, TINT, "YBAYROFF", &debayerParams.offsetY, NULL, &status);

    HasDebayer = true;

    return true;
}

void FITSData::getBayerParams(BayerParams *param)
{
    param->method       = debayerParams.method;
    param->filter       = debayerParams.filter;
    param->offsetX      = debayerParams.offsetX;
    param->offsetY      = debayerParams.offsetY;
}

void FITSData::setBayerParams(BayerParams *param)
{
    debayerParams.method   = param->method;
    debayerParams.filter   = param->filter;
    debayerParams.offsetX  = param->offsetX;
    debayerParams.offsetY  = param->offsetY;
}

bool FITSData::debayer()
{
    if (bayerBuffer == NULL)
    {
        int anynull=0, status=0;

        bayerBuffer = imageBuffer;

        if (fits_read_img(fptr, data_type, 1, stats.samples_per_channel, 0, bayerBuffer, &anynull, &status))
        {
            char errmsg[512];
            fits_get_errstatus(status, errmsg);
            KSNotification::error(i18n("Error reading image: %1", QString(errmsg)), i18n("Debayer error"));
            return false;
        }
    }

    switch (data_type)
    {
    case TBYTE:
        return debayer_8bit();

    case TUSHORT:
        return debayer_16bit();

    default:
        return false;
    }

    return false;
}

bool FITSData::debayer_8bit()
{
    dc1394error_t error_code;

    int rgb_size = stats.samples_per_channel*3*stats.bytesPerPixel;
    uint8_t * destinationBuffer = new uint8_t[rgb_size];

    if (destinationBuffer == NULL)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = stats.height;
    uint8_t * dc1394_source = bayerBuffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_8bit(dc1394_source, destinationBuffer, stats.width, ds1394_height, debayerParams.filter, debayerParams.method);

    if ( error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        channels=1;
        delete[] destinationBuffer;
        return false;
    }

    if (channels == 1)
    {
        delete[] imageBuffer;
        imageBuffer = new uint8_t[rgb_size];

        if (imageBuffer == NULL)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
            return false;
        }
    }

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint8_t * rBuff = imageBuffer;
    uint8_t * gBuff = imageBuffer + (stats.width * stats.height);
    uint8_t * bBuff = imageBuffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel*3 - 3;
    for (int i=0; i <= imax; i += 3)
    {
        *rBuff++ = destinationBuffer[i];
        *gBuff++ = destinationBuffer[i+1];
        *bBuff++ = destinationBuffer[i+2];
    }

    channels=3;
    delete[] destinationBuffer;
    bayerBuffer = NULL;
    return true;
}

bool FITSData::debayer_16bit()
{
    dc1394error_t error_code;

    int rgb_size = stats.samples_per_channel*3*stats.bytesPerPixel;
    uint8_t * destinationBuffer = new uint8_t[rgb_size];

    uint16_t * buffer    = reinterpret_cast<uint16_t*>(bayerBuffer);
    uint16_t * dstBuffer = reinterpret_cast<uint16_t*>(destinationBuffer);

    if (destinationBuffer == NULL)
    {
        KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
        return false;
    }

    int ds1394_height = stats.height;
    uint16_t * dc1394_source = buffer;

    if (debayerParams.offsetY == 1)
    {
        dc1394_source += stats.width;
        ds1394_height--;
    }

    if (debayerParams.offsetX == 1)
    {
        dc1394_source++;
    }

    error_code = dc1394_bayer_decoding_16bit(dc1394_source, dstBuffer, stats.width, ds1394_height, debayerParams.filter, debayerParams.method, 16);

    if ( error_code != DC1394_SUCCESS)
    {
        KSNotification::error(i18n("Debayer failed (%1)", error_code), i18n("Debayer error"));
        channels=1;
        delete[] destinationBuffer;
        return false;
    }

    if (channels == 1)
    {
        delete[] imageBuffer;
        imageBuffer = new uint8_t[rgb_size];

        if (imageBuffer == NULL)
        {
            delete[] destinationBuffer;
            KSNotification::error(i18n("Unable to allocate memory for temporary bayer buffer."), i18n("Debayer error"));
            return false;
        }
    }

    buffer    = reinterpret_cast<uint16_t*>(imageBuffer);

    // Data in R1G1B1, we need to copy them into 3 layers for FITS

    uint16_t * rBuff = buffer;
    uint16_t * gBuff = buffer + (stats.width * stats.height);
    uint16_t * bBuff = buffer + (stats.width * stats.height * 2);

    int imax = stats.samples_per_channel*3 - 3;
    for (int i=0; i <= imax; i += 3)
    {
        *rBuff++ = dstBuffer[i];
        *gBuff++ = dstBuffer[i+1];
        *bBuff++ = dstBuffer[i+2];
    }

    channels=3;
    delete[] destinationBuffer;
    bayerBuffer = NULL;
    return true;
}

double FITSData::getADU()
{
    double adu=0;
    for (int i=0; i < channels; i++)
        adu += stats.mean[i];

    return (adu/ (double) channels);
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
 * Web-Site: http://github.com/hipersayanX/CannyDetector
 */

#if 0
void FITSData::sobel(const QImage &image, QVector<int> &gradient, QVector<int> &direction)
{
    int size = image.width() * image.height();
    gradient.resize(size);
    direction.resize(size);

    for (int y = 0; y < image.height(); y++) {
        size_t yOffset = y * image.width();
        const quint8 *grayLine = image.constBits() + yOffset;

        const quint8 *grayLine_m1 = y < 1? grayLine: grayLine - image.width();
        const quint8 *grayLine_p1 = y >= image.height() - 1? grayLine: grayLine + image.width();

        int *gradientLine = gradient.data() + yOffset;
        int *directionLine = direction.data() + yOffset;

        for (int x = 0; x < image.width(); x++) {
            int x_m1 = x < 1? x: x - 1;
            int x_p1 = x >= image.width() - 1? x: x + 1;

            int gradX = grayLine_m1[x_p1]
                    + 2 * grayLine[x_p1]
                    + grayLine_p1[x_p1]
                    - grayLine_m1[x_m1]
                    - 2 * grayLine[x_m1]
                    - grayLine_p1[x_m1];

            int gradY = grayLine_m1[x_m1]
                    + 2 * grayLine_m1[x]
                    + grayLine_m1[x_p1]
                    - grayLine_p1[x_m1]
                    - 2 * grayLine_p1[x]
                    - grayLine_p1[x_p1];

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
            else {
                qreal a = 180. * atan(qreal(gradY) / gradX) / M_PI;

                if (a >= -22.5 && a < 22.5)
                    directionLine[x] = 0;
                else if (a >= 22.5 && a < 67.5)
                    directionLine[x] = 1;
                else if (a >= -67.5 && a < -22.5)
                    directionLine[x] = 2;
                else
                    directionLine[x] = 3;
            }
        }
    }
}
#endif

template<typename T> void FITSData::sobel(QVector<float> &gradient, QVector<float> &direction)
{
    //int size = image.width() * image.height();
    gradient.resize(stats.samples_per_channel);
    direction.resize(stats.samples_per_channel);

    for (int y = 0; y < stats.height; y++)
    {
        size_t yOffset = y * stats.width;
        const T *grayLine = reinterpret_cast<T*>(imageBuffer) + yOffset;

        const T *grayLine_m1 = y < 1? grayLine: grayLine - stats.width;
        const T *grayLine_p1 = y >= stats.height - 1? grayLine: grayLine + stats.width;

        float *gradientLine = gradient.data() + yOffset;
        float *directionLine = direction.data() + yOffset;

        for (int x = 0; x < stats.width; x++)
        {
            int x_m1 = x < 1? x: x - 1;
            int x_p1 = x >= stats.width - 1? x: x + 1;

            int gradX = grayLine_m1[x_p1]
                    + 2 * grayLine[x_p1]
                    + grayLine_p1[x_p1]
                    - grayLine_m1[x_m1]
                    - 2 * grayLine[x_m1]
                    - grayLine_p1[x_m1];

            int gradY = grayLine_m1[x_m1]
                    + 2 * grayLine_m1[x]
                    + grayLine_m1[x_p1]
                    - grayLine_p1[x_m1]
                    - 2 * grayLine_p1[x]
                    - grayLine_p1[x_p1];

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

    for (int y=1; y < height-1; y++)
    {

        for (int x=1; x < width-1; x++)
        {
            int index = x+y*width;
            float val  = gradient[index];
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
    int yOffset = y * width;
    float *cannyLine = image.data() + yOffset;
    int *idLine    = ids.data() + yOffset;

    if (idLine[x] != 0)
        return;

    idLine[x] = id;

    for (int j = -1; j < 2; j++)
    {
        int nextY = y + j;

        if (nextY < 0 || nextY >= height)
            continue;

        float *cannyLineNext = cannyLine + j * width;

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

#if 0
QVector<int> FITSData::thinning(int width, int height, const QVector<int> &gradient, const QVector<int> &direction)
{
    QVector<int> thinned(gradient.size());

    for (int y = 0; y < height; y++) {
        int yOffset = y * width;
        const int *gradientLine = gradient.constData() + yOffset;
        const int *gradientLine_m1 = y < 1? gradientLine: gradientLine - width;
        const int *gradientLine_p1 = y >= height - 1? gradientLine: gradientLine + width;
        const int *directionLine = direction.constData() + yOffset;
        int *thinnedLine = thinned.data() + yOffset;

        for (int x = 0; x < width; x++) {
            int x_m1 = x < 1? 0: x - 1;
            int x_p1 = x >= width - 1? x: x + 1;

            int direction = directionLine[x];
            int pixel = 0;

            if (direction == 0) {
                /* x x x
                 * - - -
                 * x x x
                 */
                if (gradientLine[x] < gradientLine[x_m1]
                        || gradientLine[x] < gradientLine[x_p1])
                    pixel = 0;
                else
                    pixel = gradientLine[x];
            } else if (direction == 1) {
                /* x x /
                 * x / x
                 * / x x
                 */
                if (gradientLine[x] < gradientLine_m1[x_p1]
                        || gradientLine[x] < gradientLine_p1[x_m1])
                    pixel = 0;
                else
                    pixel = gradientLine[x];
            } else if (direction == 2) {
                /* \ x x
                 * x \ x
                 * x x \
                 */
                if (gradientLine[x] < gradientLine_m1[x_m1]
                        || gradientLine[x] < gradientLine_p1[x_p1])
                    pixel = 0;
                else
                    pixel = gradientLine[x];
            } else {
                /* x | x
                 * x | x
                 * x | x
                 */
                if (gradientLine[x] < gradientLine_m1[x]
                        || gradientLine[x] < gradientLine_p1[x])
                    pixel = 0;
                else
                    pixel = gradientLine[x];
            }

            thinnedLine[x] = pixel;
        }
    }

    return thinned;
}

QVector<float> FITSData::threshold(int thLow, int thHi, const QVector<float> &image)
{
    QVector<float> thresholded(image.size());

    for (int i = 0; i < image.size(); i++)
        thresholded[i] = image[i] <= thLow? 0:
                                            image[i] >= thHi? 255:
                                                              127;

    return thresholded;
}

QVector<int> FITSData::hysteresis(int width, int height, const QVector<int> &image)
{
    QVector<int> canny(image);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            trace(width, height, canny, x, y);

    // Remaining gray pixels becomes black.
    for (int i = 0; i < canny.size(); i++)
        if (canny[i] == 127)
            canny[i] = 0;

    return canny;
}

#endif


QImage FITSData::FITSToImage(const QString &filename)
{
    QImage fitsImage;
    double min, max, val;

    FITSData data;

    bool rc = data.loadFITS(filename);
    if (rc == false)
        return fitsImage;

    data.getMinMax(&min, &max);

    if (min == max)
    {
        fitsImage.fill(Qt::white);
        return fitsImage;
    }

    if (data.getNumOfChannels() == 1)
    {
        fitsImage = QImage(data.getWidth(), data.getHeight(), QImage::Format_Indexed8);

        fitsImage.setColorCount(256);
        for (int i=0; i < 256; i++)
            fitsImage.setColor(i, qRgb(i,i,i));
    }
    else
    {
        fitsImage =  QImage(data.getWidth(), data.getHeight(), QImage::Format_RGB32);
    }

    uint16_t w = data.getWidth();
    uint16_t h = data.getHeight();
    uint32_t size = data.getSize();

    double dataMin = data.stats.mean[0] - data.stats.stddev[0];
    double dataMax = data.stats.mean[0] + data.stats.stddev[0] * 3;

    double bscale = 255. / (dataMax - dataMin);
    double bzero  = (-dataMin) * (255. / (dataMax - dataMin));

    // Long way to do this since we do not want to use templated functions here
    switch (data.getDataType())
    {

    case TBYTE:
    {
        uint8_t *buffer = data.getImageBuffer();
        uint8_t bMin = dataMin < 0 ? 0 : dataMin;
        uint8_t bMax = dataMax > 255 ? 255 : dataMax;
        if (data.getNumOfChannels() == 1)
        {
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < h; j++)
            {
                unsigned char *scanLine = fitsImage.scanLine(j);

                for (int i = 0; i < w; i++)
                {
                    val = qBound(bMin, buffer[j * w + i], bMax);
                    val = val * bscale + bzero;
                    scanLine[i]= qBound(0.0, val, 255.0);
                }
            }
        }
        else
        {
            double rval=0,gval=0,bval=0;
            QRgb value;
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < h; j++)
            {
                QRgb *scanLine = reinterpret_cast<QRgb*>((fitsImage.scanLine(j)));

                for (int i = 0; i < w; i++)
                {
                    rval = qBound(bMin, buffer[j * w + i], bMax);
                    gval = qBound(bMin, buffer[j * w + i + size], bMax);
                    bval = qBound(bMin, buffer[j * w + i + size *2], bMax);;

                    value = qRgb(rval* bscale + bzero, gval* bscale + bzero, bval* bscale + bzero);

                    scanLine[i] = value;
                }
            }
        }
    }
        break;


    case TUSHORT:
    {
        uint16_t *buffer = reinterpret_cast<uint16_t*>(data.getImageBuffer());
        uint16_t bMin = dataMin < 0 ? 0 : dataMin;
        uint16_t bMax = dataMax > USHRT_MAX ? USHRT_MAX : dataMax;
        if (data.getNumOfChannels() == 1)
        {
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < h; j++)
            {
                unsigned char *scanLine = fitsImage.scanLine(j);

                for (int i = 0; i < w; i++)
                {
                    val = qBound(bMin, buffer[j * w + i], bMax);
                    val = val * bscale + bzero;
                    scanLine[i]= qBound(0.0, val, 255.0);
                }
            }
        }
        else
        {
            double rval=0,gval=0,bval=0;
            QRgb value;
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < h; j++)
            {
                QRgb *scanLine = reinterpret_cast<QRgb*>((fitsImage.scanLine(j)));

                for (int i = 0; i < w; i++)
                {
                    rval = qBound(bMin, buffer[j * w + i], bMax);
                    gval = qBound(bMin, buffer[j * w + i + size], bMax);
                    bval = qBound(bMin, buffer[j * w + i + size *2], bMax);

                    value = qRgb(rval* bscale + bzero, gval* bscale + bzero, bval* bscale + bzero);

                    scanLine[i] = value;
                }
            }
        }
    }
    break;

   default:
        break;
    }

    return fitsImage;
}
