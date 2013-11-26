/***************************************************************************
                          FITSView.cpp  -  FITS Image
                             -------------------
    begin                : Thu Jan 22 2004
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

#include <config-kstars.h>

#include "fitsview.h"

#include <cmath>
#include <cstdlib>

#include <QApplication>
#include <QPaintEvent>
#include <QScrollArea>
#include <QFile>
#include <QCursor>
#include <QProgressDialog>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>

#include <KDebug>
#include <KLocale>
#include <KAction>
#include <KActionCollection>
#include <KStatusBar>
#include <KProgressDialog>
#include <KMessageBox>
#include <KFileDialog>

#include "ksutils.h"

#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN	10
#define ZOOM_MAX	400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

//#define FITS_DEBUG

FITSLabel::FITSLabel(FITSView *img, QWidget *parent) : QLabel(parent)
{
    image = img;

}

void FITSLabel::setSize(double w, double h)
{
    width  = w;
    height = h;
    size   = w*h;
}

FITSLabel::~FITSLabel() {}

void FITSLabel::mouseMoveEvent(QMouseEvent *e)
{
    double x,y;
    FITSImage *image_data = image->getImageData();

    float *buffer = image_data->getImageBuffer();

    if (buffer == NULL)
        return;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit newStatus(QString("%1 , %2").arg( (int)x ).arg( (int)y ), FITS_POSITION);

    // Range is 0 to dim-1 when accessing array
    x -= 1;
    y -= 1;

    if (image_data->getBPP() == -32 || image_data->getBPP() == 32)
        emit newStatus(KGlobal::locale()->formatNumber( buffer[(int) (y * width + x)], 5), FITS_VALUE);
    else
        emit newStatus(KGlobal::locale()->formatNumber( buffer[(int) (y * width + x)]), FITS_VALUE);


    if (image_data->hasWCS())
    {
        int index = x + y * width;

        wcs_point * wcs_coord = image_data->getWCSCoord();

        if (index > size)
            return;

        ra.setD(wcs_coord[index].ra);
        dec.setD(wcs_coord[index].dec);

        emit newStatus(QString("%1 , %2").arg( ra.toHMSString()).arg(dec.toDMSString()), FITS_WCS);
    }

    setCursor(Qt::CrossCursor);

    e->accept();
}

void FITSLabel::mousePressEvent(QMouseEvent *e)
{
    double x,y;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

   emit pointSelected(x, y);

}

FITSView::FITSView(QWidget * parent, FITSMode fitsMode) : QScrollArea(parent) , zoomFactor(1.2)
{
    image_frame = new FITSLabel(this);
    image_data  = NULL;
    display_image = NULL;
    firstLoad = true;

    mode = fitsMode;

    setBackgroundRole(QPalette::Dark);

    guide_x = guide_y = guide_box = -1;

    currentZoom = 0.0;
    markStars = false;

    connect(image_frame, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));

    image_frame->setMouseTracking(true);

    if (fitsMode == FITS_GUIDE)
        connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));

    // Default size
    resize(INITIAL_W, INITIAL_H);
}

FITSView::~FITSView()
{
    delete(display_image);
    delete(image_data);
}

bool FITSView::loadFITS ( const QString &inFilename )
{
    QProgressDialog fitsProg;


    delete (image_data);
    image_data = NULL;

    image_data = new FITSImage(mode);

    if (image_data->loadFITS(inFilename, &fitsProg) == false)
        return false;

    image_data->getSize(&currentWidth, &currentHeight);

    image_width  = currentWidth;
    image_height = currentHeight;

    image_frame->setSize(image_width, image_height);

    hasWCS = image_data->hasWCS();

    delete (display_image);
    display_image = NULL;

    display_image = new QImage(image_width, image_height, QImage::Format_Indexed8);

    display_image->setNumColors(256);
    for (int i=0; i < 256; i++)
        display_image->setColor(i, qRgb(i,i,i));

    // Rescale to fits window
    if (firstLoad)
    {
        currentZoom   = 100;

        if (rescale(ZOOM_FIT_WINDOW))
            return false;

        firstLoad = false;
    }
    else
    {
        if (rescale(ZOOM_KEEP_LEVEL))
            return false;
    }

    setAlignment(Qt::AlignCenter);

    if (isVisible())
    emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);

    return true;
}

int FITSView::saveFITS( const QString &newFilename )
{
    return image_data->saveFITS(newFilename);
}


int FITSView::rescale(FITSZoom type)
{
    float val=0;
    double bscale, bzero;
    double min, max;

    image_data->getMinMax(&min, &max);
    float *image_buffer = image_data->getImageBuffer();

    if (min == max)
    {
        // For focus and guide, we silenty ignore saturation error.
        if (mode == FITS_FOCUS || mode == FITS_GUIDE)
            qDebug() << "FITS image is saturated and cannot be displayed." << endl;
        else
            KMessageBox::error(0, i18n("FITS image is saturated and cannot be displayed."), i18n("FITS Open"));
        return -1;
    }


    bscale = 255. / (max - min);
    bzero  = (-min) * (255. / (max - min));

    image_frame->setScaledContents(true);
    currentWidth  = display_image->width();
    currentHeight = display_image->height();

    /* Fill in pixel values using indexed map, linear scale */
    for (int j = 0; j < image_height; j++)
        for (int i = 0; i < image_width; i++)
        {
            val = image_buffer[j * image_width + i];
            display_image->setPixel(i, j, ((int) (val * bscale + bzero)));
        }

    switch (type)
    {
    case ZOOM_FIT_WINDOW:
        if ((display_image->width() > width() || display_image->height() > height()))
        {
            // Find the zoom level which will enclose the current FITS in the default window size (640x480)
            currentZoom = floor( (INITIAL_W / currentWidth) * 10.) * 10.;

            /* If width is not the problem, try height */
            if (currentZoom > ZOOM_DEFAULT)
                currentZoom = floor( (INITIAL_H / currentHeight) * 10.) * 10.;

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
        currentZoom   = 100;

        break;
    }

    setWidget(image_frame);


   if (type != ZOOM_KEEP_LEVEL)
       emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    return 0;

}

void FITSView::ZoomIn()
{

    if (currentZoom < ZOOM_DEFAULT)
        currentZoom += ZOOM_LOW_INCR;
    else
        currentZoom += ZOOM_HIGH_INCR;


    emit actionUpdated("view_zoom_out", true);
    if (currentZoom >= ZOOM_MAX)
        emit actionUpdated("view_zoom_in", false);

    currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
    currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

}

void FITSView::ZoomOut()
{

    if (currentZoom <= ZOOM_DEFAULT)
        currentZoom -= ZOOM_LOW_INCR;
    else
        currentZoom -= ZOOM_HIGH_INCR;

    if (currentZoom <= ZOOM_MIN)
        emit actionUpdated("view_zoom_out", false);

    emit actionUpdated("view_zoom_in", true);

    currentWidth  = image_width * (currentZoom / ZOOM_DEFAULT);
    currentHeight = image_height * (currentZoom / ZOOM_DEFAULT);

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
}

void FITSView::updateFrame()
{

    QPixmap displayPixmap;
    bool ok=false;

    if (display_image == NULL)
        return;

    if (currentZoom != ZOOM_DEFAULT)
            ok = displayPixmap.convertFromImage(display_image->scaled( (int) currentWidth, (int) currentHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        else
            ok = displayPixmap.convertFromImage(*display_image);


    if (ok == false)
        return;

    QPainter painter(&displayPixmap);

    drawOverlay(&painter);

    image_frame->setPixmap(displayPixmap);
    image_frame->resize( (int) currentWidth, (int) currentHeight);
}

void FITSView::ZoomDefault()
{
    emit actionUpdated("view_zoom_out", true);
    emit actionUpdated("view_zoom_in", true);


    currentZoom   = ZOOM_DEFAULT;
    currentWidth  = image_width;
    currentHeight = image_height;

    updateFrame();

    newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);

    update();

}

void FITSView::drawOverlay(QPainter *painter)
{
    if (markStars)
         drawStarCentroid(painter);

    if (mode == FITS_GUIDE)
        drawGuideBox(painter);

}

void FITSView::updateMode(FITSMode fmode)
{
    mode = fmode;

    if (mode == FITS_GUIDE)
        connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));
    else
        image_frame->disconnect(this, SLOT(processPointSelection(int,int)));

}

void FITSView::drawStarCentroid(QPainter *painter)
{
    painter->setPen(QPen(Qt::red, 2));

    int x1,y1, w;

    // image_data->getStarCenter();

    QList<Edge*> starCenters = image_data->getStarCenters();

    for (int i=0; i < starCenters.count() ; i++)
    {
        x1 = (starCenters[i]->x - starCenters[i]->width/2) * (currentZoom / ZOOM_DEFAULT);
        y1 = (starCenters[i]->y - starCenters[i]->width/2) * (currentZoom / ZOOM_DEFAULT);
        w = (starCenters[i]->width) * (currentZoom / ZOOM_DEFAULT);

        painter->drawEllipse(x1, y1, w, w);
    }
}

void FITSView::drawGuideBox(QPainter *painter)
{
    painter->setPen(QPen(Qt::green, 2));

    int mid = guide_box/2;

    if (mid == -1 || guide_x == -1 || guide_y == -1)
        return;

    int x1 = (guide_x - mid) * (currentZoom / ZOOM_DEFAULT);
    int y1 = (guide_y - mid) * (currentZoom / ZOOM_DEFAULT);
    int w  = guide_box * (currentZoom / ZOOM_DEFAULT);

    painter->drawRect(x1, y1, w, w);
}

void FITSView::setGuideSquare(int x, int y)
{
    guide_x = x;
    guide_y = y;

    updateFrame();


}

void FITSView::setGuideBoxSize(int size)
{
    if (size != guide_box)
    {
        guide_box = size;
        updateFrame();
    }
}

void FITSView::toggleStars(bool enable)
{
     markStars = enable;

     if (markStars == true)
     {
       int count = image_data->findStars();
       if (count >= 0 && isVisible())
               emit newStatus(i18np("1 star detected.", "%1 stars detected.", count), FITS_MESSAGE);
     }
}

void FITSView::processPointSelection(int x, int y)
{
    image_data->getCenterSelection(&x, &y);

    setGuideSquare(x,y);
    emit guideStarSelected(x,y);
}

#include "fitsview.moc"
