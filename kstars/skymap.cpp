/***************************************************************************
                          skymap.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 10 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <kapp.h>
#include <kconfig.h>
#include <klocale.h>
#include <kurl.h>
#include <kiconloader.h>
#include <kstatusbar.h>
#include <kmessagebox.h>

#include <qpopupmenu.h>
#include <qlabel.h>
#include <qcursor.h>
#include <qpointarray.h>
#include <qarray.h>
#include <qfont.h>
#include <qtextstream.h>
#include <qbitmap.h>
#include <qbitmap.h>
#include <qcursor.h>

#include <math.h>
#include <stdlib.h>
#include <stream.h>
#include <unistd.h>

#include "kstars.h"
#include "skymap.h"
#include "imageviewer.h"
#include "addlinkdialog.h"

SkyMap::SkyMap(QWidget *parent, const char *name )
 : QWidget (parent,name), downloads (0), computeSkymap (true)
{
	ksw = (KStars*) kapp->mainWidget();

	pts = new QPointArray( 2000 );  // points for milkyway and horizon
	sp = new SkyPoint();            // needed by coordinate grid

	setDefaultMouseCursor();	// set the cross cursor
	kapp->config()->setGroup( "View" );
	ZoomLevel = kapp->config()->readNumEntry( "ZoomLevel", 3 );
	if (ZoomLevel == 12) ksw->actZoomIn->setEnabled (false);
	if (ZoomLevel == 0) ksw->actZoomOut->setEnabled (false);

	// load the pixmaps of stars
	starpix = new StarPixmap (ksw->GetOptions()->starColorMode, ksw->GetOptions()->starColorIntensity);

  //initialize pixelScale array, will be indexed by ZoomLevel
	for ( int i=0; i<13; ++i ) {
		pixelScale[i] = int( 256.*pow(sqrt(2.0),i) );
		Range[i] = 75.0*256.0/pixelScale[i];
	}

	setBackgroundColor( QColor( ksw->GetOptions()->colorSky ) );
	setBackgroundMode( QWidget::NoBackground );
	setFocusPolicy( QWidget::StrongFocus );
	setMouseTracking (true); //Generate MouseMove events!
	mouseButtonDown = false;
	slewing = false;

	sky = new QPixmap();
	pmenu = new QPopupMenu();

}

SkyMap::~SkyMap() {
	delete starpix;
	delete pts;
	delete sp;
	delete sky;
	delete pmenu;
}

void SkyMap::initPopupMenu( void ) {
	pmenu->clear();
	pmTitle = new QLabel( i18n( "nothing" ), pmenu );
	pmTitle->setAlignment( AlignCenter );
	QPalette pal( pmTitle->palette() );
	pal.setColor( QPalette::Normal, QColorGroup::Background, QColor( "White" ) );
	pal.setColor( QPalette::Normal, QColorGroup::Foreground, QColor( "Black" ) );
	pal.setColor( QPalette::Inactive, QColorGroup::Foreground, QColor( "Black" ) );
	pmTitle->setPalette( pal );
	pmTitle2 = new QLabel( QString::null, pmenu );
	pmTitle2->setAlignment( AlignCenter );
	pmTitle2->setPalette( pal );
	pmType = new QLabel( i18n( "no type" ), pmenu );
	pmType->setAlignment( AlignCenter );
	pmType->setPalette( pal );
	pmenu->insertItem( pmTitle );
	pmenu->insertItem( pmTitle2 );
	pmenu->insertItem( pmType );
	pmRiseTime = new QLabel( i18n( "Rise Time: 00:00" ), pmenu );
	pmRiseTime->setAlignment( AlignCenter );
	pmRiseTime->setPalette( pal );
	QFont rsFont = pmRiseTime->font();
	rsFont.setPointSize( rsFont.pointSize() - 2 );
	pmRiseTime->setFont( rsFont );
	pmSetTime = new QLabel( i18n( "Set Time: 00:00" ), pmenu );
	pmSetTime->setAlignment( AlignCenter );
	pmSetTime->setPalette( pal );
	pmSetTime->setFont( rsFont );
	pmenu->insertSeparator();
	pmenu->insertItem( pmRiseTime );
	pmenu->insertItem( pmSetTime );
	pmenu->insertSeparator();
	pmenu->insertItem( i18n( "Center and track" ), this, SLOT( slotCenter() ) );
	pmenu->insertSeparator();
}

void SkyMap::setGeometry( int x, int y, int w, int h ) {
	QWidget::setGeometry( x, y, w, h );
	sky->resize( w, h );
}

void SkyMap::setGeometry( const QRect &r ) {
	QWidget::setGeometry( r );
	sky->resize( r.width(), r.height() );
}

void SkyMap::slotCenter( void ) {
//If the requested object is below the opaque horizon, issue a warning message
	clickedPoint.RADecToAltAz( LSTh, ksw->geo->lat() );
	if ( ksw->GetOptions()->drawGround && clickedPoint.getAlt().getD() < 0.0 ) {
		QString caption = i18n( "KStars pointing below horizon!" );
		QString message = i18n( "Warning: the requested position is below the Horizon.\nWould you like me to point there anyway?" );
		if ( KMessageBox::warningYesNo( 0, message, caption )==KMessageBox::No ) {
			clickedObject = NULL;
			foundObject = NULL;
			return;
		}
	}

	//update the focus to the selected coordinates
	focus.set( clickedPoint.getRA(), clickedPoint.getDec() );
	focus.RADecToAltAz( LSTh, ksw->geo->lat() );
	oldfocus.set( clickedPoint.getRA(), clickedPoint.getDec() );
	oldfocus.setAz( focus.getAz() );
	oldfocus.setAlt( focus.getAlt() );

	double dHA = LSTh.getH() - focus.getRA().getH();
	while ( dHA < 0.0 ) dHA += 24.0;
	HourAngle.setH( dHA );

	//display coordinates in statusBar
	QString s;
	char dsgn = '+';

	if ( clickedPoint.getDec().getD() < 0 ) dsgn = '-';
	int dd = abs( clickedPoint.getDec().getDeg() );
	int dm = abs( clickedPoint.getDec().getArcMin() );
	int ds = abs( clickedPoint.getDec().getArcSec() );

	ksw->CurrentPosition = s.sprintf( " %02d:%02d:%02d, %c%02d:%02d:%02d ",
				clickedPoint.getRA().getHour(), clickedPoint.getRA().getHMin(), clickedPoint.getRA().getHSec(),
				dsgn, dd, dm, ds );
	ksw->statusBar()->changeItem( ksw->CurrentPosition, 1 );

	showFocusCoords(); //updateinfoPanel

  foundObject = clickedObject;
	if (foundObject != NULL ) { //set tracking to true
		ksw->GetOptions()->isTracking = true;
		ksw->actTrack->setIconSet( BarIcon( "lock" ) );
	} else {
		ksw->GetOptions()->isTracking = false;
		ksw->actTrack->setIconSet( BarIcon( "unlock" ) );
	}

	Update();	// must be new computed
}

void SkyMap::slotDSS( void ) {
	QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
	QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
	QString RAString, DecString;
	char decsgn;
	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", clickedPoint.getRA().getHour(),
																								 clickedPoint.getRA().getHMin(),
																								 clickedPoint.getRA().getHSec() );
	decsgn = '+';
	if (clickedPoint.getDec().getD() < 0.0) decsgn = '-';
	int dd = abs( clickedPoint.getDec().getDeg() );
	int dm = abs( clickedPoint.getDec().getArcMin() );
	int ds = abs( clickedPoint.getDec().getArcSec() );

	DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

	//concat all the segments into the kview command line:
	KURL url (URLprefix + RAString + DecString + URLsuffix);
	new ImageViewer (&url, this);
}

void SkyMap::slotDSS2( void ) {
	QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=2r" );
	QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
	QString RAString, DecString;
	char decsgn;
	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", clickedPoint.getRA().getHour(),
																								 clickedPoint.getRA().getHMin(),
																								 clickedPoint.getRA().getHSec() );
	decsgn = '+';
	if (clickedPoint.getDec().getD() < 0.0) decsgn = '-';
	int dd = abs( clickedPoint.getDec().getDeg() );
	int dm = abs( clickedPoint.getDec().getArcMin() );
	int ds = abs( clickedPoint.getDec().getArcSec() );

	DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

	//concat all the segments into the kview command line:
	KURL url (URLprefix + RAString + DecString + URLsuffix);
	new ImageViewer (&url, this);
}

void SkyMap::slotInfo( int id ) {
	QStringList::Iterator it = clickedObject->InfoList.at(id-200);
	QString sURL = (*it);
	KURL url ( sURL );
	if (!url.isEmpty())
		kapp->invokeBrowser(sURL);
}

void SkyMap::slotImage( int id ) {
	QStringList::Iterator it = clickedObject->ImageList.at(id-100);
  QString sURL = (*it);
	KURL url ( sURL );
	if (!url.isEmpty())
		new ImageViewer (&url, this);
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
	slewing = false;
//	update();
	Update();	// Need a full update to draw faint objects that are not drawn while slewing.
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
	QString s;
	float step = 2.0;
	if ( e->state() & ShiftButton ) step = 4.0;

	switch ( e->key() ) {
		case Key_Left :
			if ( ksw->GetOptions()->useAltAz ) {
				focus.setAz( focus.getAz().getD() - step * pixelScale[0]/pixelScale[ZoomLevel] );
				focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			} else {
				focus.setRA( focus.getRA().getH() + 0.05*step * pixelScale[0]/pixelScale[ZoomLevel] );
				focus.RADecToAltAz( LSTh, ksw->geo->lat() );
			}

			slewing = true;
			break;

		case Key_Right :
			if ( ksw->GetOptions()->useAltAz ) {
				focus.setAz( focus.getAz().getD() + step * pixelScale[0]/pixelScale[ZoomLevel] );
				focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			} else {
				focus.setRA( focus.getRA().getH() - 0.05*step * pixelScale[0]/pixelScale[ZoomLevel] );
				focus.RADecToAltAz( LSTh, ksw->geo->lat() );
			}


			slewing = true;
			break;

		case Key_Up :
			if ( ksw->GetOptions()->useAltAz ) {
				focus.setAlt( focus.getAlt().getD() + step * pixelScale[0]/pixelScale[ZoomLevel] );
				if ( focus.getAlt().getD() > 90.0 ) focus.setAlt( 89.99 );
				focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			} else {
				focus.setDec( focus.getDec().getD() + step * pixelScale[0]/pixelScale[ZoomLevel] );
				if (focus.getDec().getD() > 90.0) focus.setDec( 90.0 );
				focus.RADecToAltAz( LSTh, ksw->geo->lat() );
			}

			slewing = true;
			break;

		case Key_Down:
			if ( ksw->GetOptions()->useAltAz ) {
				focus.setAlt( focus.getAlt().getD() - step * pixelScale[0]/pixelScale[ZoomLevel] );
				if ( focus.getAlt().getD() < -90.0 ) focus.setAlt( -90.0 );
				focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			} else {
				focus.setDec( focus.getDec().getD() - step * pixelScale[0]/pixelScale[ZoomLevel] );
				if (focus.getDec().getD() < -90.0) focus.setDec( -90.0 );
				focus.RADecToAltAz( LSTh, ksw->geo->lat() );
			}

			slewing = true;
			break;

		case Key_Plus:   //Zoom in
		case Key_Equal:
			ksw->mZoomIn();
			break;

		case Key_Minus:  //Zoom out
		case Key_Underscore:
			ksw->mZoomOut();
			break;

		case Key_N: //center on north horizon
			focus.setAlt( 15.0 ); focus.setAz( 0.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			break;

		case Key_E: //center on east horizon
			focus.setAlt( 15.0 ); focus.setAz( 90.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			break;

		case Key_S: //center on south horizon
			focus.setAlt( 15.0 ); focus.setAz( 180.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			break;

		case Key_W: //center on west horizon
			focus.setAlt( 15.0 ); focus.setAz( 270.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			break;

		case Key_Z: //center on Zenith
			focus.setAlt( 90.0 ); focus.setAz( 180.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
			break;

		case Key_0: //center on Sun
			clickedObject = ksw->GetData()->Sun;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_1: //center on Mercury
			clickedObject = ksw->GetData()->Mercury;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_2: //center on Venus
			clickedObject = ksw->GetData()->Venus;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_3: //center on Moon
			clickedObject = ksw->GetData()->Moon;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_4: //center on Mars
			clickedObject = ksw->GetData()->Mars;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_5: //center on Jupiter
			clickedObject = ksw->GetData()->Jupiter;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_6: //center on Saturn
			clickedObject = ksw->GetData()->Saturn;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_7: //center on Uranus
			clickedObject = ksw->GetData()->Uranus;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_8: //center on Neptune
			clickedObject = ksw->GetData()->Neptune;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

		case Key_9: //center on Pluto
			clickedObject = ksw->GetData()->Pluto;
      clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );
			slotCenter();
			break;

	}

	oldfocus.set( focus.getRA(), focus.getDec() );
	oldfocus.setAz( focus.getAz() );
	oldfocus.setAlt( focus.getAlt() );

	double dHA = LSTh.getH() - focus.getRA().getH();
  while ( dHA < 0.0 ) dHA += 24.0;
	HourAngle.setH( dHA );

	if ( slewing ) {
		if ( ksw->GetOptions()->isTracking ) {
			clickedObject = NULL;
			foundObject = NULL;//no longer tracking foundObject
			ksw->GetOptions()->isTracking = false;
  	  ksw->actTrack->setIconSet( BarIcon( "unlock" ) );
		}
		showFocusCoords();
	}

	Update(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
	double dx = ( 0.5*width()  - e->x() )/pixelScale[ZoomLevel];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ZoomLevel];

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	//determine RA, Dec of mouse pointer
	MousePoint = dXdYToRaDec( dx, dy, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );

	if ( mouseButtonDown ) {
		// set the mouseMoveCursor and set slewing=true, if they are not set yet
		if (!mouseMoveCursor) setMouseMoveCursor();
		if (!slewing) {
			slewing = true;
			ksw->GetOptions()->isTracking = false; //break tracking on slew
			ksw->actTrack->setIconSet( BarIcon( "unlock" ) );
			clickedObject = NULL;
			foundObject = NULL;//no longer tracking foundObject
		}

		//Update focus such that the sky coords at mouse cursor remain approximately constant
		if ( ksw->GetOptions()->useAltAz ) {
			MousePoint.RADecToAltAz( LSTh, ksw->geo->lat() );
			clickedPoint.RADecToAltAz( LSTh, ksw->geo->lat() );
			dms dAz = MousePoint.getAz().getD() - clickedPoint.getAz().getD();
			dms dAlt = MousePoint.getAlt().getD() - clickedPoint.getAlt().getD();
			focus.setAz( focus.getAz().getD() - dAz.getD() ); //move focus in opposite direction
			focus.setAlt( focus.getAlt().getD() - dAlt.getD() );

			if ( focus.getAlt().getD() >90.0 ) focus.setAlt( 90.0 );
			if ( focus.getAlt().getD() <-90.0 ) focus.setAlt( -90.0 );
			focus.AltAzToRADec( LSTh, ksw->geo->lat() );
		} else {
			dms dRA = MousePoint.getRA().getD() - clickedPoint.getRA().getD();
			dms dDec = MousePoint.getDec().getD() - clickedPoint.getDec().getD();
			focus.setRA( focus.getRA().getH() - dRA.getH() ); //move focus in opposite direction
			focus.setDec( focus.getDec().getD() - dDec.getD() );

			if ( focus.getDec().getD() >90.0 ) focus.setDec( 90.0 );
			if ( focus.getDec().getD() <-90.0 ) focus.setDec( -90.0 );
			focus.RADecToAltAz( LSTh, ksw->geo->lat() );
		}

		oldfocus.set( focus.getRA(), focus.getDec() );

		double dHA = LSTh.getH() - focus.getRA().getH();
		while ( dHA < 0.0 ) dHA += 24.0;
		HourAngle.setH( dHA );

		//redetermine RA, Dec of mouse pointer, using new focus
		MousePoint = dXdYToRaDec( dx, dy, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
    clickedPoint.set( MousePoint.getRA(), MousePoint.getDec() );

		showFocusCoords(); //update infoPanel
		Update();  // must be new computed
	} else {

		QString dummy;
		int dd,dm,ds;
		char dsgn = '+';
		if ( MousePoint.getDec().getD() < 0.0 ) dsgn = '-';
		dd = abs( MousePoint.getDec().getDeg() );
		dm = abs( MousePoint.getDec().getArcMin() );
		ds = abs( MousePoint.getDec().getArcSec() );

		ksw->CurrentPosition = dummy.sprintf( " %02d:%02d:%02d, %c%02d:%02d:%02d ",
					MousePoint.getRA().getHour(), MousePoint.getRA().getHMin(),  MousePoint.getRA().getHSec(),
					dsgn, dd, dm, ds );
		ksw->statusBar()->changeItem( ksw->CurrentPosition, 1 );
	}
}

void SkyMap::mouseReleaseEvent( QMouseEvent *e ) {
	if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
	mouseButtonDown = false;
	slewing = false;
	Update();	// is needed because after moving the sky not all stars are shown
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
// if button is down and cursor is not moved set the move cursor after 500 ms
	QTimer t;
	t.singleShot (500, this, SLOT (setMouseMoveCursor()));

	if ( mouseButtonDown == false ) {
		if ( e->button()==LeftButton ) {
			mouseButtonDown = true;
		}

		double dx = ( 0.5*width()  - e->x() )/pixelScale[ZoomLevel];
		double dy = ( 0.5*height() - e->y() )/pixelScale[ZoomLevel];

		if (unusablePoint (dx, dy)) return;	// break if point is unusable

		//determine RA, Dec of mouse pointer
		MousePoint = dXdYToRaDec( dx, dy, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		clickedPoint.set( MousePoint.getRA(), MousePoint.getDec() );

		double rmin;
		double rstar_min = 10000000.0;
		int istar_min = -1;

		//Search stars database for nearby object.
		for ( unsigned int i=0; i<ksw->GetData()->starList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->GetData()->starList.at(i);
			double dRA = test->getRA().getH() - clickedPoint.getRA().getH();
			double dDec = test->getDec().getD() - clickedPoint.getDec().getD();
			//determine angular distance between this object and mouse cursor
			double f = 15.0*cos( test->getDec().radians() );
			double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if (r < rstar_min) {
				istar_min = i;
				rstar_min = r;
			}
		}

		rmin = rstar_min;
		int icat = 0;

		double rsolar_min = 10000000.0;
		int isolar_min = -1;

		//Sun
		double dRA = ksw->GetData()->Sun->getRA().getH() - clickedPoint.getRA().getH();
		double dDec = ksw->GetData()->Sun->getDec().getD() - clickedPoint.getDec().getD();
		double f = 15.0*cos( ksw->GetData()->Sun->getDec().radians() );
		double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 0;
			rsolar_min = r;
		}

		//Moon
		dRA = ksw->GetData()->Moon->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Moon->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Moon->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 1;
			rsolar_min = r;
		}

		//Mercury
		dRA = ksw->GetData()->Mercury->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Mercury->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Mercury->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 2;
			rsolar_min = r;
		}

		//Venus
		dRA = ksw->GetData()->Venus->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Venus->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Venus->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 3;
			rsolar_min = r;
		}

		//Mars
		dRA = ksw->GetData()->Mars->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Mars->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Mars->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 4;
			rsolar_min = r;
		}

		//Jupiter
		dRA = ksw->GetData()->Jupiter->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Jupiter->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Jupiter->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 5;
			rsolar_min = r;
		}

		//Saturn
		dRA = ksw->GetData()->Saturn->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Saturn->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Saturn->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 6;
			rsolar_min = r;
		}

		//Uranus
		dRA = ksw->GetData()->Uranus->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Uranus->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Uranus->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 7;
			rsolar_min = r;
		}

		//Neptune
		dRA = ksw->GetData()->Neptune->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Neptune->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Neptune->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 8;
			rsolar_min = r;
		}

		//Pluto
		dRA = ksw->GetData()->Pluto->getRA().getH() - clickedPoint.getRA().getH();
		dDec = ksw->GetData()->Pluto->getDec().getD() - clickedPoint.getDec().getD();
		f = 15.0*cos( ksw->GetData()->Pluto->getDec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 9;
			rsolar_min = r;
		}

		if (rsolar_min < rmin) {
			rmin = rsolar_min;
			icat = -1;
		}

		double rmess_min = 10000000.0;
		int imess_min = -1;

		//Search Messier database for nearby object.
		for ( unsigned int i=0; i<ksw->GetData()->messList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->GetData()->messList.at(i);
			double dRA = test->getRA().getH()-clickedPoint.getRA().getH();
			double dDec = test->getDec().getD()-clickedPoint.getDec().getD();
				double f = 15.0*cos( test->getDec().radians() );
			  double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if (r < rmess_min) {
					imess_min = i;
					rmess_min = r;
				}
		}

		if (rmess_min < rmin) {
			rmin = rmess_min;
			icat = 1;
		}

		double rngc_min = 10000000.0;
		int ingc_min = -1;

		//Search NGC database for nearby object.
		for ( unsigned int i=0; i<ksw->GetData()->ngcList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->GetData()->ngcList.at(i);
			double dRA = test->getRA().getH()-clickedPoint.getRA().getH();
			double dDec = test->getDec().getD()-clickedPoint.getDec().getD();
				double f = 15.0*cos( test->getDec().radians() );
			  double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if (r < rngc_min) {
					ingc_min = i;
					rngc_min = r;
				}
		}

		if (rngc_min < rmin) {
			rmin = rngc_min;
			icat = 2;
		}

		double ric_min = 10000000.0;
		int iic_min = -1;

		//Search NGC database for nearby object.
		for ( unsigned int i=0; i<ksw->GetData()->icList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->GetData()->icList.at(i);
			double dRA = test->getRA().getH()-clickedPoint.getRA().getH();
			double dDec = test->getDec().getD()-clickedPoint.getDec().getD();
				double f = 15.0*cos( test->getDec().radians() );
			  double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if (r < ric_min) {
					iic_min = i;
					ric_min = r;
				}
		}

		if (ric_min < rmin) {
			rmin = ric_min;
			icat = 3;
		}

		initPopupMenu();
		QStringList::Iterator itList;
		QStringList::Iterator itTitle;
    QString s;
		int id;
		clickedObject = NULL;
		StarObject *starobj = NULL;

		if ( rmin < 200.0/pixelScale[ZoomLevel] ) {
			switch (icat) {
				case -1: //solar system object
					switch( isolar_min ) {
						case 0: //sun
							clickedObject = (SkyObject *)ksw->GetData()->Sun;
							break;
						case 1: //moon
							clickedObject = (SkyObject *)ksw->GetData()->Moon;
							break;
						case 2: //mercury
							clickedObject = (SkyObject *)ksw->GetData()->Mercury;
							break;
						case 3: //venus
							clickedObject = (SkyObject *)ksw->GetData()->Venus;
							break;
						case 4: //mars
							clickedObject = (SkyObject *)ksw->GetData()->Mars;
							break;
						case 5: //jupiter
							clickedObject = (SkyObject *)ksw->GetData()->Jupiter;
							break;
						case 6: //saturn
							clickedObject = (SkyObject *)ksw->GetData()->Saturn;
							break;
						case 7: //uranus
							clickedObject = (SkyObject *)ksw->GetData()->Uranus;
							break;
						case 8: //neptune
							clickedObject = (SkyObject *)ksw->GetData()->Neptune;
							break;
						case 9: //pluto
							clickedObject = (SkyObject *)ksw->GetData()->Pluto;
							break;
					}

					if ( clickedObject != NULL ) clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject->translatedName() );
							pmTitle2->setText( QString::null );
							pmType->setText( i18n( "Solar System" ) );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject->ImageList.begin();
              itTitle = clickedObject->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject->ImageList.end(); ++itList ) {
								sURL = QString(*itList);
								QString t = QString(*itTitle);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
							}

							if ( clickedObject->ImageList.count() ) pmenu->insertSeparator();

              itList  = clickedObject->InfoList.begin();
              itTitle = clickedObject->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

							pmenu->popup( QCursor::pos() );

							break;
					}
					break;
				case 0: //star
					starobj = (StarObject *)ksw->GetData()->starList.at(istar_min);
					clickedObject = (SkyObject *)ksw->GetData()->starList.at(istar_min);
					clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject->translatedName() );
							pmTitle2->setText( i18n( "Spectral Type: %1" ).arg(starobj->sptype()) );
							if ( clickedObject->name != "star" ) {
								pmType->setText( i18n( "star" ) );
							} else {
								pmType->setText( QString::null );
							}

							setRiseSetLabels();

							pmenu->insertItem( i18n( "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );

							//can't add links to anonymous stars, because links indexed by object name :(
							if (clickedObject->name != "star" ) {
								pmenu->insertSeparator();
								pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );
    					}

							pmenu->popup( QCursor::pos() );
							break;
						default:
							break;
					}
					break;
				case 1: //Messier Object
					clickedObject = (SkyObject *)ksw->GetData()->messList.at(imess_min);
					clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject->translatedName() );
							if ( !clickedObject->longname.isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject->longname.latin1() ) );
							} else if ( !clickedObject->name2.isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject->name2.latin1() ) );
							}
							pmType->setText( ksw->TypeName[clickedObject->type] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject->ImageList.begin();
              itTitle = clickedObject->ImageTitle.begin();

							id=100;
							for ( ; itList != clickedObject->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject->ImageList.count() ) pmenu->insertSeparator();

							itList  = clickedObject->InfoList.begin();
							itTitle = clickedObject->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

							pmenu->popup( QCursor::pos() );
							break;
						default:
							break;
						}
					break;
				case 2: //NGC Object
					clickedObject = (SkyObject *)ksw->GetData()->ngcList.at(ingc_min);
					clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject->translatedName() );
							if ( !clickedObject->longname.isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject->longname.latin1() ) );
							} else if ( !clickedObject->name2.isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject->name2.latin1() ) );
							}
							pmType->setText( ksw->TypeName[clickedObject->type] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject->ImageList.begin();
              itTitle = clickedObject->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject->ImageList.count() ) pmenu->insertSeparator();

							itList  = clickedObject->InfoList.begin();
							itTitle = clickedObject->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

							pmenu->popup( QCursor::pos() );
							break;
						default:
							break;
					}
					break;
				case 3: //IC Object
					clickedObject = (SkyObject *)ksw->GetData()->icList.at(iic_min);
					clickedPoint.set( clickedObject->getRA(), clickedObject->getDec() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject->translatedName() );
							if ( !clickedObject->longname.isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject->longname.latin1() ) );
							} else {
								pmTitle2->setText( i18n( clickedObject->name2.latin1() ) );
							}
							pmType->setText( ksw->TypeName[clickedObject->type] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject->ImageList.begin();
              itTitle = clickedObject->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject->ImageList.count() ) pmenu->insertSeparator();

              itList  = clickedObject->InfoList.begin();
              itTitle = clickedObject->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( showImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

							pmenu->popup( QCursor::pos() );
							break;
						default:
							break;
					}
					break;
			}
		} else {
			//Empty sky selected.  If left-click, display "nothing" in the status bar.
	    //If right-click, open pmStar with "nothing" as star name.
			clickedObject = NULL;

			switch (e->button()) {
				case LeftButton:
					ksw->statusBar()->changeItem( i18n( "nothing" ), 0 );
					break;
				case RightButton:
					pmTitle->setText( i18n( "nothing" ) );
					pmTitle2->setText( QString::null );
					pmType->setText( QString::null );
					pmenu->insertItem( i18n( "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
					pmenu->insertItem( i18n( "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );

					pmenu->popup( QCursor::pos() );
					break;
				default:
					break;
			}
		}
	}
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
	double dx = ( 0.5*width()  - e->x() )/pixelScale[ZoomLevel];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ZoomLevel];

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	if ( dx != 0.0 || dy != 0.0 ) {
//		//recenter display at new RA, Dec
//		MousePoint = dXdYToRaDec( dx, dy, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		slotCenter();
	}
}

void SkyMap::paintEvent( QPaintEvent *e ) {
// if the skymap should be only repainted and constellations need not to be new computed; call this with update() (default)
	if (!computeSkymap)
	{
		bitBlt( this, 0, 0, sky );
		return ; // exit because the pixmap is repainted and that's all what we want
	}

// if the sky should be recomputed (this is not every paintEvent call needed, explicite call with Update())
	QPainter psky;
	QImage ScaledImage;

	float FOV = Range[ZoomLevel]*width()/600.;
	float dY;
  bool isPoleVisible = false;
	if ( ksw->GetOptions()->useAltAz ) {
		dY = fabs( focus.getAlt().getD() ) + FOV;
	} else {
		dY = fabs( focus.getDec().getD() ) + FOV;
	}
	if ( dY >= 90. ) isPoleVisible = true;

	psky.begin( sky );
	psky.fillRect( 0, 0, width(), height(), QBrush( ksw->GetOptions()->colorSky ) );

	QFont stdFont = psky.font();
	QFont smallFont = psky.font();
	smallFont.setPointSize( stdFont.pointSize() - 2 );

//	QPointArray pts;
	QList<QPoint> points;
	int ptsCount;

	//Draw Milky Way (draw this first so it's in the "background")
	if ( ksw->GetOptions()->drawMilkyWay ) {
		psky.setPen( QPen( QColor( ksw->GetOptions()->colorMW ) ) );
		psky.setBrush( QBrush( QColor( ksw->GetOptions()->colorMW ) ) );
		bool offscreen, lastoffscreen=false;
		int max = pixelScale[ZoomLevel]/100;

		for ( unsigned int j=0; j<11; ++j ) {
			if ( ksw->GetOptions()->fillMilkyWay ) {
				ptsCount = 0;
				bool partVisible = false;

				QPoint o = getXY( ksw->GetData()->MilkyWay[j].at(0), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if ( o.x() != -100000 && o.y() != -100000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;

				for ( unsigned int i=1; i<ksw->GetData()->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->GetData()->MilkyWay[j].at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
					if ( o.x() != -100000 && o.y() != -100000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
					if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;
				}

				if ( ptsCount && partVisible ) {
					psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
	 	  	}
			} else {
	      QPoint o = getXY( ksw->GetData()->MilkyWay[j].at(0), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if (o.x()==-100000 && o.y()==-100000) offscreen = true;
				else offscreen = false;

				psky.moveTo( o.x(), o.y() );
  	
				for ( unsigned int i=1; i<ksw->GetData()->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->GetData()->MilkyWay[j].at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
					if (o.x()==-100000 && o.y()==-100000) offscreen = true;
					else offscreen = false;

					//don't draw a line if the last point's getXY was (-100000, -100000)
					int dx = abs(o.x()-psky.pos().x());
					int dy = abs(o.y()-psky.pos().y());
					if ( (!lastoffscreen && !offscreen) && (dx<max && dy<max) ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
					}
					lastoffscreen = offscreen;
				}
			}
		}
	}

	//Draw coordinate grid
	if ( ksw->GetOptions()->drawGrid ) {
		psky.setPen( QPen( QColor( ksw->GetOptions()->colorGrid ), 0, DotLine ) ); //change to colorGrid
			
		//First, the parallels
		for ( double Dec=-80.; Dec<=80.; Dec += 20. ) {
			bool newlyVisible = false;
//			SkyPoint *sp = new SkyPoint( 0.0, Dec );
			sp->set( 0.0, Dec );
			if ( ksw->GetOptions()->useAltAz ) sp->RADecToAltAz( LSTh, ksw->geo->lat() );
			QPoint o = getXY( sp, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
			QPoint o1 = o;
			psky.moveTo( o.x(), o.y() );

			double dRA = 1./15.; //180 points along full circle of RA
			for ( double RA=dRA; RA<24.; RA+=dRA ) {
				sp->set( RA, Dec );
				if ( ksw->GetOptions()->useAltAz ) sp->RADecToAltAz( LSTh, ksw->geo->lat() );
				o = getXY( sp, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
  	
				int dx = psky.pos().x() - o.x();
				if ( abs(dx) < 250 ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );	
				}
			}

			//connect the final segment
			int dx = psky.pos().x() - o1.x();
			if (abs(dx) < 250 ) {
				psky.lineTo( o1.x(), o1.y() );
			} else {
				psky.moveTo( o1.x(), o1.y() );	
			}
		}

    //next, the meridians
		for ( double RA=0.; RA<24.; RA += 2. ) {
			bool newlyVisible = false;
			SkyPoint *sp = new SkyPoint( RA, -90. );
			if ( ksw->GetOptions()->useAltAz ) sp->RADecToAltAz( LSTh, ksw->geo->lat() );
			QPoint o = getXY( sp, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
			psky.moveTo( o.x(), o.y() );

			double dDec = 1.;
			for ( double Dec=-89.; Dec<=90.; Dec+=dDec ) {
				sp->set( RA, Dec );
				if ( ksw->GetOptions()->useAltAz ) sp->RADecToAltAz( LSTh, ksw->geo->lat() );
				o = getXY( sp, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
  	
				int dx = psky.pos().x() - o.x();
				if ( abs(dx) < 250 ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( o.x(), o.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );	
				}
			}
		}
	}

	//Draw Equator
	if ( ksw->GetOptions()->drawEquator ) {
		psky.setPen( QPen( QColor( ksw->GetOptions()->colorEq ), 0, SolidLine ) );

		QPoint o = getXY( ksw->GetData()->Equator.at(0), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		QPoint o1 = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		for ( unsigned int i=1; i<ksw->GetData()->Equator.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->Equator.at(i), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				o = getXY( ksw->GetData()->Equator.at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );

				int dx = psky.pos().x() - o.x();

				if ( abs(dx) < 250 ) {
						if ( newlyVisible ) {
						newlyVisible = false;
						QPoint last = getXY( ksw->GetData()->Equator.at(i-1), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
		}
		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		if (abs(dx) < 250 ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }

	//Draw Ecliptic
	if ( ksw->GetOptions()->drawEcliptic ) {
		psky.setPen( QColor( ksw->GetOptions()->colorEcl ) );

		QPoint o = getXY( ksw->GetData()->Ecliptic.at(0), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		QPoint o1 = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		for ( unsigned int i=1; i<ksw->GetData()->Ecliptic.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->Ecliptic.at(i), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				o = getXY( ksw->GetData()->Ecliptic.at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				int dx = psky.pos().x() - o.x();

				if ( abs(dx) < 250 ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						QPoint last = getXY( ksw->GetData()->Ecliptic.at(i-1), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
		}
		//connect the final segment
		int dx = psky.pos().x() - o1.x();
		if (abs(dx) < 250 ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }

	//Draw Constellation Lines
	if ( ksw->GetOptions()->drawConstellLines ) {
		psky.setPen( QColor( ksw->GetOptions()->colorCLine ) );
		int iLast = 0;

		for ( unsigned int i=0; i < ksw->GetData()->clineList.count(); ++i ) {
			QPoint o = getXY( ksw->GetData()->clineList.at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );

			if ( ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) &&
					 ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) ) {
				if ( ksw->GetData()->clineModeList.at(i)->latin1()=='M' ) {
					psky.moveTo( o.x(), o.y() );
				} else if ( ksw->GetData()->clineModeList.at(i)->latin1()=='D' ) {
					if ( i==iLast+1 ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
  	      }
				}
				iLast = i;
			}
		}
  }

	//Draw Constellation Names
	//Don't draw names if slewing
	if ( !slewing && ksw->GetOptions()->drawConstellNames ) {
		psky.setFont( stdFont );
		psky.setPen( QColor( ksw->GetOptions()->colorCName ) );
		for ( unsigned int i=0; i < ksw->GetData()->cnameList.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->cnameList.at(i)->pos(), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->GetData()->cnameList.at(i)->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					if ( ksw->GetOptions()->useLatinConstellNames ) {
						int dx = 5*ksw->GetData()->cnameList.at(i)->name.length();
						psky.drawText( o.x()-dx, o.y(), ksw->GetData()->cnameList.at(i)->name );	// latin constellation names
					} else if ( ksw->GetOptions()->useLocalConstellNames ) {
						int dx = 5*ksw->GetData()->cnameList.at(i)->translatedName().length();
						psky.drawText( o.x()-dx, o.y(), ksw->GetData()->cnameList.at(i)->translatedName() ); // localized constellation names
					} else {
						int dx = 5*ksw->GetData()->cnameList.at(i)->name2.length();
						psky.drawText( o.x()-dx, o.y(), ksw->GetData()->cnameList.at(i)->name2 );
					}
				}
			}
		}
  }

	//Draw Stars
	if ( ksw->GetOptions()->drawBSC ) {
//		psky.setPen( QColor( ksw->GetOptions()->colorSky ) );
//		psky.setBrush( QColor( ksw->GetOptions()->colorStar ) );
		if ( ZoomLevel < 6 ) {
			psky.setFont( smallFont );
		} else {
			psky.setFont( stdFont );
		}

		float maglim;
		float maglim0 = ksw->GetOptions()->magLimitDrawStar;
		float zoomlim = 7.0 + float(ZoomLevel)/4.0;

		if ( maglim0 < zoomlim ) maglim = maglim0;
		else maglim = zoomlim;

		unsigned int numStars = ksw->GetData()->starList.count();
	  //Only draw bright stars if slewing
		if ( slewing && maglim > 6.0 ) maglim = 6.0;
	
	for ( StarObject *curStar = ksw->GetData()->starList.first(); curStar; curStar = ksw->GetData()->starList.next() ) {
		// break loop if maglim is reached
		if ( curStar->mag > maglim ) break;
		
		if ( checkVisibility( curStar->pos(), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
			QPoint o = getXY( curStar->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );

			// draw star if currently on screen
			if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
				// but only if the star is bright enough.
					float mag = curStar->mag;
					float sizeFactor = 2.0;
					int size = int( sizeFactor*(zoomlim - mag) ) + 1;
					if (size>23) size=23;

					if ( size > 0 ) {
						psky.setPen( QColor( ksw->GetOptions()->colorSky ) );
						drawSymbol( psky, curStar->type, o.x(), o.y(), size, curStar->color() );
						// now that we have drawn the star, we can display some extra info
						if ( !slewing && (curStar->mag <= ksw->GetOptions()->magLimitDrawStarInfo )
								&& (ksw->GetOptions()->drawStarName || ksw->GetOptions()->drawStarMagnitude ) ) {
							// collect info text
							QString sTmp = "";
							if ( ksw->GetOptions()->drawStarName ) {
								if (curStar->name != "star") sTmp = curStar->name + " ";	// see line below
//								if ( curStar->skyObjectName() ) sTmp = curStar->name + " ";
							}
							if ( ksw->GetOptions()->drawStarMagnitude ) {
								sTmp += QString().sprintf("%.1f", curStar->mag );
							}
							int offset = int( 7.5 - mag ) + 5;
							psky.setPen( QColor( ksw->GetOptions()->colorSName ) );
							psky.drawText( o.x()+offset, o.y()+offset, sTmp );
  	  	  	 			}
					}
				}
			}
		}
	}
	
  //Draw IC Objects
  if ( !slewing && ksw->GetOptions()->drawIC ) {
		psky.setBrush( NoBrush );
		psky.setPen( QColor( ksw->GetOptions()->colorIC ) );

		for ( unsigned int i=0; i < ksw->GetData()->icList.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->icList.at(i)->pos(), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->GetData()->icList.at(i)->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
					int size = pixelScale[ZoomLevel]/pixelScale[0];
					if (size>8) size = 8;

					//change color if extra images are available
					if ( ksw->GetData()->icList.at(i)->ImageList.count() )
						psky.setPen( QColor( ksw->GetOptions()->colorHST ) );
					drawSymbol( psky, ksw->GetData()->icList.at(i)->type, o.x(), o.y(), size );
					psky.setPen( QColor( ksw->GetOptions()->colorIC ) );

				}
			}
		}
	}

  //Draw NGC Objects
  if ( !slewing && ksw->GetOptions()->drawNGC ) {
		psky.setBrush( NoBrush );
		psky.setPen( QColor( ksw->GetOptions()->colorNGC ) );

		for ( unsigned int i=0; i < ksw->GetData()->ngcList.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->ngcList.at(i)->pos(), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->GetData()->ngcList.at(i)->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
					int size = pixelScale[ZoomLevel]/pixelScale[0];
					if (size>8) size = 8;

					//change color if extra images are available
					if ( ksw->GetData()->ngcList.at(i)->ImageList.count() )
						psky.setPen( QColor( ksw->GetOptions()->colorHST ) );
					drawSymbol( psky, ksw->GetData()->ngcList.at(i)->type, o.x(), o.y(), size );
					psky.setPen( QColor( ksw->GetOptions()->colorNGC ) );
				}
			}
		}
	}

	//Draw Messier Objects
	if ( ksw->GetOptions()->drawMessier || ksw->GetOptions()->drawMessImages ) {
		psky.setPen( QColor( ksw->GetOptions()->colorMess ) );
		psky.setBrush( NoBrush );

		for ( unsigned int i=0; i < ksw->GetData()->messList.count(); ++i ) {
			if ( checkVisibility( ksw->GetData()->messList.at(i)->pos(), FOV, ksw->GetOptions()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->GetData()->messList.at(i)->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					if ( ksw->GetOptions()->drawMessImages && ZoomLevel>5 ) {
						QFile file;
						QImage image;
						QString fname = "m" + ksw->GetData()->messList.at(i)->name.mid(2) + ".png";
						if ( KStarsData::openDataFile( file, fname ) ) {
							file.close();
	          	image.load( file.name(), "PNG" );
							int w = int( image.width()*pixelScale[ZoomLevel]/pixelScale[12] );
              int h = int( image.height()*pixelScale[ZoomLevel]/pixelScale[12] );
							int x1 = o.x() - int( 0.5*w );
							int y1 = o.y() - int( 0.5*h );
							ScaledImage = image.smoothScale( w, h );
							psky.drawImage( x1, y1, ScaledImage );
						}
					}

					if ( ksw->GetOptions()->drawMessier ) {
						int size = pixelScale[ZoomLevel]/pixelScale[0];
						if (size > 16) size = 16;

						//change color if extra images are available
						if ( ksw->GetData()->messList.at(i)->ImageList.count() > 1 )
							psky.setPen( QColor( ksw->GetOptions()->colorHST ) );
						drawSymbol( psky, ksw->GetData()->messList.at(i)->type, o.x(), o.y(), size );
            //reset color
						psky.setPen( QColor( ksw->GetOptions()->colorMess ) );
					}			
				}
			}
		}
  }

	//Draw Sun
  if ( ksw->GetOptions()->drawSun ) {
		psky.setPen( QColor( "Yellow" ) );
		psky.setBrush( QColor( "Yellow" ) );
		QPoint o = getXY( ksw->GetData()->Sun->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 8*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>3 && !ksw->GetData()->Sun->image.isNull() ) {
				ScaledImage = ksw->GetData()->Sun->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Moon
  if ( ksw->GetOptions()->drawMoon ) {
		psky.setPen( QColor( "White" ) );
		psky.setBrush( QColor( "White" ) );
		QPoint o = getXY( ksw->GetData()->Moon->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 8*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( !ksw->GetData()->Moon->image.isNull() ) {
				ScaledImage = ksw->GetData()->Moon->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Mercury
  if ( ksw->GetOptions()->drawMercury ) {
		psky.setPen( QColor( "SlateBlue1" ) );
		psky.setBrush( QColor( "SlateBlue1" ) );
		QPoint o = getXY( ksw->GetData()->Mercury->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Mercury->image.isNull() ) {
				ScaledImage = ksw->GetData()->Mercury->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Venus
  if ( ksw->GetOptions()->drawVenus ) {
		psky.setPen( QColor( "LightGreen" ) );
		psky.setBrush( QColor( "LightGreen" ) );
		QPoint o = getXY( ksw->GetData()->Venus->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Venus->image.isNull() ) {
				ScaledImage = ksw->GetData()->Venus->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Mars
  if ( ksw->GetOptions()->drawMars ) {
		psky.setPen( QColor( "Red" ) );
		psky.setBrush( QColor( "Red" ) );
		QPoint o = getXY( ksw->GetData()->Mars->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Mars->image.isNull() ) {
				ScaledImage = ksw->GetData()->Mars->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Jupiter
  if ( ksw->GetOptions()->drawJupiter ) {
		psky.setPen( QColor( "Goldenrod" ) );
		psky.setBrush( QColor( "Goldenrod" ) );
		QPoint o = getXY( ksw->GetData()->Jupiter->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Jupiter->image.isNull() ) {
				ScaledImage = ksw->GetData()->Jupiter->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Saturn
  if ( ksw->GetOptions()->drawSaturn ) {
		psky.setPen( QColor( "LightYellow2" ) );
		psky.setBrush( QColor( "LightYellow2" ) );
		QPoint o = getXY( ksw->GetData()->Saturn->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Saturn->image.isNull() ) {
				size *= 2;  //resize for image (because of rings)
				int x1 = o.x() - size/2;
				int y1 = o.y() - size/2;
				ScaledImage = ksw->GetData()->Saturn->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Uranus
  if ( ksw->GetOptions()->drawUranus ) {
		psky.setPen( QColor( "LightSeaGreen" ) );
		psky.setBrush( QColor( "LightSeaGreen" ) );
		QPoint o = getXY( ksw->GetData()->Uranus->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Uranus->image.isNull() ) {
				ScaledImage = ksw->GetData()->Uranus->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Neptune
  if ( ksw->GetOptions()->drawNeptune ) {
		psky.setPen( QColor( "SkyBlue" ) );
		psky.setBrush( QColor( "SkyBlue" ) );
		QPoint o = getXY( ksw->GetData()->Neptune->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Neptune->image.isNull() ) {
				ScaledImage = ksw->GetData()->Neptune->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Pluto
  if ( ksw->GetOptions()->drawNeptune ) {
		psky.setPen( QColor( "gray" ) );
		psky.setBrush( QColor( "gray" ) );
		QPoint o = getXY( ksw->GetData()->Pluto->pos(), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ZoomLevel]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ZoomLevel>2 && !ksw->GetData()->Pluto->image.isNull() ) {
				ScaledImage = ksw->GetData()->Pluto->image.smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Horizon
	if (ksw->GetOptions()->drawHorizon || ksw->GetOptions()->drawGround) {
		psky.setPen( QColor ( ksw->GetOptions()->colorHorz ) );
		psky.setBrush( QColor ( ksw->GetOptions()->colorHorz ) );
		ptsCount = 0;

		for ( unsigned int i=0; i<ksw->GetData()->Horizon.count(); ++i ) {
			QPoint *o = new QPoint();
			*o = getXY( ksw->GetData()->Horizon.at(i), ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
      bool found = false;

			//Use the QList of points to pre-sort visible horizon points
			if ( o->x() > -pixelScale[ZoomLevel]/4 && o->x() < width() + pixelScale[ZoomLevel]/4 ) {

				if ( ksw->GetOptions()->useAltAz ) {
	        unsigned int j;
					for ( j=0; j<points.count(); ++j ) {
						if ( ksw->GetOptions()->useAltAz && o->x() < points.at(j)->x() ) {
							found = true;
							break;
						}
					}
					if ( found ) {
						points.insert( j, o );
					} else {
						points.append( o );
					}
				} else {
					points.append( o );
				}
			}
		}

//	Fill the pts array with sorted horizon points, Draw Horizon Line
		pts->setPoint( 0, points.at(0)->x(), points.at(0)->y() );
		if ( ksw->GetOptions()->drawHorizon ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

		for ( unsigned int i=1; i<points.count(); ++i ) {
			pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );
			if ( ksw->GetOptions()->drawHorizon ) {
				if ( !ksw->GetOptions()->useAltAz && ( abs( points.at(i)->x() - psky.pos().x() ) > 250 ||
							abs( points.at(i)->y() - psky.pos().y() ) > 250 ) ) {
					psky.moveTo( points.at(i)->x(), points.at(i)->y() );
				} else {
					psky.lineTo( points.at(i)->x(), points.at(i)->y() );
				}
			}
		}
    //connect the last segment back to the beginning
		if ( abs( points.at(0)->x() - psky.pos().x() ) < 250 && abs( points.at(0)->y() - psky.pos().y() ) < 250 )
			psky.lineTo( points.at(0)->x(), points.at(0)->y() );
  }

//  Finish the polygon by adding a square bottom edge, offscreen
	if ( ksw->GetOptions()->useAltAz && ksw->GetOptions()->drawGround ) {
		ptsCount = points.count();
		pts->setPoint( ptsCount++, width()+10, height()+10 );
		pts->setPoint( ptsCount++, -10, height()+10 );

		psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );

//  remove all items in points list

		for ( unsigned int i=0; i<points.count(); ++i ) {
			points.remove(i);
		}

//	Draw compass heading labels along horizon
		SkyPoint *c = new SkyPoint;
		QPoint cpoint;
		psky.setPen( QColor ( ksw->GetOptions()->colorSky ) );
		psky.setFont( stdFont );

		//North
		c->setAz( 359.99 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "N" ) );
		}

		//NorthEast
		c->setAz( 45.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "NE" ) );
		}

		//East
		c->setAz( 90.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "E" ) );
		}

		//SouthEast
		c->setAz( 135.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "SE" ) );
		}

		//South
		c->setAz( 179.99 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "S" ) );
		}

		//SouthWest
		c->setAz( 225.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "SW" ) );
		}

		//West
		c->setAz( 270.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "W" ) );
		}

		//NorthWest
		c->setAz( 315.0 );
		c->setAlt( 0.0 );
		if ( !ksw->GetOptions()->useAltAz ) c->AltAzToRADec( LSTh, ksw->geo->lat() );
		cpoint = getXY( c, ksw->GetOptions()->useAltAz, LSTh, ksw->geo->lat() );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "NW" ) );
		}

		delete c;
	}

	psky.end();
	bitBlt( this, 0, 0, sky );
	computeSkymap = false;	// use Update() to compute new skymap else old pixmap will be shown
}

void SkyMap::drawSymbol( QPainter &psky, int type, int x, int y, int size, QChar color ) {
	int x1 = x - size/2;
	int x2 = x + size/2;
	int y1 = y - size/2;
	int y2 = y + size/2;

	int xa = x - size/4;
	int xb = x + size/4;
	int ya = y - size/4;
	int yb = y + size/4;

	QPixmap *star;

	switch (type) {
		case 0: //star
			star = starpix->getPixmap (&color, size);
			bitBlt ((QPaintDevice *) sky, x1-star->width()/2, y1-star->height()/2, star);
			break;
		case 1: //catalog star
			//Some NGC/IC objects are stars...changed their type to 1 (was double star)
			if (size<2) size = 2;
			psky.drawEllipse( x1, y1, size/2, size/2 );
			break;
		case 2: //Planet
			break;
		case 3: //Open cluster
			psky.drawEllipse( xa, y1, 2, 2 ); // draw circle of points
			psky.drawEllipse( xb, y1, 2, 2 );
			psky.drawEllipse( xa, y2, 2, 2 );
			psky.drawEllipse( xb, y2, 2, 2 );
			psky.drawEllipse( x1, ya, 2, 2 );
			psky.drawEllipse( x1, yb, 2, 2 );
			psky.drawEllipse( x2, ya, 2, 2 );
			psky.drawEllipse( x2, yb, 2, 2 );
			break;
		case 4: //Globular Cluster
			if (size<2) size = 2;
			psky.drawEllipse( x1, y1, size, size );
			psky.moveTo( x, y1 );
			psky.lineTo( x, y2 );
			psky.moveTo( x1, y );
			psky.lineTo( x2, y );
			break;
		case 5: //Gaseous Nebula
			if (size <2) size = 2;
			psky.drawLine( x1, y1, x2, y1 );
			psky.drawLine( x2, y1, x2, y2 );
			psky.drawLine( x2, y2, x1, y2 );
			psky.drawLine( x1, y2, x1, y1 );
			break;
		case 6: //Planetary Nebula
			if (size<2) size = 2;
			psky.drawEllipse( x1, y1, size, size );
			psky.moveTo( x, y1 );
			psky.lineTo( x, y1 - size/2 );
			psky.moveTo( x, y2 );
			psky.lineTo( x, y2 + size/2 );
			psky.moveTo( x1, y );
			psky.lineTo( x1 - size/2, y );
			psky.moveTo( x2, y );
			psky.lineTo( x2 + size/2, y );
			break;
		case 7: //Supernova remnant
			if (size<2) size = 2;
			psky.moveTo( x, y1 );
			psky.lineTo( x2, y );
			psky.lineTo( x, y2 );
			psky.lineTo( x1, y );
			psky.lineTo( x, y1 );
			break;
		case 8: //Galaxy
			if ( size>2 ) {
				psky.drawEllipse( x-size, y1, 2*size, size );
			} else {
				psky.drawPoint( x, y );
			}
			break;
	}
}
//---------------------------------------------------------------------------

QPoint SkyMap::getXY( SkyPoint *o, bool Horiz, dms LSTh, dms lat ) {
	QPoint p;
	dms X, Y, X0, dX;
	double sindX, cosdX, sinY, cosY, sinY0, cosY0;

	if ( Horiz ) {
//		focus.RADecToAltAz( LSTh, lat );
//		o->RADecToAltAz( LSTh, lat );
		X0 = focus.getAz();

		X = o->getAz();
		Y = o->getAlt();

		if ( X0.getD() > 270.0 && X.getD() < 90.0 ) {
			dX = 360.0 + X0.getD() - X.getD();
		} else {
			dX = X0.getD() - X.getD();
		}

		focus.getAlt().SinCos( sinY0, cosY0 );

  } else {
		if (focus.getRA().getH() > 18.0 && o->getRA().getH() < 6.0) {
			dX = o->getRA().getD() + 360.0 - focus.getRA().getD();
		} else {
			dX = o->getRA().getD() - focus.getRA().getD();
	  }
    Y = o->getDec().getD();
		focus.getDec().SinCos( sinY0, cosY0 );
  }

	//Convert dX, Y coords to screen pixel coords.
	dX.SinCos( sindX, cosdX );
	Y.SinCos( sinY, cosY );

	double c = sinY0*sinY + cosY0*cosY*cosdX;

	if ( c < 0.0 ) { //Object is on "back side" of the celestial sphere; don't plot it.
		p.setX( -100000 );
		p.setY( -100000 );
		return p;
	}

	double k = sqrt( 2.0/( 1 + c ) );

	p.setX( int( 0.5*width()  - pixelScale[ZoomLevel]*k*cosY*sindX ) );
	p.setY( int( 0.5*height() - pixelScale[ZoomLevel]*k*( cosY0*sinY - sinY0*cosY*cosdX ) ) );

	return p;
}
//---------------------------------------------------------------------------

SkyPoint SkyMap::dXdYToRaDec( double dx, double dy, bool useAltAz, dms LSTh, dms lat ) {
	//Determine RA and Dec of a point, given (dx, dy): it's pixel
	//coordinates in the SkyMap with the center of the map as the origin.

	SkyPoint MousePoint;
	double sinDec, cosDec, sinDec0, cosDec0, sinc, cosc, sinlat, coslat;
	double xx, yy;

	double r  = sqrt( dx*dx + dy*dy );
	dms centerAngle;
	centerAngle.setRadians( 2.0*asin(0.5*r) );

	focus.getDec().SinCos( sinDec0, cosDec0 );
	centerAngle.SinCos( sinc, cosc );

	if ( useAltAz ) {
		dms HA;
		dms Dec, alt, az, alt0, az0;
		double A;
		double sinAlt, cosAlt, sinAlt0, cosAlt0, sinAz, cosAz;
//		double HA0 = LSTh - focus.getRA();
		az0 = focus.getAz();
		alt0 = focus.getAlt();
		alt0.SinCos( sinAlt0, cosAlt0 );

		dx = -dx; //Flip East-west (Az goes in opposite direction of RA)
		yy = dx*sinc;
		xx = r*cosAlt0*cosc - dy*sinAlt0*sinc;

		A = atan( yy/xx );
		//resolve ambiguity of atan():
		if ( xx<0 ) A = A + PI();
//		if ( xx>0 && yy<0 ) A = A + 2.0*PI();

		dms deltaAz;
		deltaAz.setRadians( A );
		az = focus.getAz().getD() + deltaAz.getD();
		alt.setRadians( asin( cosc*sinAlt0 + ( dy*sinc*cosAlt0 )/r ) );

		az.SinCos( sinAz, cosAz );
		alt.SinCos( sinAlt, cosAlt );
		lat.SinCos( sinlat, coslat );

		Dec.setRadians( asin( sinAlt*sinlat + cosAlt*coslat*cosAz ) );
		Dec.SinCos( sinDec, cosDec );

		HA.setRadians( acos( ( sinAlt - sinlat*sinDec )/( coslat*cosDec ) ) );
		if ( sinAz > 0.0 ) HA.setH( 24.0 - HA.getH() );

		MousePoint.setRA( LSTh.getH() - HA.getH() );
		MousePoint.setRA( MousePoint.getRA().reduce() );
		MousePoint.setDec( Dec.getD() );

		return MousePoint;

  } else {
		yy = dx*sinc;
		xx = r*cosDec0*cosc - dy*sinDec0*sinc;

		double RARad = ( atan( yy / xx ) );
		//resolve ambiguity of atan():
		if ( xx<0 ) RARad = RARad + PI();
//		if ( xx>0 && yy<0 ) RARad = RARad + 2.0*PI();

		dms deltaRA, Dec;
		deltaRA.setRadians( RARad );
		Dec.setRadians( asin( cosc*sinDec0 + (dy*sinc*cosDec0)/r ) );

		MousePoint.setRA( focus.getRA().getH() + deltaRA.getH() );
		MousePoint.setRA( MousePoint.getRA().reduce() );
		MousePoint.setDec( Dec.getD() );

		return MousePoint;
	}
}
//---------------------------------------------------------------------------

void SkyMap::setUpDownloads()
{
	downloads++;
}

void SkyMap::setDownDownloads()
{
	downloads--;
}

// force new compute of the skymap (used instead of update())
void SkyMap::Update()
{
	computeSkymap = true;
	update();
}

void SkyMap::resizeEvent( QResizeEvent * )
{
    computeSkymap = true;	// skymap must be new computed
    if ( testWState(WState_AutoMask) )
		updateMask();
}

bool SkyMap::checkVisibility( SkyPoint *p, float FOV, bool useAltAz, bool isPoleVisible ) {
	double dX, dY, XMax;

	if ( useAltAz ) {
		dY = fabs( p->getAlt().getD() - focus.getAlt().getD() );
	} else {
		dY = fabs( p->getDec().getD() - focus.getDec().getD() );
	}
	if ( dY > FOV ) return false;
	if ( isPoleVisible ) return true;

	if ( useAltAz ) {
		dX = fabs( p->getAz().getD() - focus.getAz().getD() );
		XMax = 1.2*FOV/cos( focus.getAlt().radians() );
	} else {
		dX = fabs( p->getRA().getD() - focus.getRA().getD() );
		XMax = 1.2*FOV/cos( focus.getDec().radians() );
	}
	if ( dX > 180.0 ) dX = 360.0 - dX; // take shorter distance around sky

	if ( dX < XMax ) {
		return true;
	} else {
		return false;
	}
}

bool SkyMap::unusablePoint (double dx, double dy)
{
//	qDebug ("dx=%f dy=%f", dx, dy);
	if (dx >= 1.41 || dx <= -1.41 || dy >= 1.41 || dy <= -1.41)
		return true;
	else
		return false;
}

void SkyMap::setDefaultMouseCursor()
{
	mouseMoveCursor = false;	// no mousemove cursor
	QPainter p;
	QPixmap cursorPix (32, 32); // size 32x32 (this size is compatible to all systems)
// the center of the pixmap
	int mx = cursorPix.	width() / 2;
	int my = cursorPix.	height() / 2;

	cursorPix.fill (white);  // white background
	p.begin (&cursorPix);
	p.setPen (QPen (black, 2));	// black lines
// 1. diagonal
	p.drawLine (mx - 2, my - 2, mx - 8, mx - 8);
	p.drawLine (mx + 2, my + 2, mx + 8, mx + 8);
// 2. diagonal
	p.drawLine (mx - 2, my + 2, mx - 8, mx + 8);
	p.drawLine (mx + 2, my - 2, mx + 8, mx - 8);
	p.end();

// create a mask to make parts of the pixmap invisible
	QBitmap mask (32, 32);
	mask.fill (color0);	// all is invisible

	p.begin (&mask);
// paint over the parts which should be visible
	p.setPen (QPen (color1, 3));
// 1. diagonal
	p.drawLine (mx - 2, my - 2, mx - 8, mx - 8);
	p.drawLine (mx + 2, my + 2, mx + 8, mx + 8);
// 2. diagonal
	p.drawLine (mx - 2, my + 2, mx - 8, mx + 8);
	p.drawLine (mx + 2, my - 2, mx + 8, mx - 8);
	p.end();

	cursorPix.setMask (mask);	// set the mask
	QCursor cursor (cursorPix);
	setCursor (cursor);
}

void SkyMap::setMouseMoveCursor()
{
	if (mouseButtonDown)
	{
		setCursor (9);	// cursor shape defined in qt
		mouseMoveCursor = true;
	}
}

void SkyMap::showFocusCoords( void ) {
	//display object info in infoPanel
	QString s, oname;
	char dsgn = '+', azsgn = '+', altsgn = '+';

	oname = i18n( "nothing" );
	if ( foundObject != NULL && ksw->GetOptions()->isTracking ) oname = foundObject->translatedName();
	ksw->FocusObject->setText( i18n( "Focused on: " ) + oname );

	if ( focus.getDec().getD() < 0 ) dsgn = '-';
	int dd = abs( focus.getDec().getDeg() );
	int dm = abs( focus.getDec().getArcMin() );
	int ds = abs( focus.getDec().getArcSec() );
	ksw->FocusRADec->setText( s.sprintf( "RA: %02d:%02d:%02d  Dec: %c%02d:%02d:%02d ",
					focus.getRA().getHour(), focus.getRA().getHMin(),  focus.getRA().getHSec(),
					dsgn, dd, dm, ds ) );

	if ( focus.getAlt().getD() < 0 ) altsgn = '-';
	if ( focus.getAz().getD() < 0 ) azsgn = '-';
	int altd = abs( focus.getAlt().getDeg() );
	int altm = abs( focus.getAlt().getArcMin() );
	int alts = abs( focus.getAlt().getArcSec() );
	int azd = abs( focus.getAz().getDeg() );
	int azm = abs( focus.getAz().getArcMin() );
	int azs = abs( focus.getAz().getArcSec() );
	ksw->FocusAltAz->setText( s.sprintf( "Alt: %c%02d:%02d:%02d  Az: %c%02d:%02d:%02d ",
					altsgn, altd, altm, alts,
					azsgn, azd, azm, azs ) );
}

void SkyMap::addLink( void ) {
	AddLinkDialog adialog( this );
	QString entry;
  QFile file;

	if ( adialog.exec()==QDialog::Accepted ) {
		if ( adialog.isImageLink() ) {
			//Add link to object's ImageList, and descriptive text to its ImageTitle list
			clickedObject->ImageList.append( adialog.url() );
			clickedObject->ImageTitle.append( adialog.title() );

			//Also, update the user's custom image links database
			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "myimage_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom image-links file could not be opened.\nLink cannot be recorded for future sessions." );		
				KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
				return;
			} else {
				entry = clickedObject->name + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
      }
		} else {
			clickedObject->InfoList.append( adialog.url() );
			clickedObject->InfoTitle.append( adialog.title() );

			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "myinfo_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions." );						KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
				return;
			} else {
				entry = clickedObject->name + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry;
				file.close();
      }
		}
	}
}

void SkyMap::setRiseSetLabels( void ) {
	QTime rtime = clickedObject->riseTime( ksw->GetData()->CurrentDate, ksw->geo );
	QString rt, rt2;
	if ( rtime.isValid() ) {
		int min = rtime.minute();
		if ( rtime.second() >=30 ) ++min;
		rt2.sprintf( "%02d:%02d", rtime.hour(), min );
		rt = i18n( "Rise Time: " ) + rt2;
	} else if ( clickedObject->getAlt().getD() > 0 ) {
		rt = i18n( "No Rise Time: Circumpolar" );
	} else {
		rt = i18n( "No Rise Time: Never rises" );
	}

	QTime stime = clickedObject->setTime( ksw->GetData()->CurrentDate, ksw->geo );
	QString st, st2;
	if ( stime.isValid() ) {
		int min = stime.minute();
		if ( stime.second() >=30 ) ++min;
		st2.sprintf( "%02d:%02d", stime.hour(), min );
		st = i18n( "Set Time: " ) + st2;
	} else if ( clickedObject->getAlt().getD() > 0 ) {
		st = i18n( "No Set Time: Circumpolar" );
	} else {
		st = i18n( "No Set Time: Never rises" );
	}

	pmRiseTime->setText( rt );
	pmSetTime->setText( st );
}

#include "skymap.moc"
