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

#include <QFuture>
#include <QObject>
#include <QRect>
#include <QVariant>

#ifndef KSTARS_LITE
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
class FITSHistogram;

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
    explicit FITSSkyObject(SkyObject *object, int xPos, int yPos);
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

class FITSData : public QObject
{
  Q_OBJECT

  // Name of FITS file
  Q_PROPERTY(QString filename READ filename)
  // Size of file in bytes
  Q_PROPERTY(qint64 size READ size)
  // Width in pixels
  Q_PROPERTY(quint16 width READ width)
  // Height in pixels
  Q_PROPERTY(quint16 height READ height)
  // FITS MODE --> Normal, Focus, Guide..etc
  Q_PROPERTY(FITSMode mode MEMBER m_Mode)
  // 1 channel (grayscale) or 3 channels (RGB)
  Q_PROPERTY(quint8 channels READ channels)
  // Data type (BYTE, SHORT, INT..etc)
  Q_PROPERTY(quint32 dataType MEMBER m_DataType)
  // Bits per pixel
  Q_PROPERTY(quint8 bpp READ bpp WRITE setBPP)
  // Does FITS have WSC header?
  Q_PROPERTY(bool hasWCS READ hasWCS)
  // Does FITS have bayer data?
  Q_PROPERTY(bool hasDebyaer READ hasDebayer)

  public:
    explicit FITSData(FITSMode fitsMode = FITS_NORMAL);
    explicit FITSData(const FITSData *other);
    ~FITSData();

    /** Structure to hold FITS Header records */
    typedef struct
    {
      QString key;      /** FITS Header Key */
      QVariant value;   /** FITS Header Value */
      QString comment;  /** FITS Header Comment, if any */
    } Record;

    /// Stats struct to hold statisical data about the FITS data
    typedef struct
    {
        double min[3] = {0}, max[3] = {0};
        double mean[3] = {0};
        double stddev[3] = {0};
        double median[3] = {0};
        double SNR { 0 };
        int bitpix { 8 };
        int bytesPerPixel { 1 };
        int ndim { 2 };
        int64_t size { 0 };
        uint32_t samples_per_channel { 0 };
        uint16_t width { 0 };
        uint16_t height { 0 };
    } Statistic;

    /**
     * @brief loadFITS Loading FITS file asynchronously.
     * @param inFilename Path to FITS file (or compressed fits.gz)
     * @param silent If set, error messages are ignored. If set to false, the error message will get displayed in a popup.
     * @return A QFuture that can be watched until the async operation is complete.
     */
    QFuture<bool> loadFITS(const QString &inFilename, bool silent = true);
    /* Save FITS */
    int saveFITS(const QString &newFilename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);
    /* Calculate stats */
    void calculateStats(bool refresh = false);
    /* Check if a particular point exists within the image */
    bool contains(const QPointF &point) const;

    // Access functions
    void clearImageBuffers();
    void setImageBuffer(uint8_t *buffer);
    uint8_t *getImageBuffer();    

    // Statistics
    void saveStatistics(Statistic &other);
    void restoreStatistics(Statistic &other);

    uint16_t width() const { return stats.width;}
    uint16_t height() const { return stats.height;}
    int64_t size() const { return stats.size; }
    int channels() const { return m_Channels; }
    double getMin(uint8_t channel = 0) const { return stats.min[channel]; }
    double getMax(uint8_t channel = 0) const { return stats.max[channel]; }
    void setMinMax(double newMin, double newMax, uint8_t channel = 0);
    void getMinMax(double *min, double *max, uint8_t channel = 0) const
    {
        *min = stats.min[channel];
        *max = stats.max[channel];
    }
    void setStdDev(double value, uint8_t channel = 0) { stats.stddev[channel] = value; }
    double getStdDev(uint8_t channel = 0) const { return stats.stddev[channel]; }
    void setMean(double value, uint8_t channel = 0) { stats.mean[channel] = value; }
    double getMean(uint8_t channel = 0) const { return stats.mean[channel]; }
    void setMedian(double val, uint8_t channel = 0) { stats.median[channel] = val; }
    double getMedian(uint8_t channel = 0) const { return stats.median[channel]; }

    int getBytesPerPixel() const { return stats.bytesPerPixel; }
    void setSNR(double val) { stats.SNR = val; }
    double getSNR() const { return stats.SNR; }
    void setBPP(uint8_t value) { stats.bitpix = value; }
    uint32_t bpp() const { return stats.bitpix; }
    double getADU() const;

    // FITS Record
    bool getRecordValue(const QString &key, QVariant &value) const;
    const QList<Record*> & getRecords() const {return records;}

    // Star Detection - Native KStars implementation
    void setStarAlgorithm(StarAlgorithm algorithm){ starAlgorithm = algorithm; }
    int getDetectedStars() const { return starCenters.count(); }
    bool areStarsSearched() const { return starsSearched; }
    void appendStar(Edge *newCenter) { starCenters.append(newCenter); }
    QList<Edge *> getStarCenters() const { return starCenters; }
    QList<Edge *> getStarCentersInSubFrame(QRect subFrame) const;

    int findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID, const QRect &trackingBox = QRect());

    void getCenterSelection(int *x, int *y);
    int findOneStar(const QRect &boundary);

    // Star Detection - Partially customized Canny edge detection algorithm
    static int findCannyStar(FITSData *data, const QRect &boundary = QRect());
    template <typename T>
    static int findCannyStar(FITSData *data, const QRect &boundary);

    // Use SEP (Sextractor Library) to find stars
    template <typename T>
    void getFloatBuffer(float *buffer, int x, int y, int w, int h);
    int findSEPStars(const QRect &boundary = QRect());

    // Half Flux Radius
    Edge *getMaxHFRStar() const { return maxHFRStar; }
    double getHFR(HFRType type = HFR_AVERAGE);
    double getHFR(int x, int y);

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

// Histogram
#ifndef KSTARS_LITE
    void setHistogram(FITSHistogram *inHistogram) { histogram = inHistogram; }
#endif

    // Filter
    void applyFilter(FITSScale type, uint8_t *image = nullptr, double *min = nullptr, double *max = nullptr);

    // Rotation counter. We keep count to rotate WCS keywords on save
    int getRotCounter() const;
    void setRotCounter(int value);

    // Filename
    const QString &filename() const { return m_Filename; }
    bool isTempFile() const {return m_isTemporary;}
    bool isCompressed() const {return m_isCompressed;}

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
    static QImage FITSToImage(const QString &m_Filename);

    bool getAutoRemoveTemporaryFITS() const;
    void setAutoRemoveTemporaryFITS(bool value);

    QString getLastError() const;

  signals:
    void converted(QImage);

  private:
    bool privateLoad(bool silent);
    void rotWCSFITS(int angle, int mirror);
    bool checkCollision(Edge *s1, Edge *s2);
    int calculateMinMax(bool refresh = false);
    bool checkDebayer();
    void readWCSKeys();

    // FITS Record
    bool parseHeader();
    //int getFITSRecord(QString &recordList, int &nkeys);

    // Templated functions
    template <typename T>
    bool debayer();

    template <typename T>
    bool rotFITS(int rotate, int mirror);

    // Apply Filter
    template <typename T>
    void applyFilter(FITSScale type, uint8_t *targetImage, double image_min, double image_max);
    // Star Detect - Centroid
    template <typename T>
    int findCentroid(const QRect &boundary, int initStdDev, int minEdgeWidth);
    int findCentroid(const QRect &boundary = QRect(), int initStdDev = MINIMUM_STDVAR,
                      int minEdgeWidth = MINIMUM_PIXEL_RANGE);
    // Star Detect - Threshold
    template <typename T>
    int findOneStar(const QRect &boundary);

    template <typename T>
    void calculateMinMax();

    template <typename T>
    QPair<T,T> getParitionMinMax(uint32_t start, uint32_t stride);

    /* Calculate running average & standard deviation using Welford’s method for computing variance */
    template <typename T>
    void runningAverageStdDev();
    template <typename T>
    QPair<double,double> getSquaredSumAndMean(uint32_t start, uint32_t stride);

    // Sobel detector by Gonzalo Exequiel Pedone
    template <typename T>
    void sobel(QVector<float> &gradient, QVector<float> &direction);

    template <typename T>
    void convertToQImage(double dataMin, double dataMax, double scale, double zero, QImage &image);

    // Give unique IDs to each contiguous region
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
    uint32_t m_DataType { 0 };
    /// Number of channels
    uint8_t m_Channels { 1 };
    /// Generic data image buffer
    uint8_t *m_ImageBuffer { nullptr };
    /// Is this a temporary file or one loaded from disk?
    bool m_isTemporary { false };
    /// is this file compress (.fits.fz)?
    bool m_isCompressed { false };
    /// Did we search for stars yet?
    bool starsSearched { false };
    ///Star Selection Algorithm
    StarAlgorithm starAlgorithm { ALGORITHM_GRADIENT };
    /// Do we have WCS keywords in this FITS data?
    bool HasWCS { false };
    /// Is the image debayarable?
    bool HasDebayer { false };
    /// Is WCS data loaded?
    bool WCSLoaded { false };
    /// Do we need to mark stars for the user?
    bool markStars { false };

    /// Our very own file name
    QString m_Filename;
    /// FITS Mode (Normal, WCS, Guide, Focus..etc)
    FITSMode m_Mode;

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
    QList<Edge *> localStarCenters;
    /// The biggest fattest star in the image.
    Edge *maxHFRStar { nullptr };

    uint8_t *bayerBuffer { nullptr };
    /// Bayer parameters
    BayerParams debayerParams;

    Statistic stats;

    // A list of header records
    QList<Record*> records;

    /// Remove temporary files after closing
    bool autoRemoveTemporaryFITS { true };

    QString lastError;

    static const QString m_TemporaryPath;
};
