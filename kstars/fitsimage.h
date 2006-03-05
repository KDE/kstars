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

#include <kmainwindow.h>
#include <kurl.h>

class KCommandHistory;
class Q3ScrollView;
class FITSViewer;
//class FITSFrame;

class QLabel;

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
	
	/**Bitblt the image onto the viewer widget */
	/*void paintEvent (QPaintEvent *ev);*/
	/* Resize event */
	void resizeEvent (QResizeEvent *ev);
	/* Loads FITS image, scales it, and displays it in the GUI */
	int  loadFits(const char *filename);
	/* Convert current image to a pixmap */
	void convertImageToPixmap();
	/* Clear memory */
	void clearMem();
	
	private:
	FITSViewer *viewer;					/* parent FITSViewer */
	//FITSFrame  *image_frame;				/* Frame holding the image */
	QImage  *displayImage;					/* FITS image that is displayed in the GUI */
	QPixmap image_pixmap; 					/* Pixmap for drawing */
	QLabel *image_frame;
	
 	//float	*image_buffer;					/* Native pixel values */
	float *image_buffer;				/* scaled image buffer (0-255) range */

	/* FIXME remove this */
	QImage  *templateImage;					/* backup image for currentImage */

	//KPixmapIO kpix;						/* Pixmap IO for fast converting */
	//QRect currentRect;					/* Current rectangle encapsulating the image */
	//int bitpix, bpp;					/* bits per pixel and bytes per pixels for FITS */
	int width, height;					/* Original FITS dimensions */
	double currentWidth,currentHeight;			/* Current width and height due to zoom */
	const double zoomFactor;				/* Image zoom factor */
	double currentZoom;					/* Current Zoom level */
	//QRgb   *grayTable;

	
	

	void saveTemplateImage();				/* saves a backup image */
	void reLoadTemplateImage();				/* reloads backup image into the current image */
	void destroyTemplateImage();				/* deletes backup image */
	void zoomToCurrent();					/* Zoom the image to current zoom level without modifying it */

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
	
	protected:
	/*void drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph );*/
	void contentsMouseMoveEvent ( QMouseEvent * e );
	void viewportResizeEvent ( QResizeEvent * e) ;
	
	public slots:
	void fitsZoomIn();
	void fitsZoomOut();
	void fitsZoomDefault();
};

#endif
