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

#include <tqframe.h>
#include <tqlayout.h>
#include <tqlabel.h>
#include <tqimage.h>
#include <tqpixmap.h>
#include <tqfile.h>
#include <tqrect.h>
#include <tqstyle.h>

#include <kapplication.h>
#include <kdeversion.h>
#include <kpushbutton.h>
#include <klineedit.h>
#include <klistbox.h>
#include <kmessagebox.h>
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

ThumbnailPicker::ThumbnailPicker( SkyObject *o, const TQPixmap &current, TQWidget *parent, const char *name )
 : KDialogBase( KDialogBase::Plain, i18n( "Choose Thumbnail Image" ), Ok|Cancel, Ok, parent, name ),
		SelectedImageIndex(-1), dd((DetailDialog*)parent), Object(o), bImageFound( false )
{
	Image = new TQPixmap( current );
	ImageRect = new TQRect( 0, 0, 200, 200 );

	TQFrame *page = plainPage();
	TQVBoxLayout *vlay = new TQVBoxLayout( page, 0, 0 );
	ui = new ThumbnailPickerUI( page );
	vlay->addWidget( ui );

	ui->CurrentImage->setPixmap( *Image );

	PixList.setAutoDelete( true );

	connect( ui->EditButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotEditImage() ) );
	connect( ui->UnsetButton, TQT_SIGNAL( clicked() ), this, TQT_SLOT( slotUnsetImage() ) );
	connect( ui->ImageList, TQT_SIGNAL( highlighted( int ) ),
						this, TQT_SLOT( slotSetFromList( int ) ) );
	connect( ui->ImageURLBox, TQT_SIGNAL( urlSelected( const TQString& ) ),
						this, TQT_SLOT( slotSetFromURL() ) );
	connect( ui->ImageURLBox, TQT_SIGNAL( returnPressed() ),
						this, TQT_SLOT( slotSetFromURL() ) );

	ui->ImageURLBox->lineEdit()->setTrapReturnKey( true );
	ui->EditButton->setEnabled( false );

	slotFillList();
}

ThumbnailPicker::~ThumbnailPicker()
{}

//Query online sources for images of the object
void ThumbnailPicker::slotFillList() {
	//Preload list with object's ImageList:
	TQStringList ImageList( Object->ImageList );

	//Query Google Image Search:
	KURL gURL( "http://images.google.com/images" );
	//Search for the primary name, or longname and primary name
	TQString sName = TQString("\"%1\"").arg( Object->name() );
	if ( Object->longname() != Object->name() ) {
		sName = TQString("\"%1\" ").arg( Object->longname() ) + sName;
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
	TQStringList::Iterator itList  = ImageList.begin();
	TQStringList::Iterator itListEnd = ImageList.end();
	for ( ; itList != itListEnd; ++itList ) {
		TQString s( *itList );
		KURL u( s );
		if ( u.isValid() && KIO::NetAccess::exists(u, true, this) ) {
			KTempFile ktf;
			TQFile *tmpFile = ktf.file();
			ktf.unlink(); //just need filename
			JobList.append( KIO::copy( u, KURL( tmpFile->name() ), false ) ); //false = no progress window
#if KDE_IS_VERSION( 3, 3, 90 )
			((KIO::CopyJob*)JobList.current())->setInteractive( false ); // suppress error dialogs
#endif
			connect (JobList.current(), TQT_SIGNAL (result(KIO::Job *)), TQT_SLOT (downloadReady (KIO::Job *)));

		}
	}
}

void ThumbnailPicker::parseGooglePage( TQStringList &ImList, TQString URL ) {
	TQString tmpFile;
	TQString PageHTML;

	//Read the google image page's HTML into the PageHTML TQString:
	if ( KIO::NetAccess::exists(URL, true, this) && KIO::NetAccess::download( URL, tmpFile ) ) {
		TQFile file( tmpFile );
		if ( file.open( IO_ReadOnly ) ) {
			TQTextStream instream(&file);
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

		index = PageHTML.find( "?imgurl=", index );
	}
}

void ThumbnailPicker::downloadReady(KIO::Job *job) {
	//Note: no need to delete the job, it is automatically deleted !

	//Update Progressbar
	if ( ! ui->SearchProgress->isHidden() ) {
		ui->SearchProgress->advance(1);
		if ( ui->SearchProgress->progress() == ui->SearchProgress->totalSteps() ) {
			ui->SearchProgress->hide();
			ui->SearchLabel->setText( i18n( "Search results:" ) );
		}
	}

	//If there was a problem, just return silently without adding image to list.
	if ( job->error() ) {
//		job->showErrorDialog();
		return;
	}

	KIO::CopyJob *cjob = (KIO::CopyJob*)job;
	TQFile tmp( cjob->destURL().path() );
	tmp.close(); // to get the newest information of the file

	//Add image to list
	//If image is taller than desktop, rescale it.
	//I tried to use kapp->style().pixelMetric( TQStyle::PM_TitleBarHeight )
	//for the titlebar height, but this returned zero.
	//Hard-coding 25 instead :(
	if ( tmp.exists() ) {
		TQImage im( tmp.name() );

		if ( im.isNull() ) { 
		  //KMessageBox::sorry( 0, i18n("Failed to load image"), 
		  //       i18n("Could Not Load Specified Image") );
			return;
		}

		uint w = im.width();
		uint h = im.height();
		uint pad = 4*marginHint() + 2*ui->SearchLabel->height() + actionButton( Ok )->height() + 25;
		uint hDesk = kapp->desktop()->availableGeometry().height() - pad;

//	this returns zero...
// 		//DEBUG
// 		kdDebug() << "Title bar height: " << kapp->style().pixelMetric( TQStyle::PM_TitleBarHeight ) << endl;

		if ( h > hDesk ) 
			im = im.smoothScale( w*hDesk/h, hDesk );

		PixList.append( new TQPixmap( im ) );

		//Add 50x50 image and URL to listbox
		ui->ImageList->insertItem( shrinkImage( PixList.current(), 50 ),
				cjob->srcURLs().first().prettyURL() );
	}
}

TQPixmap ThumbnailPicker::shrinkImage( TQPixmap *pm, int size, bool setImage ) {
	int w( pm->width() ), h( pm->height() );
	int bigSize( w );
	int rx(0), ry(0), sx(0), sy(0), bx(0), by(0);
	if ( size == 0 ) return TQPixmap();

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

	if ( setImage ) bigSize = int( 200.*float(pm->width())/float(w) );

	TQPixmap result( size, size );
	result.fill( TQColor( "white" ) ); //in case final image is smaller than 'size'

	if ( pm->width() > size || pm->height() > size ) { //image larger than 'size'?
		//convert to TQImage so we can smoothscale it
		TQImage im( pm->convertToImage() );
		im = im.smoothScale( w, h );
		
		//bitBlt sizexsize square section of image
		bitBlt( &result, rx, ry, &im, sx, sy, size, size );
		if ( setImage ) {
			bx = int( sx*float(pm->width())/float(w) );
			by = int( sy*float(pm->width())/float(w) );
			ImageRect->setRect( bx, by, bigSize, bigSize );
		}

	} else { //image is smaller than size x size
		bitBlt( &result, rx, ry, pm );
		if ( setImage ) {
			bx = int( rx*float(pm->width())/float(w) );
			by = int( ry*float(pm->width())/float(w) );
			ImageRect->setRect( bx, by, bigSize, bigSize );
		}
	}

	return result;
}

void ThumbnailPicker::slotEditImage() {
	ThumbnailEditor te( this );
	if ( te.exec() == TQDialog::Accepted ) {
		TQPixmap pm = te.thumbnail();
		*Image = pm;
		ui->CurrentImage->setPixmap( pm );
		ui->CurrentImage->update();
	}
}

void ThumbnailPicker::slotUnsetImage() {
	TQFile file;
	if ( KSUtils::openDataFile( file, "noimage.png" ) ) {
		file.close();
		Image->load( file.name(), "PNG" );
	} else {
		Image->resize( dd->thumbnail()->width(), dd->thumbnail()->height() );
		Image->fill( dd->paletteBackgroundColor() );
	}

	ui->EditButton->setEnabled( false );
	ui->CurrentImage->setPixmap( *Image );
	ui->CurrentImage->update();

	bImageFound = false;
}

void ThumbnailPicker::slotSetFromList( int i ) {
	//Display image in preview pane
	TQPixmap pm;
	pm = shrinkImage( PixList.at(i), 200, true ); //scale image
	SelectedImageIndex = i;

	ui->CurrentImage->setPixmap( pm );
	ui->CurrentImage->update();
	ui->EditButton->setEnabled( true );

	//Set Image to the selected 200x200 pixmap
	*Image = pm;
	bImageFound = true;
}

void ThumbnailPicker::slotSetFromURL() {
	//Attempt to load the specified URL
	KURL u = ui->ImageURLBox->url();

	if ( u.isValid() ) {
		if ( u.isLocalFile() ) {
			TQFile localFile( u.path() );

			//Add image to list
			//If image is taller than desktop, rescale it.
			TQImage im( localFile.name() );

			if ( im.isNull() ) {
				KMessageBox::sorry( 0, 
						i18n("Failed to load image at %1").arg( localFile.name() ),
						i18n("Failed to Load Image") );
				return;
			}

			uint w = im.width();
			uint h = im.height();
			uint pad = 4*marginHint() + 2*ui->SearchLabel->height() + actionButton( Ok )->height() + 25;
			uint hDesk = kapp->desktop()->availableGeometry().height() - pad;

			if ( h > hDesk ) 
				im = im.smoothScale( w*hDesk/h, hDesk );

			//Add Image to top of list and 50x50 thumbnail image and URL to top of listbox
			PixList.insert( 0, new TQPixmap( im ) );
			ui->ImageList->insertItem( shrinkImage( PixList.current(), 50 ),
					u.prettyURL(), 0 );

			//Select the new image
			ui->ImageList->setCurrentItem( 0 );
			slotSetFromList(0);

		} else if ( KIO::NetAccess::exists(u, true, this) ) {
			KTempFile ktf;
			TQFile *tmpFile = ktf.file();
			ktf.unlink(); //just need filename
			JobList.append( KIO::copy( u, KURL( tmpFile->name() ), false ) ); //false = no progress window
#if KDE_IS_VERSION( 3, 3, 90 )
			((KIO::CopyJob*)JobList.current())->setInteractive( false ); // suppress error dialogs
#endif
			connect (JobList.current(), TQT_SIGNAL (result(KIO::Job *)), TQT_SLOT (downloadReady (KIO::Job *)));

			//
		}
	}
}


#include "thumbnailpicker.moc"
