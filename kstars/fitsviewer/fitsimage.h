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

class FITSViewer;
class FITSImage;


class FITSLabel : public QLabel
{
public:
    explicit FITSLabel(FITSImage *img, QWidget *parent=NULL);
    virtual ~FITSLabel();

protected:
    virtual void mouseMoveEvent(QMouseEvent *e);

private:
    FITSImage *image;
};

class FITSImage : public QScrollArea
{
    Q_OBJECT
public:
    FITSImage(QWidget *parent = 0);
    ~FITSImage();

    enum scaleType { FITSAuto = 0 , FITSLinear, FITSLog, FITSSqrt, FITSCustom };
    enum zoomType { ZOOM_FIT_WINDOW, ZOOM_KEEP_LEVEL, ZOOM_FULL };

    /* Loads FITS image, scales it, and displays it in the GUI */
    int  loadFits(const QString &filename);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(zoomType type);
    /* Calculate stats */
    void calculateStats();


    // Access functions
    FITSViewer * getViewer() { return viewer; }
    double getCurrentZoom() { return currentZoom; }
    float * getImageBuffer() { return image_buffer; }
    void getFITSSize(double *w, double *h) { *w = stats.dim[0]; *h = stats.dim[1]; }
    void getFITSMinMax(double *min, double *max) { *min = stats.min; *max = stats.max; }
    long getWidth() { return stats.dim[0]; }
    long getHeight() { return stats.dim[1]; }
    double getStdDev() { return stats.stddev; }
    double getAverage() { return stats.average; }
    QImage * getDisplayImage() { return displayImage; }
    int getFITSRecord(QString &recordList, int &nkeys);

    // Set functions
    void setFITSMinMax(double newMin,  double newMax);

    /* TODO Make this stat PRIVATE
    stats struct to hold statisical data about the FITS data */
    struct
    {
        double min, max;
        double average;
        double stddev;
        int bitpix;
        int ndim;
        long dim[2];
    } stats;

public slots:
    void fitsZoomIn();
    void fitsZoomOut();
    void fitsZoomDefault();

private:

    double average();
    double stddev();

    int calculateMinMax(bool refresh=false);

    FITSViewer *viewer;                 /* parent FITSViewer */
    FITSLabel *image_frame;
    float *image_buffer;				/* scaled image buffer (0-255) range */

    double currentWidth,currentHeight; /* Current width and height due to zoom */
    const double zoomFactor;           /* Image zoom factor */
    double currentZoom;                /* Current Zoom level */
    fitsfile* fptr;
    int data_type;                     /* FITS data type when opened */
    QImage  *displayImage;             /* FITS image that is displayed in the GUI */
};

#endif
