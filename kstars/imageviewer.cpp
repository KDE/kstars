/***************************************************************************
                          imageviewer.cpp  -  An ImageViewer for KStars
                             -------------------
    begin                : Mon Aug 27 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qpainter.h>
#include <qfile.h>

#include <klocale.h>
#include <kurl.h>
#include <ktempfile.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kio/netaccess.h>
#include <ktoolbar.h>
#include <kaction.h>
#include <kaccel.h>
#include <kiconloader.h>
#include <kdebug.h>
#include "imageviewer.h"

#if (QT_VERSION < 300)
#include <kapp.h>
#else
#include <kapplication.h>
#endif

ImageViewer::ImageViewer (const KURL *url, QWidget *parent, const char *name)
	: KMainWindow (parent, name), imageURL (*url), fileIsImage (false),
	  ctrl (false), key_s (false), key_q (false), downloadJob(0)
{
// toolbar can dock only on top-position and can't be minimized
// JH: easier to just disable its mobility
	toolBar()->enableMoving( false );

	KAction *action = new KAction (i18n ("Close Window"), "fileclose", KAccel::stringToKey( "Ctrl+Q" ), this, SLOT (close()), actionCollection());
	action->plug (toolBar());
	action = new KAction (i18n ("Save Image"), "filesave", KAccel::stringToKey( "Ctrl+S" ), this, SLOT (saveFileToDisc()), actionCollection());
	action->plug (toolBar());

	if (imageURL.isMalformed())		//check URL
            kdDebug()<<"URL is malformed"<<endl;
	setCaption (imageURL.filename()); // the title of the window
	loadImageFromURL();
}

ImageViewer::~ImageViewer() {
// check if download job is running
	checkJob();

	if (!file->remove())		// if the file was not complete downloaded the suffix is  ".part"
	{
            kdDebug()<<QString("remove of %1 failed").arg(file->name())<<endl;
		file->setName (file->name() + ".part");		// set new suffix to filename
                kdDebug()<<QString("try to remove %1").arg( file->name())<<endl;
		if (file->remove())
                    kdDebug()<<"file removed\n";
		else
                    kdDebug()<<"file not removed\n";
	}
}

void ImageViewer::paintEvent (QPaintEvent *ev)
{
	bitBlt (this, 0, toolBar()->height() + 1, &pix);
}

void ImageViewer::resizeEvent (QResizeEvent *ev)
{
	if ( !downloadJob )  // only if image is loaded
		pix = kpix.convertToPixmap ( image.smoothScale ( size().width() , size().height() - toolBar()->height() ) );	// convert QImage to QPixmap (fastest method)
}

void ImageViewer::closeEvent (QCloseEvent *ev)
{
	if (ev)	// not if closeEvent (0) is called, because segfault
		ev->accept();	// parent-widgets should not get this event, so it will be filtered
	this->~ImageViewer();	// destroy the object, so the object can only allocated with operator new, not as a global/local variable
}


void ImageViewer::keyPressEvent (QKeyEvent *ev)
{
	ev->accept();  //make sure key press events are captured.
	switch (ev->key())
	{
		case Key_Control : ctrl = true; break;
		case Key_Q : key_q = true; break;
		case Key_S : key_s = true; break;
		default : ev->ignore();
	}
	if (ctrl && key_q)
		close();
	if (ctrl && key_s)
	{
		ctrl = false;
		key_s = false;
		saveFileToDisc();
	}
}

void ImageViewer::keyReleaseEvent (QKeyEvent *ev)
{
	ev->accept();
	switch (ev->key())
	{
		case Key_Control : ctrl = false; break;
		case Key_Q : key_q = false; break;
		case Key_S : key_s = false; break;
		default : ev->ignore();
	}
}

void ImageViewer::loadImageFromURL()
{
	file = tempfile.file();
	tempfile.unlink();		// we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be shown
	KURL saveURL (file->name());
	if (saveURL.isMalformed())
            kdDebug()<<"tempfile-URL is malformed\n";

	downloadJob = KIO::copy (imageURL, saveURL);	// starts the download asynchron
	connect (downloadJob, SIGNAL (result (KIO::Job *)), SLOT (downloadReady (KIO::Job *)));
}

void ImageViewer::downloadReady (KIO::Job *job)
{
// set downloadJob to 0, but don't delete it - the job will automatically deleted !!!
	downloadJob = 0;

	if ( job->error() )
	{
		job->showErrorDialog();
		closeEvent (0);
		return;		// exit this function
	}

	file->close(); // to get the newest informations of the file and not any informations from opening of the file

	if ( file->exists() )
	{
		showImage();
		return;
	}
	closeEvent (0);
}

void ImageViewer::showImage()
{
	if (!image.load (file->name()))		// if loading failed
	{
		QString text = i18n ("Loading of the image %1 failed!");
		KMessageBox::error (this, text.arg (imageURL.prettyURL() ));
		closeEvent (0);
		return;
	}
	fileIsImage = true;	// we loaded the file and know now, that it is an image

	int w = kapp->desktop()->width();	// screen width
	int h = int ( 0.9*kapp->desktop()->height() );	// 90% of screen height (accounts for taskbar)
	if (image.width() <= w && image.height() <= h)
		resize ( image.width(), image.height() + toolBar()->height());	// the 32 pixel are the space of the toolbar

//If the image is larger than screen width and/or screen height, shrink it to fit the screen
//while preserving the original aspect ratio

	else if (image.width() <= w) //only the height is too large
		resize ( int( image.width()*h/image.height() ), h );
	else if (image.height() <= h) //only the width is too large
		resize ( w, int( image.height()*w/image.width() ) );
	else { //uh-oh...both width and height are too large.  which needs to be shrunk least?
		float fx = float(w)/float(image.width());
		float fy = float(h)/float(image.height());
    if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
			resize ( int( image.width()*fy ), h );
		else //vice versa
			resize ( w, int( image.height()*fx ) );
	}

	show();	// hide is default
// pix will be initialized in resizeEvent(), which will automatically called first time
}

void ImageViewer::saveFileToDisc()
{
	KURL newURL = KFileDialog::getSaveURL(imageURL.filename());  // save-dialog with default filename
	if (!newURL.isEmpty())
	{
		QFile f (newURL.directory() + "/" +  newURL.filename());
                kdDebug()<<"Saving to :"<<f.name()<<endl;
		if (f.exists())
		{
                    kdDebug()<<"Warning! Remove existing file "<< f.name()<<endl;
                    f.remove();
		}
		saveFile (newURL);
	}
}

void ImageViewer::saveFile (KURL &url) {
// synchronous Access to prevent segfaults
	if (!KIO::NetAccess::copy (KURL (file->name()), url))
	{
		QString text = i18n ("Saving of the image %1 failed!");
		KMessageBox::error (this, text.arg (url.prettyURL() ));
	}
}

void ImageViewer::close() {
	closeEvent (0);
}

void ImageViewer::checkJob() {
	if ( downloadJob ) {  // if download job is running
		downloadJob->kill( true );  // close job quietly, without emitting a result
		kdDebug() << "Download job killed";
	}
}

#include "imageviewer.moc"
