/***************************************************************************
                          NewDialog.cpp  -  description
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

#include <qwidget.h>
#include <qpixmap.h>
#include <qfile.h>
#include <qpaintdevice.h>
#include <qlayout.h>
#include <qlabel.h>
#include <kmessagebox.h>
#include <klocale.h>

#include "ksutils.h"
#include "kstarssplash.h"

#include <qglobal.h>
#if (QT_VERSION <= 299)
#include <kapp.h>
#define FLUSH flushX
#else
#include <kapplication.h>
#define FLUSH flush
#endif

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
}

void KStarsSplash::paintEvent( QPaintEvent *e ) {
	bitBlt( Banner, 0, 0, splashImage, 0, 0, -1, -1 );
	label->repaint();  // standard text label
	textCurrentStatus->repaint();  // status text label
}

void KStarsSplash::setMessage( QString s ) {
	textCurrentStatus->setText( s );
	repaint();  // repaint splash screen
/**
	*Flush all event data. This is needed because events will buffered and
	*repaint call will queued in event buffer. With flush all X11 events of
	*this application will flushed.
	*/
	kapp->FLUSH();
}

#include "kstarssplash.moc"
