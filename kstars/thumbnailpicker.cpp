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
	//First add images from object's ImageList
	QStringList::Iterator itList  = Object->ImageList.begin();
	QStringList::Iterator itListEnd = Object->ImageList.end();
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

void ThumbnailPicker::downloadReady(KIO::Job *job) {
	//No need to delete the job, it is automatically deleted !
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
	QPixmap pm;
	if ( PixList.at(i)->width() > PixList.at(i)->height() )
		pm = shrinkImage( PixList.at(i), 0, 200 ); //scale width
	else
		pm = shrinkImage( PixList.at(i), 200, 0 ); //scale height

	pm.resize( 200, 200 ); //crop image to make it square
	ui->CurrentImage->setPixmap( pm );
	ui->CurrentImage->update();

	//Set Image to the selected 200x200 pixmap
	*Image = pm;
	bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL( const QString &url ) {}
void ThumbnailPicker::slotCheckValidURL( const QString &url ) {}

