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

#define INITIAL_W	640
#define INITIAL_H	480

class FITSImage;

class FITSLabel : public QLabel
{
     Q_OBJECT
public:
    explicit FITSLabel(FITSImage *img, QWidget *parent=NULL);
    virtual ~FITSLabel();

protected:
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);

private:
    FITSImage *image;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void pointSelected(int x, int y);
};

class Edge
{
public:
    int x;
    int y;
    int val;
    int scanned;
    int width;
    double HFR;
    double sum;
};

class FITSImage : public QScrollArea
{
    Q_OBJECT
public:
    FITSImage(QWidget *parent = 0, FITSMode mode=FITS_NORMAL);
    ~FITSImage();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool  loadFITS(const QString &filename);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);
    /* Calculate stats */
    void calculateStats(bool refresh=false);

    void subtract(FITSImage *darkFrame);

    // Access functions
    double getCurrentZoom() { return currentZoom; }
    float * getImageBuffer() { return image_buffer; }
    void getSize(double *w, double *h) { *w = stats.dim[0]; *h = stats.dim[1]; }
    void getMinMax(double *min, double *max) { *min = stats.min; *max = stats.max; }
    int getDetectedStars() { return starCenters.count(); }
    QList<Edge*> getStarCenters() { return starCenters;}
    long getWidth() { return stats.dim[0]; }
    long getHeight() { return stats.dim[1]; }
    double getStdDev() { return stats.stddev; }
    double getAverage() { return stats.average; }
    QImage * getDisplayImage() { return displayImage; }
    FITSMode getMode() { return mode;}

    int getFITSRecord(QString &recordList, int &nkeys);

    // Set functions
    void setFITSMinMax(double newMin,  double newMax);

    void setGuideBoxSize(int size);

    void setHistogram(FITSHistogram *inHistogram) { histogram = inHistogram; }
    void applyFilter(FITSScale type, float *image=NULL, int min=-1, int max=-1);




    // Overlay
    void drawOverlay(QPainter *);
    void drawStarCentroid(QPainter *);
    void drawGuideBox(QPainter *);
    void updateFrame();

    // Star Detection & HFR
    void toggleStars(bool enable) { markStars = enable;}
    double getHFR(HFRType type=HFR_AVERAGE);
    void findCentroid();

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


public slots:
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();
    void setGuideSquare(int x, int y);

private:


    bool checkCollision(Edge* s1, Edge*s2);
    double average();
    double stddev();
    int calculateMinMax(bool refresh=false);

    bool markStars;
    FITSLabel *image_frame;
    float *image_buffer;				/* scaled image buffer (0-255) range */
    double currentWidth,currentHeight; /* Current width and height due to zoom */
    const double zoomFactor;           /* Image zoom factor */
    double currentZoom;                /* Current Zoom level */
    fitsfile* fptr;
    int data_type;                     /* FITS data type when opened */
    QImage  *displayImage;             /* FITS image that is displayed in the GUI */
    FITSHistogram *histogram;
    int guide_x, guide_y, guide_box;
    bool firstLoad;
    bool tempFile;
    QString filename;



    FITSMode mode;

    QList<Edge*> starCenters;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void actionUpdated(const QString &name, bool enable);
    void guideStarSelected(int x, int y);
};




#endif
