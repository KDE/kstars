/*  FITS View
    Copyright (C) 2003-2017 Jasem Mutlaq <mutlaqja@ikarustech.com>
    Copyright (C) 2016-2017 Robert Lancaster <rlancaste@gmail.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#include "config-kstars.h"
#include "fitsview.h"

#include "fitsdata.h"
#include "fitslabel.h"
#include "kspopupmenu.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "Options.h"
#include "skymap.h"
#include "fits_debug.h"
#include "stretch.h"

#ifdef HAVE_INDI
#include "basedevice.h"
#include "indi/indilistener.h"
#endif

#include <KActionCollection>

#include <QtConcurrent>
#include <QScrollBar>
#include <QToolBar>
#include <QGraphicsOpacityEffect>
#include <QApplication>
#include <QGestureEvent>

#include <unistd.h>

#define BASE_OFFSET    50
#define ZOOM_DEFAULT   100.0f
#define ZOOM_MIN       10
// ZOOM_MAX is adjusted in the constructor if the amount of physical memory is known.
#define ZOOM_MAX       300
#define ZOOM_LOW_INCR  10
#define ZOOM_HIGH_INCR 50
#define FONT_SIZE      14

namespace
{

// Derive the Green and Blue stretch parameters from their previous values and the
// changes made to the Red parameters. We apply the same offsets used for Red to the
// other channels' parameters, but clip them.
void ComputeGBStretchParams(const StretchParams &newParams, StretchParams* params)
{
    float shadow_diff = newParams.grey_red.shadows - params->grey_red.shadows;
    float highlight_diff = newParams.grey_red.highlights - params->grey_red.highlights;
    float midtones_diff = newParams.grey_red.midtones - params->grey_red.midtones;

    params->green.shadows = params->green.shadows + shadow_diff;
    params->green.shadows = KSUtils::clamp(params->green.shadows, 0.0f, 1.0f);
    params->green.highlights = params->green.highlights + highlight_diff;
    params->green.highlights = KSUtils::clamp(params->green.highlights, 0.0f, 1.0f);
    params->green.midtones = params->green.midtones + midtones_diff;
    params->green.midtones = std::max(params->green.midtones, 0.0f);

    params->blue.shadows = params->blue.shadows + shadow_diff;
    params->blue.shadows = KSUtils::clamp(params->blue.shadows, 0.0f, 1.0f);
    params->blue.highlights = params->blue.highlights + highlight_diff;
    params->blue.highlights = KSUtils::clamp(params->blue.highlights, 0.0f, 1.0f);
    params->blue.midtones = params->blue.midtones + midtones_diff;
    params->blue.midtones = std::max(params->blue.midtones, 0.0f);
}

}  // namespace

// Runs the stretch checking the variables to see which parameters to use.
// We call stretch even if we're not stretching, as the stretch code still
// converts the image to the uint8 output image which will be displayed.
// In that case, it will use an identity stretch.
void FITSView::doStretch(FITSData *data, QImage *outputImage)
{
    if (outputImage->isNull())
        return;
    Stretch stretch(static_cast<int>(data->width()),
                    static_cast<int>(data->height()),
                    data->channels(), data->property("dataType").toInt());

    StretchParams tempParams;
    if (!stretchImage)
        tempParams = StretchParams();  // Keeping it linear
    else if (autoStretch)
    {
        // Compute new auto-stretch params.
        stretchParams = stretch.computeParams(data->getImageBuffer());
        tempParams = stretchParams;
    }
    else
        // Use the existing stretch params.
        tempParams = stretchParams;

    stretch.setParams(tempParams);
    stretch.run(data->getImageBuffer(), outputImage, sampling);
}

// Store stretch parameters, and turn on stretching if it isn't already on.
void FITSView::setStretchParams(const StretchParams &params)
{
    if (imageData->channels() == 3)
        ComputeGBStretchParams(params, &stretchParams);

    stretchParams.grey_red = params.grey_red;
    stretchParams.grey_red.shadows = std::max(stretchParams.grey_red.shadows, 0.0f);
    stretchParams.grey_red.highlights = std::max(stretchParams.grey_red.highlights, 0.0f);
    stretchParams.grey_red.midtones = std::max(stretchParams.grey_red.midtones, 0.0f);

    autoStretch = false;
    stretchImage = true;

    if (image_frame != nullptr && rescale(ZOOM_KEEP_LEVEL))
        updateFrame();
}

// Turn on or off stretching, and if on, use whatever parameters are currently stored.
void FITSView::setStretch(bool onOff)
{
    if (stretchImage != onOff)
    {
        stretchImage = onOff;
        if (image_frame != nullptr && rescale(ZOOM_KEEP_LEVEL))
            updateFrame();
    }
}

// Turn on stretching, using automatically generated parameters.
void FITSView::setAutoStretchParams()
{
    stretchImage = true;
    autoStretch = true;
    if (image_frame != nullptr && rescale(ZOOM_KEEP_LEVEL))
        updateFrame();
}

FITSView::FITSView(QWidget * parent, FITSMode fitsMode, FITSScale filterType) : QScrollArea(parent), zoomFactor(1.2)
{
    // stretchImage is whether to stretch or not--the stretch may or may not use automatically generated parameters.
    // The user may enter his/her own.
    stretchImage = Options::autoStretch();
    // autoStretch means use automatically-generated parameters. This is the default, unless the user overrides
    // by adjusting the stretchBar's sliders.
    autoStretch = true;

    // Adjust the maximum zoom according to the amount of memory.
    // There have been issues with users running out system memory because of zoom memory.
    // Note: this is not currently image dependent. It's possible, but not implemented,
    // to allow for more zooming on smaller images.
    zoomMax = ZOOM_MAX;

#if defined (Q_OS_LINUX) || defined (Q_OS_OSX)
    const long numPages = sysconf(_SC_PAGESIZE);
    const long pageSize = sysconf(_SC_PHYS_PAGES);

    // _SC_PHYS_PAGES "may not be standard" http://man7.org/linux/man-pages/man3/sysconf.3.html
    // If an OS doesn't support it, sysconf should return -1.
    if (numPages > 0 && pageSize > 0)
    {
        // (numPages * pageSize) will likely overflow a 32bit int, so use floating point calculations.
        const int memoryMb = numPages * (static_cast<double>(pageSize) / 1e6);
        if (memoryMb < 2000)
            zoomMax = 100;
        else if (memoryMb < 4000)
            zoomMax = 200;
        else if (memoryMb < 8000)
            zoomMax = 300;
        else if (memoryMb < 16000)
            zoomMax = 400;
        else
            zoomMax = 600;
    }
#endif

    grabGesture(Qt::PinchGesture);

    image_frame.reset(new FITSLabel(this));
    filter = filterType;
    mode   = fitsMode;

    setBackgroundRole(QPalette::Dark);

    markerCrosshair.setX(0);
    markerCrosshair.setY(0);

    setBaseSize(740, 530);

    connect(image_frame.get(), SIGNAL(newStatus(QString, FITSBar)), this, SIGNAL(newStatus(QString, FITSBar)));
    connect(image_frame.get(), SIGNAL(pointSelected(int, int)), this, SLOT(processPointSelection(int, int)));
    connect(image_frame.get(), SIGNAL(markerSelected(int, int)), this, SLOT(processMarkerSelection(int, int)));
    connect(&wcsWatcher, SIGNAL(finished()), this, SLOT(syncWCSState()));

    connect(&fitsWatcher, &QFutureWatcher<bool>::finished, this, &FITSView::loadInFrame);

    image_frame->setMouseTracking(true);
    setCursorMode(
        selectCursor); //This is the default mode because the Focus and Align FitsViews should not be in dragMouse mode

    noImageLabel = new QLabel();
    noImage.load(":/images/noimage.png");
    noImageLabel->setPixmap(noImage);
    noImageLabel->setAlignment(Qt::AlignCenter);
    this->setWidget(noImageLabel);

    redScopePixmap = QPixmap(":/icons/center_telescope_red.svg").scaled(32, 32, Qt::KeepAspectRatio, Qt::FastTransformation);
    magentaScopePixmap = QPixmap(":/icons/center_telescope_magenta.svg").scaled(32, 32, Qt::KeepAspectRatio,
                         Qt::FastTransformation);
}

FITSView::~FITSView()
{
    fitsWatcher.waitForFinished();
    wcsWatcher.waitForFinished();
    delete (imageData);
}

/**
This method looks at what mouse mode is currently selected and updates the cursor to match.
 */

void FITSView::updateMouseCursor()
{
    if (cursorMode == dragCursor)
    {
        if (horizontalScrollBar()->maximum() > 0 || verticalScrollBar()->maximum() > 0)
        {
            if (!image_frame->getMouseButtonDown())
                viewport()->setCursor(Qt::PointingHandCursor);
            else
                viewport()->setCursor(Qt::ClosedHandCursor);
        }
        else
            viewport()->setCursor(Qt::CrossCursor);
    }
    else if (cursorMode == selectCursor)
    {
        viewport()->setCursor(Qt::CrossCursor);
    }
    else if (cursorMode == scopeCursor)
    {
        viewport()->setCursor(QCursor(redScopePixmap, 10, 10));
    }
    else if (cursorMode == crosshairCursor)
    {
        viewport()->setCursor(QCursor(magentaScopePixmap, 10, 10));
    }
}

/**
This is how the mouse mode gets set.
The default for a FITSView in a FITSViewer should be the dragMouse
The default for a FITSView in the Focus or Align module should be the selectMouse
The different defaults are accomplished by putting making the actual default mouseMode
the selectMouse, but when a FITSViewer loads an image, it immediately makes it the dragMouse.
 */

void FITSView::setCursorMode(CursorMode mode)
{
    cursorMode = mode;
    updateMouseCursor();

    if (mode == scopeCursor && imageHasWCS())
    {
        if (!imageData->isWCSLoaded() && !wcsWatcher.isRunning())
        {
            QFuture<bool> future = QtConcurrent::run(imageData, &FITSData::loadWCS);
            wcsWatcher.setFuture(future);
        }
    }
}

void FITSView::resizeEvent(QResizeEvent * event)
{
    if ((imageData == nullptr) && noImageLabel != nullptr)
    {
        noImageLabel->setPixmap(
            noImage.scaled(width() - 20, height() - 20, Qt::KeepAspectRatio, Qt::FastTransformation));
        noImageLabel->setFixedSize(width() - 5, height() - 5);
    }

    QScrollArea::resizeEvent(event);
}


void FITSView::loadFITS(const QString &inFilename, bool silent)
{
    if (floatingToolBar != nullptr)
    {
        floatingToolBar->setVisible(true);
    }

    bool setBayerParams = false;

    BayerParams param;
    if ((imageData != nullptr) && imageData->hasDebayer())
    {
        setBayerParams = true;
        imageData->getBayerParams(&param);
    }

    // In case image is still loading, wait until it is done.
    fitsWatcher.waitForFinished();
    // In case loadWCS is still running for previous image data, let's wait until it's over
    wcsWatcher.waitForFinished();

    delete imageData;
    imageData = nullptr;

    filterStack.clear();
    filterStack.push(FITS_NONE);
    if (filter != FITS_NONE)
        filterStack.push(filter);

    imageData = new FITSData(mode);

    if (setBayerParams)
        imageData->setBayerParams(&param);

    fitsWatcher.setFuture(imageData->loadFITS(inFilename, silent));
}

bool FITSView::loadFITSFromData(FITSData *data, const QString &inFilename)
{
    Q_UNUSED(inFilename)

    if (floatingToolBar != nullptr)
    {
        floatingToolBar->setVisible(true);
    }

    // In case loadWCS is still running for previous image data, let's wait until it's over
    wcsWatcher.waitForFinished();

    if (imageData != nullptr)
    {
        delete imageData;
        imageData = nullptr;
    }

    filterStack.clear();
    filterStack.push(FITS_NONE);
    if (filter != FITS_NONE)
        filterStack.push(filter);

    // Takes control of the objects passed in.
    imageData = data;

    return processData();
}

bool FITSView::processData()
{
    // Set current width and height
    if (!imageData) return false;
    currentWidth = imageData->width();
    currentHeight = imageData->height();

    int image_width  = currentWidth;
    int image_height = currentHeight;

    image_frame->setSize(image_width, image_height);

    // Init the display image
    // JM 2020.01.08: Disabling as proposed by Hy
    //initDisplayImage();

    imageData->applyFilter(filter);

    // Rescale to fits window on first load
    if (firstLoad)
    {
        currentZoom = 100;

        if (rescale(ZOOM_FIT_WINDOW) == false)
        {
            m_LastError = i18n("Rescaling image failed.");
            return false;
        }

        firstLoad = false;
    }
    else
    {
        if (rescale(ZOOM_KEEP_LEVEL) == false)
        {
            m_LastError = i18n("Rescaling image failed.");
            return false;
        }
    }

    setAlignment(Qt::AlignCenter);

    // Load WCS data now if selected and image contains valid WCS header
    if (imageData->hasWCS() && Options::autoWCS() && (mode == FITS_NORMAL || mode == FITS_ALIGN) && !wcsWatcher.isRunning())
    {
        QFuture<bool> future = QtConcurrent::run(imageData, &FITSData::loadWCS);
        wcsWatcher.setFuture(future);
    }
    else
        syncWCSState();

    if (isVisible())
        emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);

    if (showStarProfile)
    {
        if(floatingToolBar != nullptr)
            toggleProfileAction->setChecked(true);
        //Need to wait till the Focus module finds stars, if its the Focus module.
        QTimer::singleShot(100, this, SLOT(viewStarProfile()));
    }

    updateFrame();
    return true;
}

void FITSView::loadInFrame()
{
    // Check if the loading was OK
    if (fitsWatcher.result() == false)
    {
        m_LastError = imageData->getLastError();
        emit failed();
        return;
    }

    // Notify if there is debayer data.
    emit debayerToggled(imageData->hasDebayer());

    if (processData())
        emit loaded();
    else
        emit failed();
}

bool FITSView::saveImage(const QString &newFilename)
{
    const QString ext = QFileInfo(newFilename).suffix();
    if (ext == "jpg" || ext == "png")
    {
        rawImage.save(newFilename, ext.toLatin1().constData());
        return true;
    }

    return imageData->saveImage(newFilename);
}

bool FITSView::rescale(FITSZoom type)
{
    switch (imageData->property("dataType").toInt())
    {
        case TBYTE:
            return rescale<uint8_t>(type);

        case TSHORT:
            return rescale<int16_t>(type);

        case TUSHORT:
            return rescale<uint16_t>(type);

        case TLONG:
            return rescale<int32_t>(type);

        case TULONG:
            return rescale<uint32_t>(type);

        case TFLOAT:
            return rescale<float>(type);

        case TLONGLONG:
            return rescale<int64_t>(type);

        case TDOUBLE:
            return rescale<double>(type);

        default:
            break;
    }

    return false;
}

FITSView::CursorMode FITSView::getCursorMode()
{
    return cursorMode;
}

void FITSView::enterEvent(QEvent * event)
{
    Q_UNUSED(event)

    if ((floatingToolBar != nullptr) && (imageData != nullptr))
    {
        QPointer<QGraphicsOpacityEffect> eff = new QGraphicsOpacityEffect(this);
        floatingToolBar->setGraphicsEffect(eff);
        QPointer<QPropertyAnimation> a = new QPropertyAnimation(eff, "opacity");
        a->setDuration(500);
        a->setStartValue(0.2);
        a->setEndValue(1);
        a->setEasingCurve(QEasingCurve::InBack);
        a->start(QPropertyAnimation::DeleteWhenStopped);
    }
}

void FITSView::leaveEvent(QEvent * event)
{
    Q_UNUSED(event)

    if ((floatingToolBar != nullptr) && (imageData != nullptr))
    {
        QPointer<QGraphicsOpacityEffect> eff = new QGraphicsOpacityEffect(this);
        floatingToolBar->setGraphicsEffect(eff);
        QPointer<QPropertyAnimation> a = new QPropertyAnimation(eff, "opacity");
        a->setDuration(500);
        a->setStartValue(1);
        a->setEndValue(0.2);
        a->setEasingCurve(QEasingCurve::OutBack);
        a->start(QPropertyAnimation::DeleteWhenStopped);
    }
}

template <typename T>
bool FITSView::rescale(FITSZoom type)
{
    // JM 2020.01.08: Disabling as proposed by Hy
    //    if (rawImage.isNull())
    //        return false;

    if (!imageData) return false;
    int image_width  = imageData->width();
    int image_height = imageData->height();
    currentWidth  = image_width;
    currentHeight = image_height;

    if (isVisible())
        emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);

    switch (type)
    {
        case ZOOM_FIT_WINDOW:
            if ((image_width > width() || image_height > height()))
            {
                double w = baseSize().width() - BASE_OFFSET;
                double h = baseSize().height() - BASE_OFFSET;

                if (!firstLoad)
                {
                    w = viewport()->rect().width() - BASE_OFFSET;
                    h = viewport()->rect().height() - BASE_OFFSET;
                }

                // Find the zoom level which will enclose the current FITS in the current window size
                double zoomX                  = floor((w / static_cast<double>(currentWidth)) * 100.);
                double zoomY                  = floor((h / static_cast<double>(currentHeight)) * 100.);
                (zoomX < zoomY) ? currentZoom = zoomX : currentZoom = zoomY;

                currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
                currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

                if (currentZoom <= ZOOM_MIN)
                    emit actionUpdated("view_zoom_out", false);
            }
            else
            {
                currentZoom   = 100;
                currentWidth  = image_width;
                currentHeight = image_height;
            }
            break;

        case ZOOM_KEEP_LEVEL:
        {
            currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
            currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);
        }
        break;

        default:
            currentZoom = 100;

            break;
    }

    initDisplayImage();
    image_frame->setScaledContents(true);
    doStretch(imageData, &rawImage);
    setWidget(image_frame.get());

    // This is needed by fitstab, even if the zoom doesn't change, to change the stretch UI.
    emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    return true;
}

void FITSView::ZoomIn()
{
    if (currentZoom >= ZOOM_DEFAULT && Options::limitedResourcesMode())
    {
        emit newStatus(i18n("Cannot zoom in further due to active limited resources mode."), FITS_MESSAGE);
        return;
    }

    if (currentZoom < ZOOM_DEFAULT)
        currentZoom += ZOOM_LOW_INCR;
    else
        currentZoom += ZOOM_HIGH_INCR;

    emit actionUpdated("view_zoom_out", true);
    if (currentZoom >= zoomMax)
    {
        currentZoom = zoomMax;
        emit actionUpdated("view_zoom_in", false);
    }

    if (!imageData) return;
    currentWidth  = imageData->width() * (currentZoom / ZOOM_DEFAULT);
    currentHeight = imageData->height() * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
}

void FITSView::ZoomOut()
{
    if (currentZoom <= ZOOM_DEFAULT)
        currentZoom -= ZOOM_LOW_INCR;
    else
        currentZoom -= ZOOM_HIGH_INCR;

    if (currentZoom <= ZOOM_MIN)
    {
        currentZoom = ZOOM_MIN;
        emit actionUpdated("view_zoom_out", false);
    }

    emit actionUpdated("view_zoom_in", true);

    if (!imageData) return;
    currentWidth  = imageData->width() * (currentZoom / ZOOM_DEFAULT);
    currentHeight = imageData->height() * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
}

void FITSView::ZoomToFit()
{
    if (rawImage.isNull() == false)
    {
        rescale(ZOOM_FIT_WINDOW);
        updateFrame();
    }
}

void FITSView::setStarFilterRange(float const innerRadius, float const outerRadius)
{
    starFilter.innerRadius = innerRadius;
    starFilter.outerRadius = outerRadius;
}

int FITSView::filterStars()
{
    return starFilter.used() ? imageData->filterStars(starFilter.innerRadius,
            starFilter.outerRadius) : imageData->getStarCenters().count();
}

// isImageLarge() returns whether we use the large-image rendering strategy or the small-image strategy.
// See the comment below in getScale() for details.
bool FITSView::isLargeImage()
{
    constexpr int largeImageNumPixels = 1000 * 1000;
    return rawImage.width() * rawImage.height() >= largeImageNumPixels;
}

// getScale() is related to the image and overlay rendering strategy used.
// If we're using a pixmap apprpriate for a large image, where we draw and render on a pixmap that's the image size
// and we let the QLabel deal with scaling and zooming, then the scale is 1.0.
// With smaller images, where memory use is not as severe, we create a pixmap that's the size of the scaled image
// and get scale returns the ratio of that pixmap size to the image size.
double FITSView::getScale()
{
    return isLargeImage() ? 1.0 : currentZoom / ZOOM_DEFAULT;
}

// scaleSize() is only used with the large-image rendering strategy. It may increase the line
// widths or font sizes, as we draw lines and render text on the full image and when zoomed out,
// these sizes may be too small.
double FITSView::scaleSize(double size)
{
    if (!isLargeImage())
        return size;
    return currentZoom > 100.0 ? size : std::round(size * 100.0 / currentZoom);
}

void FITSView::updateFrame()
{
    if (toggleStretchAction)
        toggleStretchAction->setChecked(stretchImage);

    // We employ two schemes for managing the image and its overlays, depending on the size of the image
    // and whether we need to therefore conserve memory. The small-image strategy explicitly scales up
    // the image, and writes overlays on the scaled pixmap. The large-image strategy uses a pixmap that's
    // the size of the image itself, never scaling that up.
    if (isLargeImage())
        updateFrameLargeImage();
    else
        updateFrameSmallImage();
}


void FITSView::updateFrameLargeImage()
{
    if (!displayPixmap.convertFromImage(rawImage))
        return;

    QPainter painter(&displayPixmap);

    // Possibly scale the fonts as we're drawing on the full image, not just the visible part of the scroll window.
    QFont font = painter.font();
    font.setPixelSize(scaleSize(FONT_SIZE));
    painter.setFont(font);

    if (sampling == 1)
    {
        drawOverlay(&painter, 1.0);
        drawStarFilter(&painter, 1.0);
    }
    image_frame->setPixmap(displayPixmap);

    image_frame->resize(((sampling * currentZoom) / 100.0) * image_frame->pixmap()->size());
}

void FITSView::updateFrameSmallImage()
{
    QImage scaledImage = rawImage.scaled(currentWidth, currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    if (!displayPixmap.convertFromImage(scaledImage))
        return;

    QPainter painter(&displayPixmap);

    if (sampling == 1)
    {
        drawOverlay(&painter, currentZoom / ZOOM_DEFAULT);
        drawStarFilter(&painter, currentZoom / ZOOM_DEFAULT);
    }
    image_frame->setPixmap(displayPixmap);

    image_frame->resize(currentWidth, currentHeight);
}

void FITSView::drawStarFilter(QPainter *painter, double scale)
{
    if (!starFilter.used())
        return;
    const double w = imageData->width() * scale;
    const double h = imageData->height() * scale;
    double const diagonal = std::sqrt(w * w + h * h) / 2;
    int const innerRadius = std::lround(diagonal * starFilter.innerRadius);
    int const outerRadius = std::lround(diagonal * starFilter.outerRadius);
    QPoint const center(w / 2, h / 2);
    painter->save();
    painter->setPen(QPen(Qt::blue, scaleSize(1), Qt::DashLine));
    painter->setOpacity(0.7);
    painter->setBrush(QBrush(Qt::transparent));
    painter->drawEllipse(center, outerRadius, outerRadius);
    painter->setBrush(QBrush(Qt::blue, Qt::FDiagPattern));
    painter->drawEllipse(center, innerRadius, innerRadius);
    painter->restore();
}

void FITSView::ZoomDefault()
{
    if (image_frame != nullptr)
    {
        emit actionUpdated("view_zoom_out", true);
        emit actionUpdated("view_zoom_in", true);

        currentZoom   = ZOOM_DEFAULT;
        currentWidth  = imageData->width();
        currentHeight = imageData->height();

        updateFrame();

        emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

        update();
    }
}

void FITSView::drawOverlay(QPainter * painter, double scale)
{
    painter->setRenderHint(QPainter::Antialiasing, Options::useAntialias());

    if (trackingBoxEnabled && getCursorMode() != FITSView::scopeCursor)
        drawTrackingBox(painter, scale);

    if (!markerCrosshair.isNull())
        drawMarker(painter, scale);

    if (showCrosshair)
        drawCrosshair(painter, scale);

    if (showObjects)
        drawObjectNames(painter, scale);

    if (showEQGrid)
        drawEQGrid(painter, scale);

    if (showPixelGrid)
        drawPixelGrid(painter, scale);

    if (markStars)
        drawStarCentroid(painter, scale);
}

void FITSView::updateMode(FITSMode fmode)
{
    mode = fmode;
}

void FITSView::drawMarker(QPainter * painter, double scale)
{
    painter->setPen(QPen(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")),
                         scaleSize(2)));
    painter->setBrush(Qt::NoBrush);
    const float pxperdegree = scale * (57.3 / 1.8);

    const float s1 = 0.5 * pxperdegree;
    const float s2 = pxperdegree;
    const float s3 = 2.0 * pxperdegree;

    const float x0 = scale * markerCrosshair.x();
    const float y0 = scale * markerCrosshair.y();
    const float x1 = x0 - 0.5 * s1;
    const float y1 = y0 - 0.5 * s1;
    const float x2 = x0 - 0.5 * s2;
    const float y2 = y0 - 0.5 * s2;
    const float x3 = x0 - 0.5 * s3;
    const float y3 = y0 - 0.5 * s3;

    //Draw radial lines
    painter->drawLine(QPointF(x1, y0), QPointF(x3, y0));
    painter->drawLine(QPointF(x0 + s2, y0), QPointF(x0 + 0.5 * s1, y0));
    painter->drawLine(QPointF(x0, y1), QPointF(x0, y3));
    painter->drawLine(QPointF(x0, y0 + 0.5 * s1), QPointF(x0, y0 + s2));
    //Draw circles at 0.5 & 1 degrees
    painter->drawEllipse(QRectF(x1, y1, s1, s1));
    painter->drawEllipse(QRectF(x2, y2, s2, s2));
}

bool FITSView::drawHFR(QPainter * painter, const QString &hfr, int x, int y)
{
    QRect const boundingRect(0, 0, painter->device()->width(), painter->device()->height());
    QSize const hfrSize = painter->fontMetrics().size(Qt::TextSingleLine, hfr);

    // Store the HFR text in a rect
    QPoint const hfrBottomLeft(x, y);
    QRect const hfrRect(hfrBottomLeft.x(), hfrBottomLeft.y() - hfrSize.height(), hfrSize.width(), hfrSize.height());

    // Render the HFR text only if it can be displayed entirely
    if (boundingRect.contains(hfrRect))
    {
        painter->setPen(QPen(Qt::red, scaleSize(3)));
        painter->drawText(hfrBottomLeft, hfr);
        painter->setPen(QPen(Qt::red, scaleSize(2)));
        return true;
    }
    return false;
}


void FITSView::drawStarCentroid(QPainter * painter, double scale)
{
    QFont painterFont;
    double fontSize = painterFont.pointSizeF() * 2;
    painter->setRenderHint(QPainter::Antialiasing);
    if (showStarsHFR)
    {
        // If we need to print the HFR out, give an arbitrarily sized font to the painter
        if (isLargeImage())
            fontSize = scaleSize(painterFont.pointSizeF());
        painterFont.setPointSizeF(fontSize);
        painter->setFont(painterFont);
    }

    painter->setPen(QPen(Qt::red, scaleSize(2)));

    for (auto const &starCenter : imageData->getStarCenters())
    {
        int const w  = std::round(starCenter->width) * scale;

        // Draw a circle around the detected star.
        // SEP coordinates are in the center of pixels, and Qt at the boundary.
        const double xCoord = starCenter->x - 0.5;
        const double yCoord = starCenter->y - 0.5;
        const int xc = std::round((xCoord - starCenter->width / 2.0f) * scale);
        const int yc = std::round((yCoord - starCenter->width / 2.0f) * scale);
        const int hw = w / 2;

        BahtinovEdge* bEdge = dynamic_cast<BahtinovEdge*>(starCenter);
        if (bEdge != nullptr)
        {
            // Draw lines of diffraction pattern
            painter->setPen(QPen(Qt::red, scaleSize(2)));
            painter->drawLine(bEdge->line[0].x1() * scale, bEdge->line[0].y1() * scale,
                              bEdge->line[0].x2() * scale, bEdge->line[0].y2() * scale);
            painter->setPen(QPen(Qt::green, scaleSize(2)));
            painter->drawLine(bEdge->line[1].x1() * scale, bEdge->line[1].y1() * scale,
                              bEdge->line[1].x2() * scale, bEdge->line[1].y2() * scale);
            painter->setPen(QPen(Qt::darkGreen, scaleSize(2)));
            painter->drawLine(bEdge->line[2].x1() * scale, bEdge->line[2].y1() * scale,
                              bEdge->line[2].x2() * scale, bEdge->line[2].y2() * scale);

            // Draw center circle
            painter->setPen(QPen(Qt::white, scaleSize(2)));
            painter->drawEllipse(xc, yc, w, w);

            // Draw offset circle
            double factor = 15.0;
            QPointF offsetVector = (bEdge->offset - QPointF(starCenter->x, starCenter->y)) * factor;
            int const xo = std::round((starCenter->x + offsetVector.x() - starCenter->width / 2.0f) * scale);
            int const yo = std::round((starCenter->y + offsetVector.y() - starCenter->width / 2.0f) * scale);
            painter->setPen(QPen(Qt::red, scaleSize(2)));
            painter->drawEllipse(xo, yo, w, w);

            // Draw line between center circle and offset circle
            painter->setPen(QPen(Qt::red, scaleSize(2)));
            painter->drawLine(xc + hw, yc + hw, xo + hw, yo + hw);
        }
        else
        {
            const double radius = starCenter->HFR > 0 ? 2.0f * starCenter->HFR * scale : w;
            painter->drawEllipse(QPointF(xCoord * scale, yCoord * scale), radius, radius);
        }

        if (showStarsHFR)
        {
            // Ask the painter how large will the HFR text be
            QString const hfr = QString("%1").arg(starCenter->HFR, 0, 'f', 2);
            if (!drawHFR(painter, hfr, xc + w + 5, yc + w / 2))
            {
                // Try a few more time with smaller fonts;
                for (int i = 0; i < 10; ++i)
                {
                    const double tempFontSize = painterFont.pointSizeF() - 2;
                    if (tempFontSize <= 0) break;
                    painterFont.setPointSizeF(tempFontSize);
                    painter->setFont(painterFont);
                    if (drawHFR(painter, hfr, xc + w + 5, yc + w / 2))
                        break;
                }
                // Reset the font size.
                painterFont.setPointSize(fontSize);
                painter->setFont(painterFont);
            }
        }
    }
}

void FITSView::drawTrackingBox(QPainter * painter, double scale)
{
    painter->setPen(QPen(Qt::green, scaleSize(2)));

    if (trackingBox.isNull())
        return;

    const int x1 = trackingBox.x() * scale;
    const int y1 = trackingBox.y() * scale;
    const int w  = trackingBox.width() * scale;
    const int h  = trackingBox.height() * scale;

    painter->drawRect(x1, y1, w, h);
}

/**
This Method draws a large Crosshair in the center of the image, it is like a set of axes.
 */

void FITSView::drawCrosshair(QPainter * painter, double scale)
{
    if (!imageData) return;
    const int image_width = imageData->width();
    const int image_height = imageData->height();
    const QPointF c   = QPointF((qreal)image_width / 2 * scale, (qreal)image_height / 2 * scale);
    const float midX  = (float)image_width / 2 * scale;
    const float midY  = (float)image_height / 2 * scale;
    const float maxX  = (float)image_width * scale;
    const float maxY  = (float)image_height * scale;
    const float r = 50 * scale;

    painter->setPen(QPen(QColor(KStarsData::Instance()->colorScheme()->colorNamed("TargetColor")), scaleSize(1)));

    //Horizontal Line to Circle
    painter->drawLine(0, midY, midX - r, midY);

    //Horizontal Line past Circle
    painter->drawLine(midX + r, midY, maxX, midY);

    //Vertical Line to Circle
    painter->drawLine(midX, 0, midX, midY - r);

    //Vertical Line past Circle
    painter->drawLine(midX, midY + r, midX, maxY);

    //Circles
    painter->drawEllipse(c, r, r);
    painter->drawEllipse(c, r / 2, r / 2);
}

/**
This method is intended to draw a pixel grid onto the image.  It first determines useful information
from the image.  Then it draws the axes on the image if the crosshairs are not displayed.
Finally it draws the gridlines so that there will be 4 Gridlines on either side of the axes.
Note: This has to start drawing at the center not at the edges because the center axes must
be in the center of the image.
 */

void FITSView::drawPixelGrid(QPainter * painter, double scale)
{
    const float width  = imageData->width() * scale;
    const float height = imageData->height() * scale;
    const float cX     = width / 2;
    const float cY     = height / 2;
    const float deltaX = width / 10;
    const float deltaY = height / 10;
    QFontMetrics fm(painter->font());

    //draw the Axes
    painter->setPen(QPen(Qt::red, scaleSize(1)));
    painter->drawText(cX - 30, height - 5, QString::number((int)((cX) / scale)));
    QString str = QString::number((int)((cY) / scale));
#if QT_VERSION < QT_VERSION_CHECK(5,11,0)
    painter->drawText(width - (fm.width(str) + 10), cY - 5, str);
#else
    painter->drawText(width - (fm.horizontalAdvance(str) + 10), cY - 5, str);
#endif
    if (!showCrosshair)
    {
        painter->drawLine(cX, 0, cX, height);
        painter->drawLine(0, cY, width, cY);
    }
    painter->setPen(QPen(Qt::gray, scaleSize(1)));
    //Start one iteration past the Center and draw 4 lines on either side of 0
    for (int x = deltaX; x < cX - deltaX; x += deltaX)
    {
        painter->drawText(cX + x - 30, height - 5, QString::number((int)(cX + x) / scale));
        painter->drawText(cX - x - 30, height - 5, QString::number((int)(cX - x) / scale));
        painter->drawLine(cX - x, 0, cX - x, height);
        painter->drawLine(cX + x, 0, cX + x, height);
    }
    //Start one iteration past the Center and draw 4 lines on either side of 0
    for (int y = deltaY; y < cY - deltaY; y += deltaY)
    {
        QString str = QString::number((int)((cY + y) / scale));
#if QT_VERSION < QT_VERSION_CHECK(5,11,0)
        painter->drawText(width - (fm.width(str) + 10), cY + y - 5, str);
#else
        painter->drawText(width - (fm.horizontalAdvance(str) + 10), cY + y - 5, str);
#endif
        str = QString::number((int)((cY - y) / scale));
#if QT_VERSION < QT_VERSION_CHECK(5,11,0)
        painter->drawText(width - (fm.width(str) + 10), cY - y - 5, str);
#else
        painter->drawText(width - (fm.horizontalAdvance(str) + 10), cY - y - 5, str);
#endif
        painter->drawLine(0, cY + y, width, cY + y);
        painter->drawLine(0, cY - y, width, cY - y);
    }
}

bool FITSView::imageHasWCS()
{
    if (imageData != nullptr)
        return imageData->hasWCS();
    return false;
}

void FITSView::drawObjectNames(QPainter * painter, double scale)
{
    painter->setPen(QPen(QColor(KStarsData::Instance()->colorScheme()->colorNamed("FITSObjectLabelColor"))));
    foreach (FITSSkyObject * listObject, imageData->getSkyObjects())
    {
        painter->drawRect(listObject->x() * scale - 5, listObject->y() * scale - 5, 10, 10);
        painter->drawText(listObject->x() * scale + 10, listObject->y() * scale + 10, listObject->skyObject()->name());
    }
}

/**
This method will paint EQ Gridlines in an overlay if there is WCS data present.
It determines the minimum and maximum RA and DEC, then it uses that information to
judge which gridLines to draw.  Then it calls the drawEQGridlines methods below
to draw gridlines at those specific RA and Dec values.
 */

void FITSView::drawEQGrid(QPainter * painter, double scale)
{
    const int image_width = imageData->width();
    const int image_height = imageData->height();

    if (imageData->hasWCS())
    {
        wcs_point * wcs_coord = imageData->getWCSCoord();
        if (wcs_coord != nullptr)
        {
            const int size      = image_width * image_height;
            double maxRA  = -1000;
            double minRA  = 1000;
            double maxDec = -1000;
            double minDec = 1000;

            for (int i = 0; i < (size); i++)
            {
                double ra  = wcs_coord[i].ra;
                double dec = wcs_coord[i].dec;
                if (ra > maxRA)
                    maxRA = ra;
                if (ra < minRA)
                    minRA = ra;
                if (dec > maxDec)
                    maxDec = dec;
                if (dec < minDec)
                    minDec = dec;
            }
            auto minDecMinutes = (int)(minDec * 12); //This will force the Dec Scale to 5 arc minutes in the loop
            auto maxDecMinutes = (int)(maxDec * 12);

            auto minRAMinutes =
                (int)(minRA / 15.0 *
                      120.0); //This will force the scale to 1/2 minutes of RA in the loop from 0 to 50 degrees
            auto maxRAMinutes = (int)(maxRA / 15.0 * 120.0);

            double raConvert  = 15 / 120.0; //This will undo the calculation above to retrieve the actual RA.
            double decConvert = 1.0 / 12.0; //This will undo the calculation above to retrieve the actual DEC.

            if (maxDec > 50 || minDec < -50)
            {
                minRAMinutes =
                    (int)(minRA / 15.0 * 60.0); //This will force the scale to 1 min of RA from 50 to 80 degrees
                maxRAMinutes = (int)(maxRA / 15.0 * 60.0);
                raConvert    = 15 / 60.0;
            }

            if (maxDec > 80 || minDec < -80)
            {
                minRAMinutes =
                    (int)(minRA / 15.0 * 30); //This will force the scale to 2 min of RA from 80 to 85 degrees
                maxRAMinutes = (int)(maxRA / 15.0 * 30);
                raConvert    = 15 / 30.0;
            }
            if (maxDec > 85 || minDec < -85)
            {
                minRAMinutes =
                    (int)(minRA / 15.0 * 6); //This will force the scale to 10 min of RA from 85 to 89 degrees
                maxRAMinutes = (int)(maxRA / 15.0 * 6);
                raConvert    = 15 / 6.0;
            }
            if (maxDec >= 89.25 || minDec <= -89.25)
            {
                minRAMinutes =
                    (int)(minRA /
                          15); //This will force the scale to whole hours of RA in the loop really close to the poles
                maxRAMinutes = (int)(maxRA / 15);
                raConvert    = 15;
            }

            painter->setPen(QPen(Qt::yellow));

            QPointF pixelPoint, imagePoint, pPoint;

            //This section draws the RA Gridlines

            for (int targetRA = minRAMinutes; targetRA <= maxRAMinutes; targetRA++)
            {
                painter->setPen(QPen(Qt::yellow));
                double target = targetRA * raConvert;

                if (eqGridPoints.count() != 0)
                    eqGridPoints.clear();

                double increment = std::abs((maxDec - minDec) /
                                            100.0); //This will determine how many points to use to create the RA Line

                for (double targetDec = minDec; targetDec <= maxDec; targetDec += increment)
                {
                    SkyPoint pointToGet(target / 15.0, targetDec);
                    bool inImage = imageData->wcsToPixel(pointToGet, pixelPoint, imagePoint);
                    if (inImage)
                    {
                        QPointF pt(pixelPoint.x() * scale, pixelPoint.y() * scale);
                        eqGridPoints.append(pt);
                    }
                }

                if (eqGridPoints.count() > 1)
                {
                    for (int i = 1; i < eqGridPoints.count(); i++)
                        painter->drawLine(eqGridPoints.value(i - 1), eqGridPoints.value(i));
                    QString str = QString::number(dms(target).hour()) + "h " +
                                  QString::number(dms(target).minute()) + '\'';
                    if  (maxDec <= 50 && maxDec >= -50)
                        str = str + " " + QString::number(dms(target).second()) + "''";
                    QPointF pt = getPointForGridLabel(painter, str, scale);
                    if (pt.x() != -100)
                        painter->drawText(pt.x(), pt.y(), str);
                }
            }

            //This section draws the DEC Gridlines

            for (int targetDec = minDecMinutes; targetDec <= maxDecMinutes; targetDec++)
            {
                if (eqGridPoints.count() != 0)
                    eqGridPoints.clear();

                double increment = std::abs((maxRA - minRA) /
                                            100.0); //This will determine how many points to use to create the Dec Line
                double target    = targetDec * decConvert;

                for (double targetRA = minRA; targetRA <= maxRA; targetRA += increment)
                {
                    SkyPoint pointToGet(targetRA / 15, targetDec * decConvert);
                    bool inImage = imageData->wcsToPixel(pointToGet, pixelPoint, imagePoint);
                    if (inImage)
                    {
                        QPointF pt(pixelPoint.x(), pixelPoint.y());
                        eqGridPoints.append(pt);
                    }
                }
                if (eqGridPoints.count() > 1)
                {
                    for (int i = 1; i < eqGridPoints.count(); i++)
                        painter->drawLine(eqGridPoints.value(i - 1), eqGridPoints.value(i));
                    QString str = QString::number(dms(target).degree()) + "° " + QString::number(dms(target).arcmin()) + '\'';
                    QPointF pt = getPointForGridLabel(painter, str, scale);
                    if (pt.x() != -100)
                        painter->drawText(pt.x(), pt.y(), str);
                }
            }

            //This Section Draws the North Celestial Pole if present
            SkyPoint NCP(0, 90);

            bool NCPtest = imageData->wcsToPixel(NCP, pPoint, imagePoint);
            if (NCPtest)
            {
                bool NCPinImage =
                    (pPoint.x() > 0 && pPoint.x() < image_width) && (pPoint.y() > 0 && pPoint.y() < image_height);
                if (NCPinImage)
                {
                    painter->fillRect(pPoint.x() * scale - 2, pPoint.y() * scale - 2, 4, 4,
                                      KStarsData::Instance()->colorScheme()->colorNamed("TargetColor"));
                    painter->drawText(pPoint.x() * scale + 15, pPoint.y() * scale + 15,
                                      i18nc("North Celestial Pole", "NCP"));
                }
            }

            //This Section Draws the South Celestial Pole if present
            SkyPoint SCP(0, -90);

            bool SCPtest = imageData->wcsToPixel(SCP, pPoint, imagePoint);
            if (SCPtest)
            {
                bool SCPinImage =
                    (pPoint.x() > 0 && pPoint.x() < image_width) && (pPoint.y() > 0 && pPoint.y() < image_height);
                if (SCPinImage)
                {
                    painter->fillRect(pPoint.x() * scale - 2, pPoint.y() * scale - 2, 4, 4,
                                      KStarsData::Instance()->colorScheme()->colorNamed("TargetColor"));
                    painter->drawText(pPoint.x() * scale + 15, pPoint.y() * scale + 15,
                                      i18nc("South Celestial Pole", "SCP"));
                }
            }
        }
    }
}

bool FITSView::pointIsInImage(QPointF pt, double scale)
{
    int image_width = imageData->width();
    int image_height = imageData->height();
    return pt.x() < image_width * scale && pt.y() < image_height * scale && pt.x() > 0 && pt.y() > 0;
}

QPointF FITSView::getPointForGridLabel(QPainter *painter, const QString &str, double scale)
{
    QFontMetrics fm(painter->font());
#if QT_VERSION < QT_VERSION_CHECK(5,11,0)
    int strWidth = fm.width(str);
#else
    int strWidth = fm.horizontalAdvance(str);
#endif
    int strHeight = fm.height();
    int image_width = imageData->width();
    int image_height = imageData->height();

    //These get the maximum X and Y points in the list that are in the image
    QPointF maxXPt(image_width * scale / 2, image_height * scale / 2);
    for (auto &p : eqGridPoints)
    {
        if (p.x() > maxXPt.x() && pointIsInImage(p, scale))
            maxXPt = p;
    }
    QPointF maxYPt(image_width * scale / 2, image_height * scale / 2);

    for (auto &p : eqGridPoints)
    {
        if (p.y() > maxYPt.y() && pointIsInImage(p, scale))
            maxYPt = p;
    }
    QPointF minXPt(image_width * scale / 2, image_height * scale / 2);

    for (auto &p : eqGridPoints)
    {
        if (p.x() < minXPt.x() && pointIsInImage(p, scale))
            minXPt = p;
    }
    QPointF minYPt(image_width * scale / 2, image_height * scale / 2);

    for (auto &p : eqGridPoints)
    {
        if (p.y() < minYPt.y() && pointIsInImage(p, scale))
            minYPt = p;
    }

    //This gives preference to points that are on the right hand side and bottom.
    //But if the line doesn't intersect the right or bottom, it then tries for the top and left.
    //If no points are found in the image, it returns a point off the screen
    //If all else fails, like in the case of a circle on the image, it returns the far right point.

    if (image_width * scale - maxXPt.x() < strWidth)
    {
        return QPointF(
                   image_width * scale - (strWidth + 10),
                   maxXPt.y() -
                   strHeight); //This will draw the text on the right hand side, up and to the left of the point where the line intersects
    }
    if (image_height * scale - maxYPt.y() < strHeight)
        return QPointF(
                   maxYPt.x() - (strWidth + 10),
                   image_height * scale -
                   (strHeight + 10)); //This will draw the text on the bottom side, up and to the left of the point where the line intersects
    if (minYPt.y() < strHeight)
        return QPointF(
                   minYPt.x() * scale + 10,
                   strHeight + 20); //This will draw the text on the top side, down and to the right of the point where the line intersects
    if (minXPt.x() < strWidth)
        return QPointF(
                   10,
                   minXPt.y() * scale +
                   strHeight +
                   20); //This will draw the text on the left hand side, down and to the right of the point where the line intersects
    if (maxXPt.x() == image_width * scale / 2 && maxXPt.y() == image_height * scale / 2)
        return QPointF(-100, -100); //All of the points were off the screen

    return QPoint(maxXPt.x() - (strWidth + 10), maxXPt.y() - (strHeight + 10));
}

void FITSView::setFirstLoad(bool value)
{
    firstLoad = value;
}

QPixmap &FITSView::getTrackingBoxPixmap(uint8_t margin)
{
    if (trackingBox.isNull())
        return trackingBoxPixmap;

    // We need to know which rendering strategy updateFrame used to determine the scaling.
    const float scale = getScale();

    int x1 = (trackingBox.x() - margin) * scale;
    int y1 = (trackingBox.y() - margin) * scale;
    int w  = (trackingBox.width() + margin * 2) * scale;
    int h  = (trackingBox.height() + margin * 2) * scale;

    trackingBoxPixmap = image_frame->grab(QRect(x1, y1, w, h));
    return trackingBoxPixmap;
}

void FITSView::setTrackingBox(const QRect &rect)
{
    if (rect != trackingBox)
    {
        trackingBox        = rect;
        updateFrame();
        if(showStarProfile)
            viewStarProfile();
    }
}

void FITSView::resizeTrackingBox(int newSize)
{
    int x = trackingBox.x() + trackingBox.width() / 2;
    int y = trackingBox.y() + trackingBox.height() / 2;
    int delta = newSize / 2;
    setTrackingBox(QRect( x - delta, y - delta, newSize, newSize));
}

bool FITSView::isImageStretched()
{
    return stretchImage;
}

bool FITSView::isCrosshairShown()
{
    return showCrosshair;
}

bool FITSView::isEQGridShown()
{
    return showEQGrid;
}

bool FITSView::areObjectsShown()
{
    return showObjects;
}

bool FITSView::isPixelGridShown()
{
    return showPixelGrid;
}

void FITSView::toggleCrosshair()
{
    showCrosshair = !showCrosshair;
    updateFrame();
}

void FITSView::toggleEQGrid()
{
    showEQGrid = !showEQGrid;

    if (!imageData->isWCSLoaded() && !wcsWatcher.isRunning())
    {
        QFuture<bool> future = QtConcurrent::run(imageData, &FITSData::loadWCS);
        wcsWatcher.setFuture(future);
        return;
    }

    if (image_frame != nullptr)
        updateFrame();
}

void FITSView::toggleObjects()
{
    showObjects = !showObjects;

    if (!imageData->isWCSLoaded() && !wcsWatcher.isRunning())
    {
        QFuture<bool> future = QtConcurrent::run(imageData, &FITSData::loadWCS);
        wcsWatcher.setFuture(future);
        return;
    }

    if (image_frame != nullptr)
        updateFrame();
}

void FITSView::toggleStars()
{
    toggleStars(!markStars);
    if (image_frame != nullptr)
        updateFrame();
}

void FITSView::toggleStretch()
{
    stretchImage = !stretchImage;
    if (image_frame != nullptr && rescale(ZOOM_KEEP_LEVEL))
        updateFrame();
}

void FITSView::toggleStarProfile()
{
#ifdef HAVE_DATAVISUALIZATION
    showStarProfile = !showStarProfile;
    if(showStarProfile && trackingBoxEnabled)
        viewStarProfile();
    if(toggleProfileAction)
        toggleProfileAction->setChecked(showStarProfile);

    if(showStarProfile)
    {
        //The tracking box is already on for Guide and Focus Views, but off for Normal and Align views.
        //So for Normal and Align views, we need to set up the tracking box.
        if(mode == FITS_NORMAL || mode == FITS_ALIGN)
        {
            setCursorMode(selectCursor);
            connect(this, SIGNAL(trackingStarSelected(int, int)), this, SLOT(move3DTrackingBox(int, int)));
            trackingBox = QRect(0, 0, 128, 128);
            setTrackingBoxEnabled(true);
            if(starProfileWidget)
                connect(starProfileWidget, SIGNAL(sampleSizeUpdated(int)), this, SLOT(resizeTrackingBox(int)));
        }
        if(starProfileWidget)
            connect(starProfileWidget, SIGNAL(rejected()), this, SLOT(toggleStarProfile()));
    }
    else
    {
        //This shuts down the tracking box for Normal and Align Views
        //It doesn't touch Guide and Focus Views because they still need a tracking box
        if(mode == FITS_NORMAL || mode == FITS_ALIGN)
        {
            if(getCursorMode() == selectCursor)
                setCursorMode(dragCursor);
            disconnect(this, SIGNAL(trackingStarSelected(int, int)), this, SLOT(move3DTrackingBox(int, int)));
            setTrackingBoxEnabled(false);
            if(starProfileWidget)
                disconnect(starProfileWidget, SIGNAL(sampleSizeUpdated(int)), this, SLOT(resizeTrackingBox(int)));
        }
        if(starProfileWidget)
        {
            disconnect(starProfileWidget, SIGNAL(rejected()), this, SLOT(toggleStarProfile()));
            starProfileWidget->close();
            starProfileWidget = nullptr;
        }
        emit starProfileWindowClosed();
    }
    updateFrame();
#endif
}

void FITSView::move3DTrackingBox(int x, int y)
{
    int boxSize = trackingBox.width();
    QRect starRect = QRect(x - boxSize / 2, y - boxSize / 2, boxSize, boxSize);
    setTrackingBox(starRect);
}

void FITSView::viewStarProfile()
{
#ifdef HAVE_DATAVISUALIZATION
    if(!trackingBoxEnabled)
    {
        setTrackingBoxEnabled(true);
        setTrackingBox(QRect(0, 0, 128, 128));
    }
    if(!starProfileWidget)
    {
        starProfileWidget = new StarProfileViewer(this);

        //This is a band-aid to fix a QT bug with createWindowContainer
        //It will set the cursor of the Window containing the view that called the Star Profile method to the Arrow Cursor
        //Note that Ekos Manager is a QDialog and FitsViewer is a KXmlGuiWindow
        QWidget * superParent = this->parentWidget();
        while(superParent->parentWidget() != 0 && !superParent->inherits("QDialog") && !superParent->inherits("KXmlGuiWindow"))
            superParent = superParent->parentWidget();
        superParent->setCursor(Qt::ArrowCursor);
        //This is the end of the band-aid

        connect(starProfileWidget, SIGNAL(rejected()), this, SLOT(toggleStarProfile()));
        if(mode == FITS_ALIGN || mode == FITS_NORMAL)
        {
            starProfileWidget->enableTrackingBox(true);
            imageData->setStarAlgorithm(ALGORITHM_CENTROID);
            connect(starProfileWidget, SIGNAL(sampleSizeUpdated(int)), this, SLOT(resizeTrackingBox(int)));
        }
    }
    QList<Edge *> starCenters = imageData->getStarCentersInSubFrame(trackingBox);
    if(starCenters.size() == 0)
    {
        // FIXME, the following does not work anymore.
        //imageData->findStars(&trackingBox, true);
        // FIXME replacing it with this
        imageData->findStars(ALGORITHM_CENTROID, trackingBox);
        starCenters = imageData->getStarCentersInSubFrame(trackingBox);
    }

    starProfileWidget->loadData(imageData, trackingBox, starCenters);
    starProfileWidget->show();
    starProfileWidget->raise();
    if(markStars)
        updateFrame(); //this is to update for the marked stars

#endif
}



void FITSView::togglePixelGrid()
{
    showPixelGrid = !showPixelGrid;
    updateFrame();
}

int FITSView::findStars(StarAlgorithm algorithm, const QRect &searchBox)
{
    int count = 0;

    if(trackingBoxEnabled)
        count = imageData->findStars(algorithm, trackingBox);
    else
        count = imageData->findStars(algorithm, searchBox);

    return count;
}

void FITSView::toggleStars(bool enable)
{
    markStars = enable;

    if (markStars && !imageData->areStarsSearched())
        searchStars();
}

void FITSView::searchStars()
{
    QVariant frameType;
    if (!imageData || !imageData->getRecordValue("FRAME", frameType) || frameType.toString() != "Light")
        return;

    if (!imageData->areStarsSearched())
    {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        emit newStatus(i18n("Finding stars..."), FITS_MESSAGE);
        qApp->processEvents();
        int count = findStars(ALGORITHM_SEP);

        if (count >= 0 && isVisible())
            emit newStatus(i18np("1 star detected. HFR=%2", "%1 stars detected. HFR=%2", count,
                                 imageData->getHFR()), FITS_MESSAGE);
        QApplication::restoreOverrideCursor();
    }
}

void FITSView::processPointSelection(int x, int y)
{
    emit trackingStarSelected(x, y);
}

void FITSView::processMarkerSelection(int x, int y)
{
    markerCrosshair.setX(x);
    markerCrosshair.setY(y);

    updateFrame();
}

void FITSView::setTrackingBoxEnabled(bool enable)
{
    if (enable != trackingBoxEnabled)
    {
        trackingBoxEnabled = enable;
        //updateFrame();
    }
}

void FITSView::wheelEvent(QWheelEvent * event)
{
    //This attempts to send the wheel event back to the Scroll Area if it was taken from a trackpad
    //It should still do the zoom if it is a mouse wheel
    if (event->source() == Qt::MouseEventSynthesizedBySystem)
    {
        QScrollArea::wheelEvent(event);
    }
    else
    {
        QPoint mouseCenter = getImagePoint(event->pos());
        if (event->angleDelta().y() > 0)
            ZoomIn();
        else
            ZoomOut();
        event->accept();
        cleanUpZoom(mouseCenter);
    }
}

/**
This method is intended to keep key locations in an image centered on the screen while zooming.
If there is a marker or tracking box, it centers on those.  If not, it uses the point called
viewCenter that was passed as a parameter.
 */

void FITSView::cleanUpZoom(QPoint viewCenter)
{
    int x0       = 0;
    int y0       = 0;
    double scale = (currentZoom / ZOOM_DEFAULT);
    if (!markerCrosshair.isNull())
    {
        x0 = markerCrosshair.x() * scale;
        y0 = markerCrosshair.y() * scale;
    }
    else if (trackingBoxEnabled)
    {
        x0 = trackingBox.center().x() * scale;
        y0 = trackingBox.center().y() * scale;
    }
    else
    {
        x0 = viewCenter.x() * scale;
        y0 = viewCenter.y() * scale;
    }
    ensureVisible(x0, y0, width() / 2, height() / 2);
    updateMouseCursor();
}

/**
This method converts a point from the ViewPort Coordinate System to the
Image Coordinate System.
 */

QPoint FITSView::getImagePoint(QPoint viewPortPoint)
{
    QWidget * w = widget();

    if (w == nullptr)
        return QPoint(0, 0);

    double scale       = (currentZoom / ZOOM_DEFAULT);
    QPoint widgetPoint = w->mapFromParent(viewPortPoint);
    QPoint imagePoint  = QPoint(widgetPoint.x() / scale, widgetPoint.y() / scale);
    return imagePoint;
}

void FITSView::initDisplayImage()
{
    // Account for leftover when sampling. Thus a 5-wide image sampled by 2
    // would result in a width of 3 (samples 0, 2 and 4).
    int w = (imageData->width() + sampling - 1) / sampling;
    int h = (imageData->height() + sampling - 1) / sampling;

    if (imageData->channels() == 1)
    {
        rawImage = QImage(w, h, QImage::Format_Indexed8);

        rawImage.setColorCount(256);
        for (int i = 0; i < 256; i++)
            rawImage.setColor(i, qRgb(i, i, i));
    }
    else
    {
        rawImage = QImage(w, h, QImage::Format_RGB32);
    }
}

/**
The Following two methods allow gestures to work with trackpads.
Specifically, we are targeting the pinch events, so that if one is generated,
Then the pinchTriggered method will be called.  If the event is not a pinch gesture,
then the event is passed back to the other event handlers.
 */

bool FITSView::event(QEvent * event)
{
    if (event->type() == QEvent::Gesture)
        return gestureEvent(dynamic_cast<QGestureEvent *>(event));
    return QScrollArea::event(event);
}

bool FITSView::gestureEvent(QGestureEvent * event)
{
    if (QGesture * pinch = event->gesture(Qt::PinchGesture))
        pinchTriggered(dynamic_cast<QPinchGesture *>(pinch));
    return true;
}

/**
This Method works with Trackpads to use the pinch gesture to scroll in and out
It stores a point to keep track of the location where the gesture started so that
while you are zooming, it tries to keep that initial point centered in the view.
**/
void FITSView::pinchTriggered(QPinchGesture * gesture)
{
    if (!zooming)
    {
        zoomLocation = getImagePoint(mapFromGlobal(QCursor::pos()));
        zooming      = true;
    }
    if (gesture->state() == Qt::GestureFinished)
    {
        zooming = false;
    }
    zoomTime++;           //zoomTime is meant to slow down the zooming with a pinch gesture.
    if (zoomTime > 10000) //This ensures zoomtime never gets too big.
        zoomTime = 0;
    if (zooming && (zoomTime % 10 == 0)) //zoomTime is set to slow it by a factor of 10.
    {
        if (gesture->totalScaleFactor() > 1)
            ZoomIn();
        else
            ZoomOut();
    }
    cleanUpZoom(zoomLocation);
}

/*void FITSView::handleWCSCompletion()
{
    //bool hasWCS = wcsWatcher.result();
    if(imageData->hasWCS())
        this->updateFrame();
    emit wcsToggled(imageData->hasWCS());
}*/

void FITSView::syncWCSState()
{
    bool hasWCS    = imageData->hasWCS();
    bool wcsLoaded = imageData->isWCSLoaded();

    if (hasWCS && wcsLoaded)
        this->updateFrame();

    emit wcsToggled(hasWCS);

    if (toggleEQGridAction != nullptr)
        toggleEQGridAction->setEnabled(hasWCS);
    if (toggleObjectsAction != nullptr)
        toggleObjectsAction->setEnabled(hasWCS);
    if (centerTelescopeAction != nullptr)
        centerTelescopeAction->setEnabled(hasWCS);
}

void FITSView::createFloatingToolBar()
{
    if (floatingToolBar != nullptr)
        return;

    floatingToolBar             = new QToolBar(this);
    auto * eff = new QGraphicsOpacityEffect(this);
    floatingToolBar->setGraphicsEffect(eff);
    eff->setOpacity(0.2);
    floatingToolBar->setVisible(false);
    floatingToolBar->setStyleSheet(
        "QToolBar{background: rgba(150, 150, 150, 210); border:none; color: yellow}"
        "QToolButton{background: transparent; border:none; color: yellow}"
        "QToolButton:hover{background: rgba(200, 200, 200, 255);border:solid; color: yellow}"
        "QToolButton:checked{background: rgba(110, 110, 110, 255);border:solid; color: yellow}");
    floatingToolBar->setFloatable(true);
    floatingToolBar->setIconSize(QSize(25, 25));
    //floatingToolBar->setMovable(true);

    QAction * action = nullptr;

    floatingToolBar->addAction(QIcon::fromTheme("zoom-in"),
                               i18n("Zoom In"), this, SLOT(ZoomIn()));

    floatingToolBar->addAction(QIcon::fromTheme("zoom-out"),
                               i18n("Zoom Out"), this, SLOT(ZoomOut()));

    floatingToolBar->addAction(QIcon::fromTheme("zoom-fit-best"),
                               i18n("Default Zoom"), this, SLOT(ZoomDefault()));

    floatingToolBar->addAction(QIcon::fromTheme("zoom-fit-width"),
                               i18n("Zoom to Fit"), this, SLOT(ZoomToFit()));

    toggleStretchAction = floatingToolBar->addAction(QIcon::fromTheme("transform-move"),
                          i18n("Toggle Stretch"),
                          this, SLOT(toggleStretch()));
    toggleStretchAction->setCheckable(true);


    floatingToolBar->addSeparator();

    action = floatingToolBar->addAction(QIcon::fromTheme("crosshairs"),
                                        i18n("Show Cross Hairs"), this, SLOT(toggleCrosshair()));
    action->setCheckable(true);

    action = floatingToolBar->addAction(QIcon::fromTheme("map-flat"),
                                        i18n("Show Pixel Gridlines"), this, SLOT(togglePixelGrid()));
    action->setCheckable(true);

    toggleStarsAction =
        floatingToolBar->addAction(QIcon::fromTheme("kstars_stars"),
                                   i18n("Detect Stars in Image"), this, SLOT(toggleStars()));
    toggleStarsAction->setCheckable(true);

#ifdef HAVE_DATAVISUALIZATION
    toggleProfileAction =
        floatingToolBar->addAction(QIcon::fromTheme("star-profile", QIcon(":/icons/star_profile.svg")),
                                   i18n("View Star Profile"), this, SLOT(toggleStarProfile()));
    toggleProfileAction->setCheckable(true);
#endif

    if (mode == FITS_NORMAL || mode == FITS_ALIGN)
    {
        floatingToolBar->addSeparator();

        toggleEQGridAction =
            floatingToolBar->addAction(QIcon::fromTheme("kstars_grid"),
                                       i18n("Show Equatorial Gridlines"), this, SLOT(toggleEQGrid()));
        toggleEQGridAction->setCheckable(true);
        toggleEQGridAction->setEnabled(false);

        toggleObjectsAction =
            floatingToolBar->addAction(QIcon::fromTheme("help-hint"),
                                       i18n("Show Objects in Image"), this, SLOT(toggleObjects()));
        toggleObjectsAction->setCheckable(true);
        toggleEQGridAction->setEnabled(false);

        centerTelescopeAction =
            floatingToolBar->addAction(QIcon::fromTheme("center_telescope", QIcon(":/icons/center_telescope.svg")),
                                       i18n("Center Telescope"), this, SLOT(centerTelescope()));
        centerTelescopeAction->setCheckable(true);
        centerTelescopeAction->setEnabled(false);
    }
}

/**
 This method either enables or disables the scope mouse mode so you can slew your scope to coordinates
 just by clicking the mouse on a spot in the image.
 */

void FITSView::centerTelescope()
{
    if (imageHasWCS())
    {
        if (getCursorMode() == FITSView::scopeCursor)
        {
            setCursorMode(lastMouseMode);
        }
        else
        {
            lastMouseMode = getCursorMode();
            setCursorMode(FITSView::scopeCursor);
        }
        updateFrame();
    }
    updateScopeButton();
}

void FITSView::updateScopeButton()
{
    if (centerTelescopeAction != nullptr)
    {
        if (getCursorMode() == FITSView::scopeCursor)
        {
            centerTelescopeAction->setChecked(true);
        }
        else
        {
            centerTelescopeAction->setChecked(false);
        }
    }
}

/**
This method just verifies if INDI is online, a telescope present, and is connected
 */

bool FITSView::isTelescopeActive()
{
#ifdef HAVE_INDI
    if (INDIListener::Instance()->size() == 0)
    {
        return false;
    }

    foreach (ISD::GDInterface * gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice * bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == nullptr)
            continue;

        return bd->isConnected();
    }
    return false;
#else
    return false;
#endif
}

void FITSView::setStarsEnabled(bool enable)
{
    markStars = enable;
    if (floatingToolBar != nullptr)
    {
        foreach (QAction * action, floatingToolBar->actions())
        {
            if (action->text() == i18n("Detect Stars in Image"))
            {
                action->setChecked(markStars);
                break;
            }
        }
    }
}

void FITSView::setStarsHFREnabled(bool enable)
{
    showStarsHFR = enable;
}
