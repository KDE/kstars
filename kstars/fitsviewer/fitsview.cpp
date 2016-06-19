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
#include <QToolTip>
#include <QProgressDialog>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QDebug>
#include <QAction>
#include <QStatusBar>
#include <QFileDialog>
#include <QWheelEvent>
#include <QMenu>

#include <KActionCollection>
#include <KMessageBox>
#include <KLocalizedString>

#include "kstarsdata.h"
#include "ksutils.h"
#include "Options.h"

#ifdef HAVE_INDI
#include <basedevice.h>
#include "indi/indilistener.h"
#include "indi/indistd.h"
#include "indi/driverinfo.h"
#endif

#define ZOOM_DEFAULT	100.0
#define ZOOM_MIN	10
#define ZOOM_MAX	400
#define ZOOM_LOW_INCR	10
#define ZOOM_HIGH_INCR	50

#define DECAY_CONSTANT  -0.04

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
    FITSData *image_data = image->getImageData();

    float *buffer = image_data->getImageBuffer();

    if (buffer == NULL)
        return;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit newStatus(QString("X:%1 Y:%2").arg( (int)x ).arg( (int)y ), FITS_POSITION);

    // Range is 0 to dim-1 when accessing array
    x -= 1;
    y -= 1;

    if (image_data->getBPP() == -32 || image_data->getBPP() == 32)
        emit newStatus(QLocale().toString(buffer[(int) (y * width + x)], 'f', 4), FITS_VALUE);
    else
        emit newStatus(QLocale().toString(buffer[(int) (y * width + x)], 'f', 2), FITS_VALUE);


    if (image_data->hasWCS())
    {
        int index = x + y * width;

        wcs_point * wcs_coord = image_data->getWCSCoord();

        if (wcs_coord)
        {
            if (index > size)
                return;

            ra.setD(wcs_coord[index].ra);
            dec.setD(wcs_coord[index].dec);

            emit newStatus(QString("%1 , %2").arg( ra.toHMSString()).arg(dec.toDMSString()), FITS_WCS);
        }
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

#ifdef HAVE_INDI
    FITSData *image_data = image->getImageData();

    if ( (e->buttons() & Qt::RightButton) && image_data->hasWCS())
    {
      QMenu fitspopup;
      QAction *trackAction = fitspopup.addAction(i18n("Center In Telescope"));

      if (fitspopup.exec(e->globalPos()) == trackAction)
      {
          int index = x + y * width;

          wcs_point * wcs_coord = image_data->getWCSCoord();

          if (wcs_coord)
          {
              if (index > size)
                  return;

              centerTelescope(wcs_coord[index].ra/15.0, wcs_coord[index].dec);

              return;
          }
      }
    }
#endif

   emit pointSelected(x, y);

   double HFR = image->getImageData()->getHFR(x, y);

   if (HFR > 0)
       QToolTip::showText(e->globalPos(), i18nc("Half Flux Radius", "HFR: %1", QString::number(HFR, 'g' , 3)), this);

}

void FITSLabel::mouseDoubleClickEvent(QMouseEvent *e)
{
    double x,y;

    x = round(e->x() / (image->getCurrentZoom() / ZOOM_DEFAULT));
    y = round(e->y() / (image->getCurrentZoom() / ZOOM_DEFAULT));

    x = KSUtils::clamp(x, 1.0, width);
    y = KSUtils::clamp(y, 1.0, height);

    emit markerSelected(x, y);

    return;
}

void FITSLabel::centerTelescope(double raJ2000, double decJ2000)
{
#ifdef HAVE_INDI

    if (INDIListener::Instance()->size() == 0)
    {
        KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));
        return;
    }

    foreach(ISD::GDInterface *gd, INDIListener::Instance()->getDevices())
    {
        INDI::BaseDevice *bd = gd->getBaseDevice();

        if (gd->getType() != KSTARS_TELESCOPE)
            continue;

        if (bd == NULL)
            continue;

        if (bd->isConnected() == false)
        {
            KMessageBox::error(0, i18n("Telescope %1 is offline. Please connect and retry again.", gd->getDeviceName()));
            return;
        }

        ISD::GDSetCommand SlewCMD(INDI_SWITCH, "ON_COORD_SET", "TRACK", ISS_ON, this);

        SkyObject selectedObject;

        selectedObject.setRA0(raJ2000);
        selectedObject.setDec0(decJ2000);

        selectedObject.apparentCoord(J2000, KStarsData::Instance()->ut().djd());

        gd->setProperty(&SlewCMD);
        gd->runCommand(INDI_SEND_COORDS, &selectedObject);

        return;
    }

    KMessageBox::sorry(0, i18n("KStars did not find any active telescopes."));

#else

    Q_UNUSED(raJ2000);
    Q_UNUSED(decJ2000);

#endif    
}

FITSView::FITSView(QWidget * parent, FITSMode fitsMode, FITSScale filterType) : QScrollArea(parent) , zoomFactor(1.2)
{
    image_frame = new FITSLabel(this);
    image_data  = NULL;
    display_image = NULL;
    firstLoad = true;
    gammaValue=0;
    filter = filterType;
    mode = fitsMode;    

    setBackgroundRole(QPalette::Dark);

    guide_x = guide_y =  -1;
    guide_box = 16;
    marker_x = marker_y = -1;

    currentZoom = 0.0;
    markStars = false;    

    connect(image_frame, SIGNAL(newStatus(QString,FITSBar)), this, SIGNAL(newStatus(QString,FITSBar)));
    connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));    
    connect(image_frame, SIGNAL(markerSelected(int,int)), this, SLOT(processMarkerSelection(int,int)));

    image_frame->setMouseTracking(true);

    if (fitsMode == FITS_GUIDE)
        connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));

    // Default size
    resize(INITIAL_W, INITIAL_H);
}

FITSView::~FITSView()
{
    delete(image_frame);
    delete(image_data);
    delete(display_image);    
}

bool FITSView::loadFITS (const QString &inFilename , bool silent)
{
    QProgressDialog fitsProg(this);

    bool setBayerParams=false;

    BayerParams param;
    if (image_data && image_data->hasDebayer())
    {
        setBayerParams=true;
        image_data->getBayerParams(&param);
    }

    delete (image_data);
    image_data = NULL;

    image_data = new FITSData(mode);

    if (setBayerParams)
        image_data->setBayerParams(&param);

    if (mode == FITS_NORMAL)
    {
        fitsProg.setWindowModality(Qt::WindowModal);
        fitsProg.setLabelText(i18n("Please hold while loading FITS file..."));
        fitsProg.setWindowTitle(i18n("Loading FITS"));
        fitsProg.setValue(10);
        qApp->processEvents();
    }

    if (image_data->loadFITS(inFilename, silent) == false)
        return false;


    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
           fitsProg.setValue(65);
           qApp->processEvents();
        }
    }

    emit debayerToggled(image_data->hasDebayer());

    image_data->getDimensions(&currentWidth, &currentHeight);

    image_width  = currentWidth;
    image_height = currentHeight;

    image_frame->setSize(image_width, image_height);

    hasWCS = image_data->hasWCS();

    maxPixel = image_data->getMax();
    minPixel = image_data->getMin();

    if (gammaValue != 0 && (filter== FITS_NONE || filter >= FITS_FLIP_H))
    {
        double maxGammaPixel = maxPixel* (100 * exp(DECAY_CONSTANT * gammaValue))/100.0;
        // If calculated maxPixel after gamma is different from image data max pixel, then we apply filter immediately.
        image_data->applyFilter(FITS_LINEAR, NULL, minPixel, maxGammaPixel);
    }

    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
           fitsProg.setValue(75);
           qApp->processEvents();
        }
    }

    initDisplayImage();

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

    if (mode == FITS_NORMAL)
    {
        if (fitsProg.wasCanceled())
            return false;
        else
        {
           fitsProg.setValue(100);
           qApp->processEvents();
        }
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
    double val=0;
    double bscale, bzero;
    double min, max;
    unsigned int size = image_data->getSize();
    image_data->getMinMax(&min, &max);

    calculateMaxPixel(min, max);

    min = minPixel;
    max = maxGammaPixel;

    if (min == max)
    {
        display_image->fill(Qt::white);
        emit newStatus(i18n("Image is saturated!"), FITS_MESSAGE);
    }
    else
    {
        bscale = 255. / (max - min);
        bzero  = (-min) * (255. / (max - min));

        if (image_height != image_data->getHeight() || image_width != image_data->getWidth())
        {            
            image_width  = image_data->getWidth();
            image_height = image_data->getHeight();

            initDisplayImage();

            if (isVisible())
                emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);
        }

        image_frame->setScaledContents(true);
        currentWidth  = display_image->width();
        currentHeight = display_image->height();

        float *image_buffer = image_data->getImageBuffer();

        if (image_data->getNumOfChannels() == 1)
        {
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < image_height; j++)
            {
                unsigned char *scanLine = display_image->scanLine(j);

                for (int i = 0; i < image_width; i++)
                {
                    val = image_buffer[j * image_width + i];
                    if (gammaValue > 0)
                        val = qBound(minPixel, val, maxGammaPixel);
                    scanLine[i]= (val * bscale + bzero);
                }
            }
        }
        else
        {
            double rval=0,gval=0,bval=0;
            QRgb value;
            /* Fill in pixel values using indexed map, linear scale */
            for (int j = 0; j < image_height; j++)
            {
                QRgb *scanLine = reinterpret_cast<QRgb*>((display_image->scanLine(j)));

                for (int i = 0; i < image_width; i++)
                {
                    rval = image_buffer[j * image_width + i];
                    gval = image_buffer[j * image_width + i + size];
                    bval = image_buffer[j * image_width + i + size * 2];
                    if (gammaValue > 0)
                    {
                        rval = qBound(minPixel, rval, maxGammaPixel);
                        gval = qBound(minPixel, gval, maxGammaPixel);
                        gval = qBound(minPixel, gval, maxGammaPixel);
                    }

                    value = qRgb(rval* bscale + bzero, gval* bscale + bzero, bval* bscale + bzero);

                    //display_image->setPixel(i, j, value);
                    scanLine[i] = value;

                }
            }

        }

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
    {
        currentZoom = ZOOM_MAX;
        emit actionUpdated("view_zoom_in", false);
    }

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
    {
        currentZoom = ZOOM_MIN;
        emit actionUpdated("view_zoom_out", false);
    }

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

    if (marker_x != -1)
        drawMarker(painter);

}

void FITSView::updateMode(FITSMode fmode)
{
    mode = fmode;

   // if (mode == FITS_GUIDE)
        //connect(image_frame, SIGNAL(pointSelected(int,int)), this, SLOT(processPointSelection(int,int)));
    //else
        //image_frame->disconnect(this, SLOT(processPointSelection(int,int)));

}

void FITSView::drawMarker(QPainter *painter)
{
    painter->setPen( QPen( QColor( KStarsData::Instance()->colorScheme()->colorNamed("TargetColor" ) ) ) );
    painter->setBrush( Qt::NoBrush );
    float pxperdegree = (currentZoom/ZOOM_DEFAULT)* (57.3/1.8);

    float s1 = 0.5*pxperdegree;
    float s2 = pxperdegree;
    float s3 = 2.0*pxperdegree;

    float x0 = marker_x  * (currentZoom / ZOOM_DEFAULT);
    float y0 = marker_y  * (currentZoom / ZOOM_DEFAULT);
    float x1 = x0 - 0.5*s1;  float y1 = y0 - 0.5*s1;
    float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
    float x3 = x0 - 0.5*s3;  float y3 = y0 - 0.5*s3;

    //Draw radial lines
    painter->drawLine( QPointF(x1, y0), QPointF(x3, y0) );
    painter->drawLine( QPointF(x0+s2, y0), QPointF(x0+0.5*s1, y0) );
    painter->drawLine( QPointF(x0, y1), QPointF(x0, y3) );
    painter->drawLine( QPointF(x0, y0+0.5*s1), QPointF(x0, y0+s2) );
    //Draw circles at 0.5 & 1 degrees
    painter->drawEllipse( QRectF(x1, y1, s1, s1) );
    painter->drawEllipse( QRectF(x2, y2, s2, s2) );
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
       QApplication::setOverrideCursor(Qt::WaitCursor);
       emit newStatus(i18n("Finding stars..."), FITS_MESSAGE);
       qApp->processEvents();
       int count = image_data->findStars();
       if (count >= 0 && isVisible())
               emit newStatus(i18np("1 star detected.", "%1 stars detected.", count), FITS_MESSAGE);
       QApplication::restoreOverrideCursor();
     }
}

void FITSView::processPointSelection(int x, int y)
{
    if (mode != FITS_GUIDE)
        return;

    image_data->getCenterSelection(&x, &y);

    //setGuideSquare(x,y);
    emit guideStarSelected(x,y);
}

void FITSView::processMarkerSelection(int x, int y)
{
   marker_x = x;
   marker_y = y;

   updateFrame();
}

int FITSView::getGammaValue() const
{
    return gammaValue;
}

void FITSView::setGammaValue(int value)
{
    if (value == gammaValue)
        return;

    gammaValue = value;

    calculateMaxPixel(minPixel, maxPixel);

    // If calculated maxPixel after gamma is different from image data max pixel, then we apply filter immediately.
    //image_data->applyFilter(FITS_LINEAR, NULL, minPixel, maxGammaPixel);
    qApp->processEvents();
    rescale(ZOOM_KEEP_LEVEL);
    qApp->processEvents();
    updateFrame();

}

void FITSView::calculateMaxPixel(double min, double max)
{
    minPixel=min;
    maxPixel=max;

    if (gammaValue == 0)
        maxGammaPixel = maxPixel;
    else
        maxGammaPixel = maxPixel* (100 * exp(DECAY_CONSTANT * gammaValue))/100.0;
}


void FITSView::wheelEvent(QWheelEvent* event)
{
    if (event->angleDelta().y() > 0)
        ZoomIn();
    else
        ZoomOut();

    event->accept();
}

void FITSView::initDisplayImage()
{
    delete (display_image);
    display_image = NULL;

    if (image_data->getNumOfChannels() == 1)
    {
        display_image = new QImage(image_width, image_height, QImage::Format_Indexed8);

        display_image->setColorCount(256);
        for (int i=0; i < 256; i++)
            display_image->setColor(i, qRgb(i,i,i));
    }
    else
    {
        display_image = new QImage(image_width, image_height, QImage::Format_RGB32);
    }
}

