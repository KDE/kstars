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

#include <qwidget.h>
#include <qstring.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qframe.h>
#include <qrect.h> 
#include <qptrlist.h>
#include <qscrollview.h>

#include <kpixmapio.h>
#include <kpixmap.h>
#include <kdialog.h>
#include <kmainwindow.h>
#include <kurl.h>

#include "indi/fitsrw.h"

class KCommandHistory;
class QScrollView;
class FITSViewer;

class FITSImage : public QScrollView  {
	Q_OBJECT

	public:
	
	friend class ContrastBrightnessDlg;
	friend class FITSViewer;
	
	FITSImage(QWidget * parent, const char * name = 0);
	~FITSImage();
	
	/**Bitblt the image onto the viewer widget */
	void paintEvent (QPaintEvent *ev);
	/* Resize event */
	void resizeEvent (QResizeEvent *ev);
	/* Loads FITS image, scales it, and displays it in the GUI */
	int  loadFits(const char *filename);
	/* Convert current image to a pixmap */
	void convertImageToPixmap();
	
	private:
	FITSViewer *viewer;					/* parent FITSViewer */
	QFrame  *imgFrame;					/* Frame holding the image */
	QImage  *currentImage;					/* FITS image that is displayed in the GUI */
	QImage  *templateImage;					/* backup image for currentImage */
	QPixmap qpix; 						/* Pixmap for drawing */
	KPixmapIO kpix;						/* Pixmap IO for fast converting */
	QRect currentRect;					/* Current rectangle encapsulating the image */
	int bitpix, bpp;					/* bits per pixel and bytes per pixels for FITS */
	int width, height;					/* FITS dimensions */
	unsigned char *reducedImgBuffer;			/* scaled image buffer (0-255) range */
	
	void saveTemplateImage();				/* saves a backup image */
	void reLoadTemplateImage();				/* reloads backup image into the current image */
	void destroyTemplateImage();				/* deletes backup image */
	void rescale();						/* rescale image from main image buffer */
	
	protected:
	void drawContents ( QPainter * p, int clipx, int clipy, int clipw, int cliph );
	void contentsMouseMoveEvent ( QMouseEvent * e );
	void viewportResizeEvent ( QResizeEvent * e) ;
};

#endif
