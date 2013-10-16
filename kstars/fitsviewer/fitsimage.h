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

#ifndef FITSIMAGE_H_
#define FITSIMAGE_H_

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QScrollArea>
#include <QLabel>

#include <kxmlguiwindow.h>
#include <kurl.h>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>
#include "fitshistogram.h"
#include "fitscommon.h"

#include "skypoint.h"
#include "dms.h"

#define INITIAL_W	640
#define INITIAL_H	480

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

class FITSImage
{
public:
    FITSImage(FITSMode mode=FITS_NORMAL);
    ~FITSImage();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool  loadFITS(const QString &filename, QProgressDialog *progress=NULL);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);
    /* Calculate stats */
    void calculateStats(bool refresh=false);

    void subtract(float *darkFrame);

    // Access functions
    float * getImageBuffer() { return image_buffer; }
    void getSize(double *w, double *h) { *w = stats.dim[0]; *h = stats.dim[1]; }
    void getMinMax(double *min, double *max) { *min = stats.min; *max = stats.max; }
    double getMin() { return stats.min; }
    double getMax() { return stats.max; }
    int getDetectedStars() { return starCenters.count(); }
    QList<Edge*> getStarCenters() { return starCenters;}
    long getWidth() { return stats.dim[0]; }
    long getHeight() { return stats.dim[1]; }
    double getStdDev() { return stats.stddev; }
    double getAverage() { return stats.average; }
    int getBPP() { return stats.bitpix; }
    FITSMode getMode() { return mode;}


    int getFITSRecord(QString &recordList, int &nkeys);

    // Set functions
    void setFITSMinMax(double newMin,  double newMax);

    void setHistogram(FITSHistogram *inHistogram) { histogram = inHistogram; }
    void applyFilter(FITSScale type, float *image=NULL, int min=-1, int max=-1);


    // Star Detection & HFR
    int findStars();
    double getHFR(HFRType type=HFR_AVERAGE);
    void findCentroid(int initStdDev=MINIMUM_STDVAR, int minEdgeWidth=MINIMUM_PIXEL_RANGE);
    void getCenterSelection(int *x, int *y);

    // WCS
    bool hasWCS() { return HasWCS; }
    wcs_point *getWCSCoord()  { return wcs_coord; }

    /* stats struct to hold statisical data about the FITS data */
    struct
    {
        double min, max;
        double average;
        double stddev;
        int bitpix;
        int ndim;
        long dim[2];
    } stats;

private:


    bool checkCollision(Edge* s1, Edge*s2);
    double average();
    double stddev();
    int calculateMinMax(bool refresh=false);
    void checkWCS();
    void readWCSKeys();

    bool markStars;
    float *image_buffer;				/* scaled image buffer (0-255) range */
    fitsfile* fptr;
    int data_type;                     /* FITS data type when opened */
    FITSHistogram *histogram;
    bool tempFile;
    bool starsSearched;
    bool HasWCS;
    QString filename;
    FITSMode mode;

    wcs_point *wcs_coord;
    QList<Edge*> starCenters;

};

#endif
