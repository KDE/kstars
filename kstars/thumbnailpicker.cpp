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
		if ( u.isValid() ) {
			KTempFile ktf;
			QFile *tmpFile = ktf.file();
			ktf.unlink(); //just need filename
			JobList.append( KIO::copy( u, KURL( tmpFile->name() ), false ) ); //false = no progress window
			connect (JobList.current(), SIGNAL (result(KIO::Job *)), SLOT (downloadReady (KIO::Job *)));

		}
	}
}

void ThumbnailPicker::parseGooglePage( QStringList &ImList, QString URL ) {
	QString tmpFile;
	QString PageHTML;

	//Read the google image page's HTML into the PageHTML QString:
	if ( KIO::NetAccess::download( URL, tmpFile ) ) {
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
		ui->ImageList->insertItem( shrinkImage( PixList.current(), 0, 50 ), // 0 means scale width 
				cjob->srcURLs().first().prettyURL() );
	}
}

QPixmap ThumbnailPicker::shrinkImage( QPixmap *pm, int w, int h ) {
	if ( w == 0 & &h == 0 ) return QPixmap();
	if ( w == 0 ) w = h*pm->width()/pm->height();
	if ( h == 0 ) h = w*pm->height()/pm->width();

	//Resize a copy of the image so it will fit in listbox:
	QImage im( pm->convertToImage() );
	if ( pm->height() > h ) im = im.smoothScale( w, h );
	QPixmap result; result.convertFromImage(im);
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
	QPixmap pm, pm2;
	if ( PixList.at(i)->width() > PixList.at(i)->height() )
		pm = shrinkImage( PixList.at(i), 0, 200 ); //scale width
	else
		pm = shrinkImage( PixList.at(i), 200, 0 ); //scale height

	pm2.resize( 200, 200 ); //crop image to make it square
	int sx(0), sy(0);
	if ( pm.width() > pm.height() ) 
		sx = (pm.width() - 200)/2;
	else 
		sy = (pm.height() - 200)/2;

	bitBlt( &pm2, 0, 0, &pm, sx, sy );
	ui->CurrentImage->setPixmap( pm2 );
	ui->CurrentImage->update();

	//Set Image to the selected 200x200 pixmap
	*Image = pm2;
	bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL( const QString &url ) {}
void ThumbnailPicker::slotCheckValidURL( const QString &url ) {}

