/***************************************************************************
                          skymapevents.cpp  -  K Desktop Planetarium
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

//This file contains Event handlers for the SkyMap class.

#include <qcursor.h>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qtextstream.h>
#include <qtimer.h>

#include <kiconloader.h>
#include <kstatusbar.h>

#include <stdlib.h>
#include <math.h> //using fabs()

#include "kstars.h"
#include "ksutils.h"
#include "skymap.h"

void SkyMap::resizeEvent( QResizeEvent * )
{
    computeSkymap = true;	// skymap must be new computed
    if ( testWState(WState_AutoMask) )
		updateMask();
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

//			setDestination( focus() );
			slewing = true;
			++scrollCount;
			break;

		case Key_Right :
			if ( ksw->options()->useAltAz ) {
				focus()->setAz( focus()->az().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra().Hours() - 0.05*step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}

//			setDestination( focus() );
			slewing = true;
			++scrollCount;
			break;

		case Key_Up :
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if ( focus()->alt().Degrees() > 90.0 ) focus()->setAlt( 89.99 );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec().Degrees() + step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if (focus()->dec().Degrees() > 90.0) focus()->setDec( 90.0 );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}

//			setDestination( focus() );
			slewing = true;
			++scrollCount;
			break;

		case Key_Down:
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt().Degrees() - step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if ( focus()->alt().Degrees() < -90.0 ) focus()->setAlt( -90.0 );
				focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec().Degrees() - step * pixelScale[0]/pixelScale[ ksw->data()->ZoomLevel ] );
				if (focus()->dec().Degrees() < -90.0) focus()->setDec( -90.0 );
				focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			}

//			setDestination( focus() );
			slewing = true;
			++scrollCount;
			break;

		case Key_Plus:   //Zoom in
		case Key_Equal:
			ksw->slotZoomIn();
			break;

		case Key_Minus:  //Zoom out
		case Key_Underscore:
			ksw->slotZoomOut();
			break;

//In the following cases, we set slewing=true in order to disengage tracking
		case Key_N: //center on north horizon
			setClickedObject( NULL );
			clickedPoint()->setAlt( 15.0 ); clickedPoint()->setAz( 0.0 );
			clickedPoint()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slotCenter();
			break;

		case Key_E: //center on east horizon
			setClickedObject( NULL );
			clickedPoint()->setAlt( 15.0 ); clickedPoint()->setAz( 90.0 );
			clickedPoint()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slotCenter();
			break;

		case Key_S: //center on south horizon
			setClickedObject( NULL );
			clickedPoint()->setAlt( 15.0 ); clickedPoint()->setAz( 180.0 );
			clickedPoint()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slotCenter();
			break;

		case Key_W: //center on west horizon
			setClickedObject( NULL );
			clickedPoint()->setAlt( 15.0 ); clickedPoint()->setAz( 270.0 );
			clickedPoint()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slotCenter();
			break;

		case Key_Z: //center on Zenith
			setClickedObject( NULL );
			clickedPoint()->setAlt( 90.0 ); clickedPoint()->setAz( 180.0 );
			clickedPoint()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
			slotCenter();
			break;

		case Key_0: //center on Sun
			setClickedObject( ksw->data()->PC.findByName("Sun") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_1: //center on Mercury
			setClickedObject( ksw->data()->PC.findByName("Mercury") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_2: //center on Venus
			setClickedObject( ksw->data()->PC.findByName("Venus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_3: //center on Moon
			setClickedObject( ksw->data()->Moon );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_4: //center on Mars
			setClickedObject( ksw->data()->PC.findByName("Mars") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_5: //center on Jupiter
			setClickedObject( ksw->data()->PC.findByName("Jupiter") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_6: //center on Saturn
			setClickedObject( ksw->data()->PC.findByName("Saturn") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_7: //center on Uranus
			setClickedObject( ksw->data()->PC.findByName("Uranus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_8: //center on Neptune
			setClickedObject( ksw->data()->PC.findByName("Neptune") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_9: //center on Pluto
			setClickedObject( ksw->data()->PC.findByName("Pluto") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

//DEBUG
		case Key_X: //Spit out coords of horizon polygon
			dumpHorizon = true;
			break;
//END_DEBUG

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
  	  ksw->actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );
		}

		if ( scrollCount > 10 ) {
//			ksw->showFocusCoords();
			setDestination( focus() );
			scrollCount = 0;
		}
	}

	Update(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Key_Left :  //no break; continue to Key_Down
		case Key_Right :  //no break; continue to Key_Down
		case Key_Up :  //no break; continue to Key_Down
		case Key_Down :
			slewing = false;
			scrollCount = 0;
			ksw->showFocusCoords();
			Update();	// Need a full update to draw faint objects that are not drawn while slewing.
			break;
	}
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->data()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->data()->ZoomLevel ];
	double dyPix = 0.5*height() - e->y();

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	//determine RA, Dec of mouse pointer
	setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );

	if ( midMouseButtonDown ) { //zoom according to y-offset
		float yoff = dyPix - y0;

		if (yoff > 10 ) {
			y0 = dyPix;
			ksw->slotZoomIn();
		}
		if (yoff < -10 ) {
			y0 = dyPix;
			ksw->slotZoomOut();
		}
	}

	if ( mouseButtonDown ) {
		// set the mouseMoveCursor and set slewing=true, if they are not set yet
		if (!mouseMoveCursor) setMouseMoveCursor();
		if (!slewing) {
			slewing = true;
			ksw->options()->isTracking = false; //break tracking on slew
			ksw->actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );
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

			if ( focus()->alt().Degrees() >90.0 ) focus()->setAlt( 90.0 );
			if ( focus()->alt().Degrees() <-90.0 ) focus()->setAlt( -90.0 );
			focus()->setAz( focus()->az().reduce() );
			focus()->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
		} else {
			dms dRA = mousePoint()->ra().Degrees() - clickedPoint()->ra().Degrees();
			dms dDec = mousePoint()->dec().Degrees() - clickedPoint()->dec().Degrees();
			focus()->setRA( focus()->ra().Hours() - dRA.Hours() ); //move focus in opposite direction
			focus()->setDec( focus()->dec().Degrees() - dDec.Degrees() );

			if ( focus()->dec().Degrees() >90.0 ) focus()->setDec( 90.0 );
			if ( focus()->dec().Degrees() <-90.0 ) focus()->setDec( -90.0 );
			focus()->setRA( focus()->ra().reduce() );
			focus()->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
		}

		++scrollCount;
		if ( scrollCount > 4 ) {
			ksw->showFocusCoords();
			scrollCount = 0;
		}

		setOldFocus( focus() );

		double dHA = ksw->data()->LSTh.Hours() - focus()->ra().Hours();
		while ( dHA < 0.0 ) dHA += 24.0;
		ksw->data()->HourAngle.setH( dHA );

		//redetermine RA, Dec of mouse pointer, using new focus
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );
		setClickedPoint( mousePoint() );

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

void SkyMap::wheelEvent( QWheelEvent *e ) {
	if ( e->delta() > 0 ) ksw->slotZoomIn();
	else ksw->slotZoomOut();
}

void SkyMap::mouseReleaseEvent( QMouseEvent *e ) {
	if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
	if (mouseButtonDown) { //false if double-clicked, becuase it's unset there.
		mouseButtonDown = false;
		setDestination( focus() );
		slewing = false;
		setOldFocus( focus() );
		Update();	// is needed because after moving the sky not all stars are shown
	}
	if ( midMouseButtonDown ) midMouseButtonDown = false;  // if middle button was pressed unset here

	scrollCount = 0;
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
// if button is down and cursor is not moved set the move cursor after 500 ms
	QTimer t;
	t.singleShot (500, this, SLOT (setMouseMoveCursor()));

	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->data()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->data()->ZoomLevel ];
	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	if ( !midMouseButtonDown && e->button() == MidButton ) {
		y0 = 0.5*height() - e->y();  //record y pixel coordinate for middle-button zooming
		midMouseButtonDown = true;
	}

	if ( !mouseButtonDown ) {
		if ( e->button()==LeftButton ) {
			mouseButtonDown = true;
			scrollCount = 0;
		}

		//determine RA, Dec of mouse pointer
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );
		setClickedPoint( mousePoint() );

		double r0 = 200.0/pixelScale[ ksw->data()->ZoomLevel ];  //the maximum search radius
		double rmin = r0;

		//Search stars database for nearby object.
		double rstar_min = r0;
		double starmag_min = 20.0;      //absurd initial value
		int istar_min = -1;

		if ( ksw->options()->drawSAO ) { //Can only click on a star if it's being drawn!
			for ( unsigned int i=0; i<ksw->data()->starList.count(); ++i ) {
				//test RA and dec to see if this star is roughly nearby
				SkyObject *test = (SkyObject *)ksw->data()->starList.at(i);
				double dRA = test->ra().Hours() - clickedPoint()->ra().Hours();
				double dDec = test->dec().Degrees() - clickedPoint()->dec().Degrees();
				//determine angular distance between this object and mouse cursor
				double f = 15.0*cos( test->dec().radians() );
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
		SkyObject *solarminobj = ksw->data()->PC.findClosest(clickedPoint(), r);

		if ( r < r0 ) {
			rsolar_min = r;
		} else {
			solarminobj = NULL;
		}

		//Moon
		if ( ksw->options()->drawMoon ) {
			double dRA = ksw->data()->Moon->ra().Hours() - clickedPoint()->ra().Hours();
			double dDec = ksw->data()->Moon->dec().Degrees() - clickedPoint()->dec().Degrees();
			double f = 15.0*cos( ksw->data()->Moon->dec().radians() );
			r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if (r < rsolar_min) {
				solarminobj= ksw->data()->Moon;
				rsolar_min = r;
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

		for ( SkyObject *o = ksw->data()->deepSkyList.first(); o; o = ksw->data()->deepSkyList.next() ) {
			bool checkObject = false;
			if ( o->catalog() == "M" &&
					( ksw->options()->drawMessier || ksw->options()->drawMessImages ) ) checkObject = true;
			if ( o->catalog() == "NGC" && ksw->options()->drawNGC ) checkObject = true;
			if ( o->catalog() == "IC" && ksw->options()->drawIC ) checkObject = true;
			if ( o->catalog().isEmpty() && ksw->options()->drawOther ) checkObject = true;

			if ( checkObject ) {
				//test RA and dec to see if this star is roughly nearby
				double dRA = o->ra().Hours() - clickedPoint()->ra().Hours();
				double dDec = o->dec().Degrees() - clickedPoint()->dec().Degrees();
				double f = 15.0*cos( o->dec().radians() );
				double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if ( o->catalog() == "M" && r < rmess_min) {
					imess_min = ksw->data()->deepSkyList.at();
					rmess_min = r;
				}
				if ( o->catalog() == "NGC" && r < rngc_min) {
					ingc_min = ksw->data()->deepSkyList.at();
					rngc_min = r;
				}
				if ( o->catalog() == "IC" && r < ric_min) {
					iic_min = ksw->data()->deepSkyList.at();
					ric_min = r;
				}
				if ( o->catalog().isEmpty() && r < rother_min) {
					iother_min = ksw->data()->deepSkyList.at();
					rother_min = r;
				}
			}
		}

//Next, search for nearest object within r0 among the custom catalogs
		double rcust_min = r0;
		int icust_min = -1;
		int icust_cat = -1;

		for ( unsigned int j=0; j<ksw->options()->CatalogCount; ++j ) {
			if ( ksw->options()->drawCatalog[j] ) {
				QList<SkyObject> cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[j] ];

				for ( unsigned int i=0; i<cat.count(); ++i ) {
					//test RA and dec to see if this object is roughly nearby
					SkyObject *test = (SkyObject *)cat.at(i);
					double dRA = test->ra().Hours()-clickedPoint()->ra().Hours();
					double dDec = test->dec().Degrees()-clickedPoint()->dec().Degrees();
					double f = 15.0*cos( test->dec().radians() );
					double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
					if (r < rcust_min) {
						icust_cat = j;
						icust_min = i;
						rcust_min = r;
					}
				}
			}
		}

		initPopupMenu();
		QStringList::Iterator itList;
		QStringList::Iterator itTitle;
		QString s, DisplayName;
		int id, jmin(-1);
		int icat(-1);
		setClickedObject( NULL );
		StarObject *starobj = NULL;

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

		if ( icat >= 0 && rmin < r0 ) { //was any object found within r0?

			QList<SkyObject> cat;

			switch (icat) {
				case 0: //star
					starobj = (StarObject *)ksw->data()->starList.at(istar_min);
					setClickedObject( (SkyObject *)ksw->data()->starList.at(istar_min) );
					setClickedPoint( clickedObject() );

					//set DisplayName to be either the Arabic name, the genetive name, or both
					DisplayName = clickedObject()->translatedName();
					if ( clickedObject()->name2().length() ) {
						if ( DisplayName.length() ) {
							DisplayName += " (" + clickedObject()->name2() + ")";
						} else {
							DisplayName = clickedObject()->name2();
						}
					}

					if ( e->button() == RightButton ) {
						pmTitle->setText( DisplayName );
						pmTitle2->setText( i18n( "Spectral Type: %1" ).arg(starobj->sptype()) );
						if ( clickedObject()->name() != "star" ) {
							pmType->setText( i18n( "star" ) );
						} else {
							pmType->setText( QString::null );
						}

						setRiseSetLabels();

//					If the star is named, add custom items to popup menu based on object's ImageList and InfoList
						if ( clickedObject()->name() != "star" ) {
							itList  = clickedObject()->ImageList.begin();
							itTitle = clickedObject()->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotImage( int ) ), 0, id++ );
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
								pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}

							pmenu->insertSeparator();
							pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );
						} else {
							pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
							pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
						}

						pmenu->popup( QCursor::pos() );
					}
					break;

				case 1: //Deep-Sky Objects
					setClickedObject( (SkyObject *)ksw->data()->deepSkyList.at(jmin) );
					setClickedPoint( clickedObject());
					DisplayName = clickedObject()->translatedName();

					if (e->button() == RightButton ) {
						pmTitle->setText( clickedObject()->translatedName() );
						if ( !clickedObject()->longname().isEmpty() ) {
							pmTitle2->setText( i18n( clickedObject()->longname().local8Bit() ) );
						} else if ( !clickedObject()->name2().isEmpty() ) {
							pmTitle2->setText( i18n( clickedObject()->name2().local8Bit() ) );
						}
						pmType->setText( ksw->data()->TypeName[ clickedObject()->type() ] );

						setRiseSetLabels();

						//Add custom items to popup menu based on object's ImageList and InfoList
						itList  = clickedObject()->ImageList.begin();
						itTitle = clickedObject()->ImageTitle.begin();

						id = 100;
						for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
							QString t = QString(*itTitle);
							sURL = QString(*itList);
							pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotImage( int ) ), 0, id++ );
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
							pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotInfo( int ) ), 0, id++ );
							++itTitle;
						}

						pmenu->insertSeparator();
						pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

						pmenu->popup( QCursor::pos() );
					}
					break;

				case 2: //Custom Catalog Object
					cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[icust_cat] ];
					setClickedObject( (SkyObject *)cat.at(icust_min) );
					setClickedPoint( clickedObject() );
					DisplayName = clickedObject()->translatedName();

					if ( e->button() == RightButton ) {
						pmTitle->setText( clickedObject()->translatedName() );
						if ( !clickedObject()->longname().isEmpty() ) {
							pmTitle2->setText( i18n( clickedObject()->longname().local8Bit() ) );
						} else if ( !clickedObject()->name2().isEmpty() ) {
							pmTitle2->setText( i18n( clickedObject()->name2().local8Bit() ) );
						}
						pmType->setText( ksw->data()->TypeName[ clickedObject()->type() ] );

						setRiseSetLabels();

/* images and info links temporarily disabled...
//					Add custom items to popup menu based on object's ImageList and InfoList
							itList  = clickedObject()->ImageList.begin();
							itTitle = clickedObject()->ImageTitle.begin();

							id = 100;
							for ( ; itList != clickedObject()->ImageList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotImage( int ) ), 0, id++ );
								++itTitle;
							}
*/
						pmenu->insertItem( i18n( "First Generation Digitized Sky Survey", "Show 1st-Gen DSS image" ), this, SLOT( slotDSS() ) );
						pmenu->insertItem( i18n( "Second Generation Digitized Sky Survey", "Show 2nd-Gen DSS image" ), this, SLOT( slotDSS2() ) );
						if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

/*
							itList  = clickedObject()->InfoList.begin();
							itTitle = clickedObject()->InfoTitle.begin();

							id = 200;
							for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
								QString t = QString(*itTitle);
								sURL = QString(*itList);
								pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotInfo( int ) ), 0, id++ );
								++itTitle;
							}
*/

						pmenu->insertSeparator();
						pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );
						pmenu->setItemEnabled( id-1, false ); //disable adding links to xutom objects for now

						pmenu->popup( QCursor::pos() );
					}
					break;

				case 3: //solar system object
					setClickedObject(solarminobj);
					if ( clickedObject() != NULL ) setClickedPoint( clickedObject() );
					DisplayName = clickedObject()->translatedName();

					if ( e->button() == RightButton ) {
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
							pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotImage( int ) ), 0, id++ );
						}

						if ( clickedObject()->ImageList.count() ) pmenu->insertSeparator();

						itList  = clickedObject()->InfoList.begin();
						itTitle = clickedObject()->InfoTitle.begin();

						id = 200;
						for ( ; itList != clickedObject()->InfoList.end(); ++itList ) {
							QString t = QString(*itTitle);
							sURL = QString(*itList);
							pmenu->insertItem( i18n( t.local8Bit() ), this, SLOT( slotInfo( int ) ), 0, id++ );
							++itTitle;
						}

						pmenu->insertSeparator();
						pmenu->insertItem( i18n( "Add Link..." ), this, SLOT( addLink() ), 0, id++ );

						pmenu->popup( QCursor::pos() );
					}
					break;
			}

			if ( e->button() == LeftButton ) {
				//Display name in status bar
				ksw->statusBar()->changeItem( DisplayName, 0 );

				//show label in skymap
//				labelClickedObject = true;
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

	//determine RA, Dec of mouse pointer
	setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->data()->LSTh, ksw->geo()->lat() ) );
	setClickedPoint( mousePoint() );

	if (mouseButtonDown ) mouseButtonDown = false;
	if ( dx != 0.0 || dy != 0.0 )  slotCenter();
}

void SkyMap::drawPlanet(QPainter &psky, KSPlanetBase *p, QColor c,
		int sizemin, double mult, int zoommin, int resize_mult) {
	psky.setPen( c );
	psky.setBrush( c );
	QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );
	if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
		int size = int( mult * pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0] );
		if ( size < sizemin ) size = sizemin;
		int x1 = o.x() - size/2;
		int y1 = o.y() - size/2;

                                             //Only draw planet image if:
		if ( ksw->options()->drawPlanetImage &&  //user wants them,
				ksw->data()->ZoomLevel > zoommin &&  //zoomed in enough,
				!p->image()->isNull() &&             //image loaded ok,
				size < width() ) {                   //and size isn't too big.

			if (resize_mult != 1) {
				size *= resize_mult;
				x1 = o.x() - size/2;
				y1 = o.y() - size/2;
			}

			psky.drawImage( x1, y1,  p->image()->smoothScale( size, size ));

		} else { //Otherwise, draw a simple circle.

			psky.drawEllipse( x1, y1, size, size );
		}

		//draw Name
		if ( ksw->options()->drawPlanetName ) {
			int offset( int( 0.5*size ) );
			if ( offset < sizemin ) offset = sizemin;

			psky.setPen( QColor( ksw->options()->colorPName ) );
			psky.drawText( o.x()+offset, o.y()+offset, p->translatedName() );
		}
	}
}
		
void SkyMap::paintEvent( QPaintEvent *e ) {
// if the skymap should be only repainted and constellations need not to be new computed; call this with update() (default)
	if (!computeSkymap)
	{
		bitBlt( this, 0, 0, sky );
		return ; // exit because the pixmap is repainted and that's all what we want
	}

// if the sky should be recomputed (this is not every paintEvent call needed, explicitly call with Update())
	QPainter psky;
	QImage ScaledImage;

	float FOV = fov();
	float Ymax;
  bool isPoleVisible = false;
	if ( ksw->options()->useAltAz ) {
		Ymax = fabs( focus()->alt().Degrees() ) + FOV;
	} else {
		Ymax = fabs( focus()->dec().Degrees() ) + FOV;
	}
	if ( Ymax >= 90. ) isPoleVisible = true;

//checkSlewing combines the slewing flag (which is true when the display is actually in motion),
//the hideOnSlew option (which is true if slewing should hide objects),
//and a check of the current timescale (if the time rate is fast enough, it automatically mimics slewing)
	bool checkSlewing = ( slewing || ksw->clockScale() >= fabs( ksw->options()->slewTimeScale ) && ksw->options()->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool hideFaintStars( checkSlewing && ksw->options()->hideStars );
	bool drawPlanets( ksw->options()->drawPlanets && !(checkSlewing && ksw->options()->hidePlanets) );
	bool drawMess( ksw->options()->drawDeepSky && ( ksw->options()->drawMessier || ksw->options()->drawMessImages ) && !(checkSlewing && ksw->options()->hideMess) );
	bool drawNGC( ksw->options()->drawDeepSky && ksw->options()->drawNGC && !(checkSlewing && ksw->options()->hideNGC) );
	bool drawOther( ksw->options()->drawDeepSky && ksw->options()->drawOther && !(checkSlewing && ksw->options()->hideOther) );
	bool drawIC( ksw->options()->drawDeepSky && ksw->options()->drawIC && !(checkSlewing && ksw->options()->hideIC) );
	bool drawMW( ksw->options()->drawMilkyWay && !(checkSlewing && ksw->options()->hideMW) );
	bool drawCNames( ksw->options()->drawConstellNames && !(checkSlewing && ksw->options()->hideCNames) );
	bool drawCLines( ksw->options()->drawConstellLines &&!(checkSlewing && ksw->options()->hideCLines) );
	bool drawGrid( ksw->options()->drawGrid && !(checkSlewing && ksw->options()->hideGrid) );

	psky.begin( sky );
	psky.fillRect( 0, 0, width(), height(), QBrush( ksw->options()->colorSky ) );

	QFont stdFont = psky.font();
	QFont smallFont = psky.font();
	smallFont.setPointSize( stdFont.pointSize() - 2 );

	QList<QPoint> points;
	points.setAutoDelete(true);
	int ptsCount;
	int mwmax = pixelScale[ ksw->data()->ZoomLevel ]/100;
	int guidemax = pixelScale[ ksw->data()->ZoomLevel ]/10;

	//at high zoom, double FOV for guide lines so they don't disappear.
	float guideFOV = fov();
	if ( ksw->data()->ZoomLevel > 4 ) guideFOV *= 2.0;
			
	//Draw Milky Way (draw this first so it's in the "background")
	if ( drawMW ) {
		psky.setPen( QPen( QColor( ksw->options()->colorMW ) ) );
		psky.setBrush( QBrush( QColor( ksw->options()->colorMW ) ) );
		bool offscreen, lastoffscreen=false;

		for ( unsigned int j=0; j<11; ++j ) {
			if ( ksw->options()->fillMilkyWay ) {
				ptsCount = 0;
				bool partVisible = false;

				QPoint o = getXY( ksw->data()->MilkyWay[j].at(0), ksw->options()->useAltAz, ksw->options()->useRefraction );
				if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;

				for ( SkyPoint *p = ksw->data()->MilkyWay[j].first(); p; p = ksw->data()->MilkyWay[j].next() ) {
					o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );
					if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
					if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) partVisible = true;
				}

				if ( ptsCount && partVisible ) {
					psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
	 	  	}
			} else {
	      QPoint o = getXY( ksw->data()->MilkyWay[j].at(0), ksw->options()->useAltAz, ksw->options()->useRefraction );
				if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
				else offscreen = false;

				psky.moveTo( o.x(), o.y() );
  	
				for ( unsigned int i=1; i<ksw->data()->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->data()->MilkyWay[j].at(i), ksw->options()->useAltAz, ksw->options()->useRefraction );
					if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
					else offscreen = false;

					//don't draw a line if the last point's getXY was (-10000000, -10000000)
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
	if ( drawGrid ) {
		psky.setPen( QPen( QColor( ksw->options()->colorGrid ), 0, DotLine ) ); //change to colorGrid

		//First, the parallels
		for ( double Dec=-80.; Dec<=80.; Dec += 20. ) {
			bool newlyVisible = false;
			sp->set( 0.0, Dec );
			if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			QPoint o = getXY( sp, ksw->options()->useAltAz, ksw->options()->useRefraction );
			QPoint o1 = o;
			psky.moveTo( o.x(), o.y() );

			double dRA = 1./15.; //180 points along full circle of RA
			for ( double RA=dRA; RA<24.; RA+=dRA ) {
				sp->set( RA, Dec );
				if ( ksw->options()->useAltAz ) sp->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );

				if ( checkVisibility( sp, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
					o = getXY( sp, ksw->options()->useAltAz, ksw->options()->useRefraction );
  	
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
			SkyPoint *sp1 = new SkyPoint( RA, -90. );
			if ( ksw->options()->useAltAz ) sp1->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );
			QPoint o = getXY( sp1, ksw->options()->useAltAz, ksw->options()->useRefraction );
			psky.moveTo( o.x(), o.y() );

			double dDec = 1.;
			for ( double Dec=-89.; Dec<=90.; Dec+=dDec ) {
				sp1->set( RA, Dec );
				if ( ksw->options()->useAltAz ) sp1->EquatorialToHorizontal( ksw->data()->LSTh, ksw->geo()->lat() );

				if ( checkVisibility( sp1, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
					o = getXY( sp1, ksw->options()->useAltAz, ksw->options()->useRefraction );
  	  	
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
			delete sp1;  // avoid memory leak
		}
	}

	//Draw Equator (currently can't be hidden on slew)
	if ( ksw->options()->drawEquator ) {
		psky.setPen( QPen( QColor( ksw->options()->colorEq ), 0, SolidLine ) );

		SkyPoint *p = ksw->data()->Equator.first();
		QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );
		QPoint o1 = o;
		QPoint last = o;
		psky.moveTo( o.x(), o.y() );
		bool newlyVisible = false;

		//start loop at second item
		for ( p = ksw->data()->Equator.next(); p; p = ksw->data()->Equator.next() ) {
			if ( checkVisibility( p, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );

				int dx = psky.pos().x() - o.x();
				int dy = psky.pos().y() - o.y();

				if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
						if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
			last = o;
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

	//Draw Ecliptic (currently can't be hidden on slew)
	if ( ksw->options()->drawEcliptic ) {
		psky.setPen( QColor( ksw->options()->colorEcl ) );

		SkyPoint *p = ksw->data()->Ecliptic.first();
		QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );
		QPoint o1 = o;
		QPoint last = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		//Start loop at second item
		for ( p = ksw->data()->Ecliptic.next(); p; p = ksw->data()->Ecliptic.next() ) {
			if ( checkVisibility( p, guideFOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );

				int dx = psky.pos().x() - o.x();
				int dy = psky.pos().y() - o.y();
				if ( abs(dx) < guidemax && abs(dy) < guidemax ) {
					if ( newlyVisible ) {
						newlyVisible = false;
						psky.moveTo( last.x(), last.y() );
					}
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			} else {
				newlyVisible = true;
			}
			last = o;
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
	if ( drawCLines ) {
		psky.setPen( QColor( ksw->options()->colorCLine ) );
		int iLast = -1;

		for ( SkyPoint *p = ksw->data()->clineList.first(); p; p = ksw->data()->clineList.next() ) {
			QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );

			if ( ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) &&
					 ( o.x() >= -1000 && o.x() <= width()+1000 && o.y() >=-1000 && o.y() <=height()+1000 ) ) {
				if ( ksw->data()->clineModeList.at(ksw->data()->clineList.at())->latin1()=='M' ) {
					psky.moveTo( o.x(), o.y() );
				} else if ( ksw->data()->clineModeList.at(ksw->data()->clineList.at())->latin1()=='D' ) {
					if ( ksw->data()->clineList.at()== (uint)(iLast+1) ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
  	      }
				}
				iLast = ksw->data()->clineList.at();
			}
		}
  }

	//Draw Constellation Names
	//Don't draw names if slewing
	if ( drawCNames ) {
		psky.setFont( stdFont );
		psky.setPen( QColor( ksw->options()->colorCName ) );
		for ( SkyObject *p = ksw->data()->cnameList.first(); p; p = ksw->data()->cnameList.next() ) {
			if ( checkVisibility( p, FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction );
				if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
					if ( ksw->options()->useLatinConstellNames ) {
						int dx = 5*p->name().length();
						psky.drawText( o.x()-dx, o.y(), p->name() );  // latin constellation names
					} else if ( ksw->options()->useLocalConstellNames ) {
						// can't use translatedName() because we need the context string in i18n()
						int dx = 5*( i18n( "Constellation name (optional)", p->name().local8Bit().data() ).length() );
						psky.drawText( o.x()-dx, o.y(), i18n( "Constellation name (optional)", p->name().local8Bit().data() ) ); // localized constellation names
					} else {
						int dx = 5*p->name2().length();
						psky.drawText( o.x()-dx, o.y(), p->name2() ); //name2 is the IAU abbreviation
					}
				}
			}
		}
  }

	//Draw Stars
	if ( ksw->options()->drawSAO ) {

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
		if ( hideFaintStars && maglim > ksw->options()->magLimitHideStar ) maglim = ksw->options()->magLimitHideStar;
	
	for ( StarObject *curStar = ksw->data()->starList.first(); curStar; curStar = ksw->data()->starList.next() ) {
		// break loop if maglim is reached
		if ( curStar->mag() > maglim ) break;

		if ( checkVisibility( curStar, FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
			QPoint o = getXY( curStar, ksw->options()->useAltAz, ksw->options()->useRefraction );

			// draw star if currently on screen
			if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
				// but only if the star is bright enough.
					float mag = curStar->mag();
					float sizeFactor = 2.0;
					int size = int( sizeFactor*(zoomlim - mag) ) + 1;
					if (size>23) size=23;

					if ( size > 0 ) {
						psky.setPen( QColor( ksw->options()->colorSky ) );
						drawSymbol( psky, curStar->type(), o.x(), o.y(), size, 1.0, 0, curStar->color() );

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

	//Draw Deep-Sky Objects
	for ( SkyObject *obj = ksw->data()->deepSkyList.first(); obj; obj = ksw->data()->deepSkyList.next() ) {
		bool drawObject = false;
		bool drawImage = false;
		if ( obj->catalog() == "M" && drawMess ) {
			psky.setBrush( NoBrush );
			psky.setPen( QColor( ksw->options()->colorMess ) );
			drawObject = true;
			drawImage = ksw->options()->drawMessImages;
		} else if ( obj->catalog() == "NGC" && drawNGC ) {
			psky.setBrush( NoBrush );
			psky.setPen( QColor( ksw->options()->colorNGC ) );
			drawObject = true;
			drawImage = ksw->options()->drawMessImages;
		} else if ( obj->catalog() == "IC" && drawIC ) {
			psky.setBrush( NoBrush );
			psky.setPen( QColor( ksw->options()->colorIC ) );
			drawObject = true;
			drawImage = ksw->options()->drawMessImages;
		} else if ( obj->catalog().isEmpty() && drawOther ) {
			psky.setBrush( NoBrush );
			psky.setPen( QColor( ksw->options()->colorNGC ) ); //Use NGC color for now...
			drawObject = true;
			drawImage = ksw->options()->drawMessImages;
		}

		if ( drawObject || drawImage ) {
			if ( checkVisibility( obj, FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
				QPoint o = getXY( obj, ksw->options()->useAltAz, ksw->options()->useRefraction );
				if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {
					int PositionAngle = findPA( obj, o.x(), o.y() );

					//Draw Image
					if ( drawImage && ksw->data()->ZoomLevel >3 ) {
						QFile file;

						//readImage reads image from disk, or returns pointer to existing image.
						QImage *image=obj->readImage();
						if ( image ) {
							int w = int( obj->a()*PI()*pixelScale[ ksw->data()->ZoomLevel ]/10800.0 );
							int h = int( w*image->height()/image->width() ); //preserve image's aspect ratio
							int dx = int( 0.5*w );
							int dy = int( 0.5*h );
							//int x1 = o.x() - dx;
							//int y1 = o.y() - dy;
							ScaledImage = image->smoothScale( w, h );
							psky.translate( o.x(), o.y() );
							psky.rotate( double( PositionAngle ) );  //rotate the coordinate system
							psky.drawImage( -dx, -dy, ScaledImage );
							psky.resetXForm();
						}
					}

					//Draw Symbol
					if ( drawObject ) {
						int type = obj->type();
						if (type==0) type = 1; //use catalog star draw fcn
						//change color if extra images are available
						if ( obj->catalog() == "M" && obj->ImageList.count() > 1 )
							psky.setPen( QColor( ksw->options()->colorHST ) );
						else if ( obj->catalog() != "M" && obj->ImageList.count() )
							psky.setPen( QColor( ksw->options()->colorHST ) );

						int Size = int( obj->a()*PI()*pixelScale[ ksw->data()->ZoomLevel ]/10800.0 );

						drawSymbol( psky, obj->type(),
												o.x(), o.y(), Size, obj->e(), PositionAngle );
					}
				}
			} else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
//Uncomment to test whether objects get deleted properly when moving offscreen
//(as of 25/03/2002 it works :)
//					kdWarning() << obj->name() << endl;
					obj->deleteImage();
				}
			}
		}
	}

	//Draw Custom Catalogs
	for ( unsigned int i=0; i<ksw->options()->CatalogCount; ++i ) { //loop over custom catalogs
		if ( ksw->options()->drawCatalog[i] ) {

			psky.setBrush( NoBrush );
			psky.setPen( QColor( ksw->options()->colorNGC ) );

			QList<SkyObject> cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[i] ];

			for ( SkyObject *obj = cat.first(); obj; obj = cat.next() ) {

				if ( checkVisibility( obj, FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
					QPoint o = getXY( obj, ksw->options()->useAltAz, ksw->options()->useRefraction );

					if ( o.x() >= 0 && o.x() <= width() && o.y() >= 0 && o.y() <= height() ) {

						if ( obj->type()==0 || obj->type()==1 ) {
							StarObject *starobj = (StarObject*)obj;
							float zoomlim = 7.0 + float( ksw->data()->ZoomLevel )/4.0;
							float mag = starobj->mag();
							float sizeFactor = 2.0;
							int size = int( sizeFactor*(zoomlim - mag) ) + 1;
							if (size>23) size=23;
							if ( size > 0 ) drawSymbol( psky, starobj->type(), o.x(), o.y(), size, starobj->color() );
						} else {
							int size = pixelScale[ ksw->data()->ZoomLevel ]/pixelScale[0];
							if (size>8) size = 8;
							drawSymbol( psky, obj->type(), o.x(), o.y(), size );
						}
					}
				}
			}
		}
	}

	//Draw Sun
	if ( ksw->options()->drawSun && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Sun"), QColor( "Yellow" ), 8, 2.2, 3 );
	}

	//Draw Moon
	if ( ksw->options()->drawMoon && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->Moon, QColor( "White" ), 8, 2.2, -1 );
	}

	//Draw Mercury
	if ( ksw->options()->drawMercury && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Mercury"), QColor( "SlateBlue1" ), 4, .004, 4 );
	}

	//Draw Venus
	if ( ksw->options()->drawVenus && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Venus"), QColor( "LightGreen" ), 4, .01, 2 );
	}

	//Draw Mars
	if ( ksw->options()->drawMars && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Mars"), QColor( "Red" ), 4, .00555, 2 );
	}

	//Draw Jupiter
	if ( ksw->options()->drawJupiter && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Jupiter"), QColor( "Goldenrod" ), 4, .024, 2 );
	}

	//Draw Saturn
	if ( ksw->options()->drawSaturn && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Saturn"), QColor( "LightYellow2" ), 4, 0.024, 2, 2 );
	}

	//Draw Uranus
	if ( ksw->options()->drawUranus && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Uranus"), QColor( "LightSeaGreen" ), 4, 0.007, 2 );
	}

	//Draw Neptune
	if ( ksw->options()->drawNeptune && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Neptune"), QColor( "SkyBlue" ), 4, 0.0035, 2 );
	}

	//Draw Pluto
	if ( ksw->options()->drawPluto && drawPlanets ) {
	  	drawPlanet(psky, ksw->data()->PC.findByName("Pluto"), QColor( "gray" ), 4, 0.001, 4 );
	}

	//Label the clicked Object...commenting out for now
	//I'd like the text to "fade out", but this will require masking the skymap image
  //and updating just the portion of the label on a more rapid timescale than the rest
  //It's a lot of work for something completely superfluous, but I don't think the label
  //will work unless it's not persistent, and I don't want it to just disappear.
  //Maybe I'll do a tooltip window instead.
/*
	if ( labelClickedObject && clickedObject() ) {
		if ( checkVisibility( clickedObject(), FOV, ksw->options()->useAltAz, isPoleVisible ) ) {
			QPoint o = getXY( clickedObject(), ksw->options()->useAltAz, ksw->options()->useRefraction );
			if (o.x() >= 0 && o.x() <= width() && o.y() >=0 && o.y() <=height() ) {
				int dx(20), dy(20);
				if ( clickedObject()->a() ) {
					dx = int( clickedObject()->a()*PI()*pixelScale[ ksw->data()->ZoomLevel ]/10800.0 );
					dy = int( dx*clickedObject()->e() );
				}
				psky.setBrush( QColor( "White" ) );
				psky.drawText( o.x()+dy, o.y()+dy, clickedObject()->name() );  // latin constellation names
			}
		}
	}
*/

	//Draw Horizon
	//The horizon should not be corrected for atmospheric refraction, so getXY has doRefract=false...
	if (ksw->options()->drawHorizon || ksw->options()->drawGround) {
		QPoint OutLeft(0,0), OutRight(0,0);

		psky.setPen( QColor ( ksw->options()->colorHorz ) );
		psky.setBrush( QColor ( ksw->options()->colorHorz ) );
		ptsCount = 0;

		int maxdist = pixelScale[ ksw->data()->ZoomLevel ]/4;

		for ( SkyPoint *p = ksw->data()->Horizon.first(); p; p = ksw->data()->Horizon.next() ) {
			QPoint *o = new QPoint();
			*o = getXY( p, ksw->options()->useAltAz, false );  //false: do not refract the horizon
			bool found = false;

			//Use the QList of points to pre-sort visible horizon points
//			if ( o->x() > -1*maxdist && o->x() < width() + maxdist ) {
			if ( o->x() > -100 && o->x() < width() + 100 && o->y() > -100 && o->y() < height() + 100 ) {
				if ( ksw->options()->useAltAz ) {
					unsigned int j;
					for ( j=0; j<points.count(); ++j ) {
						if ( o->x() < points.at(j)->x() ) {
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
			} else {  //find the out-of-bounds points closest to the left and right borders
				if ( ( OutLeft.x() == 0 || o->x() > OutLeft.x() ) && o->x() < -100 ) {
					OutLeft.setX( o->x() );
					OutLeft.setY( o->y() );
				}
				if ( ( OutRight.x() == 0 || o->x() < OutRight.x() ) && o->x() > width() + 100 ) {
					OutRight.setX( o->x() );
					OutRight.setY( o->y() );
				}
			}
		}

		//Add left-edge and right-edge points based on interpolating the first/last onscreen points
		//to the nearest offscreen points.
		if ( ksw->options()->useAltAz && points.count() > 0 ) {
     //Interpolate from first sorted onscreen point to x=-100,
     //using OutLeft to determine the slope
			int xtotal = ( points.at( 0 )->x() - OutLeft.x() );
			int xx = ( points.at( 0 )->x() + 100 ) / xtotal;
			int yp = xx*OutRight.y() + (1-xx)*points.at( 0 )->y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.insert( 0, LeftEdge ); //Prepend LeftEdge to the beginning of points

    	//Interpolate from the last sorted onscreen point to width()+100,
			//using OutRight to determine the slope.
			xtotal = ( OutRight.x() - points.at( points.count() - 1 )->x() );
			xx = ( width() + 100 - points.at( points.count() - 1 )->x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*points.at( points.count() - 1 )->y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( width()+100, yp );
			points.append( RightEdge );

//If there are no horizon points, then either the horizon doesn't pass through the screen
//or we're at high zoom, and horizon points lie on either side of the screen.
		} else if ( ksw->options()->useAltAz && OutLeft.y() !=0 && OutRight.y() !=0 &&
            !( OutLeft.y() > height() + 100 && OutRight.y() > height() + 100 ) &&
            !( OutLeft.y() < -100 && OutRight.y() < -100 ) ) {

     //It's possible at high zoom that /no/ horizon points are onscreen.  In this case,
     //interpolate between OutLeft and OutRight directly to construct the horizon polygon.
			int xtotal = ( OutRight.x() - OutLeft.x() );
			int xx = ( OutRight.x() + 100 ) / xtotal;
			int yp = xx*OutLeft.y() + (1-xx)*OutRight.y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.append( LeftEdge );

			xx = ( width() + 100 - OutLeft.x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*OutLeft.y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( width()+100, yp );
			points.append( RightEdge );
 		}
		
		if ( points.count() ) {
//		Fill the pts array with sorted horizon points, Draw Horizon Line
			pts->setPoint( 0, points.at(0)->x(), points.at(0)->y() );
			if ( ksw->options()->drawHorizon ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

			for ( unsigned int i=1; i<points.count(); ++i ) {
				pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );

				if ( ksw->options()->drawHorizon ) {
					if ( !ksw->options()->useAltAz && ( abs( points.at(i)->x() - psky.pos().x() ) > maxdist ||
								abs( points.at(i)->y() - psky.pos().y() ) > maxdist ) ) {
						psky.moveTo( points.at(i)->x(), points.at(i)->y() );
					} else {
						psky.lineTo( points.at(i)->x(), points.at(i)->y() );
					}

				}
			}

  	 //connect the last segment back to the beginning
			if ( abs( points.at(0)->x() - psky.pos().x() ) < maxdist && abs( points.at(0)->y() - psky.pos().y() ) < maxdist )
				psky.lineTo( points.at(0)->x(), points.at(0)->y() );

//DEBUG
			if (dumpHorizon) {
				//First, make sure output file doesn't exist yet (so this only happens once)
				QFile dumpFile( "horizon.xy" );
				if ( !dumpFile.exists() && dumpFile.open( IO_WriteOnly ) ) {
					QTextStream t( &dumpFile );
					for ( uint i=0; i < points.count(); ++i ) {
						t << points.at(i)->x() << " " << points.at(i)->y() << endl;
					}
					dumpFile.close();
				}

				dumpHorizon = false;
			}
//END_DEBUG


//		Finish the Ground polygon by adding a square bottom edge, offscreen
			if ( ksw->options()->useAltAz && ksw->options()->drawGround ) {
				ptsCount = points.count();
				pts->setPoint( ptsCount++, width()+100, height()+100 );   //bottom right corner
				pts->setPoint( ptsCount++, -100, height()+100 );         //bottom left corner
 	
				psky.drawPolygon( ( const QPointArray ) *pts, false, 0, ptsCount );

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
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
				}
 	
		//NorthEast
				c->setAz( 45.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
				}

		//East
				c->setAz( 90.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
				}

		//SouthEast
				c->setAz( 135.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
				}

		//South
				c->setAz( 179.99 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
				}

		//SouthWest
				c->setAz( 225.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
				}

		//West
				c->setAz( 270.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
				}

		//NorthWest
				c->setAz( 315.0 );
				c->setAlt( 0.0 );
				if ( !ksw->options()->useAltAz ) c->HorizontalToEquatorial( ksw->data()->LSTh, ksw->geo()->lat() );
				cpoint = getXY( c, ksw->options()->useAltAz, false );
				cpoint.setY( cpoint.y() + 20 );
				if (cpoint.x() > 0 && cpoint.x() < width() && cpoint.y() > 0 && cpoint.y() < height() ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
				}

				delete c;
			}
		}
	}  //endif drawing horizon

	psky.end();
	bitBlt( this, 0, 0, sky );
	computeSkymap = false;	// use Update() to compute new skymap else old pixmap will be shown
}

