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

#include <QFont>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QCloseEvent>
#include <QDesktopWidget>

#include <klocale.h>
#include <kmessagebox.h>
#include <kfiledialog.h>
#include <kstatusbar.h>
#include <kio/netaccess.h>
#include <kaction.h>

#include <kdebug.h>
#include <ktoolbar.h>
#include "imageviewer.h"

#include <kapplication.h>

ImageViewerUI::ImageViewerUI( QWidget *parent ) : QFrame( parent ) {
	setupUi( this );
}

ImageViewer::ImageViewer (const KUrl &url, const QString &capText, QWidget *parent)
	: KDialog(parent, url.fileName(), KDialog::User1|KDialog::Close, 0, 
		KGuiItem(i18n("Save"), "filesave") ), 
		m_ImageUrl (url), fileIsImage(false), downloadJob(0)
{
	ui = new ImageViewerUI( this );
	setMainWidget( ui );
	ui->Caption->setText( capText );

	connect( this, SIGNAL( user1Clicked() ), this, SLOT ( saveFileToDisc() ) );

	if (!m_ImageUrl.isValid())		//check URL
		kDebug()<<"URL is malformed"<<endl;
	setWindowTitle (m_ImageUrl.fileName()); // the title of the window
	loadImageFromURL();
}

ImageViewer::~ImageViewer() {
// check if download job is running
	checkJob();

	if (!file->remove())		// if the file was not complete downloaded the suffix is  ".part"
	{
		kDebug()<<QString("remove of %1 failed").arg(file->fileName())<<endl;
		file->setFileName (file->fileName() + ".part");		// set new suffix to filename
		kDebug()<<QString("try to remove %1").arg( file->fileName())<<endl;
		if (file->remove())
			kDebug()<<"file removed\n";
		else
			kDebug()<<"file not removed\n";
	}
}

void ImageViewer::paintEvent (QPaintEvent */*ev*/)
{
/*	QPainter p;
	p.begin( ui->View );
	p.drawImage( 0, 0, image.scaled( ui->View->size(), 
			Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
	p.end();*/
}

//Deprecated?
void ImageViewer::resizeEvent (QResizeEvent */*ev*/)
{
	update();
}

void ImageViewer::closeEvent (QCloseEvent *ev)
{
	if (ev)	// not if closeEvent (0) is called, because segfault
		ev->accept();	// parent-widgets should not get this event, so it will be filtered
	this->~ImageViewer();	// destroy the object, so the object can only allocated with operator new, not as a global/local variable
}

void ImageViewer::loadImageFromURL()
{
	file = tempfile.file();
	tempfile.unlink();		// we just need the name and delete the tempfile from disc; if we don't do it, a dialog will be shown
	KUrl saveURL = KUrl::fromPath( file->fileName() );
	if (!saveURL.isValid())
            kDebug()<<"tempfile-URL is malformed\n";

	downloadJob = KIO::copy (m_ImageUrl, saveURL);	// starts the download asynchron
	connect (downloadJob, SIGNAL (result (KJob *)), SLOT (downloadReady (KJob *)));
}

void ImageViewer::downloadReady (KJob *job)
{
// set downloadJob to 0, but don't delete it - the job will automatically deleted !!!
	downloadJob = 0;

	if ( job->error() )
	{
		static_cast<KIO::Job*>(job)->showErrorDialog();
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
	if (!image.load (file->fileName()))		// if loading failed
	{
		QString text = i18n ("Loading of the image %1 failed.", m_ImageUrl.prettyURL());
		KMessageBox::error (this, text);
		closeEvent (0);
		return;
	}
	fileIsImage = true;	// we loaded the file and know now, that it is an image

	//If the image is larger than screen width and/or screen height, 
	//shrink it to fit the screen
	QRect deskRect = kapp->desktop()->availableGeometry();
	int w = deskRect.width(); // screen width
	int h = deskRect.height(); // screen height

	if ( image.width() <= w && image.height() > h ) //Window is taller than desktop
		ui->View->resize( int( image.width()*h/image.height() ), h );

	else if ( image.height() <= h && image.width() > w ) //window is wider than desktop
		ui->View->resize( w, int( image.height()*w/image.width() ) );

	else if ( image.width() > w && image.height() > h ) { //window is too tall and too wide
		//which needs to be shrunk least, width or height?
		float fx = float(w)/float(image.width());
		float fy = float(h)/float(image.height());
    if (fx > fy) //width needs to be shrunk less, so shrink to fit in height
			ui->View->resize( int( image.width()*fy ), h );
		else //vice versa
			ui->View->resize( w, int( image.height()*fx ) );
	} else
		ui->View->resize( image.size() );

	ui->View->setPixmap( QPixmap::fromImage( image.scaled( ui->View->size(), 
			Qt::KeepAspectRatio, Qt::SmoothTransformation ) ) );

	show();	// hide is default
}

void ImageViewer::saveFileToDisc()
{
	KUrl newURL = KFileDialog::getSaveURL(m_ImageUrl.fileName());  // save-dialog with default filename
	if (!newURL.isEmpty())
	{
		QFile f (newURL.directory() + '/' +  newURL.fileName());
		if (f.exists())
		{
			int r=KMessageBox::warningContinueCancel(static_cast<QWidget *>(parent()),
									i18n( "A file named \"%1\" already exists. "
											"Overwrite it?" , newURL.fileName()),
									i18n( "Overwrite File?" ),
									i18n( "&Overwrite" ) );
			if(r==KMessageBox::Cancel) return;

			f.remove();
		}
		saveFile (newURL);
	}
}

void ImageViewer::saveFile (KUrl &url) {
// synchronous Access to prevent segfaults
	if (!KIO::NetAccess::copy (KUrl (file->fileName()), url, (QWidget*) 0))
	{
		QString text = i18n ("Saving of the image %1 failed.", url.prettyURL());
		KMessageBox::error (this, text);
	}
}

void ImageViewer::close() {
	closeEvent (0);
}

void ImageViewer::checkJob() {
	if ( downloadJob ) {  // if download job is running
		downloadJob->kill( true );  // close job quietly, without emitting a result
		kDebug() << "Download job killed";
	}
}

#include "imageviewer.moc"
