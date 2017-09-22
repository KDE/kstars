/***************************************************************************
                          fitsimage.cpp  -  FITS Image
                             -------------------
    begin                : Tue Feb 24 2004
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

#pragma once

#include "config-kstars.h"

#include "bayer.h"
#include "fitscommon.h"

#ifdef WIN32
// This header must be included before fitsio.h to avoid compiler errors with Visual Studio
#include <windows.h>
#endif

#include <fitsio.h>

#include <QObject>
#include <QRect>
#include <QRectF>

#ifndef KSTARS_LITE
#include "fitshistogram.h"

#include <kxmlguiwindow.h>
#ifdef HAVE_WCSLIB
#include <wcs.h>
#endif
#endif

#define MINIMUM_PIXEL_RANGE 5
#define MINIMUM_STDVAR      5

class QProgressDialog;

class SkyObject;
class SkyPoint;

typedef struct
{
    float ra;
    float dec;
} wcs_point;

class Edge
{
  public:
    float x;
    float y;
    int val;
    int scanned;
    float width;
    float HFR;
    float sum;
};

class FITSSkyObject : public QObject
{
    Q_OBJECT
  public:
    explicit FITSSkyObject(SkyObject *skyObject, int xPos, int yPos);
    SkyObject *skyObject();
    int x();
    int y();
    void setX(int xPos);
    void setY(int yPos);

  private:
    SkyObject *skyObjectStored;
    int xLoc;
    int yLoc;
};

class FITSData
{
  public:
    explicit FITSData(FITSMode mode = FITS_NORMAL);
    explicit FITSData(const FITSData *other);
    ~FITSData();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool loadFITS(const QString &filename, bool silent = true);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);
    /* Calculate stats */
    void calculateStats(bool refresh = false);

    bool contains(const QPointF &point) const;

    // Access functions
    void clearImageBuffers();
    void setImageBuffer(uint8_t *buffer);
    uint8_t *getImageBuffer();

    int getDataType() { return data_type; }
    void setDataType(int value) { data_type = value; }

    // Stats
    unsigned int getSize() { return stats.samples_per_channel; }
    void getDimensions(uint16_t *w, uint16_t *h)
    {
        *w = stats.width;
        *h = stats.height;
    }
    void setWidth(uint16_t w)
    {
        stats.width               = w;
        stats.samples_per_channel = stats.width * stats.height;
    }
    void setHeight(uint16_t h)
    {
        stats.height              = h;
        stats.samples_per_channel = stats.width * stats.height;
    }
    uint16_t getWidth() { return stats.width; }
    uint16_t getHeight() { return stats.height; }

    // Statistics
    int getNumOfChannels() { return channels; }
    void setMinMax(double newMin, double newMax, uint8_t channel = 0);
    void getMinMax(double *min, double *max, uint8_t channel = 0)
    {
        *min = stats.min[channel];
        *max = stats.max[channel];
    }
    double getMin(uint8_t channel = 0) { return stats.min[channel]; }
    double getMax(uint8_t channel = 0) { return stats.max[channel]; }
    void setStdDev(double value, uint8_t channel = 0) { stats.stddev[channel] = value; }
    double getStdDev(uint8_t channel = 0) { return stats.stddev[channel]; }
    void setMean(double value, uint8_t channel = 0) { stats.mean[channel] = value; }
    double getMean(uint8_t channel = 0) { return stats.mean[channel]; }
    void setMedian(double val, uint8_t channel = 0) { stats.median[channel] = val; }
    double getMedian(uint8_t channel = 0) { return stats.median[channel]; }

    int getBytesPerPixel() { return stats.bytesPerPixel; }
    void setSNR(double val) { stats.SNR = val; }
    double getSNR() { return stats.SNR; }
    void setBPP(int value) { stats.bitpix = value; }
    int getBPP() { return stats.bitpix; }
    double getADU();

    // Star Detection - Native KStars implementation
    int getDetectedStars() { return starCenters.count(); }
    bool areStarsSearched() { return starsSearched; }
    void appendStar(Edge *newCenter) { starCenters.append(newCenter); }
    QList<Edge *> getStarCenters() { return starCenters; }
    int findStars(const QRectF &boundary = QRectF(), bool force = false);
    void findCentroid(const QRectF &boundary = QRectF(), int initStdDev = MINIMUM_STDVAR,
                      int minEdgeWidth = MINIMUM_PIXEL_RANGE);
    void getCenterSelection(int *x, int *y);
    int findOneStar(const QRectF &boundary);

    // Star Detection - Partially customized Canny edge detection algorithm
    static int findCannyStar(FITSData *data, const QRect &boundary = QRect());
    template <typename T>
    static int findCannyStar(FITSData *data, const QRect &boundary);

    // Half Flux Radius
    Edge *getMaxHFRStar() { return maxHFRStar; }
    double getHFR(HFRType type = HFR_AVERAGE);
    double getHFR(int x, int y);

    // FITS Mode (Normal, Guide, Focus..etc).
    FITSMode getMode() { return mode; }

    // WCS
    // Check if image has valid WCS header information and set HasWCS accordingly. Call in loadFITS()
    bool checkForWCS();
    // Does image have valid WCS?
    bool hasWCS() { return HasWCS; }
    // Load WCS data
    bool loadWCS();
    // Is WCS Image loaded?
    bool isWCSLoaded() { return WCSLoaded; }

    wcs_point *getWCSCoord() { return wcs_coord; }

    /**
         * @brief wcsToPixel Given J2000 (RA0,DE0) coordinates. Find in the image the corresponding pixel coordinates.
         * @param wcsCoord Coordinates of target
         * @param wcsPixelPoint Return XY FITS coordinates
         * @param wcsImagePoint Return XY Image coordinates
         * @return True if conversion is successful, false otherwise.
         */
    bool wcsToPixel(SkyPoint &wcsCoord, QPointF &wcsPixelPoint, QPointF &wcsImagePoint);

    /**
         * @brief pixelToWCS Convert Pixel coordinates to J2000 world coordinates
         * @param wcsPixelPoint Pixel coordinates in XY Image space.
         * @param wcsCoord Store back WCS world coordinate in wcsCoord
         * @return True if successful, false otherwise.
         */
    bool pixelToWCS(const QPointF &wcsPixelPoint, SkyPoint &wcsCoord);

    /**
         * @brief createWCSFile Create a new FITS file given the WCS information supplied. Construct the necessary WCS keywords and save the
         * new file as the current active file
         * @param newWCSFile New file name
         * @param orientation Solver orientation, degrees E of N.
         * @param ra J2000 Right Ascension
         * @param dec J2000 Declination
         * @param pixscale Pixel scale in arcsecs per pixel
         * @return  True if file is successfully created and saved, false otherwise.
         */
    bool createWCSFile(const QString &newWCSFile, double orientation, double ra, double dec, double pixscale);

    // Debayer
    bool hasDebayer() { return HasDebayer; }
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    void getBayerParams(BayerParams *param);
    void setBayerParams(BayerParams *param);

    // FITS Record
    int getFITSRecord(QString &recordList, int &nkeys);

// Histogram
#ifndef KSTARS_LITE
    void setHistogram(FITSHistogram *inHistogram) { histogram = inHistogram; }
#endif

    // Filter
    void applyFilter(FITSScale type, uint8_t *image = nullptr, float *min = nullptr, float *max = nullptr);

    // Rotation counter. We keep count to rotate WCS keywords on save
    int getRotCounter() const;
    void setRotCounter(int value);

    // Filename
    const QString &getFilename() { return filename; }

    // Horizontal flip counter. We keep count to rotate WCS keywords on save
    int getFlipHCounter() const;
    void setFlipHCounter(int value);

    // Horizontal flip counter. We keep count to rotate WCS keywords on save
    int getFlipVCounter() const;
    void setFlipVCounter(int value);

#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
    void findObjectsInImage(double world[], double phi, double theta, double imgcrd[], double pixcrd[], int stat[]);
#endif
#endif
    QList<FITSSkyObject *> getSkyObjects();
    QList<FITSSkyObject *> objList; //Does this need to be public??

    // Create autostretch image from FITS File
    static QImage FITSToImage(const QString &filename);

    bool getAutoRemoveTemporaryFITS() const;
    void setAutoRemoveTemporaryFITS(bool value);

    QString getLastError() const;

  private:
    void rotWCSFITS(int angle, int mirror);
    bool checkCollision(Edge *s1, Edge *s2);
    int calculateMinMax(bool refresh = false);
    bool checkDebayer();
    void readWCSKeys();

    // Templated functions
    template <typename T>
    bool debayer();

    template <typename T>
    bool rotFITS(int rotate, int mirror);

    // Apply Filter
    template <typename T>
    void applyFilter(FITSScale type, uint8_t *targetImage, float image_min, float image_max);
    // Star Detect - Centroid
    template <typename T>
    void findCentroid(const QRectF &boundary, int initStdDev, int minEdgeWidth);
    // Star Detect - Threshold
    template <typename T>
    int findOneStar(const QRectF &boundary);

    template <typename T>
    void calculateMinMax();
    /* Calculate running average & standard deviation using Welford’s method for computing variance */
    template <typename T>
    void runningAverageStdDev();

    // Sobel detector by Gonzalo Exequiel Pedone
    template <typename T>
    void sobel(QVector<float> &gradient, QVector<float> &direction);

    template <typename T>
    void convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image);

    // Give unique IDs to each contigous region
    int partition(int width, int height, QVector<float> &gradient, QVector<int> &ids);
    void trace(int width, int height, int id, QVector<float> &image, QVector<int> &ids, int x, int y);

#if 0
        QVector<int> thinning(int width, int height, const QVector<int> &gradient, const QVector<int> &direction);
        QVector<float> threshold(int thLow, int thHi, const QVector<float> &image);
        QVector<int> hysteresis(int width, int height, const QVector<int> &image);
#endif

#ifndef KSTARS_LITE
    FITSHistogram *histogram { nullptr }; // Pointer to the FITS data histogram
#endif
    /// Pointer to CFITSIO FITS file struct
    fitsfile *fptr { nullptr };

    /// FITS image data type (TBYTE, TUSHORT, TINT, TFLOAT, TLONG, TDOUBLE)
    int data_type { 0 };
    /// Number of channels
    int channels { 1 };
    /// Generic data image buffer
    uint8_t *imageBuffer { nullptr };

    /// Is this a tempoprary file or one loaded from disk?
    bool tempFile { false };
    /// Did we search for stars yet?
    bool starsSearched { false };
    /// Do we have WCS keywords in this FITS data?
    bool HasWCS { false };
    /// Is WCS data loaded?
    bool WCSLoaded { false };
    /// Do we need to mark stars for the user?
    bool markStars { false };
    /// Is the image debayarable?
    bool HasDebayer { false };

    /// Our very own file name
    QString filename;
    /// FITS Mode (Normal, WCS, Guide, Focus..etc)
    FITSMode mode;

    /// How many times the image was rotated? Useful for WCS keywords rotation on save.
    int rotCounter { 0 };
    /// How many times the image was flipped horizontally?
    int flipHCounter { 0 };
    /// How many times the image was flipped vertically?
    int flipVCounter { 0 };

    /// Pointer to WCS coordinate data, if any.
    wcs_point *wcs_coord { nullptr };
    /// WCS Struct
    struct wcsprm *wcs { nullptr };
    /// All the stars we detected, if any.
    QList<Edge *> starCenters;
    /// The biggest fattest star in the image.
    Edge *maxHFRStar { nullptr };

    uint8_t *bayerBuffer { nullptr };
    /// Bayer parameters
    BayerParams debayerParams;

    /// Stats struct to hold statisical data about the FITS data
    struct
    {
        double min[3], max[3];
        double mean[3];
        double stddev[3];
        double median[3];
        double SNR { 0 };
        int bitpix { 8 };
        int bytesPerPixel { 1 };
        int ndim { 2 };
        uint32_t samples_per_channel { 0 };
        uint16_t width { 0 };
        uint16_t height { 0 };
    } stats;

    /// Remove temproray files after closing
    bool autoRemoveTemporaryFITS { true };

    QString lastError;
};
