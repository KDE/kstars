/***************************************************************************
                          thumbnailpicker.cpp  -  description
                             -------------------
    begin                : Thu Mar 2 2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <qframe.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qimage.h>
#include <qpixmap.h>
#include <qfile.h>

#include <kpushbutton.h>
#include <klistbox.h>
#include <kprogress.h>
#include <kurl.h>
#include <kurlrequester.h>
#include <klocale.h>
#include <ktempfile.h>

#include "thumbnailpicker.h"
#include "thumbnailpickerui.h"
#include "thumbnaileditor.h"
#include "ksutils.h"
#include "detaildialog.h"
#include "skyobject.h"

ThumbnailPicker::ThumbnailPicker( SkyObject *o, const QPixmap &current, QWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Choose a Thumbnail Image" ), Ok|Cancel, Ok, parent, name ),
		dd((DetailDialog*)parent), Object(o), bImageFound( false )
{
	Image = new QPixmap( current );

	QFrame *page = plainPage();
	QVBoxLayout *vlay = new QVBoxLayout( page, 0, 0 );
	ui = new ThumbnailPickerUI( page );
	vlay->addWidget( ui );

	ui->CurrentImage->setPixmap( *Image );

	PixList.setAutoDelete( true );

	connect( ui->EditButton, SIGNAL( clicked() ), this, SLOT( slotEditImage() ) );
	connect( ui->UnsetButton, SIGNAL( clicked() ), this, SLOT( slotUnsetImage() ) );
	connect( ui->ImageList, SIGNAL( highlighted( int ) ),
						this, SLOT( slotSetFromList( int ) ) );
	connect( ui->ImageURLBox, SIGNAL( urlSelected( const QString& ) ),
						this, SLOT( slotSetFromURL( const QString& ) ) );
	connect( ui->ImageURLBox, SIGNAL( textChanged( const QString& ) ),
						this, SLOT( slotCheckValidURL( const QString& ) ) );

	slotFillList();
}

ThumbnailPicker::~ThumbnailPicker()
{}

//Query online sources for images of the object
void ThumbnailPicker::slotFillList() {
	//Preload list with object's ImageList:
	QStringList ImageList( Object->ImageList );

	//Query Google Image Search:
	KURL gURL( "http://images.google.com/images" );
	//Search for the primary name, or longname and primary name
	QString sName = QString("\"%1\"").arg( Object->name() );
	if ( Object->longname() != Object->name() ) {
		sName = QString("\"%1\" ").arg( Object->longname() ) + sName;
	}
	gURL.addQueryItem( "q", sName ); //add the Google-image query string

	//Download the google page and parse it for image URLs
	parseGooglePage( ImageList, gURL.prettyURL() );

	//Total Number of images to be loaded:
	int nImages = ImageList.count();
	if ( nImages ) {
		ui->SearchProgress->setTotalSteps( nImages );
		ui->SearchLabel->setText( i18n( "Loading images..." ) );
	}

	//Add images from the ImageList
	QStringList::Iterator itList  = ImageList.begin();
	QStringList::Iterator itListEnd = ImageList.end();
	for ( ; itList != itListEnd; ++itList ) {
		QString s( *itList );
		KURL u( s );
		if ( u.isValid() && KIO::NetAccess::exists(u, true, this) ) {
			KTempFile ktf;
			QFile *tmpFile = ktf.file();
			ktf.unlink(); //just need filename
			JobList.append( KIO::copy( u, KURL( tmpFile->name() ), false ) ); //false = no progress window
			((KIO::CopyJob*)JobList.current())->setInteractive( false ); // suppress error dialogs
			connect (JobList.current(), SIGNAL (result(KIO::Job *)), SLOT (downloadReady (KIO::Job *)));

		}
	}
}

void ThumbnailPicker::parseGooglePage( QStringList &ImList, QString URL ) {
	QString tmpFile;
	QString PageHTML;

	//Read the google image page's HTML into the PageHTML QString:
	if ( KIO::NetAccess::exists(URL, true, this) && KIO::NetAccess::download( URL, tmpFile ) ) {
		QFile file( tmpFile );
		if ( file.open( IO_ReadOnly ) ) {
			QTextStream instream(&file);
			PageHTML = instream.read();
			file.close();
		} else {
			kdDebug() << "Could not read local copy of google image page" << endl;
			return;
		}
	} else {
		kdDebug() << KIO::NetAccess::lastErrorString() << endl;
		return;
	}

	int index = PageHTML.find( "?imgurl=", 0 );
	while ( index >= 0 ) {
		index += 8; //move to end of "?imgurl=" marker

		//Image URL is everything from index to next occurence of "&"
		ImList.append( PageHTML.mid( index, PageHTML.find( "&", index ) - index ) );

//		//DEBUG
//		kdDebug() << index << "::" << ImList.last() << endl;

		index = PageHTML.find( "?imgurl=", index );
	}
}

void ThumbnailPicker::downloadReady(KIO::Job *job) {
	//Note: no need to delete the job, it is automatically deleted !

	//Update Progressbar
	ui->SearchProgress->advance(1);
	if ( ui->SearchProgress->progress() == ui->SearchProgress->totalSteps() ) {
		ui->SearchProgress->hide();
		ui->SearchLabel->setText( i18n( "Search results:" ) );
	}

	//If there was a problem, just return silently without adding image to list.
	if ( job->error() ) {
//		job->showErrorDialog();
		return;
	}

	KIO::CopyJob *cjob = (KIO::CopyJob*)job;
	QFile tmp( cjob->destURL().path() );
	tmp.close(); // to get the newest information of the file

	//Add image to list
	if ( tmp.exists() ) {
		PixList.append( new QPixmap( tmp.name() ) );

		//Add small image and URL to listbox
		ui->ImageList->insertItem( shrinkImage( PixList.current(), 50 ),
				cjob->srcURLs().first().prettyURL() );
	}
}

QPixmap ThumbnailPicker::shrinkImage( QPixmap *pm, uint size ) {
	uint w( pm->width() ), h( pm->height() );
	int rx(0), ry(0), sx(0), sy(0);
	if ( size == 0 ) return QPixmap();

	//Prepare variables for rescaling image (if it is larger than 'size')
	if ( w > size && w >= h ) {
		h = size;
		w = size*pm->width()/pm->height();
	} else if ( h > size && h > w ) {
		w = size;
		h = size*pm->height()/pm->width();
	}
	sx = (w - size)/2;
	sy = (h - size)/2;
	if ( sx < 0 ) { rx = -sx; sx = 0; }
	if ( sy < 0 ) { ry = -sy; sy = 0; }

	QPixmap result( size, size );
	result.fill( QColor( "white" ) ); //in case final image is smaller than 'size'

	if ( pm->width() > size || pm->height() > size ) { //image larger than 'size'?
		//convert to QImage so we can smoothscale it
		QImage im( pm->convertToImage() );
		im = im.smoothScale( w, h );
		
		//bitBlt sizexsize square section of image
		bitBlt( &result, rx, ry, &im, sx, sy, size, size );

	} else { //image is smaller than size x size
		bitBlt( &result, rx, ry, pm );
	}

	return result;
}

void ThumbnailPicker::slotEditImage() {
	ThumbnailEditor te( this );
	if ( te.exec() == QDialog::Accepted ) {
		//FIXME: set Image to the edited pixmap.  
		//note it should always be 200x200 
	}
}

void ThumbnailPicker::slotUnsetImage() {
	QFile file;
	if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
		file.close();
		Image->load( file.name(), "PNG" );
	} else {
		Image->resize( dd->thumbnail()->width(), dd->thumbnail()->height() );
		Image->fill( dd->paletteBackgroundColor() );
	}

	ui->CurrentImage->setPixmap( *Image );
	ui->CurrentImage->update();

	bImageFound = false;
}

void ThumbnailPicker::slotSetFromList( int i ) {
	//Display image in preview pane
	QPixmap pm;
	pm = shrinkImage( PixList.at(i), 200 ); //scale width

	ui->CurrentImage->setPixmap( pm );
	ui->CurrentImage->update();

	//Set Image to the selected 200x200 pixmap
	*Image = pm;
	bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL( const QString &url ) {}
void ThumbnailPicker::slotCheckValidURL( const QString &url ) {}

