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

#ifndef FITSDATA_H_
#define FITSDATA_H_

#include <config-kstars.h>

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QScrollArea>
#include <QLabel>
#include <QStringList>

#include "skyobject.h"

#ifndef KSTARS_LITE
#ifdef HAVE_WCSLIB
#include <wcs.h>
#endif


#include <kxmlguiwindow.h>
#include "fitshistogram.h"
#endif

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>
#include "fitscommon.h"

#include "skypoint.h"
#include "dms.h"
#include "bayer.h"

#define MINIMUM_PIXEL_RANGE 5
#define MINIMUM_STDVAR  5

class QProgressDialog;

typedef struct
{
    double ra;
    double dec;
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

class FITSSkyObject : public QObject{
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

    typedef enum { CHANNEL_ONE, CHANNEL_TWO, CHANNEL_THREE } ColorChannel;    

    FITSData(FITSMode mode=FITS_NORMAL);
    ~FITSData();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool  loadFITS(const QString &filename, bool silent=true);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);
    /* Calculate stats */
    void calculateStats(bool refresh=false);    

    // Access functions
    void clearImageBuffers();
    void setImageBuffer(uint8_t *buffer);
    uint8_t * getImageBuffer();

    int getDataType() { return data_type; }
    void setDataType(int value) { data_type = value; }

    // Stats
    unsigned int getSize() { return stats.samples_per_channel; }
    void getDimensions(uint16_t *w, uint16_t *h) { *w = stats.width; *h = stats.height; }
    void setWidth(uint16_t w) { stats.width = w; stats.samples_per_channel = stats.width * stats.height;}
    void setHeight(uint16_t h) { stats.height = h; stats.samples_per_channel = stats.width * stats.height;}
    uint16_t getWidth() { return stats.width; }
    uint16_t getHeight() { return stats.height; }

    // Statistics
    int getNumOfChannels() { return channels;}
    void setMinMax(double newMin,  double newMax, uint8_t channel=0);
    void getMinMax(double *min, double *max,uint8_t channel=0) { *min = stats.min[channel]; *max = stats.max[channel]; }
    double getMin(uint8_t channel=0) { return stats.min[channel]; }
    double getMax(uint8_t channel=0) { return stats.max[channel]; }
    void setStdDev(double value, uint8_t channel=0) { stats.stddev[channel] = value;}
    double getStdDev(uint8_t channel=0) { return stats.stddev[channel]; }
    void setMean(double value, uint8_t channel=0) { stats.mean[channel] = value; }
    double getMean(uint8_t channel=0) { return stats.mean[channel]; }
    void setMedian(double val, uint8_t channel=0) { stats.median[channel] = val;}
    double getMedian(uint8_t channel=0) { return stats.median[channel];}

    int getBytesPerPixel() { return stats.bytesPerPixel; }
    void setSNR(double val) { stats.SNR = val;}
    double getSNR() { return stats.SNR;}
    void setBPP(int value) { stats.bitpix = value;}
    int getBPP() { return stats.bitpix; }
    double getADU();

    // Star detection
    int getDetectedStars() { return starCenters.count(); }
    bool areStarsSearched() { return starsSearched; }
    void appendStar(Edge* newCenter) { starCenters.append(newCenter); }
    QList<Edge*> getStarCenters() { return starCenters;}
    int findStars(const QRectF &boundary = QRectF(), bool force=false);
    void findCentroid(const QRectF &boundary = QRectF(), int initStdDev=MINIMUM_STDVAR, int minEdgeWidth=MINIMUM_PIXEL_RANGE);
    void getCenterSelection(int *x, int *y);
    int findOneStar(const QRectF &boundary);

    // Find single star based on partially customized Canny edge detection
    static int findCannyStar(FITSData *data, const QRect &boundary = QRect());
    template<typename T> static int findCannyStar(FITSData *data, const QRect &boundary);

    // Half Flux Radius
    Edge * getMaxHFRStar() { return maxHFRStar;}
    double getHFR(HFRType type=HFR_AVERAGE);
    double getHFR(int x, int y);

    // FITS Mode (Normal, Guide, Focus..etc).
    FITSMode getMode() { return mode;}

    // WCS
    bool checkWCS();
    bool hasWCS() { return HasWCS; }
    wcs_point *getWCSCoord()  { return wcs_coord; }

    // Debayer
    bool hasDebayer() { return HasDebayer; }
    bool debayer();
    bool debayer_8bit();
    bool debayer_16bit();
    void getBayerParams(BayerParams *param);
    void setBayerParams(BayerParams *param);

    // FITS Record
    int getFITSRecord(QString &recordList, int &nkeys);

   // QVariant getFITSHeaderValue(QString &keyword);

    // Histogram
    #ifndef KSTARS_LITE
    void setHistogram(FITSHistogram *inHistogram) { histogram = inHistogram; }
    #endif

    // Filter
    void applyFilter(FITSScale type, uint8_t *image=NULL, float * min= NULL, float * max= NULL);

    // Rotation counter. We keep count to rotate WCS keywords on save
    int getRotCounter() const;
    void setRotCounter(int value);

    // Filename
    const QString & getFilename() { return filename; }

    // Horizontal flip counter. We keep count to rotate WCS keywords on save
    int getFlipHCounter() const;
    void setFlipHCounter(int value);

    // Horizontal flip counter. We keep count to rotate WCS keywords on save
    int getFlipVCounter() const;
    void setFlipVCounter(int value);        

    #ifndef KSTARS_LITE
    #ifdef HAVE_WCSLIB
    void findObjectsInImage(struct wcsprm *wcs, double world[], double phi, double theta, double imgcrd[], double pixcrd[], int stat[]);
    #endif
    #endif
    QList<FITSSkyObject *> getSkyObjects();
    QList<FITSSkyObject*> objList;//Does this need to be public??

    // Create autostretch image from FITS File
    static QImage FITSToImage(const QString &filename);

private:

    void rotWCSFITS (int angle, int mirror);
    bool checkCollision(Edge* s1, Edge*s2);
    int calculateMinMax(bool refresh=false);    
    bool checkDebayer();
    void readWCSKeys();

    // Templated functions

    template<typename T> bool debayer();

    template<typename T> bool rotFITS (int rotate, int mirror);

    // Apply Filter
    template<typename T> void applyFilter(FITSScale type, uint8_t *targetImage, float image_min, float image_max);
    // Star Detect - Centroid
    template<typename T> void findCentroid(const QRectF &boundary, int initStdDev, int minEdgeWidth);
    // Star Detect - Threshold
    template<typename T> int findOneStar(const QRectF &boundary);


    template<typename T> void calculateMinMax();
    /* Calculate running average & standard deviation using Welford’s method for computing variance */
    template<typename T> void runningAverageStdDev();

    // Sobel detector by Gonzalo Exequiel Pedone
    template<typename T> void sobel(QVector<float> &gradient, QVector<float> &direction);

    // Give unique IDs to each contigous region
    int partition(int width, int height, QVector<float> &gradient, QVector<int> &ids);
    void trace(int width, int height, int id, QVector<float> &image, QVector<int> &ids, int x, int y);

    #if 0
    QVector<int> thinning(int width, int height, const QVector<int> &gradient, const QVector<int> &direction);
    QVector<float> threshold(int thLow, int thHi, const QVector<float> &image);
    QVector<int> hysteresis(int width, int height, const QVector<int> &image);    
    #endif

    #ifndef KSTARS_LITE
    FITSHistogram *histogram = NULL;    // Pointer to the FITS data histogram
    #endif
    fitsfile* fptr;                     // Pointer to CFITSIO FITS file struct

    int data_type;                      // FITS image data type (TBYTE, TUSHORT, TINT, TFLOAT, TLONG, TDOUBLE)
    int channels;                       // Number of channels    
    uint8_t *imageBuffer = NULL;        // Generic data image buffer


    bool tempFile;                      // Is this a tempoprary file or one loaded from disk?
    bool starsSearched;                 // Did we search for stars yet?
    bool HasWCS;                        // Do we have WCS keywords in this FITS data?
    bool markStars;                     // Do we need to mark stars for the user?
    bool HasDebayer;                    // Is the image debayarable?

    QString filename;                   // Our very own file name
    FITSMode mode;                      // FITS Mode (Normal, WCS, Guide, Focus..etc)

    int rotCounter;                     // How many times the image was rotated? Useful for WCS keywords rotation on save.
    int flipHCounter;                   // How many times the image was flipped horizontally?
    int flipVCounter;                   // How many times the image was flipped vertically?

    wcs_point *wcs_coord;               // Pointer to WCS coordinate data, if any.
    QList<Edge*> starCenters;           // All the stars we detected, if any.
    Edge* maxHFRStar;                   // The biggest fattest star in the image.

    uint8_t *bayerBuffer = NULL;
    BayerParams debayerParams;          // Bayer parameters

    /* stats struct to hold statisical data about the FITS data */
    struct
    {
        double min[3], max[3];
        double mean[3];
        double stddev[3];
        double median[3];
        double SNR;
        int bitpix;
        int bytesPerPixel;
        int ndim;
        uint32_t samples_per_channel;
        uint16_t width;
        uint16_t height;
    } stats;
};

#endif
