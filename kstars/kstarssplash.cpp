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
#include <kmessagebox.h>
#include <kapplication.h>
#include <klocale.h>

#include "kstarssplash.h"

KStarsSplash::KStarsSplash( KStarsData* kstarsData, QWidget *parent, const char* name, bool modal )
		: KDialogBase( KDialogBase::Plain, i18n( "Loading KStars..." ),
			0 /*no buttons please*/, Ok, parent ) {
	QFrame *page = plainPage();
	page->setBackgroundColor( QColor( "Black" ) );
	setBackgroundColor( QColor( "Black" ) );

	this->kstarsData = kstarsData;
  QVBoxLayout *topLayout = new QVBoxLayout( page, 0, 0 );//spacingHint() );
	topLayout->setMargin( 0 ); //still have an ugly margin :(
	
	QFile imFile;
	if ( kstarsData->openDataFile( imFile, "kstars.png" ) ) {
		imFile.close(); //Just need the filename...
		splashImage = new QPixmap( imFile.name() );
	} else {
		splashImage = new QPixmap(); //null image
	}

	Banner = new QWidget( page );
	Banner->setFixedWidth( splashImage->width() );
	Banner->setFixedHeight( splashImage->height() );
	topLayout->addWidget( Banner );
	
  QLabel *label = new QLabel( page, "label1" );
	QPalette pal( label->palette() );
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "Black" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "White" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "White" ) );
  label->setPalette( pal );
	label->setAlignment( AlignHCenter );
  label->setText( i18n( "Welcome to KStars. Please stand by while loading..." ) );
  topLayout->addWidget( label );

  //
  // It is very simple to add a new label here
  //
  textCurrentStatus = new QLabel( page, "label2" );
  textCurrentStatus->setPalette( pal );
	textCurrentStatus->setAlignment( AlignHCenter );
  textCurrentStatus->setText( i18n("Loading city data" ) );
  topLayout->addWidget( textCurrentStatus );

  topLayout->activate();

	// start the loading sequence. This must be timer based.
	// Otherwise the labels won't display.                 	
	loadStatus = 0;
  qtimer = new QTimer();

	connect ( qtimer, SIGNAL (timeout()), this, SLOT( slotLoadDataFile() ) );
	// special timer: fire when message queue is empty. Ideal for long jobs
	// see QT documentation of class QTimer;
  qtimer->start(0); 	
}

void KStarsSplash::paintEvent( QPaintEvent *e ) {
	bitBlt( Banner, 0, 0, splashImage, 0, 0, -1, -1 );
}
	
//JH: Under KDE3, there is a weird problem with the textCurrentStatus QLabel: it was
//only being updated on every other repaint event!  I added debug messages to make sure the
//paint events were happening.  They were, but only half of them displayed the updated
//text in the Label.  I don't know why this is happening, but as a workaround, I changed
//the case labels in slotLoadDataFile() so that only even-numbered loadStatus values trigger
//an action.  odd-numbered values do nothing.  (I also had to move the "loadStatus++" command
//outside the switch statement).  With this change, all status messages are now displayed.
//It's a kludge, but it works :/

void KStarsSplash::slotLoadDataFile()
{
	QFile imFile;
	QString ImageName;
	
	switch (loadStatus)
	{
	case 2:
		if ( !kstarsData->readCityData( ) ) {
			qtimer->stop();
			KludgeError( "Cities.dat" );
			reject();
		}
	  textCurrentStatus->setText( i18n("Loading star data" ) ); // next load, show text now
		break;
	case 4:
		if ( !kstarsData->readStarData( ) ) {
			qtimer->stop();
			KludgeError( "sao.dat" );
			reject();
		}
	  textCurrentStatus->setText( i18n("Loading NGC data" ) ); // next load, show text now
		break;
	case 6:
		if ( !kstarsData->readNGCData( ) ) {
			qtimer->stop();
			KludgeError( "ngc2000.dat" );
			reject();
		}
	  textCurrentStatus->setText( i18n("Loading constellations" ) ); // next load, show text now
		break;
	case 8:
		if ( !kstarsData->readCLineData( ) ) {
			qtimer->stop();
			KludgeError( "clines.dat" );
			reject();
		}
	  textCurrentStatus->setText( i18n("Loading constellation names" ) ); // next load, show text now
		break;
	case 10:
		if ( !kstarsData->readCNameData( ) ) {
			qtimer->stop();
			KludgeError( kstarsData->cnameFile );
			reject();
		}
	  textCurrentStatus->setText( i18n("Loading Milky Way" ) ); // next load, show text now
		break;
	case 12:
		if ( !kstarsData->readMWData( ) ) {
			qtimer->stop();
			KludgeError( "mw*.dat" );
			reject();
		}
	  textCurrentStatus->setText( i18n("Starting Clock" ) ); // next load, show text now
		break;
	case 14:
		//start the clock
		//"now" and "then" always refer to current system clock.	
		//LTime and UTime represent the current simulation time.
		//For now, initialize LTime to system clock; perhaps it should remember
		//the time of the last session (unless the time was the current time,
		//in that case, the user may want the current time this session as well...)
		kstarsData->now   = QDateTime::currentDateTime();
		kstarsData->then  = kstarsData->now;
		kstarsData->LTime = kstarsData->now;	
		kstarsData->UTime = kstarsData->now; //delay timezone offset to KStars constructor
		kstarsData->CurrentDate = kstarsData->getJD( kstarsData->UTime );
		kstarsData->LastPlanetUpdate = kstarsData->CurrentDate;
		kstarsData->LastMoonUpdate   = kstarsData->CurrentDate;
	  textCurrentStatus->setText( i18n("Creating Sun" ) ); // next load, show text now
		break;
	case 16:
		kstarsData->Sun = new KSSun( kstarsData->CurrentDate );
		ImageName = "sun.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Sun->image.load( imFile.name() );
		}

		kstarsData->objList->append( kstarsData->Sun );
		kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Sun->name, kstarsData->objList->current()));
	  textCurrentStatus->setText( i18n("Creating Moon" ) ); // next load, show text now
		break;
	case 18:
		kstarsData->Moon = new KSMoon( kstarsData->CurrentDate );
		kstarsData->Moon->findPhase( kstarsData->Sun );
		kstarsData->objList->append( kstarsData->Moon );
		kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Moon->name, kstarsData->objList->current()));

	  textCurrentStatus->setText( i18n("Creating Earth" ) ); // next load, show text now
		break;
	case 20:
		//Earth is only needed for calculating Geocentric coordinates of planets;
		//it is not displayed in the skymap, so there is no image, and we
		//don't add it to objList/ObjNames
		kstarsData->Earth = new KSPlanet( I18N_NOOP( "Earth" ) );
		kstarsData->Earth->findPosition( kstarsData->CurrentDate );

	  textCurrentStatus->setText( i18n( "Creating Mercury" ) ); // next load, show text now
		break;
	case 22:		
		kstarsData->Mercury = new KSPlanet( I18N_NOOP( "Mercury" ) );
		ImageName = "mercury.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Mercury->image.load( imFile.name() );
		}
		
		if ( kstarsData->Mercury->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Mercury );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Mercury->name, kstarsData->objList->current()));
		}

	  textCurrentStatus->setText( i18n( "Creating Venus" ) ); // next load, show text now
		break;
	case 24:
		kstarsData->Venus = new KSPlanet( I18N_NOOP( "Venus" ) );
		ImageName = "venus.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Venus->image.load( imFile.name() );
		}

		if ( kstarsData->Venus->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Venus );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Venus->name, kstarsData->objList->last()));
		}

	  textCurrentStatus->setText( i18n("Creating Mars" ) ); // next load, show text now
		break;
	case 26:
		kstarsData->Mars = new KSPlanet( I18N_NOOP( "Mars" ) );
		ImageName = "mars.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Mars->image.load( imFile.name() );
		}

		if ( kstarsData->Mars->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Mars );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Mars->name, kstarsData->objList->last()));
		}

	  textCurrentStatus->setText( i18n("Creating Jupiter" ) ); // next load, show text now
		break;
	case 28:
		kstarsData->Jupiter = new KSPlanet( I18N_NOOP( "Jupiter" ) );
		ImageName = "jupiter.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Jupiter->image.load( imFile.name() );
		}
		
		if ( kstarsData->Jupiter->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Jupiter );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Jupiter->name, kstarsData->objList->last()));
    }

	  textCurrentStatus->setText( i18n("Creating Saturn" ) ); // next load, show text now
		break;
	case 30:	
		kstarsData->Saturn = new KSPlanet( I18N_NOOP( "Saturn" ) );
		ImageName = "saturn.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Saturn->image.load( imFile.name() );
		}
		
		if ( kstarsData->Saturn->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Saturn );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Saturn->name, kstarsData->objList->last()));
    }

	  textCurrentStatus->setText( i18n("Creating Uranus" ) ); // next load, show text now
		break;
	case 32:
		kstarsData->Uranus = new KSPlanet( I18N_NOOP( "Uranus" ) );
		ImageName = "uranus.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Uranus->image.load( imFile.name() );
		}
		
		if ( kstarsData->Uranus->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Uranus );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Uranus->name, kstarsData->objList->last()));
    }

	  textCurrentStatus->setText( i18n("Creating Neptune" ) ); // next load, show text now
		break;
	case 34:			
		kstarsData->Neptune = new KSPlanet( I18N_NOOP( "Neptune" ) );
		ImageName = "neptune.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Neptune->image.load( imFile.name() );
		}
		
		if ( kstarsData->Neptune->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Neptune );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Neptune->name, kstarsData->objList->last()));
		}
	  textCurrentStatus->setText( i18n("Creating Pluto" ) ); // next load, show text now
		break;
	case 36:			
		kstarsData->Pluto = new KSPluto();
		ImageName = "pluto.png";
		if ( KStarsData::openDataFile( imFile, ImageName ) ) {
			imFile.close();
			kstarsData->Pluto->image.load( imFile.name() );
		}
		
		if ( kstarsData->Pluto->findPosition( kstarsData->CurrentDate, kstarsData->Earth ) ) {
			kstarsData->objList->append( kstarsData->Pluto );
			kstarsData->ObjNames->append( new SkyObjectName (kstarsData->Pluto->name, kstarsData->objList->last()));
		}
	  textCurrentStatus->setText( i18n("Loading Image URLs" ) ); // next load, show text now
		break;

	case 38:
		if ( !kstarsData->readURLData( "image_url.dat", 0 ) ) {
			qtimer->stop();
			KludgeError( "image_url.dat", false );
			qtimer->start(0);
		}
		if ( !kstarsData->readURLData( "myimage_url.dat", 0 ) ) {
		//Don't do anything if the local file is not found.
		}
	  textCurrentStatus->setText( i18n("Loading Information URLs" ) ); // next load, show text now
		break;
	case 40:
		if ( !kstarsData->readURLData( "info_url.dat", 1 ) ) {
			qtimer->stop();
			KludgeError( "info_url.dat", false );
			qtimer->start(0);
		}
		if ( !kstarsData->readURLData( "myinfo_url.dat", 1 ) ) {
		//Don't do anything if the local file is not found.
		}

		// that's all. Now quit the splash dialog and open the main window (see main.cpp)
		qtimer->stop();

		//need accept() instead of close() because close returns QDialog::Rejected
		accept();		
		break;	
	}

	loadStatus++;
	repaint();
}

void KStarsSplash::KludgeError( QString s, bool required ) {
	QString message, caption;
	if (required) {
		message = i18n( "The file " ) + s + i18n( " could not be found." )+ "\n" +
					i18n( "Shutting down, because KStars cannot run properly without this file.") + "\n" +
					i18n( "Place it in one of the following locations, then try again:" ) + "\n\n" +
					(QString)"\t$(KDEDIR)/share/apps/kstars/" + s +
					(QString)"\n\t~/.kde/share/apps/kstars/" + s +
					(QString)"\n\t$(PATH_TO_KSTARS)/data/" + s;
		QString caption = i18n( "Critical file not found: " ) + s;
  } else {
			message = i18n( "The file " ) + s + i18n( " could not be found." )+ "\n" +
				i18n( "KStars can still run without this file, so I will continue.") + "\n" +
				i18n( "However, to avoid seeing this message in the future, you may want to " ) + "\n" +
				i18n( "place the file in one of the following locations:" ) + "\n\n" +
				(QString)"\t$(KDEDIR)/share/apps/kstars/" + s +
				(QString)"\n\t~/.kde/share/apps/kstars/" + s +
				(QString)"\n\t$(PATH_TO_KSTARS)/data/" + s;
			caption = i18n( "Non-critical file not found: " ) + s;
	}
	
	KMessageBox::information( 0, message, caption );
}
#include "kstarssplash.moc"
