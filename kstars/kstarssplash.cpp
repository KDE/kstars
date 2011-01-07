/***************************************************************************
                          kstarssplash.cpp  -  description
                             -------------------
    begin                : Thu Jul 26 2001
    copyright            : (C) 2001 by Heiko Evermann
    email                : heiko@evermann.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <tqfile.h>
#include <tqlabel.h>
#include <tqlayout.h>
#include <klocale.h>

#include <kapplication.h>

#include "kstarssplash.h"
#include "ksutils.h"

KStarsSplash::KStarsSplash( TQWidget *parent, const char* name )
	: KDialogBase( KDialogBase::Plain, i18n( "Loading KStars..." ),
			0 /*no buttons*/, Ok, parent, name, false /*not modal*/ ) {

	//Set up widgets for splashscreen.
	TQFrame *page = plainPage();
	page->setBackgroundColor( TQColor( "Black" ) );
	setBackgroundColor( TQColor( "Black" ) );

	TQVBoxLayout *topLayout = new TQVBoxLayout( page, 0, 0 );
	topLayout->setMargin( 0 );
	
	//Load the KStars banner.  Use an empty image if it can't be opened.
	TQFile imFile;
	if ( KSUtils::openDataFile( imFile, "kstars.png" ) ) {
		imFile.close(); //Just need the filename...
		splashImage = new TQPixmap( imFile.name() );
	} else {
		splashImage = new TQPixmap(); //null image
	}

	Banner = new TQWidget( page );
	Banner->setFixedWidth( splashImage->width() );
	Banner->setFixedHeight( splashImage->height() );
	topLayout->addWidget( Banner );
	
//initialize the "Welcome to KStars message label
	label = new TQLabel( page, "label1" );
	TQPalette pal( label->palette() );
	pal.setColor( TQPalette::Normal, TQColorGroup::Background, TQColor( "Black" ) );
	pal.setColor( TQPalette::Inactive, TQColorGroup::Background, TQColor( "Black" ) );
	pal.setColor( TQPalette::Normal, TQColorGroup::Foreground, TQColor( "White" ) );
	pal.setColor( TQPalette::Inactive, TQColorGroup::Foreground, TQColor( "White" ) );
	label->setPalette( pal );
	label->setAlignment( AlignHCenter );
	label->setText( i18n( "Welcome to KStars. Please stand by while loading..." ) );
	topLayout->addWidget( label );

//initialize the progress message label
	textCurrentStatus = new TQLabel( page, "label2" );
	textCurrentStatus->setPalette( pal );
	textCurrentStatus->setAlignment( AlignHCenter );
	topLayout->addWidget( textCurrentStatus );

	topLayout->activate();
	disableResize();
	setMessage(TQString::null);  // force repaint of widget with no text
}

KStarsSplash::~KStarsSplash() {
	delete splashImage;
}

void KStarsSplash::paintEvent( TQPaintEvent* ) {
	bitBlt( Banner, 0, 0, splashImage, 0, 0, -1, -1 );
	label->repaint();  // standard text label
	textCurrentStatus->repaint();  // status text label
}

void KStarsSplash::closeEvent( TQCloseEvent *e ) {
	e->ignore();
	emit closeWindow();
}

void KStarsSplash::setMessage( TQString s ) {
	textCurrentStatus->setText( s );
	repaint();  // repaint splash screen
/**
	*Flush all event data. This is needed because events will buffered and
	*repaint call will queued in event buffer. With flush all X11 events of
	*this application will flushed.
	*/
	kapp->flush();
}

#include "kstarssplash.moc"
