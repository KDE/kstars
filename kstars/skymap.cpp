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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <kconfig.h>
#include <kiconloader.h>
#include <kstatusbar.h>
#include <kmessagebox.h>

#include <qcursor.h>
#include <qbitmap.h>

#include <math.h>
#include <stdlib.h>
#include <unistd.h>

#include "detaildialog.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "imageviewer.h"
#include "infoboxes.h"
#include "addlinkdialog.h"
#include "kspopupmenu.h"
#include "skymap.h"

#include <kapplication.h>
#include <qmemarray.h>

SkyMap::SkyMap(KStarsData *d, QWidget *parent, const char *name )
 : QWidget (parent,name), data(d), computeSkymap (true), IBoxes(0), ClickedObject(0), FocusObject(0)
{
	if ( parent ) ksw = (KStars*) parent->parent();
	else ksw = 0;
	pts = new QPointArray( 2000 );  // points for milkyway and horizon
	sp = new SkyPoint();            // needed by coordinate grid

//DEBUG
	dumpHorizon = false;
//END_DEBUG

	setDefaultMouseCursor();	// set the cross cursor

	//read ZoomFactor from config object
	//JH: this has already been done by KStarsData::loadOptions()
	//kapp->config()->setGroup( "View" );
	//data->options->ZoomFactor = kapp->config()->readDoubleNumEntry( "ZoomFactor", MINZOOM );
	//if ( zoomFactor() > MAXZOOM ) data->options->ZoomFactor = MAXZOOM;
	//if ( zoomFactor() < MINZOOM ) data->options->ZoomFactor = MINZOOM;

	// load the pixmaps of stars
	starpix = new StarPixmap( data->options->colorScheme()->starColorMode(), data->options->colorScheme()->starColorIntensity() );

	setBackgroundColor( QColor( data->options->colorScheme()->colorNamed( "SkyColor" ) ) );
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
	pmenu = new KSPopupMenu( ksw );
	IBoxes = new InfoBoxes( data->options->windowWidth, data->options->windowHeight,
			data->options->posTimeBox, data->options->shadeTimeBox,
			data->options->posGeoBox, data->options->shadeGeoBox,
			data->options->posFocusBox, data->options->shadeFocusBox,
			data->options->colorScheme()->colorNamed( "BoxTextColor" ),
			data->options->colorScheme()->colorNamed( "BoxGrabColor" ),
			data->options->colorScheme()->colorNamed( "BoxBGColor" ) );

	IBoxes->showTimeBox( data->options->showTimeBox );
	IBoxes->showFocusBox( data->options->showFocusBox );
	IBoxes->showGeoBox( data->options->showGeoBox );
	IBoxes->timeBox()->setAnchorFlag( data->options->stickyTimeBox );
	IBoxes->geoBox()->setAnchorFlag( data->options->stickyGeoBox );
	IBoxes->focusBox()->setAnchorFlag( data->options->stickyFocusBox );

	IBoxes->geoChanged( data->options->Location() );

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
		double alt = -1.75 + index*0.5;  //start at -0.75 degrees to get midpoint value for each interval.

		RefractCorr1[index] = 1.02 / tan( dms::PI*( alt + 10.3/(alt + 5.11) )/180.0 ) / 60.0; //correction in degrees.
		RefractCorr2[index] = -1.0 / tan( dms::PI*( alt + 7.31/(alt + 4.4) )/180.0 ) / 60.0;
	}
}

SkyMap::~SkyMap() {
	if ( starpix ) delete starpix;
	if ( pts ) delete pts;
	if ( sp ) delete sp;
	if ( sky ) delete sky;
	if ( pmenu ) delete pmenu;
	if ( IBoxes ) delete IBoxes;

//delete any remaining object Image pointers
	for ( SkyObject *obj = data->deepSkyListMessier.first(); obj; obj = data->deepSkyListMessier.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( SkyObject *obj = data->deepSkyListNGC.first(); obj; obj = data->deepSkyListNGC.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( SkyObject *obj = data->deepSkyListIC.first(); obj; obj = data->deepSkyListIC.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
	for ( SkyObject *obj = data->deepSkyListOther.first(); obj; obj = data->deepSkyListOther.next() ) {
		if ( obj->image() ) obj->deleteImage();
  }
}

void SkyMap::setGeometry( int x, int y, int w, int h ) {
	QWidget::setGeometry( x, y, w, h );
	sky->resize( w, h );
}

void SkyMap::setGeometry( const QRect &r ) {
	QWidget::setGeometry( r );
	sky->resize( r.width(), r.height() );
}


void SkyMap::showFocusCoords( void ) {
	//display object info in infoBoxes
	QString oname;

	oname = i18n( "nothing" );
	if ( focusObject() != NULL && data->options->isTracking ) {
		oname = focusObject()->translatedName();
		//add genetive name for stars
	  if ( focusObject()->type()==0 && focusObject()->name2().length() )
			oname += " (" + focusObject()->name2() + ")";
	}

	infoBoxes()->focusObjChanged(oname);

	if ( data->options->useAltAz && data->options->useRefraction ) {
		SkyPoint corrFocus( *(focus()) );
		corrFocus.setAlt( refract( focus()->alt(), false ) );
		corrFocus.HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
		infoBoxes()->focusCoordChanged( &corrFocus );
	} else {
		infoBoxes()->focusCoordChanged( focus() );
	}
}

//Slots
void SkyMap::slotCenter( void ) {
	clickedPoint()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );

	//clear the planet trail of focusObject, if it was temporary
	if ( data->isSolarSystem( focusObject() ) && data->temporaryTrail ) {
		((KSPlanetBase*)focusObject())->clearTrail();
		data->temporaryTrail = false;
	}

//If the requested object is below the opaque horizon, issue a warning message
//(unless user is already pointed below the horizon)
	if ( data->options->useAltAz && data->options->drawGround &&
			focus()->alt()->Degrees() > -1.0 && clickedPoint()->alt()->Degrees() < -1.0 ) {

		QString caption = i18n( "Requested Position Below Horizon" );
		QString message = i18n( "The requested position is below the horizon.\nWould you like to go there anyway?" );
		if ( KMessageBox::warningYesNo( this, message, caption,
				KStdGuiItem::yes(), KStdGuiItem::no(), "dag_focus_below_horiz" )==KMessageBox::No ) {
			setClickedObject( NULL );
			setFocusObject( NULL );
			data->options->isTracking = false;

			return;
		}
	}

//set FocusObject before slewing.  Otherwise, KStarsData::updateTime() can reset
//destination to previous object...
	setFocusObject( ClickedObject );
	data->options->isTracking = true;
	if ( ksw ) ksw->actionCollection()->action("track_object")->setIconSet( BarIcon( "encrypted" ) );

	//If focusObject is a SS body and doesn't already have a trail, set the temporaryTrail
	if ( focusObject() && data->isSolarSystem( focusObject() )
			&& data->options->useAutoTrail
			&& ! ((KSPlanetBase*)focusObject())->hasTrail() ) {
		((KSPlanetBase*)focusObject())->addToTrail();
		data->temporaryTrail = true;
	}

//update the destination to the selected coordinates
	if ( data->options->useAltAz && data->options->useRefraction ) { //correct for atmospheric refraction if using horizontal coords
		setDestinationAltAz( refract( clickedPoint()->alt(), true ).Degrees(), clickedPoint()->az()->Degrees() );
	} else {
		setDestination( clickedPoint() );
		destination()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
	}

	//display coordinates in statusBar
	QString sRA, sDec, s;
	char dsgn = '+';

	if ( clickedPoint()->dec()->Degrees() < 0 ) dsgn = '-';
	int dd = abs( clickedPoint()->dec()->degree() );
	int dm = abs( clickedPoint()->dec()->arcmin() );
	int ds = abs( clickedPoint()->dec()->arcsec() );

	sRA = sRA.sprintf( "%02d:%02d:%02d", clickedPoint()->ra()->hour(), clickedPoint()->ra()->minute(), clickedPoint()->ra()->second() );
	sDec = sDec.sprintf( "%c%02d:%02d:%02d", dsgn, dd, dm, ds );
	s = sRA + ",  " + sDec;
	if ( ksw ) ksw->statusBar()->changeItem( s, 1 );

	showFocusCoords(); //update FocusBox
//	forceUpdate();	// must be new computed
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
		clickedPoint()->precessFromAnyEpoch( data->CurrentDate, J2000 );
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
	new ImageViewer (&url, this);
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
		clickedPoint()->precessFromAnyEpoch( data->CurrentDate, J2000 );
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
	forceUpdate();
}

void SkyMap::slotRemovePlanetTrail( void ) {
	//probably don't need this if-statement, but just to be sure...
	if ( data->isSolarSystem( clickedObject() ) ) {
		((KSPlanetBase*)clickedObject())->clearTrail();
		forceUpdate();
	}
}

void SkyMap::slotAddPlanetTrail( void ) {
	//probably don't need this if-statement, but just to be sure...
	if ( data->isSolarSystem( clickedObject() ) ) {
		((KSPlanetBase*)clickedObject())->addToTrail();
		forceUpdate();
	}
}

void SkyMap::slotDetail( void ) {
// check if object is selected
	if ( !clickedObject() ) {
    	KMessageBox::sorry( this, i18n("No Object selected!"), i18n("Object Details") );
		return;
   }
	DetailDialog detail( clickedObject(), data->LTime, data->options->Location(), ksw );
	detail.exec();
}

void SkyMap::slotClockSlewing() {
//If the current timescale exceeds slewTimeScale, set clockSlewing=true, and stop the clock.
	if ( fabs( data->clock->scale() ) > data->options->slewTimeScale ) {
		if ( ! clockSlewing ) {
			clockSlewing = true;
			data->clock->setManualMode( true );

			// don't change automatically the DST status
			if ( ksw ) ksw->updateTime( false );
		}
	} else {
		if ( clockSlewing ) {
			clockSlewing = false;
			data->clock->setManualMode( false );

			// don't change automatically the DST status
			if ( ksw ) ksw->updateTime( false );
		}
	}
}

void SkyMap::setFocusAltAz(double alt, double az) {
	focus()->setAlt(alt);
	focus()->setAz(az);
	focus()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
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
	destination()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
	emit destinationChanged();
}

void SkyMap::setDestinationAltAz(double alt, double az) {
	destination()->setAlt(alt);
	destination()->setAz(az);
	destination()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
	emit destinationChanged();
}

void SkyMap::updateFocus() {
	if ( data->options->isTracking && focusObject() != NULL ) {
		if ( data->options->useAltAz ) {
			//Tracking any object in Alt/Az mode requires focus updates
			setDestinationAltAz(
					refract( focusObject()->alt(), true ).Degrees(),
					focusObject()->az()->Degrees() );
			destination()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
			setFocus( destination() );
		} else {
			//Tracking in equatorial coords
			setDestination( focusObject() );
			setFocus( destination() );
			focus()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
		}
	} else if ( data->options->isTracking ) {
		if ( data->options->useAltAz ) {
			//Tracking on empty sky in Alt/Az mode
			setDestination( clickedPoint() );
			destination()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
			setFocus( destination() );
		}
	} else if ( ! isSlewing() ) {
		//Not tracking and not slewing, let sky drift by
		if ( data->options->useAltAz ) {
			focus()->setAlt( destination()->alt()->Degrees() );
			focus()->setAz( destination()->az()->Degrees() );
			focus()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
			//destination()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
		} else {
			focus()->setRA( data->LST->Hours() - data->HourAngle->Hours() );
			setDestination( focus() );
			focus()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
			destination()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
		}
	}

	setOldFocus( focus() );
	oldfocus()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
}

void SkyMap::slewFocus( void ) {
	double dX, dY, fX, fY, r;
	double step = 1.0;
	SkyPoint newFocus;

//Don't slew if the mouse button is pressed
//Also, no animated slews if the Manual Clock is active
//08/2002: added possibility for one-time skipping of slew with snapNextFocus
	if ( !mouseButtonDown ) {
		bool goSlew = ( data->options->useAnimatedSlewing &&
			! data->options->snapNextFocus() ) &&
			!( data->clock->isManualMode() && data->clock->isActive() );
		if ( goSlew  ) {
			if ( data->options->useAltAz ) {
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

				if ( data->options->useAltAz ) {
					focus()->setAlt( focus()->alt()->Degrees() + fY*step );
					focus()->setAz( focus()->az()->Degrees() + fX*step );
					focus()->HorizontalToEquatorial( data->LST, data->options->Location()->lat() );
				} else {
					fX = fX/15.; //convert RA degrees to hours
					newFocus.set( focus()->ra()->Hours() + fX*step, focus()->dec()->Degrees() + fY*step );
					setFocus( &newFocus );
					focus()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
				}

				slewing = true;
				forceUpdate();
				kapp->processEvents(10); //keep up with other stuff

				if ( data->options->useAltAz ) {
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
		setFocus( destination() );
		focus()->EquatorialToHorizontal( data->LST, data->options->Location()->lat() );

		if ( focusObject() )
			infoBoxes()->focusObjChanged( focusObject()->translatedName() );

		infoBoxes()->focusCoordChanged( focus() );

		data->HourAngle->setH( data->LST->Hours() - focus()->ra()->Hours() );
		slewing = false;

		//Turn off snapNextFocus, we only want it to happen once
		if ( data->options->snapNextFocus() ) {
			data->options->setSnapNextFocus(false);
		}

		forceUpdate();
	}
}

void SkyMap::invokeKey( int key ) {
	QKeyEvent *e = new QKeyEvent( QEvent::KeyPress, key, 0, 0 );
	keyPressEvent( e );
	delete e;
}

int SkyMap::findPA( SkyObject *o, int x, int y ) {
//	//no need for position angle for stars or open clusters
//	if ( o->type() == 0 || o->type() == 1 || o->type() == 3 ) return 0;

	//Find position angle of North using a test point displaced to the north
	//displace by 100/zoomFactor radians (so distance is always 100 pixels)
	//this is 5730/zoomFactor degrees
	double newDec = o->dec()->Degrees() + 5730.0/zoomFactor();
	if ( newDec > 90.0 ) newDec = 90.0;
	SkyPoint test( o->ra()->Hours(), newDec );
	if ( data->options->useAltAz ) test.EquatorialToHorizontal( data->LST, data->options->Location()->lat() );
	QPoint t = getXY( &test, data->options->useAltAz, data->options->useRefraction );
	double dx = double( x - t.x() );  //backwards to get counterclockwise angle
	double dy = double( t.y() - y );
	double north;
	if ( dy ) {
		north = atan( dx/dy )*180.0/dms::PI;
	} else {
		north = 90.0;
		if ( dx > 0 ) north = -90.0;
	}

	int pa( 90 + int( north ) - o->pa() );
	return pa;
}

QPoint SkyMap::getXY( SkyPoint *o, bool Horiz, bool doRefraction, double scale ) {
	QPoint p;

	double Y, dX;
	double sindX, cosdX, sinY, cosY, sinY0, cosY0;

	int Width = int( width() * scale );
	int Height = int( height() * scale );

	double pscale = zoomFactor() * scale;

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
	#if ( __GLIBC__ >= 2 && __GLIBC_MINOR__ >=1 )
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

	if ( findApparent ) {
		result.setD( alt->Degrees() + RefractCorr1[index] );
	} else {
		result.setD( alt->Degrees() + RefractCorr2[index] );
	}

	return result;
}

//---------------------------------------------------------------------------


// force a new calculation of the skymap (used instead of update(), which may skip the redraw)
//if now=true, SkyMap::paintEvent() is run immediately, rather than being added to the event queue
void SkyMap::forceUpdate( bool now )
{
	computeSkymap = true;
	if ( now ) repaint();
	else update();
}

float SkyMap::fov( void ) {
	return 32.*width()/zoomFactor();
}

bool SkyMap::checkVisibility( SkyPoint *p, float FOV, double XMax ) {
	double dX, dY;

//Skip objects below the horizon if the ground is drawn.
//commented out because ground disappears if it fills the view
//	if ( useAltAz && drawGround && p->alt()->Degrees() < -2.0 ) return false;

	bool useAltAz = data->options->useAltAz;

	if ( useAltAz ) {
		dY = fabs( p->alt()->Degrees() - focus()->alt()->Degrees() );
	} else {
		dY = fabs( p->dec()->Degrees() - focus()->dec()->Degrees() );
	}
	if ( dY > FOV ) return false;
	if ( isPoleVisible ) return true;

	//XMax is now computed once in SkyMap::paintEvent()
	if ( useAltAz ) {
		dX = fabs( p->az()->Degrees() - focus()->az()->Degrees() );
		//XMax = 1.2*FOV/cos( focus()->alt()->radians() );
	} else {
		dX = fabs( p->ra()->Degrees() - focus()->ra()->Degrees() );
		//XMax = 1.2*FOV/cos( focus()->dec()->radians() );
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
				KMessageBox::sorry( 0, message, i18n( "Could Not Open File" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
              emit linkAdded();
      }
		} else {
			clickedObject()->InfoList.append( adialog.url() );
			clickedObject()->InfoTitle.append( adialog.title() );

			//check for user's image-links database.  If it doesn't exist, create it.
			file.setName( locateLocal( "appdata", "myinfo_url.dat" ) ); //determine filename in local user KDE directory tree.

			if ( !file.open( IO_ReadWrite | IO_Append ) ) {
				QString message = i18n( "Custom information-links file could not be opened.\nLink cannot be recorded for future sessions." );						KMessageBox::sorry( 0, message, i18n( "Could not Open File" ) );
				return;
			} else {
				entry = clickedObject()->name() + ":" + adialog.title() + ":" + adialog.url();
				QTextStream stream( &file );
				stream << entry << endl;
				file.close();
              emit linkAdded();
      }
		}
	}
}

double SkyMap::zoomFactor() const {
	return data->options->ZoomFactor;
}
bool SkyMap::setColors( QString filename ) {
	if ( data->options->colorScheme()->load( filename ) ) {
		if ( starColorMode() != data->options->colorScheme()->starColorMode() )
			setStarColorMode( data->options->colorScheme()->starColorMode() );

		forceUpdate();
		return true;
	} else {
		return false;
	}
}

#include "skymap.moc"
