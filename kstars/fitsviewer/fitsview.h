/*  FITS Label
    Copyright (C) 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/


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

class FITSLabel;

class FITSView : public QScrollArea
{
        Q_OBJECT
    public:

        FITSView(QWidget * parent = 0, FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE);
        ~FITSView();

        /* Loads FITS image, scales it, and displays it in the GUI */
        bool  loadFITS(const QString &filename, bool silent=true);
        /* Save FITS */
        int saveFITS(const QString &filename);
        /* Rescale image lineary from image_buffer, fit to window if desired */
        int rescale(FITSZoom type);

        void setImageData(FITSData * d)
        {
            imageData = d;
        }

        // Access functions
        FITSData * getImageData()
        {
            return imageData;
        }
        double getCurrentZoom()
        {
            return currentZoom;
        }
        QImage * getDisplayImage()
        {
            return display_image;
        }

        // Tracking square
        void setTrackingBoxEnabled(bool enable);
        bool isTrackingBoxEnabled()
        {
            return trackingBoxEnabled;
        }
        QPixmap &getTrackingBoxPixmap();
        void setTrackingBox(const QRect &rect);
        const QRect &getTrackingBox()
        {
            return trackingBox;
        }

        // Overlay
        virtual void drawOverlay(QPainter *);

        // Overlay objects
        void drawStarCentroid(QPainter *);
        void drawTrackingBox(QPainter *);
        void drawMarker(QPainter *);
        void drawCrosshair(QPainter *);
        void drawEQGrid(QPainter *);
        void drawObjectNames(QPainter * painter);
        void drawPixelGrid(QPainter * painter);

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

        void updateScopeButton();
        void setScopeButton(QAction * action)
        {
            centerTelescopeAction=action;
        }
        int lastMouseMode;

        // Zoom related
        void cleanUpZoom(QPoint viewCenter);
        QPoint getImagePoint(QPoint viewPortPoint);

        // Star Detection
        int findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID);
        void toggleStars(bool enable);
        void setStarsEnabled(bool enable);

        // FITS Mode
        void updateMode(FITSMode mode);
        FITSMode getMode()
        {
            return mode;
        }

        void setFilter(FITSScale newFilter)
        {
            filter = newFilter;
        }

        void setFirstLoad(bool value);

        void pushFilter(FITSScale value)
        {
            filterStack.push(value);
        }
        FITSScale popFilter()
        {
            return filterStack.pop();
        }

        // Floating toolbar
        void createFloatingToolBar();

        void setLoadWCSEnabled(bool value);

protected:
        void wheelEvent(QWheelEvent * event);
        void resizeEvent(QResizeEvent * event);

        QFutureWatcher<bool> wcsWatcher;                // WCS Future Watcher
        bool loadWCSEnabled=true;                       // Load WCS data?
        QPointF markerCrosshair;                        // Cross hair
        FITSData * imageData;                           // Pointer to FITSData object
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

        // Stars
        void toggleStars();

        void centerTelescope();

        void processPointSelection(int x, int y);
        void processMarkerSelection(int x, int y);

    protected slots:
        void handleWCSCompletion();

    private:
        QLabel * noImageLabel=new QLabel();
        QPixmap noImage;

        bool event(QEvent * event);
        bool gestureEvent(QGestureEvent * event);
        void pinchTriggered(QPinchGesture * gesture);


        template<typename T> int rescale(FITSZoom type);

        double average();
        double stddev();
        void calculateMaxPixel(double min, double max);
        void initDisplayImage();

        QVector<QPointF> eqGridPoints;

        FITSLabel * image_frame;

        int image_width, image_height;

        uint16_t currentWidth,currentHeight; /* Current width and height due to zoom */
        const double zoomFactor;           /* Image zoom factor */

        int data_type;                     /* FITS data type when opened */
        QImage * display_image;            /* FITS image that is displayed in the GUI */
        FITSHistogram * histogram;

        double maxPixel, minPixel;

        bool firstLoad;
        bool markStars=false;
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

        // Scope pixmap
        QPixmap scopePixmap;

        // Floating toolbar
        QToolBar * floatingToolBar = nullptr;
        QAction * centerTelescopeAction = nullptr;

    signals:
        void newStatus(const QString &msg, FITSBar id);
        void debayerToggled(bool);
        void wcsToggled(bool);
        void actionUpdated(const QString &name, bool enable);
        void trackingStarSelected(int x, int y);

        friend class FITSLabel;
};

#endif
