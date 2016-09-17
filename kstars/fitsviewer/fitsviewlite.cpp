/***************************************************************************
                          FITSViewLite.cpp  -  FITS Image
                             -------------------
    begin                : Fri Jul 22 2016
    copyright            : (C) 2016 by Jasem Mutlaq and Artem Fedoskin
    email                : mutlaqja@ikarustech.com, afedoskin3@gmail.com
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

#include <cmath>
#include <cstdlib>

#include <KLocalizedString>
#include "fitsviewlite.h"

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

FITSViewLite::FITSViewLite(FITSMode fitsMode, FITSScale filterType) :zoomFactor(1.2)
{
    image_data  = NULL;
    display_image = NULL;
    firstLoad = true;
    gammaValue=0;
    filter = filterType;
    mode = fitsMode;    

    markerCrosshair.setX(0);
    markerCrosshair.setY(0);

    currentZoom = 0.0;
    markStars = false;

}

FITSViewLite::~FITSViewLite()
{
    delete(image_data);
    delete(display_image);
}

QImage *FITSViewLite::loadFITS (const QString &inFilename , bool silent)
{
    bool setBayerParams=false;

    BayerParams param;
    if (image_data && image_data->hasDebayer())
    {
        setBayerParams=true;
        image_data->getBayerParams(&param);
    }

    delete (image_data);
    image_data = NULL;

    image_data = new FITSDataLite(mode);

    if (setBayerParams)
        image_data->setBayerParams(&param);

    if (mode == FITS_NORMAL)
    {
        /*fitsProg.setWindowModality(Qt::WindowModal);
        fitsProg.setLabelText(i18n("Please hold while loading FITS file..."));
        fitsProg.setWindowTitle(i18n("Loading FITS"));
        fitsProg.setValue(10);
        qApp->processEvents();*/
    }

    if (image_data->loadFITS(inFilename, silent) == false)
        return false;

    image_data->getDimensions(&currentWidth, &currentHeight);

    image_width  = currentWidth;
    image_height = currentHeight;

    hasWCS = image_data->hasWCS();

    maxPixel = image_data->getMax();
    minPixel = image_data->getMin();

    if (gammaValue != 0 && (filter== FITS_NONE || filter >= FITS_FLIP_H))
    {
        double maxGammaPixel = maxPixel* (100 * exp(DECAY_CONSTANT * gammaValue))/100.0;
        // If calculated maxPixel after gamma is different from image data max pixel, then we apply filter immediately.
        image_data->applyFilter(FITS_LINEAR, NULL, minPixel, maxGammaPixel);
    }

    initDisplayImage();

    if(rescale()) {
        return display_image;
    }
    return NULL;
}

int FITSViewLite::saveFITS( const QString &newFilename )
{
    return image_data->saveFITS(newFilename);
}


bool FITSViewLite::rescale()
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
        //emit newStatus(i18n("Image is saturated!"), FITS_MESSAGE);
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

            //if (isVisible())
                //emit newStatus(QString("%1x%2").arg(image_width).arg(image_height), FITS_RESOLUTION);
        }

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
            return true;
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
            return true;
        }
    }

   //if (type != ZOOM_KEEP_LEVEL)
       //emit newStatus(QString("%1%").arg(currentZoom), FITS_ZOOM);
    return false;
}

void FITSViewLite::calculateMaxPixel(double min, double max)
{
    minPixel=min;
    maxPixel=max;

    if (gammaValue == 0)
        maxGammaPixel = maxPixel;
    else
        maxGammaPixel = maxPixel* (100 * exp(DECAY_CONSTANT * gammaValue))/100.0;
}

void FITSViewLite::initDisplayImage()
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
