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

#include <kapplication.h>
#include <kconfig.h>
#include <kiconloader.h>
#include <kstatusbar.h>
#include <kmessagebox.h>
#include <kaction.h>
#include <kstandarddirs.h>

#include <qmemarray.h>
#include <qpointarray.h>
#include <qcursor.h>
#include <qbitmap.h>
#include <qpainter.h>

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "skymap.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "imageviewer.h"
#include "infoboxes.h"
#include "detaildialog.h"
#include "addlinkdialog.h"
#include "kspopupmenu.h"
#include "simclock.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "ksmoon.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "starobject.h"
#include "customcatalog.h"

SkyMap::SkyMap(KStarsData *d, QWidget *parent, const char *name )
	: QWidget (parent,name), computeSkymap(true), angularDistanceMode(false),
		ksw(0), data(d), pmenu(0), sky(0), sky2(0), IBoxes(0), 
		ClickedObject(0), FocusObject(0), TransientObject(0),
		starpix(0), pts(0), sp(0)
{
	if ( parent ) ksw = (KStars*) parent->parent();
	else ksw = 0;
	
	pts = new QPointArray( 2000 );  // points for milkyway and horizon
	sp = new SkyPoint();            // needed by coordinate grid

	ZoomRect = QRect();

	setDefaultMouseCursor();	// set the cross cursor

	// load the pixmaps of stars
	starpix = new StarPixmap( data->colorScheme()->starColorMode(), data->colorScheme()->starColorIntensity() );

	setBackgroundColor( QColor( data->colorScheme()->colorNamed( "SkyColor" ) ) );
	setBackgroundMode( QWidget::NoBackground );
	setFocusPolicy( QWidget::StrongFocus );
	setMinimumSize( 380, 250 );
	setSizePolicy( QSizePolicy( QSizePolicy::Expanding, QSizePolicy::Expanding ) );

	setMouseTracking (true); //Generate MouseMove events!
	midMouseButtonDown = false;
	mouseButtonDown = false;
	slewing = false;
	clockSlewing = false;

	ClickedObject = NULL;
	FocusObject = NULL;

	sky = new QPixmap();
	sky2 = new QPixmap();
	pmenu = new KSPopupMenu( ksw );
	
	//Initialize Transient label stuff
	TransientTimeout = 100; //fade label color every 0.2 sec
	connect( &HoverTimer, SIGNAL( timeout() ), this, SLOT( slotTransientLabel() ) );
	connect( &TransientTimer, SIGNAL( timeout() ), this, SLOT( slotTransientTimeout() ) );
	
	IBoxes = new InfoBoxes( Options::windowWidth(), Options::windowHeight(),
			Options::positionTimeBox(), Options::shadeTimeBox(),
			Options::positionGeoBox(), Options::shadeGeoBox(),
			Options::positionFocusBox(), Options::shadeFocusBox(),
			data->colorScheme()->colorNamed( "BoxTextColor" ),
			data->colorScheme()->colorNamed( "BoxGrabColor" ),
			data->colorScheme()->colorNamed( "BoxBGColor" ) );

	IBoxes->showTimeBox( Options::showTimeBox() );
	IBoxes->showFocusBox( Options::showFocusBox() );
	IBoxes->showGeoBox( Options::showGeoBox() );
	IBoxes->timeBox()->setAnchorFlag( Options::stickyTimeBox() );
	IBoxes->geoBox()->setAnchorFlag( Options::stickyGeoBox() );
	IBoxes->focusBox()->setAnchorFlag( Options::stickyFocusBox() );

	IBoxes->geoChanged( data->geo() );

	connect( IBoxes->timeBox(),  SIGNAL( shaded(bool) ), data, SLOT( saveTimeBoxShaded(bool) ) );
	connect( IBoxes->geoBox(),   SIGNAL( shaded(bool) ), data, SLOT( saveGeoBoxShaded(bool) ) );
	connect( IBoxes->focusBox(), SIGNAL( shaded(bool) ), data, SLOT( saveFocusBoxShaded(bool) ) );
	connect( IBoxes->timeBox(),  SIGNAL( moved(QPoint) ), data, SLOT( saveTimeBoxPos(QPoint) ) );
	connect( IBoxes->geoBox(),   SIGNAL( moved(QPoint) ), data, SLOT( saveGeoBoxPos(QPoint) ) );
	connect( IBoxes->focusBox(), SIGNAL( moved(QPoint) ), data, SLOT( saveFocusBoxPos(QPoint) ) );

	connect( this, SIGNAL( destinationChanged() ), this, SLOT( slewFocus() ) );

	//Initialize Refraction correction lookup table arrays.  RefractCorr1 is for calculating
	//the apparent altitude from the true altitude, and RefractCorr2 is for the reverse.
	for ( unsigned int index = 0; index <184; ++index ) {
		double alt = -1.75 + index*0.5;  //start at -1.75 degrees to get midpoint value for each interval.

		RefractCorr1[index] = 1.02 / tan( dms::PI*( alt + 10.3/(alt + 5.11) )/180.0 ) / 60.0; //correction in degrees.
		RefractCorr2[index] = -1.0 / tan( dms::PI*( alt + 7.31/(alt + 4.4) )/180.0 ) / 60.0;
	}
}

SkyMap::~SkyMap() {
	delete starpix;
	delete pts;
	delete sp;
	delete sky;
	delete sky2;
	delete pmenu;
	delete IBoxes;

//Deprecated...DeepSkyObject dtor now handles this itself.
/*//delete any remaining object Image pointers
	for ( DeepSkyObject *obj = data->deepSkyListMessier.first(); obj; obj = data->deepSkyListMessier.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( DeepSkyObject *obj = data->deepSkyListNGC.first(); obj; obj = data->deepSkyListNGC.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( DeepSkyObject *obj = data->deepSkyListIC.first(); obj; obj = data->deepSkyListIC.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( DeepSkyObject *obj = data->deepSkyListOther.first(); obj; obj = data->deepSkyListOther.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }*/
}

void SkyMap::setGeometry( int x, int y, int w, int h ) {
	QWidget::setGeometry( x, y, w, h );
	sky->resize( w, h );
	sky2->resize( w, h );
}

void SkyMap::setGeometry( const QRect &r ) {
	QWidget::setGeometry( r );
	sky->resize( r.width(), r.height() );
	sky2->resize( r.width(), r.height() );
}


void SkyMap::showFocusCoords( bool coordsOnly ) {
	if ( ! coordsOnly ) {
		//display object info in infoBoxes
		QString oname;
		oname = i18n( "nothing" );
		if ( focusObject() != NULL && Options::isTracking() ) 
			oname = focusObject()->translatedLongName();
		
		infoBoxes()->focusObjChanged(oname);
	}

	if ( Options::useAltAz() && Options::useRefraction() ) {
		SkyPoint corrFocus( *(focus()) );
		corrFocus.setAlt( refract( focus()->alt(), false ) );
		corrFocus.HorizontalToEquatorial( data->LST, data->geo()->lat() );
		corrFocus.setAlt( refract( focus()->alt(), true ) );
		infoBoxes()->focusCoordChanged( &corrFocus );
	} else {
		infoBoxes()->focusCoordChanged( focus() );
	}
}

SkyObject* SkyMap::objectNearest( SkyPoint *p ) {
	double r0 = 200.0/Options::zoomFactor();  //the maximum search radius
	double rmin = r0;

	//Search stars database for nearby object.
	double rstar_min = r0;
	double starmag_min = 20.0;      //absurd initial value
	int istar_min = -1;

	if ( Options::showStars() ) { //Can only click on a star if it's being drawn!

		//test RA and dec to see if this star is roughly nearby

		for ( register unsigned int i=0; i<data->starList.count(); ++i ) {
			SkyObject *test = (SkyObject *)data->starList.at(i);

			double dRA = test->ra()->Hours() - p->ra()->Hours();
			double dDec = test->dec()->Degrees() - p->dec()->Degrees();
			//determine angular distance between this object and mouse cursor
			double f = 15.0*cos( test->dec()->radians() );
			double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if (r < r0 && test->mag() < starmag_min ) {
				istar_min = i;
				rstar_min = r;
				starmag_min = test->mag();
			}
		}
	}

	//Next, find the nearest solar system body within r0
	double r = 0.0;
	double rsolar_min = r0;
	SkyObject *solarminobj = NULL;

	if ( Options::showPlanets() )
		solarminobj = data->PCat->findClosest( p, r );

	if ( r < r0 ) {
		rsolar_min = r;
	} else {
		solarminobj = NULL;
	}

	//Moon
	if ( Options::showMoon() ) {
		double dRA = data->Moon->ra()->Hours() - p->ra()->Hours();
		double dDec = data->Moon->dec()->Degrees() - p->dec()->Degrees();
		double f = 15.0*cos( data->Moon->dec()->radians() );
		r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
		if (r < rsolar_min) {
			solarminobj= data->Moon;
			rsolar_min = r;
		}
	}

	//Asteroids
	if ( Options::showAsteroids() ) {
		for ( KSAsteroid *ast = data->asteroidList.first(); ast; ast = data->asteroidList.next() ) {
			//test RA and dec to see if this object is roughly nearby
			double dRA = ast->ra()->Hours() - p->ra()->Hours();
			double dDec = ast->dec()->Degrees() - p->dec()->Degrees();
			double f = 15.0*cos( ast->dec()->radians() );
			double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if ( r < rsolar_min && ast->mag() < Options::magLimitAsteroid() ) {
				solarminobj = ast;
				rsolar_min = r;
			}
		}
	}

	//Comets
	if ( Options::showComets() ) {
		for ( KSComet *com = data->cometList.first(); com; com = data->cometList.next() ) {
			//test RA and dec to see if this object is roughly nearby
			double dRA = com->ra()->Hours() - p->ra()->Hours();
			double dDec = com->dec()->Degrees() - p->dec()->Degrees();
			double f = 15.0*cos( com->dec()->radians() );
			double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if ( r < rsolar_min ) {
				solarminobj = com;
				rsolar_min = r;
			}
		}
	}

	//Next, search for nearest deep-sky object within r0
	double rmess_min = r0;
	double rngc_min = r0;
	double ric_min = r0;
	double rother_min = r0;
	int imess_min = -1;
	int ingc_min = -1;
	int iic_min = -1;
	int iother_min = -1;

	for ( DeepSkyObject *o = data->deepSkyList.first(); o; o = data->deepSkyList.next() ) {
		bool checkObject = false;
		if ( o->isCatalogM() &&
				( Options::showMessier() || Options::showMessierImages() ) ) checkObject = true;
		if ( o->isCatalogNGC() && Options::showNGC() ) checkObject = true;
		if ( o->isCatalogIC() && Options::showIC() ) checkObject = true;
		if ( o->catalog().isEmpty() && Options::showOther() ) checkObject = true;

		if ( checkObject ) {
			//test RA and dec to see if this object is roughly nearby
			double dRA = o->ra()->Hours() - p->ra()->Hours();
			double dDec = o->dec()->Degrees() - p->dec()->Degrees();
			double f = 15.0*cos( o->dec()->radians() );
			double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if ( o->isCatalogM() && r < rmess_min) {
				imess_min = data->deepSkyList.at();
				rmess_min = r;
			}
			if ( o->isCatalogNGC() && r < rngc_min) {
				ingc_min = data->deepSkyList.at();
				rngc_min = r;
			}
			if ( o->isCatalogIC() && r < ric_min) {
				iic_min = data->deepSkyList.at();
				ric_min = r;
			}
			if ( o->catalog().isEmpty() && r < rother_min) {
				iother_min = data->deepSkyList.at();
				rother_min = r;
			}
		}
	}

	//Next, search for nearest object within r0 among the custom catalogs
	double rcust_min = r0;
	int icust_min = -1;
	int icust_cat = -1;

	for ( register unsigned int j=0; j< data->CustomCatalogs.count(); ++j ) {
		if ( Options::showCatalog()[j] ) {
			QPtrList<SkyObject> catList = data->CustomCatalogs.at(j)->objList();

			for ( register unsigned int i=0; i<catList.count(); ++i ) {
				//test RA and dec to see if this object is roughly nearby
				SkyObject *test = (SkyObject *)catList.at(i);
				double dRA = test->ra()->Hours()-p->ra()->Hours();
				double dDec = test->dec()->Degrees()-p->dec()->Degrees();
				double f = 15.0*cos( test->dec()->radians() );
				double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if (r < rcust_min) {
					icust_cat = j;
					icust_min = i;
					rcust_min = r;
				}
			}
		}
	}

	int jmin(-1);
	int icat(-1);

	//Among the objects selected within r0, prioritize the selection by catalog:
	//Planets, Messier, NGC, IC, stars
	if ( istar_min >= 0 && rstar_min < r0 ) {
		rmin = rstar_min;
		icat = 0; //set catalog to star
	}

	//IC object overrides star, unless star is twice as close as IC object
	if ( iic_min >= 0 && ric_min < r0 && rmin > 0.5*ric_min ) {
		rmin = ric_min;
		icat = 1; //set catalog to Deep Sky
		jmin = iic_min;
	}

	//NGC object overrides previous selection, unless previous is twice as close
	if ( ingc_min >= 0 && rngc_min < r0 && rmin > 0.5*rngc_min ) {
		rmin = rngc_min;
		icat = 1; //set catalog to Deep Sky
		jmin = ingc_min;
	}

	//"other" object overrides previous selection, unless previous is twice as close
	if ( iother_min >= 0 && rother_min < r0 && rmin > 0.5*rother_min ) {
		rmin = rother_min;
		icat = 1; //set catalog to Deep Sky
		jmin = iother_min;
	}

	//Messier object overrides previous selection, unless previous is twice as close
	if ( imess_min >= 0 && rmess_min < r0 && rmin > 0.5*rmess_min ) {
		rmin = rmess_min;
		icat = 1; //set catalog to Deep Sky
		jmin = imess_min;
	}

	//Custom object overrides previous selection, unless previous is twice as close
	if ( icust_min >= 0 && rcust_min < r0 && rmin > 0.5*rcust_min ) {
		rmin = rcust_min;
		icat = 2; //set catalog to Custom
	}

	//Solar system body overrides previous selection, unless previous selection is twice as close
	if ( solarminobj != NULL && rmin > 0.5*rsolar_min ) {
		rmin = rsolar_min;
		icat = 3; //set catalog to solar system
	}

	QPtrList<SkyObject> cat;

	switch (icat) {
		case 0: //star
			return data->starList.at(istar_min);
			break;

		case 1: //Deep-Sky Objects
			return data->deepSkyList.at(jmin);
			break;

		case 2: //Custom Catalog Object
			cat = data->CustomCatalogs.at(icust_cat)->objList();
			return cat.at(icust_min);
			break;

		case 3: //solar system object
			return solarminobj;
			break;

		default: //no object found
			return NULL;
			break;
	}
}

void SkyMap::slotTransientLabel( void ) {
	//This function is only called if the HoverTimer manages to timeout.
	//(HoverTimer is restarted with every mouseMoveEvent; so if it times 
	//out, that means there was no mouse movement for HOVER_INTERVAL msec.)
	//Identify the object nearest to the mouse cursor as the
	//TransientObject.  The TransientObject is automatically labeled 
	//in SkyMap::paintEvent().
	//Note that when the TransientObject pointer is not NULL, the next 
	//mouseMoveEvent calls fadeTransientLabel(), which will fade out the 
	//TransientLabel and then set TransientObject to NULL.
	//
	//Do not show a transient label if the map is in motion, or if the mouse 
	//pointer is below the opaque horizon, or if the object has a permanent label
	if ( ! slewing && ! ( Options::useAltAz() && Options::showGround() && 
			mousePoint()->alt()->Degrees() < 0.0 ) ) {
		SkyObject *so = objectNearest( mousePoint() );
		
		if ( so && ! isObjectLabeled( so ) ) {
			setTransientObject( so );
			
			TransientColor = data->colorScheme()->colorNamed( "UserLabelColor" );
			if ( TransientTimer.isActive() ) TransientTimer.stop();
			update();
		}
	}
}


//Slots

void SkyMap::slotTransientTimeout( void ) {
	//Don't fade label if the transientObject is now the focusObject!
	if ( transientObject() == focusObject() && Options::useAutoLabel() ) {
		setTransientObject( NULL );
		TransientTimer.stop();
		return;
	}

	//to fade the labels, we will need to smoothly transition from UserLabelColor to SkyColor.
	QColor c1 = data->colorScheme()->colorNamed( "UserLabelColor" );
	QColor c2 = data->colorScheme()->colorNamed( "SkyColor" );
	
	int dRed =   ( c2.red()   - c1.red()   )/20;
	int dGreen = ( c2.green() - c1.green() )/20;
	int dBlue =  ( c2.blue()  - c1.blue()  )/20;
	int newRed   = TransientColor.red()   + dRed;
	int newGreen = TransientColor.green() + dGreen;
	int newBlue  = TransientColor.blue()  + dBlue;
	
	//Check to see if we have arrived at the target color (SkyColor).
	//If so, point TransientObject to NULL.
	if ( abs(newRed-c2.red()) < abs(dRed) || abs(newGreen-c2.green()) < abs(dGreen) || abs(newBlue-c2.blue()) < abs(dBlue) ) {
		setTransientObject( NULL );
		TransientTimer.stop();
	} else { 
		TransientColor.setRgb( newRed, newGreen, newBlue );
	}

	update();
}

void SkyMap::setFocusObject( SkyObject *o ) { 
	FocusObject = o; 
	
	if ( FocusObject ) 
		Options::setFocusObject( FocusObject->name() );
	else 
		Options::setFocusObject( i18n( "nothing" ) );
}

void SkyMap::slotCenter( void ) {
	setFocusPoint( clickedPoint() );
	if ( Options::useAltAz() ) 
		focusPoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );

	//clear the planet trail of old focusObject, if it was temporary
	if ( focusObject() && focusObject()->isSolarSystem() && data->temporaryTrail ) {
		((KSPlanetBase*)focusObject())->clearTrail();
		data->temporaryTrail = false;
	}

//If the requested object is below the opaque horizon, issue a warning message
//(unless user is already pointed below the horizon)
	if ( Options::useAltAz() && Options::showGround() &&
			focus()->alt()->Degrees() > -1.0 && focusPoint()->alt()->Degrees() < -1.0 ) {

		QString caption = i18n( "Requested Position Below Horizon" );
		QString message = i18n( "The requested position is below the horizon.\nWould you like to go there anyway?" );
		if ( KMessageBox::warningYesNo( this, message, caption,
				i18n("Go Anyway"), i18n("Keep Position"), "dag_focus_below_horiz" )==KMessageBox::No ) {
			setClickedObject( NULL );
			setFocusObject( NULL );
			Options::setIsTracking( false );

			return;
		}
	}

//set FocusObject before slewing.  Otherwise, KStarsData::updateTime() can reset
//destination to previous object...
	setFocusObject( ClickedObject );
	Options::setIsTracking( true );
	if ( ksw ) {
	  ksw->actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );
	  ksw->toolBar( "mainToolBar" )->setButtonIconSet( 4, BarIcon( "encrypted" ) );
	  ksw->actionCollection()->action("track_object")->setText( i18n( "Stop &Tracking" ) );
	}

	//If focusObject is a SS body and doesn't already have a trail, set the temporaryTrail
	if ( focusObject() && focusObject()->isSolarSystem()
	     && Options::useAutoTrail()
			&& ! ((KSPlanetBase*)focusObject())->hasTrail() ) {
		((KSPlanetBase*)focusObject())->addToTrail();
		data->temporaryTrail = true;
	}

	//update the destination to the selected coordinates
	if ( Options::useAltAz() ) { 
		if ( Options::useRefraction() )
			setDestinationAltAz( refract( focusPoint()->alt(), true ).Degrees(), focusPoint()->az()->Degrees() );
		else
			setDestinationAltAz( focusPoint()->alt()->Degrees(), focusPoint()->az()->Degrees() );
	} else {
		setDestination( focusPoint() );
	}

	focusPoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );

	//display coordinates in statusBar
	if ( ksw ) {
		QString sX = focusPoint()->az()->toDMSString();
		QString sY = focusPoint()->alt()->toDMSString(true);
		if ( Options::useAltAz() && Options::useRefraction() )
			sY = refract( focusPoint()->alt(), true ).toDMSString(true);
		QString s = sX + ",  " + sY;
		ksw->statusBar()->changeItem( s, 1 );
		s = focusPoint()->ra()->toHMSString() + ",  " + focusPoint()->dec()->toDMSString(true);
		ksw->statusBar()->changeItem( s, 2 );
	}
	
	showFocusCoords(); //update FocusBox
}

void SkyMap::slotDSS( void ) {
	QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=1" );
	QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
	dms ra(0.0), dec(0.0);
	QString RAString, DecString;
	char decsgn;

	//ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
	//if we clicked on empty sky, we need to precess to J2000.
	if ( clickedObject() ) {
		ra.setH( clickedObject()->ra0()->Hours() );
		dec.setD( clickedObject()->dec0()->Degrees() );
	} else {
		//move present coords temporarily to ra0,dec0 (needed for precessToAnyEpoch)
		clickedPoint()->setRA0( clickedPoint()->ra()->Hours() );
		clickedPoint()->setDec0( clickedPoint()->dec()->Degrees() );
		clickedPoint()->precessFromAnyEpoch( data->ut().djd(), J2000 );
		ra.setH( clickedPoint()->ra()->Hours() );
		dec.setD( clickedPoint()->dec()->Degrees() );

		//restore coords from present epoch
		clickedPoint()->setRA( clickedPoint()->ra0()->Hours() );
		clickedPoint()->setDec( clickedPoint()->dec0()->Degrees() );
	}

	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );

	decsgn = '+';
	if ( dec.Degrees() < 0.0 ) decsgn = '-';
	int dd = abs( dec.degree() );
	int dm = abs( dec.arcmin() );
	int ds = abs( dec.arcsec() );
	DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

	//concat all the segments into the kview command line:
	KURL url (URLprefix + RAString + DecString + URLsuffix);
	
	QString message = i18n( "Digitized Sky Survey image provided by the Space Telescope Science Institute." );
	new ImageViewer (&url, message, this);
}

void SkyMap::slotDSS2( void ) {
	QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?v=2r" );
	QString URLsuffix( "&e=J2000&h=15.0&w=15.0&f=gif&c=none&fov=NONE" );
	dms ra(0.0), dec(0.0);
	QString RAString, DecString;
	char decsgn;

	//ra and dec must be the coordinates at J2000.  If we clicked on an object, just use the object's ra0, dec0 coords
	//if we clicked on empty sky, we need to precess to J2000.
	if ( clickedObject() ) {
		ra.setH( clickedObject()->ra0()->Hours() );
		dec.setD( clickedObject()->dec0()->Degrees() );
	} else {
		//move present coords temporarily to ra0,dec0 (needed for precessToAnyEpoch)
		clickedPoint()->setRA0( clickedPoint()->ra()->Hours() );
		clickedPoint()->setDec0( clickedPoint()->dec()->Degrees() );
		clickedPoint()->precessFromAnyEpoch( data->ut().djd(), J2000 );
		ra.setH( clickedPoint()->ra()->Hours() );
		dec.setD( clickedPoint()->dec()->Degrees() );

		//restore coords from present epoch
		clickedPoint()->setRA( clickedPoint()->ra0()->Hours() );
		clickedPoint()->setDec( clickedPoint()->dec0()->Degrees() );
	}

	RAString = RAString.sprintf( "&r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );

	decsgn = '+';
	if ( dec.Degrees() < 0.0 ) decsgn = '-';
	int dd = abs( dec.degree() );
	int dm = abs( dec.arcmin() );
	int ds = abs( dec.arcsec() );

	DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );

	//concat all the segments into the kview command line:
	KURL url (URLprefix + RAString + DecString + URLsuffix);
	
	QString message = i18n( "Digitized Sky Survey image provided by the Space Telescope Science Institute." );
	new ImageViewer (&url, message, this);
}

void SkyMap::slotInfo( int id ) {
	QStringList::Iterator it = clickedObject()->InfoList.at(id-200);
	QString sURL = (*it);
	KURL url ( sURL );
	if (!url.isEmpty())
		kapp->invokeBrowser(sURL);
}

void SkyMap::slotBeginAngularDistance(void) {
	setPreviousClickedPoint( mousePoint() );
	angularDistanceMode = true;
	beginRulerPoint = getXY( previousClickedPoint(), Options::useAltAz(), Options::useRefraction() );
	endRulerPoint =  QPoint( beginRulerPoint.x(),beginRulerPoint.y() );
}

void SkyMap::slotEndAngularDistance(void) {
	dms angularDistance;
	if(angularDistanceMode) {
		if ( SkyObject *so = objectNearest( mousePoint() ) ) {
			angularDistance = so->angularDistanceTo( previousClickedPoint() );
			ksw->statusBar()->changeItem( so->translatedLongName() + 
					"     " +
					i18n("Angular distance: " ) +
					angularDistance.toDMSString(), 0 );
		} else {
			angularDistance = mousePoint()->angularDistanceTo( previousClickedPoint() );
			ksw->statusBar()->changeItem( i18n("Angular distance: " ) +
				angularDistance.toDMSString(), 0 );
		}
		angularDistanceMode=false;
	}
}

void SkyMap::slotCancelAngularDistance(void) {
	angularDistanceMode=false;
}

void SkyMap::slotImage( int id ) {
	QStringList::Iterator it = clickedObject()->ImageList.at(id-100);
	QStringList::Iterator it2 = clickedObject()->ImageTitle.at(id-100);
	QString sURL = (*it);
	QString message = (*it2);
	KURL url ( sURL );
	if (!url.isEmpty())
		new ImageViewer (&url, clickedObject()->messageFromTitle(message), this);
}

bool SkyMap::isObjectLabeled( SkyObject *object ) {
	for ( SkyObject *o = data->ObjLabelList.first(); o; o = data->ObjLabelList.next() ) {
		if ( o == object ) return true;
	}

	return false;
}

void SkyMap::slotRemoveObjectLabel( void ) {
	for ( SkyObject *o = data->ObjLabelList.first(); o; o = data->ObjLabelList.next() ) {
		if ( o == clickedObject() ) {
			//remove object from list
			data->ObjLabelList.remove();
			break;
		}
	}

	forceUpdate();
}

void SkyMap::slotAddObjectLabel( void ) {
	data->ObjLabelList.append( clickedObject() );
	//Since we just added a permanent label, we don't want it to fade away!
	if ( transientObject() == clickedObject() ) setTransientObject( NULL );
	forceUpdate();
}

void SkyMap::slotRemovePlanetTrail( void ) {
	//probably don't need this if-statement, but just to be sure...
	if ( clickedObject() && clickedObject()->isSolarSystem() ) {
		((KSPlanetBase*)clickedObject())->clearTrail();
		forceUpdate();
	}
}

void SkyMap::slotAddPlanetTrail( void ) {
	//probably don't need this if-statement, but just to be sure...
	if ( clickedObject() && clickedObject()->isSolarSystem() ) {
		((KSPlanetBase*)clickedObject())->addToTrail();
		forceUpdate();
	}
}

void SkyMap::slotDetail( void ) {
// check if object is selected
	if ( !clickedObject() ) {
		KMessageBox::sorry( this, i18n("No object selected."), i18n("Object Details") );
		return;
	}
	DetailDialog detail( clickedObject(), data->ut(), data->geo(), ksw );
	detail.exec();
}

void SkyMap::slotClockSlewing() {
//If the current timescale exceeds slewTimeScale, set clockSlewing=true, and stop the clock.
	if ( fabs( data->clock()->scale() ) > Options::slewTimeScale() ) {
		if ( ! clockSlewing ) {
			clockSlewing = true;
			data->clock()->setManualMode( true );

			// don't change automatically the DST status
			if ( ksw ) ksw->updateTime( false );
		}
	} else {
		if ( clockSlewing ) {
			clockSlewing = false;
			data->clock()->setManualMode( false );

			// don't change automatically the DST status
			if ( ksw ) ksw->updateTime( false );
		}
	}
}

void SkyMap::setFocus( SkyPoint *p ) {
	setFocus( p->ra()->Hours(), p->dec()->Degrees() );
}

void SkyMap::setFocus( const dms &ra, const dms &dec ) {
	setFocus( ra.Hours(), dec.Degrees() );
}

void SkyMap::setFocus( double ra, double dec ) {
	Focus.set( ra, dec );
	focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
}

void SkyMap::setFocusAltAz( const dms &alt, const dms &az) {
	setFocusAltAz( alt.Degrees(), az.Degrees() );
}

void SkyMap::setFocusAltAz(double alt, double az) {
	focus()->setAlt(alt);
	focus()->setAz(az);
	focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
	slewing = false;

	oldfocus()->set( focus()->ra(), focus()->dec() );
	oldfocus()->setAz( focus()->az()->Degrees() );
	oldfocus()->setAlt( focus()->alt()->Degrees() );

	double dHA = data->LST->Hours() - focus()->ra()->Hours();
	while ( dHA < 0.0 ) dHA += 24.0;
	data->HourAngle->setH( dHA );

	forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::setDestination( SkyPoint *p ) {
	Destination.set( p->ra(), p->dec() );
	destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
	emit destinationChanged();
}

void SkyMap::setDestination( const dms &ra, const dms &dec ) {
	Destination.set( ra, dec );
	destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
	emit destinationChanged();
}

void SkyMap::setDestination( double ra, double dec ) {
	Destination.set( ra, dec );
	destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
	emit destinationChanged();
}

void SkyMap::setDestinationAltAz( const dms &alt, const dms &az) {
	destination()->setAlt(alt);
	destination()->setAz(az);
	destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
	emit destinationChanged();
}

void SkyMap::setDestinationAltAz(double alt, double az) {
	destination()->setAlt(alt);
	destination()->setAz(az);
	destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
	emit destinationChanged();
}

void SkyMap::updateFocus() {
	if ( Options::isTracking() && focusObject() != NULL ) {
		if ( Options::useAltAz() ) {
			//Tracking any object in Alt/Az mode requires focus updates
			double dAlt = focusObject()->alt()->Degrees();
			if ( Options::useRefraction() ) 
				dAlt = refract( focusObject()->alt(), true ).Degrees();
			setFocusAltAz( dAlt, focusObject()->az()->Degrees() );
			focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			setDestination( focus() );
		} else {
			//Tracking in equatorial coords
			setFocus( focusObject() );
			focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			setDestination( focus() );
		}
	} else if ( Options::isTracking() && focusPoint() != NULL ) {
		if ( Options::useAltAz() ) {
			//Tracking on empty sky in Alt/Az mode
			setFocus( focusPoint() );
			focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			setDestination( focus() );
		}
	} else if ( ! slewing ) {
		//Not tracking and not slewing, let sky drift by
		if ( Options::useAltAz() ) {
			focus()->setAlt( destination()->alt()->Degrees() );
			focus()->setAz( destination()->az()->Degrees() );
			focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			//destination()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
		} else {
			focus()->setRA( data->LST->Hours() - data->HourAngle->Hours() );
			setDestination( focus() );
			focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			destination()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		}
	}

	//Update the Hour Angle
	data->setHourAngle( data->LST->Hours() - focus()->ra()->Hours() );

	setOldFocus( focus() );
	oldfocus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
}

void SkyMap::slewFocus( void ) {
	double dX, dY, fX, fY, r;
	double step = 1.0;
	SkyPoint newFocus;

//Don't slew if the mouse button is pressed
//Also, no animated slews if the Manual Clock is active
//08/2002: added possibility for one-time skipping of slew with snapNextFocus
	if ( !mouseButtonDown ) {
		bool goSlew = ( Options::useAnimatedSlewing() &&
			! data->snapNextFocus() ) &&
			!( data->clock()->isManualMode() && data->clock()->isActive() );
		if ( goSlew  ) {
			if ( Options::useAltAz() ) {
				dX = destination()->az()->Degrees() - focus()->az()->Degrees();
				dY = destination()->alt()->Degrees() - focus()->alt()->Degrees();
			} else {
				dX = destination()->ra()->Degrees() - focus()->ra()->Degrees();
				dY = destination()->dec()->Degrees() - focus()->dec()->Degrees();
			}

			//switch directions to go the short way around the celestial sphere, if necessary.
			if ( dX < -180.0 ) dX = 360.0 + dX;
			else if ( dX > 180.0 ) dX = -360.0 + dX;

			r = sqrt( dX*dX + dY*dY );

			while ( r > step ) {
				fX = dX / r;
				fY = dY / r;

				if ( Options::useAltAz() ) {
					focus()->setAlt( focus()->alt()->Degrees() + fY*step );
					focus()->setAz( dms( focus()->az()->Degrees() + fX*step ).reduce() );
					focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
				} else {
					fX = fX/15.; //convert RA degrees to hours
					newFocus.set( focus()->ra()->Hours() + fX*step, focus()->dec()->Degrees() + fY*step );
					setFocus( &newFocus );
					focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
				}

				slewing = true;
				//since we are slewing, fade out the transient label
				if ( transientObject() && ! TransientTimer.isActive() ) 
					fadeTransientLabel();
				
				forceUpdate();
				kapp->processEvents(10); //keep up with other stuff

				if ( Options::useAltAz() ) {
					dX = destination()->az()->Degrees() - focus()->az()->Degrees();
					dY = destination()->alt()->Degrees() - focus()->alt()->Degrees();
				} else {
					dX = destination()->ra()->Degrees() - focus()->ra()->Degrees();
					dY = destination()->dec()->Degrees() - focus()->dec()->Degrees();
				}

				//switch directions to go the short way around the celestial sphere, if necessary.
				if ( dX < -180.0 ) dX = 360.0 + dX;
				else if ( dX > 180.0 ) dX = -360.0 + dX;

				r = sqrt( dX*dX + dY*dY );
			}
		}

		//Either useAnimatedSlewing==false, or we have slewed, and are within one step of destination
		//set focus=destination.
		if ( Options::useAltAz() ) {
			setFocusAltAz( destination()->alt()->Degrees(), destination()->az()->Degrees() );
			focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
		} else {
			setFocus( destination() );
			focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		}
		
		data->HourAngle->setH( data->LST->Hours() - focus()->ra()->Hours() );
		slewing = false;

		//Turn off snapNextFocus, we only want it to happen once
		if ( data->snapNextFocus() ) {
			data->setSnapNextFocus(false);
		}

		//Start the HoverTimer. if the user leaves the mouse in place after a slew,
		//we want to attach a label to the nearest object.
		if ( Options::useHoverLabel() )
			HoverTimer.start( HOVER_INTERVAL, true );
		
		forceUpdate();
	}
}

void SkyMap::invokeKey( int key ) {
	QKeyEvent *e = new QKeyEvent( QEvent::KeyPress, key, 0, 0 );
	keyPressEvent( e );
	delete e;
}

double SkyMap::findPA( SkyObject *o, int x, int y, double scale ) {
	//Find position angle of North using a test point displaced to the north
	//displace by 100/zoomFactor radians (so distance is always 100 pixels)
	//this is 5730/zoomFactor degrees
	double newDec = o->dec()->Degrees() + 5730.0/Options::zoomFactor();
	if ( newDec > 90.0 ) newDec = 90.0;
	SkyPoint test( o->ra()->Hours(), newDec );
	if ( Options::useAltAz() ) test.EquatorialToHorizontal( data->LST, data->geo()->lat() );
	QPoint t = getXY( &test, Options::useAltAz(), Options::useRefraction(), scale );
	double dx = double( t.x() - x );  
	double dy = double( y - t.y() );  //backwards because QWidget Y-axis increases to the bottom
	double north;
	if ( dy ) {
		north = atan( dx/dy )*180.0/dms::PI;
		//resolve atan ambiguity:
		if ( dy < 0.0 ) north += 180.0;
		if ( north >= 360.0 ) north -= 360.;
	} else {
		north = 90.0;
		if ( dx > 0 ) north = -90.0;
	}

	return ( north + o->pa() );
}

QPoint SkyMap::getXY( SkyPoint *o, bool Horiz, bool doRefraction, double scale ) {
	QPoint p;

	double Y, dX;
	double sindX, cosdX, sinY, cosY, sinY0, cosY0;

	int Width = int( width() * scale );
	int Height = int( height() * scale );

	double pscale = Options::zoomFactor() * scale;

	if ( Horiz ) {
		if ( doRefraction ) Y = refract( o->alt(), true ).radians(); //account for atmospheric refraction
		else Y = o->alt()->radians();

		if ( focus()->az()->Degrees() > 270.0 && o->az()->Degrees() < 90.0 ) {
			dX = 2*dms::PI + focus()->az()->radians() - o->az()->radians();
		} else {
			dX = focus()->az()->radians() - o->az()->radians();
		}

		focus()->alt()->SinCos( sinY0, cosY0 );

  } else {
		if (focus()->ra()->Hours() > 18.0 && o->ra()->Hours() < 6.0) {
			dX = 2*dms::PI + o->ra()->radians() - focus()->ra()->radians();
		} else {
			dX = o->ra()->radians() - focus()->ra()->radians();
		}
    Y = o->dec()->radians();
		focus()->dec()->SinCos( sinY0, cosY0 );
  }

	//Convert dX, Y coords to screen pixel coords.
	#if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1 ) && !defined(__UCLIBC__)
	//GNU version
	sincos( dX, &sindX, &cosdX );
	sincos( Y, &sinY, &cosY );
	#else
	//ANSI version
	sindX = sin(dX);
	cosdX = cos(dX);
	sinY  = sin(Y);
	cosY  = cos(Y);
	#endif

	double c = sinY0*sinY + cosY0*cosY*cosdX;

	if ( c < 0.0 ) { //Object is on "back side" of the celestial sphere; don't plot it.
		p.setX( -10000000 );
		p.setY( -10000000 );
		return p;
	}

	double k = sqrt( 2.0/( 1 + c ) );

	p.setX( int( 0.5*Width  - pscale*k*cosY*sindX ) );
	p.setY( int( 0.5*Height - pscale*k*( cosY0*sinY - sinY0*cosY*cosdX ) ) );

	return p;
}

SkyPoint SkyMap::dXdYToRaDec( double dx, double dy, bool useAltAz, dms *LST, const dms *lat, bool doRefract ) {
	//Determine RA and Dec of a point, given (dx, dy): it's pixel
	//coordinates in the SkyMap with the center of the map as the origin.

	SkyPoint result;
	double sinDec, cosDec, sinDec0, cosDec0, sinc, cosc, sinlat, coslat;
	double xx, yy;

	double r  = sqrt( dx*dx + dy*dy );
	dms centerAngle;
	centerAngle.setRadians( 2.0*asin(0.5*r) );

	focus()->dec()->SinCos( sinDec0, cosDec0 );
	centerAngle.SinCos( sinc, cosc );

	if ( useAltAz ) {
		dms HA;
		dms Dec, alt, az, alt0, az0;
		double A;
		double sinAlt, cosAlt, sinAlt0, cosAlt0, sinAz, cosAz;
//		double HA0 = LST - focus.ra();
		az0 = focus()->az()->Degrees();
		alt0 = focus()->alt()->Degrees();
		alt0.SinCos( sinAlt0, cosAlt0 );

		dx = -dx; //Flip East-west (Az goes in opposite direction of RA)
		yy = dx*sinc;
		xx = r*cosAlt0*cosc - dy*sinAlt0*sinc;

		A = atan( yy/xx );
		//resolve ambiguity of atan():
		if ( xx<0 ) A = A + dms::PI;
//		if ( xx>0 && yy<0 ) A = A + 2.0*dms::PI;

		dms deltaAz;
		deltaAz.setRadians( A );
		az = focus()->az()->Degrees() + deltaAz.Degrees();
		alt.setRadians( asin( cosc*sinAlt0 + ( dy*sinc*cosAlt0 )/r ) );

		if ( doRefract ) alt.setD( refract( &alt, false ).Degrees() );  //find true altitude from apparent altitude

		az.SinCos( sinAz, cosAz );
		alt.SinCos( sinAlt, cosAlt );
		lat->SinCos( sinlat, coslat );

		Dec.setRadians( asin( sinAlt*sinlat + cosAlt*coslat*cosAz ) );
		Dec.SinCos( sinDec, cosDec );

		HA.setRadians( acos( ( sinAlt - sinlat*sinDec )/( coslat*cosDec ) ) );
		if ( sinAz > 0.0 ) HA.setH( 24.0 - HA.Hours() );

		result.setRA( LST->Hours() - HA.Hours() );
		result.setRA( result.ra()->reduce() );
		result.setDec( Dec.Degrees() );

		return result;

  } else {
		yy = dx*sinc;
		xx = r*cosDec0*cosc - dy*sinDec0*sinc;

		double RARad = ( atan( yy / xx ) );
		//resolve ambiguity of atan():
		if ( xx<0 ) RARad = RARad + dms::PI;
//		if ( xx>0 && yy<0 ) RARad = RARad + 2.0*dms::PI;

		dms deltaRA, Dec;
		deltaRA.setRadians( RARad );
		Dec.setRadians( asin( cosc*sinDec0 + (dy*sinc*cosDec0)/r ) );

		result.setRA( focus()->ra()->Hours() + deltaRA.Hours() );
		result.setRA( result.ra()->reduce() );
		result.setDec( Dec.Degrees() );

		return result;
	}
}

dms SkyMap::refract( const dms *alt, bool findApparent ) {
	if ( alt->Degrees() <= -2.000 ) return dms( alt->Degrees() );

	int index = int( ( alt->Degrees() + 2.0 )*2. );  //RefractCorr arrays start at alt=-2.0 degrees.
	dms result;

	//Failsafe: if the index is out of range, return the original angle
	if ( index < 0 || index > 183 ) {
		return dms( alt->Degrees() );
	}

	if ( findApparent ) {
		result.setD( alt->Degrees() + RefractCorr1[index] );
	} else {
		result.setD( alt->Degrees() + RefractCorr2[index] );
	}

	return result;
}

//---------------------------------------------------------------------------


// force a new calculation of the skymap (used instead of update(), which may skip the redraw)
// if now=true, SkyMap::paintEvent() is run immediately, rather than being added to the event queue
// also, determine new coordinates of mouse cursor.
void SkyMap::forceUpdate( bool now )
{
	QPoint mp( mapFromGlobal( QCursor::pos() ) );
	double dx = ( 0.5*width()  - mp.x() )/Options::zoomFactor();
	double dy = ( 0.5*height() - mp.y() )/Options::zoomFactor();

	if (! unusablePoint (dx, dy)) {
		//determine RA, Dec of mouse pointer
		setMousePoint( dXdYToRaDec( dx, dy, Options::useAltAz(), data->LST, data->geo()->lat(), Options::useRefraction() ) );
	}

	computeSkymap = true;
	if ( now ) repaint();
	else update();
}

float SkyMap::fov() {
	if ( width() >= height() ) 
		return 28.65*width()/Options::zoomFactor();
	else
		return 28.65*height()/Options::zoomFactor();
}

bool SkyMap::checkVisibility( SkyPoint *p, float FOV, double XMax ) {
	double dX, dY;
	bool useAltAz = Options::useAltAz();

	//Skip objects below the horizon if:
	// + using Horizontal coords,
	// + the ground is drawn,
	// + and either of the following is true:
	//   - focus is above the horizon
	//   - field of view is larger than 50 degrees
	if ( useAltAz && Options::showGround() && p->alt()->Degrees() < -2.0 
				&& ( focus()->alt()->Degrees() > 0. || FOV > 50. ) ) return false;

	if ( useAltAz ) {
		dY = fabs( p->alt()->Degrees() - focus()->alt()->Degrees() );
	} else {
		dY = fabs( p->dec()->Degrees() - focus()->dec()->Degrees() );
	}
	if ( isPoleVisible ) dY *= 0.75; //increase effective FOV when pole visible.
	if ( dY > FOV ) return false;
	if ( isPoleVisible ) return true;

	if ( useAltAz ) {
		dX = fabs( p->az()->Degrees() - focus()->az()->Degrees() );
	} else {
		dX = fabs( p->ra()->Degrees() - focus()->ra()->Degrees() );
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

void SkyMap::setZoomMouseCursor()
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

	p.drawEllipse( mx - 7, my - 7, 14, 14 );
	p.drawLine( mx + 5, my + 5, mx + 11, my + 11 );
	p.end();

// create a mask to make parts of the pixmap invisible
	QBitmap mask (32, 32);
	mask.fill (color0);	// all is invisible

	p.begin (&mask);
// paint over the parts which should be visible
	p.setPen (QPen (color1, 3));
	p.drawEllipse( mx - 7, my - 7, 14, 14 );
	p.drawLine( mx + 5, my + 5, mx + 12, my + 12 );
	p.end();
	
	cursorPix.setMask (mask);	// set the mask
	QCursor cursor (cursorPix);
	setCursor (cursor);
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
	AddLinkDialog adialog( this, clickedObject()->name() );
	QString entry;
  QFile file;

	if ( adialog.exec()==QDialog::Accepted ) {
		if ( adialog.isImageLink() ) {
			//Add link to object's ImageList, and descriptive text to its ImageTitle list
			clickedObject()->ImageList.append( adialog.url() );
			clickedObject()->ImageTitle.append( adialog.desc() );

			//Also, update the user's custom image links database
			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "image_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom image-links file could not be opened.\nLink cannot be recorded for future sessions." );
				KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.desc() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
				emit linkAdded();
			}
		} else {
			clickedObject()->InfoList.append( adialog.url() );
			clickedObject()->InfoTitle.append( adialog.desc() );

			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "info_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions." );						KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.desc() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
				emit linkAdded();
			}
		}
	}
}

void SkyMap::updateAngleRuler() {
	if ( Options::useAltAz() ) PreviousClickedPoint.EquatorialToHorizontal( data->LST, data->geo()->lat() );
	beginRulerPoint = getXY( previousClickedPoint(), Options::useAltAz(), Options::useRefraction() );

//	endRulerPoint =  QPoint(e->x(), e->y());
	endRulerPoint = mapFromGlobal( QCursor::pos() );
}

#include "skymap.moc"
