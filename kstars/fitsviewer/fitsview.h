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

#include <QFutureWatcher>
#include <QEvent>
#include <QGestureEvent>
#include <QGestureEvent>
#include <QPinchGesture>

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
#include "fitsdata.h"

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
    void centerTelescope(double raJ2000, double decJ2000);
    bool getMouseButtonDown();

protected:

    virtual void mouseMoveEvent(QMouseEvent *e);
    virtual void mousePressEvent(QMouseEvent *e);
    virtual void mouseReleaseEvent(QMouseEvent *e);
    virtual void mouseDoubleClickEvent(QMouseEvent *e);

private:
    bool mouseButtonDown=false;
    QPoint lastMousePoint;
    FITSView *image;
    dms ra;
    dms dec;
    double width,height,size;

signals:
    void newStatus(const QString &msg, FITSBar id);
    void pointSelected(int x, int y);
    void markerSelected(int x, int y);
};

class FITSView : public QScrollArea
{
    Q_OBJECT
public:    

    FITSView(QWidget *parent = 0, FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE);
    ~FITSView();

    /* Loads FITS image, scales it, and displays it in the GUI */
    bool  loadFITS(const QString &filename, bool silent=true);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    int rescale(FITSZoom type);    

    void setImageData(FITSData *d) { imageData = d; }

    // Access functions
    FITSData *getImageData() { return imageData; }
    double getCurrentZoom() { return currentZoom; }
    QImage * getDisplayImage() { return display_image; }    

    // Tracking square
    void setTrackingBoxEnabled(bool enable);
    bool isTrackingBoxEnabled() { return trackingBoxEnabled; }
    QPixmap & getTrackingBoxPixmap();
    void setTrackingBox(const QRect & rect);
    const QRect & getTrackingBox() { return trackingBox; }

    // Overlay
    virtual void drawOverlay(QPainter *);

    // Overlay objects
    void drawStarCentroid(QPainter *);
    void drawTrackingBox(QPainter *);
    void drawMarker(QPainter *);    
    void drawCrosshair(QPainter *);
    void drawEQGrid(QPainter *);
    void drawObjectNames(QPainter *painter);
    void drawPixelGrid(QPainter *painter);

    bool isCrosshairShown();
    bool areObjectsShown();
    bool isEQGridShown();
    bool isPixelGridShown();
    bool imageHasWCS();    

    void updateFrame();

    bool isTelescopeActive();

    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);
    int getMouseMode();
    void setMouseMode(int mode);
    void updateMouseCursor();

    static const int dragMouse=0;
    static const int selectMouse=1;
    static const int scopeMouse=2;

    // Zoom related
    void cleanUpZoom(QPoint viewCenter);
    QPoint getImagePoint(QPoint viewPortPoint);

    // Star Detection
    int findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID);
    void toggleStars(bool enable);
    void setStarsEnabled(bool enable) { markStars  = enable; }

    // FITS Mode
    void updateMode(FITSMode mode);
    FITSMode getMode() { return mode;}

    void setFilter(FITSScale newFilter) { filter = newFilter;}

    void setFirstLoad(bool value);

    void pushFilter(FITSScale value) { filterStack.push(value); }
    FITSScale popFilter() { return filterStack.pop(); }

    // Floating toolbar
    void createFloatingToolBar();

protected:
    void wheelEvent(QWheelEvent* event);
    void resizeEvent(QResizeEvent * event);

    QFutureWatcher<bool> wcsWatcher;                // WCS Future Watcher
    QPointF markerCrosshair;                        // Cross hair
    FITSData *imageData;                            // Pointer to FITSData object
    double currentZoom;                             // Current zoom level

public slots:
    void ZoomIn();
    void ZoomOut();
    void ZoomDefault();
    void ZoomToFit();

    // Grids
    void toggleEQGrid();
    void toggleObjects();
    void togglePixelGrid();
    void toggleCrosshair();

    void centerTelescope();

    void processPointSelection(int x, int y);
    void processMarkerSelection(int x, int y);

protected slots:
    void handleWCSCompletion();

private:
    QLabel *noImageLabel=new QLabel();
    QPixmap noImage;

    bool event(QEvent *event);
    bool gestureEvent(QGestureEvent *event);
    void pinchTriggered(QPinchGesture *gesture);
    void updateScopeButton();

    template<typename T> int rescale(FITSZoom type);

    double average();
    double stddev();
    void calculateMaxPixel(double min, double max);
    void initDisplayImage();

    QVector<QPointF> eqGridPoints;

    FITSLabel *image_frame;

    int image_width, image_height;

    uint16_t currentWidth,currentHeight; /* Current width and height due to zoom */
    const double zoomFactor;           /* Image zoom factor */    

    int data_type;                     /* FITS data type when opened */
    QImage  *display_image;            /* FITS image that is displayed in the GUI */
    FITSHistogram *histogram;

    double maxPixel, minPixel;

    bool firstLoad;
    bool markStars;
    bool showCrosshair=false;
    bool showObjects=false;
    bool showEQGrid=false;
    bool showPixelGrid=false;
    bool starsSearched=false;

    QPointF getPointForGridLabel();
    bool pointIsInImage(QPointF pt, bool scaled);

    int mouseMode=1;
    bool zooming=false;
    int zoomTime=0;
    QPoint zoomLocation;

    QString filename;
    FITSMode mode;
    FITSScale filter;

    QStack<FITSScale> filterStack;



    // Star selection algorithm
    StarAlgorithm starAlgorithm = ALGORITHM_GRADIENT;

    // Tracking box
    bool trackingBoxEnabled;
    bool trackingBoxUpdated;
    QRect trackingBox;
    QPixmap trackingBoxPixmap;

    // Floating toolbar
    QToolBar *floatingToolBar = NULL;
    QAction *centerTelescopeAction = NULL;    

signals:
    void newStatus(const QString &msg, FITSBar id);
    void debayerToggled(bool);
    void wcsToggled(bool);
    void actionUpdated(const QString &name, bool enable);
    void trackingStarSelected(int x, int y);

  friend class FITSLabel;
};

#endif
