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

#include <qfile.h>
#include <qlabel.h>
#include <qlayout.h>
#include <klocale.h>

#include <kapplication.h>

#include "kstarssplash.h"
#include "ksutils.h"

KStarsSplash::KStarsSplash( QWidget *parent, const char* name )
	: KDialogBase( KDialogBase::Plain, i18n( "Loading KStars..." ),
			0 /*no buttons*/, Ok, parent, name, false /*not modal*/ ) {

	//Set up widgets for splashscreen.
	QFrame *page = plainPage();
	page->setBackgroundColor( QColor( "Black" ) );
	setBackgroundColor( QColor( "Black" ) );

	QVBoxLayout *topLayout = new QVBoxLayout( page, 0, 0 );
	topLayout->setMargin( 0 );
	
	//Load the KStars banner.  Use an empty image if it can't be opened.
	QFile imFile;
	if ( KSUtils::openDataFile( imFile, "kstars.png" ) ) {
		imFile.close(); //Just need the filename...
		splashImage = new QPixmap( imFile.name() );
	} else {
		splashImage = new QPixmap(); //null image
	}

	Banner = new QWidget( page );
	Banner->setFixedWidth( splashImage->width() );
	Banner->setFixedHeight( splashImage->height() );
	topLayout->addWidget( Banner );
	
//initialize the "Welcome to KStars message label
	label = new QLabel( page, "label1" );
	QPalette pal( label->palette() );
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "Black" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Background, QColor( "Black" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "White" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "White" ) );
	label->setPalette( pal );
	label->setAlignment( AlignHCenter );
	label->setText( i18n( "Welcome to KStars. Please stand by while loading..." ) );
	topLayout->addWidget( label );

//initialize the progress message label
	textCurrentStatus = new QLabel( page, "label2" );
	textCurrentStatus->setPalette( pal );
	textCurrentStatus->setAlignment( AlignHCenter );
	topLayout->addWidget( textCurrentStatus );

	topLayout->activate();
	disableResize();
	setMessage(QString::null);  // force repaint of widget with no text
}

KStarsSplash::~KStarsSplash() {
	delete splashImage;
}

void KStarsSplash::paintEvent( QPaintEvent* ) {
	bitBlt( Banner, 0, 0, splashImage, 0, 0, -1, -1 );
	label->repaint();  // standard text label
	textCurrentStatus->repaint();  // status text label
}

void KStarsSplash::closeEvent( QCloseEvent *e ) {
	e->ignore();
	emit closeWindow();
}

void KStarsSplash::setMessage( QString s ) {
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
