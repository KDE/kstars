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

#ifndef FITSIMAGE_H
#define FITSIMAGE_H

#include <QFrame>
#include <QImage>
#include <QPixmap>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QScrollArea>
#include <QLabel>

#include <kmainwindow.h>
#include <kurl.h>

#include "indi/cfitsio/fitsio.h"

class KCommandHistory;
class FITSViewer;
class FITSImage;


class FITSLabel : public QLabel
{
  public:

  FITSLabel(FITSImage *img, QWidget *parent=NULL);
  ~FITSLabel();

  protected:
  void mouseMoveEvent(QMouseEvent *e);

  private:
  FITSImage *image;

};

class FITSImage : public QScrollArea
{
	Q_OBJECT

	public:
	
	friend class ContrastBrightnessDlg;
	friend class FITSProcess;
	friend class FITSFrame;
	friend class FITSViewer;
	friend class FITSHistogram;
	friend class FITSHistogramCommand;
	friend class FITSChangeCommand;
	friend class FITSProcessCommand;
	
	FITSImage(QWidget * parent, const char * name = 0);
	~FITSImage();
	
	enum scaleType { FITSAuto = 0 , FITSLinear, FITSLog, FITSSqrt, FITSCustom };
	
	/* Loads FITS image, scales it, and displays it in the GUI */
	int  loadFits(const char *filename);
	/* Convert current image to a pixmap */
	void convertImageToPixmap();
	/* Clear memory */
	void clearMem();
	/* Rescale image lineary from image_buffer, fit to window if desired */
	int rescale(bool fitToWindow=false);

	// Access functions
        FITSViewer * getViewer() { return viewer; }
        double getCurrentZoom() { return currentZoom; }
	float * getImageBuffer() { return image_buffer; }
	void getFITSSize(double *w, double *h) { *w = stats.dim[0]; *h = stats.dim[1]; }
	void getFITSMinMax(double *min, double *max) { *min = stats.min; *max = stats.max; }

	private:

        int calculateMinMax();
	void saveTemplateImage();			/* saves a backup image */
	void reLoadTemplateImage();			/* reloads backup image into the current image */
	void destroyTemplateImage();			/* deletes backup image */
	void zoomToCurrent();				/* Zoom the image to current zoom level without modifying it */

	FITSViewer *viewer;				/* parent FITSViewer */	
	QImage  *displayImage;				/* FITS image that is displayed in the GUI */
	FITSLabel *image_frame;
	float *image_buffer;				/* scaled image buffer (0-255) range */

	/* FIXME remove this */
	QImage  *templateImage;				/* backup image for currentImage */
	double currentWidth,currentHeight;		/* Current width and height due to zoom */
	const double zoomFactor;			/* Image zoom factor */
	double currentZoom;				/* Current Zoom level */
	fitsfile* fptr;

	/* stats struct to hold statisical data about the FITS data */
	struct {
		double min, max;
		int minAt, maxAt;
		double average;
		double stddev;
		int bitpix;
		int ndim;
		long dim[2];
	} stats;
	
	public slots:
	void fitsZoomIn();
	void fitsZoomOut();
	void fitsZoomDefault();
};

#endif
