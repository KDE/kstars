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
#include <qcursor.h>
#include <qtimer.h>

#include <math.h>
#include <stdlib.h>
#include <iostream.h>
#include <unistd.h>

#include "kstars.h"
#include "skymap.h"
#include "imageviewer.h"
#include "addlinkdialog.h"

SkyMap::SkyMap(QWidget *parent, const char *name )
 : QWidget (parent,name), ClickedObject(0), FoundObject(0), downloads (0), computeSkymap (true)
{
	ksw = (KStars*) kapp->mainWidget();

	pts = new QPointArray( 2000 );  // points for milkyway and horizon
	sp = new SkyPoint();            // needed by coordinate grid

	setDefaultMouseCursor();	// set the cross cursor
	kapp->config()->setGroup( "View" );
	ksw->data()->ZoomLevel = kapp->config()->readNumEntry( "ZoomLevel", 3 );
	if ( ksw->data()->ZoomLevel > 12 ) ksw->data()->ZoomLevel = 12;
	if ( ksw->data()->ZoomLevel < 0 )  ksw->data()->ZoomLevel = 0;
	if ( ksw->data()->ZoomLevel == 12 ) ksw->actZoomIn->setEnabled( false );
	if ( ksw->data()->ZoomLevel == 0  ) ksw->actZoomOut->setEnabled( false );

	// load the pixmaps of stars
	starpix = new StarPixmap (ksw->options()->starColorMode, ksw->options()->starColorIntensity);

  //initialize pixelScale array, will be indexed by ZoomLevel
	for ( int i=0; i<13; ++i ) {
		pixelScale[i] = int( 256.*pow(sqrt(2.0),i) );
		Range[i] = 75.0*256.0/pixelScale[i];
	}

	setBackgroundColor( QColor( ksw->options()->colorSky ) );
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
	clickedPoint()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
	if ( ksw->options()->drawGround && clickedPoint()->alt().Degrees() < 0.0 ) {
		QString caption = i18n( "Requested position below horizon" );
		QString message = i18n( "The requested position is below the horizon.\nWould you like to go there anyway?" );
		if ( KMessageBox::warningYesNo( 0, message, caption )==KMessageBox::No ) {
			setClickedObject( NULL );
			setFoundObject( NULL );
			return;
		}
	}

	//update the focus to the selected coordinates
	setFocus( clickedPoint() );
	focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
	setOldFocus( clickedPoint() );
	oldfocus()->setAz( focus()->az() );
	oldfocus()->setAlt( focus()->alt() );

	double dHA = ksw->data()->LSTh.Hours() - focus()->ra().Hours();
	while ( dHA < 0.0 ) dHA += 24.0;
	ksw->data()->HourAngle.setH( dHA );

	//display coordinates in statusBar
	QString sRA, sDec, s;
	char dsgn = '+';

	if ( clickedPoint()->dec().Degrees() < 0 ) dsgn = '-';
	int dd = abs( clickedPoint()->dec().degree() );
	int dm = abs( clickedPoint()->dec().getArcMin() );
	int ds = abs( clickedPoint()->dec().getArcSec() );

	sRA = sRA.sprintf( "%02d:%02d:%02d", clickedPoint()->ra().hour(), clickedPoint()->ra().minute(), clickedPoint()->ra().second() );
	sDec = sDec.sprintf( "%c%02d:%02d:%02d", dsgn, dd, dm, ds );
	s = sRA + ",  " + sDec;
	ksw->statusBar()->changeItem( s, 1 );

  setFoundObject( ClickedObject );
	if ( foundObject() != NULL ) { //set tracking to true
		ksw->options()->isTracking = true;
		ksw->actTrack->setIconSet( BarIcon( "encrypted" ) );
	} else {
		ksw->options()->isTracking = false;
		ksw->actTrack->setIconSet( BarIcon( "decrypted" ) );
	}

	ksw->showFocusCoords(); //updateinfoPanel
	Update();	// must be new computed
}

void SkyMap::slotDSS( void ) {
	QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
	QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
	QString RAString, DecString;
	char decsgn;
	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", clickedPoint()->ra().hour(),
																								 clickedPoint()->ra().minute(),
																								 clickedPoint()->ra().second() );
	decsgn = '+';
	if (clickedPoint()->dec().Degrees() < 0.0) decsgn = '-';
	int dd = abs( clickedPoint()->dec().degree() );
	int dm = abs( clickedPoint()->dec().getArcMin() );
	int ds = abs( clickedPoint()->dec().getArcSec() );

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
	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", clickedPoint()->ra().hour(),
																								 clickedPoint()->ra().minute(),
																								 clickedPoint()->ra().second() );
	decsgn = '+';
	if (clickedPoint()->dec().Degrees() < 0.0) decsgn = '-';
	int dd = abs( clickedPoint()->dec().degree() );
	int dm = abs( clickedPoint()->dec().getArcMin() );
	int ds = abs( clickedPoint()->dec().getArcSec() );

	DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

	//concat all the segments into the kview command line:
	KURL url (URLprefix + RAString + DecString + URLsuffix);
	new ImageViewer (&url, this);
}

void SkyMap::slotInfo( int id ) {
	QStringList::Iterator it = clickedObject()->InfoList.at(id-200);
	QString sURL = (*it);
	KURL url ( sURL );
	if (!url.isEmpty())
		kapp->invokeBrowser(sURL);
}

void SkyMap::slotImage( int id ) {
	QStringList::Iterator it = clickedObject()->ImageList.at(id-100);
  QString sURL = (*it);
	KURL url ( sURL );
	if (!url.isEmpty())
		new ImageViewer (&url, this);
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
	slewing = false;
	Update();	// Need a full update to draw faint objects that are not drawn while slewing.
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
	QString s;
	float step = 2.0;
	if ( e->state() & ShiftButton ) step = 4.0;

	switch ( e->key() ) {
		case Key_Left :
			if ( ksw->options()->useAltAz ) {
				focus()->setAz( focus()->az().Degrees() - step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra().Hours() + 0.05*step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}

			slewing = true;
			break;

		case Key_Right :
			if ( ksw->options()->useAltAz ) {
				focus()->setAz( focus()->az().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra().Hours() - 0.05*step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}


			slewing = true;
			break;

		case Key_Up :
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if ( focus()->alt().Degrees() >= 90.0 ) focus()->setAlt( 89.9999 );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if (focus()->dec().Degrees() > 90.0) focus()->setDec( 90.0 );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}

			slewing = true;
			break;

		case Key_Down:
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt().Degrees() - step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if ( focus()->alt().Degrees() <= -90.0 ) focus()->setAlt( -89.9999 );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec().Degrees() - step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if (focus()->dec().Degrees() < -90.0) focus()->setDec( -90.0 );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
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

//In the following cases, we set slewing=true in order to disengage tracking
		case Key_N: //center on north horizon
			focus()->setAlt( 15.0001 ); focus()->setAz( 0.0001 );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slewing = true;
			break;

		case Key_E: //center on east horizon
			focus()->setAlt( 15.0001 ); focus()->setAz( 90.0001 );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slewing = true;
			break;

		case Key_S: //center on south horizon
			focus()->setAlt( 15.0001 ); focus()->setAz( 180.0 );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slewing = true;
			break;

		case Key_W: //center on west horizon
			focus()->setAlt( 15.0001 ); focus()->setAz( 270.0 );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slewing = true;
			break;

		case Key_Z: //center on Zenith
			focus()->setAlt( 89.99995 ); focus()->setAz( 180.0 );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slewing = true;
			break;

		case Key_0: //center on Sun
			setClickedObject( ksw->data()->Sun );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_1: //center on Mercury
			setClickedObject( ksw->data()->Mercury );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_2: //center on Venus
			setClickedObject( ksw->data()->Venus );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_3: //center on Moon
			setClickedObject( ksw->data()->Moon );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_4: //center on Mars
			setClickedObject( ksw->data()->Mars );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_5: //center on Jupiter
			setClickedObject( ksw->data()->Jupiter );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_6: //center on Saturn
			setClickedObject( ksw->data()->Saturn );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_7: //center on Uranus
			setClickedObject( ksw->data()->Uranus );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_8: //center on Neptune
			setClickedObject( ksw->data()->Neptune );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;

		case Key_9: //center on Pluto
			setClickedObject( ksw->data()->Pluto );
     setClickedPoint( clickedObject()->pos() );
			slotCenter();
			break;
	}

	setOldFocus( focus() );
	oldfocus()->setAz( focus()->az() );
	oldfocus()->setAlt( focus()->alt() );

	double dHA = ksw->data()->LSTh.Hours() - focus()->ra().Hours();
  while ( dHA < 0.0 ) dHA += 24.0;
	ksw->data()->HourAngle.setH( dHA );

	if ( slewing ) {
		if ( ksw->options()->isTracking ) {
			setClickedObject( NULL );
			setFoundObject( NULL );//no longer tracking foundObject
			ksw->options()->isTracking = false;
  	  ksw->actTrack->setIconSet( BarIcon( "decrypted" ) );
		}
		ksw->showFocusCoords();
	}

	Update(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->data()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->data()->ZoomLevel ];

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	//determine RA, Dec of mouse pointer
	setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );

	if ( mouseButtonDown ) {
		// set the mouseMoveCursor and set slewing=true, if they are not set yet
		if (!mouseMoveCursor) setMouseMoveCursor();
		if (!slewing) {
			slewing = true;
			ksw->options()->isTracking = false; //break tracking on slew
			ksw->actTrack->setIconSet( BarIcon( "decrypted" ) );
			setClickedObject( NULL );
			setFoundObject( NULL );//no longer tracking foundObject
		}

		//Update focus such that the sky coords at mouse cursor remain approximately constant
		if ( ksw->options()->useAltAz ) {
			mousePoint()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			clickedPoint()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			dms dAz = mousePoint()->az().Degrees() - clickedPoint()->az().Degrees();
			dms dAlt = mousePoint()->alt().Degrees() - clickedPoint()->alt().Degrees();
			focus()->setAz( focus()->az().Degrees() - dAz.Degrees() ); //move focus in opposite direction
			focus()->setAlt( focus()->alt().Degrees() - dAlt.Degrees() );

			if ( focus()->alt().Degrees() >= 90.0 ) focus()->setAlt( 89.9999 );
			if ( focus()->alt().Degrees() <= -90.0 ) focus()->setAlt( -89.9999 );

			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		} else {
			dms dRA = mousePoint()->ra().Degrees() - clickedPoint()->ra().Degrees();
			dms dDec = mousePoint()->dec().Degrees() - clickedPoint()->dec().Degrees();
			focus()->setRA( focus()->ra().Hours() - dRA.Hours() ); //move focus in opposite direction
			focus()->setDec( focus()->dec().Degrees() - dDec.Degrees() );

			if ( focus()->dec().Degrees() >90.0 ) focus()->setDec( 90.0 );
			if ( focus()->dec().Degrees() <-90.0 ) focus()->setDec( -90.0 );
			focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
		}

		setOldFocus( focus() );

		double dHA = ksw->data()->LSTh.Hours() - focus()->ra().Hours();
		while ( dHA < 0.0 ) dHA += 24.0;
		ksw->data()->HourAngle.setH( dHA );

		//redetermine RA, Dec of mouse pointer, using new focus
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );
		setClickedPoint( mousePoint() );

		ksw->showFocusCoords(); //update infoPanel
		Update();  // must be new computed
	} else {

		QString sRA, sDec, s;
		int dd,dm,ds;
		char dsgn = '+';
		if ( mousePoint()->dec().Degrees() < 0.0 ) dsgn = '-';
		dd = abs( mousePoint()->dec().degree() );
		dm = abs( mousePoint()->dec().getArcMin() );
		ds = abs( mousePoint()->dec().getArcSec() );

		sRA = sRA.sprintf( " %02d:%02d:%02d", mousePoint()->ra().hour(), mousePoint()->ra().minute(),  mousePoint()->ra().second() );
		sDec = sDec.sprintf( "%c%02d:%02d:%02d ", dsgn, dd, dm, ds );
		s = sRA + ",  " + sDec;
		ksw->statusBar()->changeItem( s, 1 );
	}
}

void SkyMap::mouseReleaseEvent( QMouseEvent *e ) {
	if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
	mouseButtonDown = false;
	slewing = false;
	Update();	// is needed because after moving the sky not all stars are shown
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
	int id=0;

  // if button is down and cursor is not moved set the move cursor after 500 ms
	QTimer t;
	t.singleShot (500, this, SLOT (setMouseMoveCursor()));

	if ( mouseButtonDown == false ) {
		if ( e->button()==LeftButton ) {
			mouseButtonDown = true;
		}

		double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->data()->ZoomLevel ];
		double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->data()->ZoomLevel ];

		if (unusablePoint (dx, dy)) return;	// break if point is unusable

		//determine RA, Dec of mouse pointer
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );
		setClickedPoint( mousePoint() );

		double rmin;
		double rstar_min = 10000000.0;
		int istar_min = -1;

		//Search stars database for nearby object.
		for ( unsigned int i=0; i<ksw->data()->starList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->data()->starList.at(i);
			double dRA = test->ra().Hours() - clickedPoint()->ra().Hours();
			double dDec = test->dec().Degrees() - clickedPoint()->dec().Degrees();
			//determine angular distance between this object and mouse cursor
			double f = 15.0*cos( test->dec().radians() );
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
		double dRA = ksw->data()->Sun->ra().Hours() - clickedPoint()->ra().Hours();
		double dDec = ksw->data()->Sun->dec().Degrees() - clickedPoint()->dec().Degrees();
		double f = 15.0*cos( ksw->data()->Sun->dec().radians() );
		double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 0;
			rsolar_min = r;
		}

		//Moon
		dRA = ksw->data()->Moon->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Moon->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Moon->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 1;
			rsolar_min = r;
		}

		//Mercury
		dRA = ksw->data()->Mercury->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Mercury->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Mercury->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 2;
			rsolar_min = r;
		}

		//Venus
		dRA = ksw->data()->Venus->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Venus->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Venus->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 3;
			rsolar_min = r;
		}

		//Mars
		dRA = ksw->data()->Mars->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Mars->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Mars->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 4;
			rsolar_min = r;
		}

		//Jupiter
		dRA = ksw->data()->Jupiter->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Jupiter->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Jupiter->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 5;
			rsolar_min = r;
		}

		//Saturn
		dRA = ksw->data()->Saturn->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Saturn->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Saturn->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 6;
			rsolar_min = r;
		}

		//Uranus
		dRA = ksw->data()->Uranus->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Uranus->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Uranus->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 7;
			rsolar_min = r;
		}

		//Neptune
		dRA = ksw->data()->Neptune->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Neptune->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Neptune->dec().radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			isolar_min = 8;
			rsolar_min = r;
		}

		//Pluto
		dRA = ksw->data()->Pluto->ra().Hours() - clickedPoint()->ra().Hours();
		dDec = ksw->data()->Pluto->dec().Degrees() - clickedPoint()->dec().Degrees();
		f = 15.0*cos( ksw->data()->Pluto->dec().radians() );
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
		for ( unsigned int i=0; i<ksw->data()->messList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->data()->messList.at(i);
			double dRA = test->ra().Hours()-clickedPoint()->ra().Hours();
			double dDec = test->dec().Degrees()-clickedPoint()->dec().Degrees();
				double f = 15.0*cos( test->dec().radians() );
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
		for ( unsigned int i=0; i<ksw->data()->ngcList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->data()->ngcList.at(i);
			double dRA = test->ra().Hours()-clickedPoint()->ra().Hours();
			double dDec = test->dec().Degrees()-clickedPoint()->dec().Degrees();
				double f = 15.0*cos( test->dec().radians() );
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
		for ( unsigned int i=0; i<ksw->data()->icList.count(); ++i ) {
			//test RA and dec to see if this star is roughly nearby
			SkyObject *test = (SkyObject *)ksw->data()->icList.at(i);
			double dRA = test->ra().Hours()-clickedPoint()->ra().Hours();
			double dDec = test->dec().Degrees()-clickedPoint()->dec().Degrees();
				double f = 15.0*cos( test->dec().radians() );
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
    QString s, DisplayName;

		setClickedObject( NULL );
		StarObject *starobj = NULL;

		if ( rmin < 200.0/pixelScale[ ksw->data()->ZoomLevel ] ) {
			switch (icat) {
				case -1: //solar system object
					switch( isolar_min ) {
						case 0: //sun
							setClickedObject( (SkyObject *)ksw->data()->Sun );
							break;
						case 1: //moon
							setClickedObject( (SkyObject *)ksw->data()->Moon );
							break;
						case 2: //mercury
							setClickedObject( (SkyObject *)ksw->data()->Mercury );
							break;
						case 3: //venus
							setClickedObject( (SkyObject *)ksw->data()->Venus );
							break;
						case 4: //mars
							setClickedObject( (SkyObject *)ksw->data()->Mars );
							break;
						case 5: //jupiter
							setClickedObject( (SkyObject *)ksw->data()->Jupiter );
							break;
						case 6: //saturn
							setClickedObject( (SkyObject *)ksw->data()->Saturn );
							break;
						case 7: //uranus
							setClickedObject( (SkyObject *)ksw->data()->Uranus );
							break;
						case 8: //neptune
							setClickedObject( (SkyObject *)ksw->data()->Neptune );
							break;
						case 9: //pluto
							setClickedObject( (SkyObject *)ksw->data()->Pluto );
							break;
					}

					if ( clickedObject() != NULL ) setClickedPoint( clickedObject()->pos() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject()->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject()->translatedName() );
							pmTitle2->setText( QString::null );
							pmType->setText( i18n( "Solar System" ) );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject()->ImageList.begin();
              itTitle = clickedObject()->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								sURL = QString(*itList);
								QString t = QString(*itTitle);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
							}

							if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

              itList  = clickedObject()->InfoList.begin();
              itTitle = clickedObject()->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

							pmenu->popup( QCursor::pos() );

							break;
						default: // avoid compiler warnings about not cases not handled
							// all other events will be ignored
							break;
					}
					break;
				case 0: //star
					starobj = (StarObject *)ksw->data()->starList.at(istar_min);
					setClickedObject( (SkyObject *)ksw->data()->starList.at(istar_min) );
					setClickedPoint( clickedObject()->pos() );

					//set DisplayName to be either the Arabic name, the genetive name, or both
					DisplayName = clickedObject()->translatedName();
					if ( clickedObject()->name2().length() ) {
						if ( DisplayName.length() ) {
							DisplayName += " (" + clickedObject()->name2() + ")";
						} else {
							DisplayName = clickedObject()->name2();
						}
					}

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( DisplayName, 0 );
							break;
						case RightButton:
							pmTitle->setText( DisplayName );
							pmTitle2->setText( i18n( "Spectral Type: %1" ).arg(starobj->sptype()) );
							if ( clickedObject()->name() != "star" ) {
								pmType->setText( i18n( "star" ) );
							} else {
								pmType->setText( QString::null );
							}

							setRiseSetLabels();

							pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );

							//can't add links to anonymous stars, because links indexed by object name :(
							if (clickedObject()->name() != "star" ) {
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
					setClickedObject( (SkyObject *)ksw->data()->messList.at(imess_min) );
					setClickedPoint( clickedObject()->pos() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject()->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject()->translatedName() );
							if ( !clickedObject()->longname().isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject()->longname().latin1() ) );
							} else if ( !clickedObject()->name2().isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject()->name2().latin1() ) );
							}
							pmType->setText( ksw->data()->TypeName[ clickedObject()->type() ] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject()->ImageList.begin();
              itTitle = clickedObject()->ImageTitle.begin();

							id=100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

							itList  = clickedObject()->InfoList.begin();
							itTitle = clickedObject()->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
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
					setClickedObject( (SkyObject *)ksw->data()->ngcList.at(ingc_min) );
					setClickedPoint( clickedObject()->pos() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject()->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject()->translatedName() );
							if ( !clickedObject()->longname().isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject()->longname().latin1() ) );
							} else if ( !clickedObject()->name2().isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject()->name2().latin1() ) );
							}
							pmType->setText( ksw->data()->TypeName[ clickedObject()->type() ] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject()->ImageList.begin();
              itTitle = clickedObject()->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

							itList  = clickedObject()->InfoList.begin();
							itTitle = clickedObject()->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
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
					setClickedObject( (SkyObject *)ksw->data()->icList.at(iic_min) );
					setClickedPoint( clickedObject()->pos() );

					switch (e->button()) {
						case LeftButton:
							ksw->statusBar()->changeItem( clickedObject()->translatedName(), 0 );
							break;
						case RightButton:
							pmTitle->setText( clickedObject()->translatedName() );
							if ( !clickedObject()->longname().isEmpty() ) {
								pmTitle2->setText( i18n( clickedObject()->longname().latin1() ) );
							} else {
								pmTitle2->setText( i18n( clickedObject()->name2().latin1() ) );
							}
							pmType->setText( ksw->data()->TypeName[ clickedObject()->type() ] );

							setRiseSetLabels();

//					Add custom items to popup menu based on object's ImageList and InfoList
              itList  = clickedObject()->ImageList.begin();
              itTitle = clickedObject()->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.latin1() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
							if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

              itList  = clickedObject()->InfoList.begin();
              itTitle = clickedObject()->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
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
			setClickedObject( NULL );

			switch (e->button()) {
				case LeftButton:
					ksw->statusBar()->changeItem( i18n( "nothing" ), 0 );
					break;
				case RightButton:
					pmTitle->setText( i18n( "nothing" ) );
					pmTitle2->setText( QString::null );
					pmType->setText( QString::null );
					pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
					pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );

					pmenu->popup( QCursor::pos() );
					break;
				default:
					break;
			}
		}
	}
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->data()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->data()->ZoomLevel ];

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	if ( dx != 0.0 || dy != 0.0 )  slotCenter();
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

	float FOV = Range[ ksw->data()->ZoomLevel ]*width()/600.;
	float dY;
  bool isPoleVisible = false;
	if ( ksw->options()->useAltAz ) {
		dY = fabs( focus()->alt().Degrees() ) + FOV;
	} else {
		dY = fabs( focus()->dec().Degrees() ) + FOV;
	}
	if ( dY >= 90. ) isPoleVisible = true;

	psky.begin( sky );
	psky.fillRect( 0, 0, width(), height(), QBrush( ksw->options()->colorSky ) );

	QFont stdFont = psky.font();
	QFont smallFont = psky.font();
	smallFont.setPointSize( stdFont.pointSize() - 2 );

	QList<QPoint> points;
	int ptsCount;
	int mwmax = pixelScale[ ksw->data()->ZoomLevel ]/100;
	int guidemax = pixelScale[ ksw->data()->ZoomLevel ]/10;

	//at high zoom, double FOV for guide lines so they don't disappear.
	float guideFOV = FOV;
	if ( ksw->data()->ZoomLevel > 4 ) guideFOV *= 2.0;
			

	//Draw Milky Way (draw this first so it's in the "background")
	if ( !slewing && ksw->options()->drawMilkyWay ) {
		psky.setPen( QPen( QColor( ksw->options()->colorMW ) ) );
		psky.setBrush( QBrush( QColor( ksw->options()->colorMW ) ) );
		bool offscreen, lastoffscreen=false;

		for ( unsigned int j=0; j<11; ++j ) {
			if ( ksw->options()->fillMilkyWay ) {
				ptsCount = 0;
				bool partVisible = false;

				QPoint o = getXY( ksw->data()->MilkyWay[j].at(0), ksw->options()->useAltAz );
				if ( o.x() != -100000 && o.y() != -100000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;

				for ( unsigned int i=1; i<ksw->data()->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->data()->MilkyWay[j].at(i), ksw->options()->useAltAz );
					if ( o.x() != -100000 && o.y() != -100000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
					if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;
				}

				if ( ptsCount && partVisible ) {
					psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
	 	  	}
			} else {
	      QPoint o = getXY( ksw->data()->MilkyWay[j].at(0), ksw->options()->useAltAz );
				if (o.x()==-100000 && o.y()==-100000) offscreen = true;
				else offscreen = false;

				psky.moveTo( o.x(), o.y() );
  	
				for ( unsigned int i=1; i<ksw->data()->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->data()->MilkyWay[j].at(i), ksw->options()->useAltAz );
					if (o.x()==-100000 && o.y()==-100000) offscreen = true;
					else offscreen = false;

					//don't draw a line if the last point's getXY was (-100000, -100000)
					int dx = abs(o.x()-psky.pos().x());
					int dy = abs(o.y()-psky.pos().y());
					if ( (!lastoffscreen && !offscreen) && (dx<mwmax && dy<mwmax) ) {
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
	if ( !slewing && ksw->options()->drawGrid ) {
		psky.setPen( QPen( QColor( ksw->options()->colorGrid ), 0, DotLine ) ); //change to colorGrid

		//First, the parallels
		for ( double Dec=-80.; Dec<=80.; Dec += 20. ) {
			bool newlyVisible = false;
			sp->set( 0.0, Dec );
			if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			QPoint o = getXY( sp, ksw->options()->useAltAz );
			QPoint o1 = o;
			psky.moveTo( o.x(), o.y() );

			double dRA = 1./15.; //180 points along full circle of RA
			for ( double RA=dRA; RA<24.; RA+=dRA ) {
				sp->set( RA, Dec );
				if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );

				if ( checkVisibility( sp, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
					o = getXY( sp, ksw->options()->useAltAz );
  	
					int dx = psky.pos().x() - o.x();
					int dy = psky.pos().y() - o.y();
					if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
						if ( newlyVisible ) {
							newlyVisible = false;
							psky.moveTo( o.x(), o.y() );
						} else {
							psky.lineTo( o.x(), o.y() );
						}
					} else {
						psky.moveTo( o.x(), o.y() );	
					}
				}
     }

			//connect the final segment
			int dx = psky.pos().x() - o1.x();
			int dy = psky.pos().y() - o1.y();
			if (abs(dx) < guidemax && abs(dy) < guidemax ) {
				psky.lineTo( o1.x(), o1.y() );
			} else {
				psky.moveTo( o1.x(), o1.y() );	
			}
		}

    //next, the meridians
		for ( double RA=0.; RA<24.; RA += 2. ) {
			bool newlyVisible = false;
			SkyPoint *sp = new SkyPoint( RA, -90. );
			if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			QPoint o = getXY( sp, ksw->options()->useAltAz );
			psky.moveTo( o.x(), o.y() );

			double dDec = 1.;
			for ( double Dec=-89.; Dec<=90.; Dec+=dDec ) {
				sp->set( RA, Dec );
				if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );

				if ( checkVisibility( sp, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
					o = getXY( sp, ksw->options()->useAltAz );
  	  	
					int dx = psky.pos().x() - o.x();
					int dy = psky.pos().y() - o.y();
					if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
						if ( newlyVisible ) {
							newlyVisible = false;
							psky.moveTo( o.x(), o.y() );
						} else {
							psky.lineTo( o.x(), o.y() );
						}
					} else {
						psky.moveTo( o.x(), o.y() );	
					}
				}
			}
		}
	}

	//Draw Equator
	if ( ksw->options()->drawEquator ) {
		psky.setPen( QPen( QColor( ksw->options()->colorEq ), 0, SolidLine ) );

		QPoint o = getXY( ksw->data()->Equator.at(0), ksw->options()->useAltAz );
		QPoint o1 = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		for ( unsigned int i=1; i<ksw->data()->Equator.count(); ++i ) {
			if ( checkVisibility( ksw->data()->Equator.at(i), guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				o = getXY( ksw->data()->Equator.at(i), ksw->options()->useAltAz );

				int dx = psky.pos().x() - o.x();
				int dy = psky.pos().y() - o.y();

				if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
						if ( newlyVisible ) {
						newlyVisible = false;
						QPoint last = getXY( ksw->data()->Equator.at(i-1), ksw->options()->useAltAz );
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
		int dy = psky.pos().y() - o1.y();
		if (abs(dx) < guidemax && abs(dy) < guidemax ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }

	//Draw Ecliptic
	if ( ksw->options()->drawEcliptic ) {
		psky.setPen( QColor( ksw->options()->colorEcl ) );

		QPoint o = getXY( ksw->data()->Ecliptic.at(0), ksw->options()->useAltAz );
		QPoint o1 = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		for ( unsigned int i=1; i<ksw->data()->Ecliptic.count(); ++i ) {
			if ( checkVisibility( ksw->data()->Ecliptic.at(i), guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				o = getXY( ksw->data()->Ecliptic.at(i), ksw->options()->useAltAz );

				int dx = psky.pos().x() - o.x();
				int dy = psky.pos().y() - o.y();
				if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						QPoint last = getXY( ksw->data()->Ecliptic.at(i-1), ksw->options()->useAltAz );
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
		int dy = psky.pos().y() - o1.y();
		if (abs(dx) < width() && abs(dy) < height() ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }

	//Draw Constellation Lines
	if ( ksw->options()->drawConstellLines ) {
		psky.setPen( QColor( ksw->options()->colorCLine ) );
		int iLast = -1;

		for ( unsigned int i=0; i < ksw->data()->clineList.count(); ++i ) {
			QPoint o = getXY( ksw->data()->clineList.at(i), ksw->options()->useAltAz );

			if ( ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) &&
					 ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) ) {
				if ( ksw->data()->clineModeList.at(i)->latin1()=='M' ) {
					psky.moveTo( o.x(), o.y() );
				} else if ( ksw->data()->clineModeList.at(i)->latin1()=='D' ) {
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
	if ( !slewing && ksw->options()->drawConstellNames ) {
		psky.setFont( stdFont );
		psky.setPen( QColor( ksw->options()->colorCName ) );
		for ( unsigned int i=0; i < ksw->data()->cnameList.count(); ++i ) {
			if ( checkVisibility( ksw->data()->cnameList.at(i)->pos(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->data()->cnameList.at(i)->pos(), ksw->options()->useAltAz );
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					if ( ksw->options()->useLatinConstellNames ) {
						int dx = 5*ksw->data()->cnameList.at(i)->name().length();
						psky.drawText( o.x()-dx, o.y(), ksw->data()->cnameList.at(i)->name() );	// latin constellation names
					} else if ( ksw->options()->useLocalConstellNames ) {
						//can't use translatedName() because we need the i18n() comment string...
						int dx = 5*( i18n( "Constellation name (optional)", ksw->data()->cnameList.at(i)->name().local8Bit().data() ).length() );
						psky.drawText( o.x()-dx, o.y(), i18n( "Constellation name (optional)", ksw->data()->cnameList.at(i)->name().local8Bit().data() ) ); // localized constellation names
					} else {
						int dx = 5*ksw->data()->cnameList.at(i)->name2().length();
						psky.drawText( o.x()-dx, o.y(), ksw->data()->cnameList.at(i)->name2() );
					}
				}
			}
		}
  }

  //Draw IC Objects
  if ( !slewing && ksw->options()->drawIC ) {
		psky.setBrush( NoBrush );
		psky.setPen( QColor( ksw->options()->colorIC ) );

		for ( unsigned int i=0; i < ksw->data()->icList.count(); ++i ) {
			if ( checkVisibility( ksw->data()->icList.at(i)->pos(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->data()->icList.at(i)->pos(), ksw->options()->useAltAz );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
					int size = pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
					if (size>8) size = 8;

					//change color if extra images are available
					if ( ksw->data()->icList.at(i)->ImageList.count() )
						psky.setPen( QColor( ksw->options()->colorHST ) );
					drawSymbol( psky, ksw->data()->icList.at(i)->type(), o.x(), o.y(), size );
					psky.setPen( QColor( ksw->options()->colorIC ) );

				}
			}
		}
	}

  //Draw NGC Objects
  if ( !slewing && ksw->options()->drawNGC ) {
		psky.setBrush( NoBrush );
		psky.setPen( QColor( ksw->options()->colorNGC ) );

		for ( unsigned int i=0; i < ksw->data()->ngcList.count(); ++i ) {
			if ( checkVisibility( ksw->data()->ngcList.at(i)->pos(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->data()->ngcList.at(i)->pos(), ksw->options()->useAltAz );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
					int size = pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
					if (size>8) size = 8;

					//change color if extra images are available
					if ( ksw->data()->ngcList.at(i)->ImageList.count() )
						psky.setPen( QColor( ksw->options()->colorHST ) );
					drawSymbol( psky, ksw->data()->ngcList.at(i)->type(), o.x(), o.y(), size );
					psky.setPen( QColor( ksw->options()->colorNGC ) );
				}
			}
		}
	}

	//Draw Messier Objects
	if ( ksw->options()->drawMessier || ksw->options()->drawMessImages ) {
		psky.setPen( QColor( ksw->options()->colorMess ) );
		psky.setBrush( NoBrush );

		for ( unsigned int i=0; i < ksw->data()->messList.count(); ++i ) {
			if ( checkVisibility( ksw->data()->messList.at(i)->pos(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( ksw->data()->messList.at(i)->pos(), ksw->options()->useAltAz );
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					if ( ksw->options()->drawMessImages && ksw->data()->ZoomLevel >5 ) {
						QFile file;
						QImage image;
						QString fname = "m" + ksw->data()->messList.at(i)->name().mid(2) + ".png";
						if ( KStarsData::openDataFile( file, fname ) ) {
							file.close();
	          	image.load( file.name(), "PNG" );
							int w = int( image.width()*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[12] );
              int h = int( image.height()*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[12] );
							int x1 = o.x() - int( 0.5*w );
							int y1 = o.y() - int( 0.5*h );
							ScaledImage = image.smoothScale( w, h );
							psky.drawImage( x1, y1, ScaledImage );
						}
					}

					if ( ksw->options()->drawMessier ) {
						int size = pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
						if (size > 16) size = 16;

						//change color if extra images are available
						if ( ksw->data()->messList.at(i)->ImageList.count() > 1 )
							psky.setPen( QColor( ksw->options()->colorHST ) );
						drawSymbol( psky, ksw->data()->messList.at(i)->type(), o.x(), o.y(), size );
            //reset color
						psky.setPen( QColor( ksw->options()->colorMess ) );
					}			
				}
			}
		}
  }

	//Draw Stars
	if ( ksw->options()->drawBSC ) {

		if ( ksw->data()->ZoomLevel < 6 ) {
			psky.setFont( smallFont );
		} else {
			psky.setFont( stdFont );
		}

		float maglim;
		float maglim0 = ksw->options()->magLimitDrawStar;
		float zoomlim = 7.0 + float( ksw->data()->ZoomLevel )/4.0;

		if ( maglim0 < zoomlim ) maglim = maglim0;
		else maglim = zoomlim;

	  //Only draw bright stars if slewing
		if ( slewing && maglim > 6.0 ) maglim = 6.0;
	
		for ( StarObject *curStar = ksw->data()->starList.first(); curStar; curStar = ksw->data()->starList.next() ) {
			// break loop if maglim is reached
			if ( curStar->mag() > maglim ) break;
		
			if ( checkVisibility( curStar->pos(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( curStar->pos(), ksw->options()->useAltAz );

				// draw star if currently on screen
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					// but only if the star is bright enough.
					float mag = curStar->mag();
					float sizeFactor = 2.0;
					int size = int( sizeFactor*(zoomlim - mag) ) + 1;
					if (size>23) size=23;

					if ( size > 0 ) {
						psky.setPen( QColor( ksw->options()->colorSky ) );
						drawSymbol( psky, curStar->type(), o.x(), o.y(), size, curStar->color() );
						// now that we have drawn the star, we can display some extra info
						if ( !slewing && (curStar->mag() <= ksw->options()->magLimitDrawStarInfo )
								&& (ksw->options()->drawStarName || ksw->options()->drawStarMagnitude ) ) {
							// collect info text
							QString sTmp = "";
							if ( ksw->options()->drawStarName ) {
								if (curStar->name() != "star") sTmp = curStar->name() + " ";	// see line below
//								if ( curStar->skyObjectName() ) sTmp = curStar->name + " ";
							}
							if ( ksw->options()->drawStarMagnitude ) {
								sTmp += QString().sprintf("%.1f", curStar->mag() );
							}
							int offset = 3 + int(0.5*(5.0-mag)) + int(0.5*( ksw->data()->ZoomLevel - 6));
						
							psky.setPen( QColor( ksw->options()->colorSName ) );
							psky.drawText( o.x()+offset, o.y()+offset, sTmp );
						}
					}
				}
			}
		}
	}
	
	//Draw Pluto
  if ( ksw->options()->drawPluto ) {
		psky.setPen( QColor( "gray" ) );
		psky.setBrush( QColor( "gray" ) );
		QPoint o = getXY( ksw->data()->Pluto->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Pluto->image()->isNull() ) {
				ScaledImage = ksw->data()->Pluto->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Neptune
  if ( ksw->options()->drawNeptune ) {
		psky.setPen( QColor( "SkyBlue" ) );
		psky.setBrush( QColor( "SkyBlue" ) );
		QPoint o = getXY( ksw->data()->Neptune->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Neptune->image()->isNull() ) {
				ScaledImage = ksw->data()->Neptune->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Uranus
  if ( ksw->options()->drawUranus ) {
		psky.setPen( QColor( "LightSeaGreen" ) );
		psky.setBrush( QColor( "LightSeaGreen" ) );
		QPoint o = getXY( ksw->data()->Uranus->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Uranus->image()->isNull() ) {
				ScaledImage = ksw->data()->Uranus->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Saturn
  if ( ksw->options()->drawSaturn ) {
		psky.setPen( QColor( "LightYellow2" ) );
		psky.setBrush( QColor( "LightYellow2" ) );
		QPoint o = getXY( ksw->data()->Saturn->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Saturn->image()->isNull() ) {
				size *= 2;  //resize for image (because of rings)
				int x1 = o.x() - size/2;
				int y1 = o.y() - size/2;
				ScaledImage = ksw->data()->Saturn->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Jupiter
  if ( ksw->options()->drawJupiter ) {
		psky.setPen( QColor( "Goldenrod" ) );
		psky.setBrush( QColor( "Goldenrod" ) );
		QPoint o = getXY( ksw->data()->Jupiter->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Jupiter->image()->isNull() ) {
				ScaledImage = ksw->data()->Jupiter->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Mars
  if ( ksw->options()->drawMars ) {
		psky.setPen( QColor( "Red" ) );
		psky.setBrush( QColor( "Red" ) );
		QPoint o = getXY( ksw->data()->Mars->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Mars->image()->isNull() ) {
				ScaledImage = ksw->data()->Mars->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Venus
  if ( ksw->options()->drawVenus ) {
		psky.setPen( QColor( "LightGreen" ) );
		psky.setBrush( QColor( "LightGreen" ) );
		QPoint o = getXY( ksw->data()->Venus->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Venus->image()->isNull() ) {
				ScaledImage = ksw->data()->Venus->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Mercury
  if ( ksw->options()->drawMercury ) {
		psky.setPen( QColor( "SlateBlue1" ) );
		psky.setBrush( QColor( "SlateBlue1" ) );
		QPoint o = getXY( ksw->data()->Mercury->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 4*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 2 && !ksw->data()->Mercury->image()->isNull() ) {
				ScaledImage = ksw->data()->Mercury->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Sun
  if ( ksw->options()->drawSun ) {
		psky.setPen( QColor( "Yellow" ) );
		psky.setBrush( QColor( "Yellow" ) );
		QPoint o = getXY( ksw->data()->Sun->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 8*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( ksw->data()->ZoomLevel > 3 && !ksw->data()->Sun->image()->isNull() ) {
				ScaledImage = ksw->data()->Sun->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Moon
  if ( ksw->options()->drawMoon ) {
		psky.setPen( QColor( "White" ) );
		psky.setBrush( QColor( "White" ) );
		QPoint o = getXY( ksw->data()->Moon->pos(), ksw->options()->useAltAz );
		if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
			int size = 8*pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
			int x1 = o.x() - size/2;
			int y1 = o.y() - size/2;
			if ( !ksw->data()->Moon->image()->isNull() ) {
				ScaledImage = ksw->data()->Moon->image()->smoothScale( size, size );
				psky.drawImage( x1, y1, ScaledImage );
			} else {
				psky.drawEllipse( x1, y1, size, size );
			}
		}
	}

	//Draw Horizon
	if (ksw->options()->drawHorizon || ksw->options()->drawGround) {
		psky.setPen( QColor ( ksw->options()->colorHorz ) );
		psky.setBrush( QColor ( ksw->options()->colorHorz ) );
		ptsCount = 0;

		for ( unsigned int i=0; i<ksw->data()->Horizon.count(); ++i ) {
			QPoint *o = new QPoint();
			*o = getXY( ksw->data()->Horizon.at(i), ksw->options()->useAltAz );
      bool found = false;

			//Use the QList of points to pre-sort visible horizon points
			if ( o->x() > -pixelScale[ ksw->data()->ZoomLevel ]/4 && o->x() < width() + pixelScale[ ksw->data()->ZoomLevel ]/4 ) {

				if ( ksw->options()->useAltAz ) {
	        unsigned int j;
					for ( j=0; j<points.count(); ++j ) {
						if ( ksw->options()->useAltAz && o->x() < points.at(j)->x() ) {
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
		if ( ksw->options()->drawHorizon ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

		for ( unsigned int i=1; i<points.count(); ++i ) {
			pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );
			if ( ksw->options()->drawHorizon ) {
				if ( !ksw->options()->useAltAz && ( abs( points.at(i)->x() - psky.pos().x() ) > 250 ||
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
	if ( ksw->options()->useAltAz && ksw->options()->drawGround ) {
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
		psky.setPen( QColor ( ksw->options()->colorSky ) );
		psky.setFont( stdFont );

		//North
		c->setAz( 359.99 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
		}

		//NorthEast
		c->setAz( 45.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
		}

		//East
		c->setAz( 90.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
		}

		//SouthEast
		c->setAz( 135.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
		}

		//South
		c->setAz( 179.99 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
		}

		//SouthWest
		c->setAz( 225.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
		}

		//West
		c->setAz( 270.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
		}

		//NorthWest
		c->setAz( 315.0 );
		c->setAlt( 0.0 );
		if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		cpoint = getXY( c, ksw->options()->useAltAz );
		cpoint.setY( cpoint.y() + 20 );
		if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
			psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
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
			//This line should only execute for KDE 3...the starpix images look bad for size==2.
			if ( QT_VERSION >=300 && size==2 ) size = 1;

			star = starpix->getPixmap (&color, size);
			bitBlt ((QPaintDevice *) sky, xa-star->width()/2, ya-star->height()/2, star);
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

QPoint SkyMap::getXY( SkyPoint *o, bool Horiz ) {
	QPoint p;
	dms X, Y, X0, dX;
	double sindX, cosdX, sinY, cosY, sinY0, cosY0;

	if ( Horiz ) {
		X0 = focus()->az();

		X = o->az();
		Y = o->alt();

		if ( X0.Degrees() > 270.0 && X.Degrees() < 90.0 ) {
			dX = 360.0 + X0.Degrees() - X.Degrees();
		} else {
			dX = X0.Degrees() - X.Degrees();
		}

		focus()->alt().SinCos( sinY0, cosY0 );

  } else {
		if (focus()->ra().Hours() > 18.0 && o->ra().Hours() < 6.0) {
			dX = o->ra().Degrees() + 360.0 - focus()->ra().Degrees();
		} else {
			dX = o->ra().Degrees() - focus()->ra().Degrees();
	  }
    Y = o->dec().Degrees();
		focus()->dec().SinCos( sinY0, cosY0 );
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

	p.setX( int( 0.5 + 0.5*width()  - pixelScale[ ksw->data()->ZoomLevel ]*k*cosY*sindX ) );
	p.setY( int( 0.5 + 0.5*height() - pixelScale[ ksw->data()->ZoomLevel ]*k*( cosY0*sinY - sinY0*cosY*cosdX ) ) );

	return p;
}
//---------------------------------------------------------------------------

SkyPoint SkyMap::dXdYToRaDec( double dx, double dy, bool useAltAz, dms LSTh, dms lat ) {
	//Determine RA and Dec of a point, given (dx, dy): it's pixel
	//coordinates in the SkyMap with the center of the map as the origin.

	SkyPoint result;
	double sinDec, cosDec, sinDec0, cosDec0, sinc, cosc, sinlat, coslat;
	double xx, yy;

	double r  = sqrt( dx*dx + dy*dy );
	dms centerAngle;
	centerAngle.setRadians( 2.0*asin(0.5*r) );

	focus()->dec().SinCos( sinDec0, cosDec0 );
	centerAngle.SinCos( sinc, cosc );

	if ( useAltAz ) {
		dms HA;
		dms Dec, alt, az, alt0, az0;
		double A;
		double sinAlt, cosAlt, sinAlt0, cosAlt0, sinAz, cosAz;
//		double HA0 = LSTh - focus.ra();
		az0 = focus()->az();
		alt0 = focus()->alt();
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
		az = focus()->az().Degrees() + deltaAz.Degrees();
		alt.setRadians( asin( cosc*sinAlt0 + ( dy*sinc*cosAlt0 )/r ) );

		az.SinCos( sinAz, cosAz );
		alt.SinCos( sinAlt, cosAlt );
		lat.SinCos( sinlat, coslat );

		Dec.setRadians( asin( sinAlt*sinlat + cosAlt*coslat*cosAz ) );
		Dec.SinCos( sinDec, cosDec );

		HA.setRadians( acos( ( sinAlt - sinlat*sinDec )/( coslat*cosDec ) ) );
		if ( sinAz > 0.0 ) HA.setH( 24.0 - HA.Hours() );

		result.setRA( LSTh.Hours() - HA.Hours() );
		result.setRA( result.ra().reduce() );
		result.setDec( Dec.Degrees() );

		return result;

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

		result.setRA( focus()->ra().Hours() + deltaRA.Hours() );
		result.setRA( result.ra().reduce() );
		result.setDec( Dec.Degrees() );

		return result;
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
		dY = fabs( p->alt().Degrees() - focus()->alt().Degrees() );
	} else {
		dY = fabs( p->dec().Degrees() - focus()->dec().Degrees() );
	}
	if ( dY > 1.2*FOV ) return false;
	if ( isPoleVisible ) return true;

	if ( useAltAz ) {
		dX = fabs( p->az().Degrees() - focus()->az().Degrees() );
		XMax = 1.2*FOV/cos( focus()->alt().radians() );
	} else {
		dX = fabs( p->ra().Degrees() - focus()->ra().Degrees() );
		XMax = 1.2*FOV/cos( focus()->dec().radians() );
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

void SkyMap::addLink( void ) {
	AddLinkDialog adialog( this );
	QString entry;
  QFile file;

	if ( adialog.exec()==QDialog::Accepted ) {
		if ( adialog.isImageLink() ) {
			//Add link to object's ImageList, and descriptive text to its ImageTitle list
			clickedObject()->ImageList.append( adialog.url() );
			clickedObject()->ImageTitle.append( adialog.title() );

			//Also, update the user's custom image links database
			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "myimage_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom image-links file could not be opened.\nLink cannot be recorded for future sessions." );		
				KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
      }
		} else {
			clickedObject()->InfoList.append( adialog.url() );
			clickedObject()->InfoTitle.append( adialog.title() );

			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "myinfo_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions." );						KMessageBox::sorry( 0, message, i18n( "Could not open file" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry;
				file.close();
      }
		}
	}
}

void SkyMap::setRiseSetLabels( void ) {
	QTime rtime = clickedObject()->riseTime( ksw->data()->CurrentDate, ksw->geo() );
	QString rt, rt2;
	if ( !rtime.isNull() ) {
		int min = rtime.minute();
		if ( rtime.second() >=30 ) ++min;
		rt2.sprintf( "%02d:%02d", rtime.hour(), min );
		rt = i18n( "Rise Time: " ) + rt2;
	} else if ( clickedObject()->alt().Degrees() > 0 ) {
		rt = i18n( "No Rise Time: Circumpolar" );
	} else {
		rt = i18n( "No Rise Time: Never rises" );
	}

	QTime stime = clickedObject()->setTime( ksw->data()->CurrentDate, ksw->geo() );
	QString st, st2;
	if ( !stime.isNull() ) {
		int min = stime.minute();
		if ( stime.second() >=30 ) ++min;
		st2.sprintf( "%02d:%02d", stime.hour(), min );
		st = i18n( "Set Time: " ) + st2;
	} else if ( clickedObject()->alt().Degrees() > 0 ) {
		st = i18n( "No Set Time: Circumpolar" );
	} else {
		st = i18n( "No Set Time: Never rises" );
	}

	pmRiseTime->setText( rt );
	pmSetTime->setText( st );
}

int SkyMap::getPixelScale( void ) {
	return pixelScale[ ksw->data()->ZoomLevel ];
}

#include "skymap.moc"
