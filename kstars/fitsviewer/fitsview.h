/***************************************************************************
                          FITSView.cpp  -  FITS Image
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

#ifndef FITSView_H_
#define FITSView_H_

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QScrollArea>
#include <QLabel>

#include <kxmlguiwindow.h>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>
#include "fitshistogram.h"
#include "fitscommon.h"

#include "dms.h"
#include "fitsimage.h"

#define INITIAL_W	640
#define INITIAL_H	480

#define MINIMUM_PIXEL_RANGE 5
#define MINIMUM_STDVAR  5

class FITSView;


class FITSLabel : public QLabel
{
     Q_OBJECT
public:
    explicit FITSLabel(FITSView *img, QWidget *parent=NULL);
    virtual ~FITSLabel();
    void setSize(double w, double h);

protected:
    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);

private:
    FITSView *image;
    dms ra;
    dms dec;
    double width,height,size;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void pointSelected(int x, int y);


};

class FITSView : public QScrollArea
{
    Q_OBJECT
public:
    FITSView(QWidget *parent = 0, FITSMode mode=FITS_NORMAL);
    ~FITSView();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool  loadFITS(const QString &filename);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);

    void setImageData(FITSImage *d) { image_data = d; }

    // Access functions
    FITSImage *getImageData() { return image_data; }
    double getCurrentZoom() { return currentZoom; }
    QImage * getDisplayImage() { return display_image; }
    FITSMode getMode() { return mode;}

    void setGuideBoxSize(int size);
    void setGuideSquare(int x, int y);

    // Overlay
    void drawOverlay(QPainter *);
    void drawStarCentroid(QPainter *);
    void drawGuideBox(QPainter *);
    void updateFrame();

    // Star Detection
    void toggleStars(bool enable);

    void updateMode(FITSMode mode);


public slots:
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();

    void processPointSelection(int x, int y);

private:

    double average();
    double stddev();

    bool markStars;
    FITSLabel *image_frame;
    FITSImage *image_data;
    int image_width, image_height;
    double currentWidth,currentHeight; /* Current width and height due to zoom */
    const double zoomFactor;           /* Image zoom factor */
    double currentZoom;                /* Current Zoom level */
    int data_type;                     /* FITS data type when opened */
    QImage  *display_image;             /* FITS image that is displayed in the GUI */
    FITSHistogram *histogram;
    int guide_x, guide_y, guide_box;
    bool firstLoad;
    bool starsSearched;
    bool hasWCS;
    QString filename;
    FITSMode mode;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void actionUpdated(const QString &name, bool enable);
    void guideStarSelected(int x, int y);

  friend class FITSLabel;
};

#endif
