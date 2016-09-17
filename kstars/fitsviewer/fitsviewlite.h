/***************************************************************************
                          fitsviewlite.h  -  FITS Image
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

#ifndef FITSView_H_
#define FITSView_H_

#ifdef WIN32
// avoid compiler warning when windows.h is included after fitsio.h
#include <windows.h>
#endif

#include "fitsio.h"
#include "fitscommon.h"

#include "dms.h"
#include "fitsdatalite.h"

#define INITIAL_W	640
#define INITIAL_H	480

#define MINIMUM_PIXEL_RANGE 5
#define MINIMUM_STDVAR  5

class FITSView;

class FITSViewLite : public QObject//: public QScrollArea
{
    Q_OBJECT
public:
    FITSViewLite(FITSMode mode=FITS_NORMAL, FITSScale filter=FITS_NONE);
    ~FITSViewLite();

    /* Loads FITS image, scales it, and displays it in the GUI */
    QImage *loadFITS(const QString &filename, bool silent=true);
    /* Save FITS */
    int saveFITS(const QString &filename);
    /* Rescale image lineary from image_buffer, fit to window if desired */
    bool rescale();

    void setImageData(FITSDataLite *d) { image_data = d; }

    // Access functions
    FITSDataLite *getImageData() { return image_data; }
    double getCurrentZoom() { return currentZoom; }
    QImage * getDisplayImage() { return display_image; }

    QImage * getImageFromFITS();

    int getGammaValue() const;
    void setGammaValue(int value);
    void setFilter(FITSScale newFilter) { filter = newFilter;}

private:

    double average();
    double stddev();
    void calculateMaxPixel(double min, double max);
    void initDisplayImage();

    //FITSLabel *image_frame;
    FITSDataLite *image_data;
    int image_width, image_height;

    double currentWidth,currentHeight; /* Current width and height due to zoom */
    const double zoomFactor;           /* Image zoom factor */
    double currentZoom;                /* Current Zoom level */

    int data_type;                     /* FITS data type when opened */
    QImage  *display_image;            /* FITS image that is displayed in the GUI */

    int gammaValue;
    double maxPixel, maxGammaPixel, minPixel;

    bool firstLoad;
    bool markStars;
    bool starsSearched;
    bool hasWCS;

    QString filename;
    FITSMode mode;
    FITSScale filter;

    // Cross hair
    QPointF markerCrosshair;
};

#endif
