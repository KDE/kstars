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

#ifndef FITSViewer_H_
#define FITSViewer_H_

#include <QKeyEvent>
#include <QCloseEvent>

#include <kdialog.h>
#include <kxmlguiwindow.h>
#include <kurl.h>

#include "fitsio.h"

#define INITIAL_W	640
#define INITIAL_H	480

class K3CommandHistory;
class FITSImage;
class FITSHistogram;

class FITSViewer : public KXmlGuiWindow  
{
	Q_OBJECT

	public:
	
	friend class FITSChangeCommand;
	friend class FITSImage;
	friend class FITSHistogram;
	friend class FITSHistogramCommand;
	
	/**Constructor. */
	FITSViewer (const KUrl *imageName, QWidget *parent);
	~FITSViewer();

	protected:
	
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
	void imageHistogram();
	
	private:
	bool    initFITS();
	
	FITSImage *image;					/* FITS image object */
	FITSHistogram *histo;					/* FITS Histogram */
	
	K3CommandHistory *history;				/* History for undo/redo */
	int Dirty;						/* Document modified? */
	KUrl currentURL;					/* FITS File name and path */
		
};

#endif
