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

#include "kstarssplash.h"

#include <QFile>
#include <QLabel>
#include <QPixmap>
#include <QVBoxLayout>
#include <QFrame>
#include <QPaintEvent>
#include <QPainter>
#include <QCloseEvent>
#include <QApplication>

#include <klocale.h>
#include <kdebug.h>

#include "ksutils.h"

//FIXME: program was crashing with KStarsSplash derived from KDialog,
//so I am temporarily using QDialog.  Try switching back to KDialog
//later on (this message added 20-June-2006)
KStarsSplash::KStarsSplash( QWidget *parent, const QString& customMessage)
	: QDialog( parent )
{

	QString message = customMessage.isEmpty() ? 
		i18n( "Welcome to KStars. Please stand by while loading..." ) :
		customMessage;

	QPalette p = palette();
	p.setColor( QPalette::Window, Qt::black );
	setPalette( p );

	//Set up widgets for splashscreen.
// KDialog stuff:
//	QFrame *page = new QFrame( this );
//        setMainWidget( page );
//        setCaption( i18n( "Loading KStars..." ) );
//        setButtons( 0 );

    setWindowTitle( i18n("KStars") );

    QVBoxLayout *topLayout = new QVBoxLayout( this );
	topLayout->setMargin( 0 );
	topLayout->setSpacing( 0 );

	//Load the KStars banner.  Use an empty image if it can't be opened.
	QFile imFile;
	QPixmap pmSplash;
	if ( KSUtils::openDataFile( imFile, "kstars.png" ) ) {
		imFile.close(); //Just need the filename...
		pmSplash.load( imFile.fileName() );
	}

	Banner = new QLabel( this ); //reparent to "page" if KDialog
	Banner->setPixmap( pmSplash );
	topLayout->addWidget( Banner );

//initialize the "Welcome to KStars message label
	label = new QLabel( this ); //reparent to "page" if KDialog
	label->setObjectName( "label1" );

	QPalette pal( label->palette() );
	pal.setColor( QPalette::Normal, QPalette::Window, QColor( "Black" ) );
	pal.setColor( QPalette::Inactive, QPalette::Window, QColor( "Black" ) );
	pal.setColor( QPalette::Normal, QPalette::WindowText, QColor( "White" ) );
	pal.setColor( QPalette::Inactive, QPalette::WindowText, QColor( "White" ) );
	label->setPalette( pal );
	label->setAlignment( Qt::AlignHCenter );
	label->setText( message );
	topLayout->addWidget( label );

//initialize the progress message label
	textCurrentStatus = new QLabel( this ); //reparent to "page" if KDialog
	textCurrentStatus->setObjectName( "label2" );
	textCurrentStatus->setPalette( pal );
	textCurrentStatus->setAlignment( Qt::AlignHCenter );
	topLayout->addWidget( textCurrentStatus );

	setMessage(QString());  // force repaint of widget with no text
}

KStarsSplash::~KStarsSplash() {
}

void KStarsSplash::closeEvent( QCloseEvent *e ) {
	e->ignore();
	emit closeWindow();
}

void KStarsSplash::setMessage( const QString &s ) {
	textCurrentStatus->setText( s );
	//repaint();  // repaint splash screen  -jbb: this seems no longer needed
/**
	*Flush all event data. This is needed because events will buffered and
	*repaint call will queued in event buffer. With flush all X11 events of
	*this application will flushed.
	*/
	//qApp->flush();                       -jbb: this seems no longer needed
}

#include "kstarssplash.moc"
