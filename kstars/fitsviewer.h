/***************************************************************************
                          fitsviewer.cpp  -  An FitsViewer for KStars
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

#ifndef FITSVIEWER_H
#define FITSVIEWER_H

#include <qwidget.h>
#include <qstring.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qrect.h> 

#include <kpixmapio.h>
#include <kpixmap.h>
#include <kdialog.h>
#include <kmainwindow.h>
#include <kurl.h>

#include "indi/fitsrw.h"

class FitsViewer : public KMainWindow  {
	Q_OBJECT

	public:
		/**Constructor. */
		FitsViewer (const KURL *imageName, QWidget *parent, const char *name = 0);

		~FitsViewer();
		
	protected:
	/**Bitblt the image onto the viewer widget */
	void paintEvent (QPaintEvent *ev);
	/* Resize even */
	void resizeEvent (QResizeEvent *ev);
	
	void keyPressEvent (QKeyEvent *ev);
	/* Calculate stats */
	void calculateStats();
	
	private slots:
	void fileSave();
        void fileSaveAs();
	void fitsUNDO();
	void fitsREDO();
	void fitsCOPY();
	void imageReduction();
	void BrightContrastDlg();
	void imageFilters();
	
	private:
	/* Attempts to load image into QImage */
	int  loadImage();
	int  loadFits(char *filename, FITS_FILE *ifp, uint picnum, uint ncompose);
	void check_load_vals ();
	void show_fits_errors();
	/* Image data for fast pixel manipulation */
	QImage	image;
	/* Pixmap for drawing */
	KPixmap	pix; 
	/* Pixmap IO for fast converting */
	KPixmapIO kpix;
	
	bool Dirty;
	KURL currentURL;
	QRect currentRect;
	int currentContrast;
	int currentBrightness;
	unsigned char *imgBuffer;
		
	struct {
		double min, max;
		double average;
		double stddev;
	} stats;
		
	double average();
	double min();
	double max();
	double stddev();
	void convertTo8Bit();
		
};

#endif
