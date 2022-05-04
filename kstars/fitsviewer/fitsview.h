/*
    SPDX-FileCopyrightText: 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    SPDX-FileCopyrightText: 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "fitscommon.h"

#include <config-kstars.h>
#include "stretch.h"

#ifdef HAVE_DATAVISUALIZATION
#include "starprofileviewer.h"
#endif

#include <QFutureWatcher>
#include <QPixmap>
#include <QScrollArea>
#include <QStack>
#include <QTimer>
#include <QPointer>

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include <fitsio.h>

#include <memory>

class QAction;
class QEvent;
class QGestureEvent;
class QImage;
class QLabel;
class QPinchGesture;
class QResizeEvent;
class QToolBar;

class FITSData;
class FITSLabel;

class FITSView : public QScrollArea
{
        Q_OBJECT
        Q_PROPERTY(bool suspended MEMBER m_Suspended)

    public:
        explicit FITSView(QWidget *parent = nullptr, FITSMode fitsMode = FITS_NORMAL, FITSScale filterType = FITS_NONE);
        virtual ~FITSView() override;

        typedef enum {dragCursor, selectCursor, scopeCursor, crosshairCursor } CursorMode;

        /**
         * @brief loadFITS Loads FITS data and displays it in a FITSView frame
         * @param inFilename FITS File name
         * @param silent if set, error popups are suppressed.
         * @note If image is successfully, loaded() signal is emitted, otherwise failed() signal is emitted.
         * Obtain error by calling lastError()
         */
        void loadFile(const QString &inFilename, bool silent = true);

        /**
         * @brief loadFITSFromData Takes ownership of the FITSData instance passed in and displays it in a FITSView frame
         * @param data pointer to FITSData objects
         */
        bool loadData(const QSharedPointer<FITSData> &data);

        /**
         * @brief clearView Reset view to NO IMAGE
         */
        void clearData();

        // Save FITS
        bool saveImage(const QString &newFilename);
        // Rescale image lineary from image_buffer, fit to window if desired
        bool rescale(FITSZoom type);

        const QSharedPointer<FITSData> &imageData() const
        {
            return m_ImageData;
        }

        double getCurrentZoom() const
        {
            return currentZoom;
        }
        const QImage &getDisplayImage() const
        {
            return rawImage;
        }
        const QPixmap &getDisplayPixmap() const
        {
            return displayPixmap;
        }

        // Tracking square
        void setTrackingBoxEnabled(bool enable);
        bool isTrackingBoxEnabled() const
        {
            return trackingBoxEnabled;
        }
        QPixmap &getTrackingBoxPixmap(uint8_t margin = 0);
        void setTrackingBox(const QRect &rect);
        const QRect &getTrackingBox() const
        {
            return trackingBox;
        }

        // last error
        const QString &lastError() const
        {
            return m_LastError;
        }

        // Overlay
        virtual void drawOverlay(QPainter *, double scale);

        // Overlay objects
        void drawStarFilter(QPainter *, double scale);
        void drawStarCentroid(QPainter *, double scale);
        void drawClipping(QPainter *);
        void drawTrackingBox(QPainter *, double scale);
        void drawMarker(QPainter *, double scale);
        void drawCrosshair(QPainter *, double scale);

#if !defined(KSTARS_LITE) && defined(HAVE_WCSLIB)
        void drawEQGrid(QPainter *, double scale);
#endif
        void drawObjectNames(QPainter *painter, double scale);
        void drawPixelGrid(QPainter *painter, double scale);
        void drawMagnifyingGlass(QPainter *painter, double scale);

        bool isImageStretched();
        bool isCrosshairShown();
        bool isClippingShown();
        bool areObjectsShown();
        bool isEQGridShown();
        bool isPixelGridShown();
        bool imageHasWCS();

        // Setup the graphics.
        void updateFrame(bool now = false);

        bool isTelescopeActive();

        void enterEvent(QEvent *event) override;
        void leaveEvent(QEvent *event) override;
        CursorMode getCursorMode();
        void setCursorMode(CursorMode mode);
        void updateMouseCursor();

        void updateScopeButton();
        void setScopeButton(QAction *action)
        {
            centerTelescopeAction = action;
        }

        // Zoom related
        void cleanUpZoom(QPoint viewCenter = QPoint());
        QPoint getImagePoint(QPoint viewPortPoint);
        uint16_t zoomedWidth()
        {
            return currentWidth;
        }
        uint16_t zoomedHeight()
        {
            return currentHeight;
        }
        double ZoomFactor() const
        {
            return m_ZoomFactor;
        }


        // Star Detection
        QFuture<bool> findStars(StarAlgorithm algorithm = ALGORITHM_CENTROID, const QRect &searchBox = QRect());
        void toggleStars(bool enable);
        void searchStars();
        void setStarsEnabled(bool enable);
        void setStarsHFREnabled(bool enable);
        void setStarFilterRange(float const innerRadius, float const outerRadius);
        int filterStars();

        // FITS Mode
        void updateMode(FITSMode fmode);
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

        CursorMode lastMouseMode { selectCursor };
        bool isStarProfileShown()
        {
            return showStarProfile;
        }
        // Floating toolbar
        void createFloatingToolBar();

        //void setLoadWCSEnabled(bool value);

        // Returns the params set to stretch the image.
        StretchParams getStretchParams() const
        {
            return stretchParams;
        }

        // Returns true if we're automatically generating stretch parameters.
        // Note: this is not whether we're stretching, that's controlled by stretchImage.
        bool getAutoStretch() const
        {
            return autoStretch;
        }

        // Sets the params for stretching. Will also stretch and re-display the image.
        // This only sets the first channel stretch params. For RGB images, the G&B channel
        // stretch parameters are a function of the Red input param and the existing RGB params.
        void setStretchParams(const StretchParams &params);

        // Sets whether to stretch the image or not.
        // Will also re-display the image if onOff != stretchImage.
        void setStretch(bool onOff);

        // Automatically generates stretch parameters and use them to re-display the image.
        void setAutoStretchParams();

        // When sampling is > 1, we will display the image at a lower resolution.
        // When sampling = 0, reset to the adaptive sampling value
        void setPreviewSampling(uint8_t value)
        {
            if (value == 0)
            {
                m_PreviewSampling = m_AdaptiveSampling;
                m_StretchingInProgress = false;
            }
            else
            {
                m_PreviewSampling = value * m_AdaptiveSampling;
                m_StretchingInProgress = true;
            }
        }

    public slots:
        void wheelEvent(QWheelEvent *event) override;
        void resizeEvent(QResizeEvent *event) override;
        void ZoomIn();
        void ZoomOut();
        void ZoomDefault();
        void ZoomToFit();
        void updateMagnifyingGlass(int x, int y);

        // Grids
        void toggleEQGrid();
        void toggleObjects();
        void togglePixelGrid();
        void toggleCrosshair();

        // Stars
        void toggleStars();
        void toggleStarProfile();
        void viewStarProfile();

        void centerTelescope();

        void toggleStretch();
        void toggleClipping();

        virtual void processPointSelection(int x, int y);
        virtual void processMarkerSelection(int x, int y);
        void move3DTrackingBox(int x, int y);
        void resizeTrackingBox(int newSize);

    protected slots:
        /**
             * @brief syncWCSState Update toolbar and actions depending on whether WCS is available or not
             */
        void syncWCSState();

        bool event(QEvent *event) override;
        bool gestureEvent(QGestureEvent *event);
        void pinchTriggered(QPinchGesture *gesture);

    protected:
        double average();
        double stddev();
        void calculateMaxPixel(double min, double max);
        void initDisplayImage();

        QPointF getPointForGridLabel(QPainter *painter, const QString &str, double scale);
        bool pointIsInImage(QPointF pt, double scale);

        void loadInFrame();

        double getScale();

        /// Floating toolbar
        QToolBar *floatingToolBar { nullptr };
        /// WCS Future Watcher
        QFutureWatcher<bool> wcsWatcher;
        /// FITS Future Watcher
        QFutureWatcher<bool> fitsWatcher;
        /// Cross hair
        QPointF markerCrosshair;
        /// Pointer to FITSData object
        QSharedPointer<FITSData> m_ImageData;
        /// Current zoom level
        double currentZoom { 0 };
        // The maximum percent zoom. The value is recalculated in the constructor
        // based on the amount of physical memory.
        int zoomMax { 400 };

    private:
        bool processData();
        void doStretch(QImage *outputImage);
        double scaleSize(double size);
        bool isLargeImage();
        void updateFrameLargeImage();
        void updateFrameSmallImage();
        bool drawHFR(QPainter * painter, const QString &hfr, int x, int y);

        QPointer<QLabel> noImageLabel;
        QPixmap noImage;
        QPointer<FITSLabel> m_ImageFrame;
        QVector<QPointF> eqGridPoints;

        /// Current width due to zoom
        uint16_t currentWidth { 0 };
        /// Current height due to zoom
        uint16_t currentHeight { 0 };
        /// Image zoom factor
        const double m_ZoomFactor;

        // Original full-size image
        QImage rawImage;
        // Actual pixmap after all the overlays
        QPixmap displayPixmap;

        bool firstLoad { true };
        bool markStars { false };
        bool showStarProfile { false };
        bool showCrosshair { false };
        bool showObjects { false };
        bool showEQGrid { false };
        bool showPixelGrid { false };
        bool showStarsHFR { false };
        bool showClipping { false };

        // Should the image be displayed in linear (false) or stretched (true).
        // Initial value controlled by Options::autoStretch.
        bool stretchImage { false };

        // When stretching, should we automatically compute parameters.
        // When first displaying, this should be true, but may be set to false
        // if the user has overridden the automatically set parameters.
        bool autoStretch { true };

        // Params for stretching image.
        StretchParams stretchParams;

        // Resolution for display. Sampling=2 means display every other sample.
        uint8_t m_PreviewSampling { 1 };
        bool m_StretchingInProgress { false};
        // Adaptive sampling is based on available RAM
        uint8_t m_AdaptiveSampling {1};

        struct
        {
            bool used() const
            {
                return innerRadius != 0.0f || outerRadius != 1.0f;
            }
            float innerRadius { 0.0f };
            float outerRadius { 1.0f };
        } starFilter;

        CursorMode cursorMode { selectCursor };
        bool zooming { false };
        int zoomTime { 0 };
        QPoint zoomLocation;

        QString filename;
        FITSMode mode;
        FITSScale filter;
        QString m_LastError;
        QTimer m_UpdateFrameTimer;

        QStack<FITSScale> filterStack;

        // Tracking box
        bool trackingBoxEnabled { false };
        QRect trackingBox;
        QPixmap trackingBoxPixmap;

        // Scope pixmap
        QPixmap redScopePixmap;
        // Magenta Scope Pixmap
        QPixmap magentaScopePixmap;

        QAction *centerTelescopeAction { nullptr };
        QAction *toggleEQGridAction { nullptr };
        QAction *toggleObjectsAction { nullptr };
        QAction *toggleStarsAction { nullptr };
        QAction *toggleProfileAction { nullptr };
        QAction *toggleStretchAction { nullptr };

        // State for the magnifying glass overlay.
        int magnifyingGlassX { -1 };
        int magnifyingGlassY { -1 };
        bool showMagnifyingGlass { false };
        bool m_Suspended {false};

        QMutex updateMutex;

        //Star Profile Viewer
#ifdef HAVE_DATAVISUALIZATION
        QPointer<StarProfileViewer> starProfileWidget;
#endif

    signals:
        void newStatus(const QString &msg, FITSBar id);
        void debayerToggled(bool);
        void wcsToggled(bool);
        void actionUpdated(const QString &name, bool enable);
        void trackingStarSelected(int x, int y);
        void loaded();
        void failed();
        void starProfileWindowClosed();

        friend class FITSLabel;
};
