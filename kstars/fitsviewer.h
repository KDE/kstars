/***************************************************************************
                          FITSViewer.cpp  -  A FITSViewer for KStars
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

#ifndef FITSViewer_H
#define FITSViewer_H

#include <qwidget.h>
#include <qstring.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qframe.h>
#include <qrect.h> 
#include <qptrlist.h>
#include <qstringlist.h>
#include <qscrollview.h>

#include <kpixmapio.h>
#include <kpixmap.h>
#include <kdialog.h>
#include <kmainwindow.h>
#include <kurl.h>
#include <kcommand.h>

#include "indi/fitsrw.h"

class KCommandHistory;
class ContrastBrightnessDlg;
class QScrollView;
class FITSImage;
class FITSHistogram;

class FITSViewer : public KMainWindow  {
	Q_OBJECT

	public:
	
	friend class ContrastBrightnessDlg;
	friend class FITSChangeCommand;
	friend class FITSProcess;
	friend class FITSImage;
	friend class FITSHistogram;
	friend class FITSHistogramCommand;
	friend class FITSProcessCommand;
	
	/**Constructor. */
	FITSViewer (const KURL *imageName, QWidget *parent, const char *name = 0);
	~FITSViewer();

	
	
	enum undoTypes { CONTRAST_BRIGHTNESS, IMAGE_REDUCTION, IMAGE_FILTER };
			
	protected:
	/* key press event */
	void keyPressEvent (QKeyEvent *ev);
	/* Calculate stats */
	void calculateStats();
	void closeEvent(QCloseEvent *ev);
	
	public slots:
	void fitsChange();
	
	private slots:
	void fileOpen();
	void fileSave();
        void fileSaveAs();
	void fitsCOPY();
	void fitsRestore();
	void fitsStatistics();
	void fitsHeader();
	void slotClose();
	void imageReduction();
	void imageHistogram();
	void BrightContrastDlg();
	void updateImgBuffer();
	
	private:
	//int  loadImage(unsigned int *buffer, bool displayImage = false);
	float * loadData(const char * filename, float *buffer);
	bool    initFITS();
	void show_fits_errors();

	double average();
	double min(int & minIndex);
	double max(int & maxIndex);
	double stddev();

	FITSImage *image;					/* FITS image object */
	int Dirty;						/* Document modified? */
	KURL currentURL;					/* FITS File name and path */
	float *imgBuffer;					/* Main unmodified FITS data buffer */
	KCommandHistory *history;				/* History for undo/redo */
	QStringList record;					/* FITS records */
	FITSHistogram *histo;
	
	/* stats struct to hold statisical data about the FITS data */
	struct {
		double min, max;
		int minAt, maxAt;
		double average;
		double stddev;
		int bitpix, width, height;
	} stats;
		
		
};

class FITSChangeCommand : public KCommand
{
  public:
        FITSChangeCommand(QWidget * parent, int inType, QImage *newIMG, QImage *oldIMG);
	~FITSChangeCommand();
            
        void execute();
        void unexecute();
        QString name() const;

    private:
        int type;
	
    protected:
        FITSViewer *viewer;
        QImage *newImage;
	QImage *oldImage;
};


#endif
