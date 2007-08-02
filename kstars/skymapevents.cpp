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

#include <stdlib.h>
#include <math.h> //using fabs()

#include <QCursor>
#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QPaintEvent>

#include <kiconloader.h>
#include <kstatusbar.h>

#include "skymap.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "simclock.h"
#include "infoboxes.h"
#include "ksfilereader.h"
#include "kspopupmenu.h"
#include "ksmoon.h"

void SkyMap::resizeEvent( QResizeEvent * )
{
	computeSkymap = true; // skymap must be new computed

//FIXME: No equivalent for this line in Qt4 ??
//	if ( testWState( Qt::WState_AutoMask ) ) updateMask();

	// avoid phantom positions of infoboxes
	if ( ks && ( isVisible() || width() == ks->width() || height() == ks->height() ) ) {
		infoBoxes()->resize( width(), height() );
	}
	*sky = sky->scaled( width(), height() );
	*sky2 = sky2->scaled( width(), height() );
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
	QString s;
	bool arrowKeyPressed( false );
	bool shiftPressed( false );
	float step = 1.0;
	if ( e->modifiers() & Qt::ShiftModifier ) { step = 10.0; shiftPressed = true; }

	//If the DBus resume key is not empty, then DBus processing is
	//paused while we wait for a keypress
	if ( ! data->resumeKey.isEmpty() && QKeySequence(e->key()) == data->resumeKey ) {
                //The resumeKey was pressed.  Signal that it was pressed by
                //resetting it to empty; this will break the loop in
                //KStars::waitForKey()
		data->resumeKey = QString();
		return;
	}

	switch ( e->key() ) {
		case Qt::Key_Left :
			if ( Options::useAltAz() ) {
				focus()->setAz( dms( focus()->az()->Degrees() - step * MINZOOM/Options::zoomFactor() ).reduce() );
				focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra()->Hours() + 0.05*step * MINZOOM/Options::zoomFactor() );
				focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			}

			arrowKeyPressed = true;
			slewing = true;
			++scrollCount;
			break;

		case Qt::Key_Right :
			if ( Options::useAltAz() ) {
				focus()->setAz( dms( focus()->az()->Degrees() + step * MINZOOM/Options::zoomFactor() ).reduce() );
				focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra()->Hours() - 0.05*step * MINZOOM/Options::zoomFactor() );
				focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			}

			arrowKeyPressed = true;
			slewing = true;
			++scrollCount;
			break;

		case Qt::Key_Up :
			if ( Options::useAltAz() ) {
				focus()->setAlt( focus()->alt()->Degrees() + step * MINZOOM/Options::zoomFactor() );
				if ( focus()->alt()->Degrees() > 90.0 ) focus()->setAlt( 90.0 );
				focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec()->Degrees() + step * MINZOOM/Options::zoomFactor() );
				if (focus()->dec()->Degrees() > 90.0) focus()->setDec( 90.0 );
				focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			}

			arrowKeyPressed = true;
			slewing = true;
			++scrollCount;
			break;

		case Qt::Key_Down:
			if ( Options::useAltAz() ) {
				focus()->setAlt( focus()->alt()->Degrees() - step * MINZOOM/Options::zoomFactor() );
				if ( focus()->alt()->Degrees() < -90.0 ) focus()->setAlt( -90.0 );
				focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec()->Degrees() - step * MINZOOM/Options::zoomFactor() );
				if (focus()->dec()->Degrees() < -90.0) focus()->setDec( -90.0 );
				focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			}

			arrowKeyPressed = true;
			slewing = true;
			++scrollCount;
			break;

		case Qt::Key_Plus:   //Zoom in
		case Qt::Key_Equal:
			if ( ks ) ks->slotZoomIn();
			break;

		case Qt::Key_Minus:  //Zoom out
		case Qt::Key_Underscore:
			if ( ks ) ks->slotZoomOut();
			break;

//In the following cases, we set slewing=true in order to disengage tracking
		case Qt::Key_N: //center on north horizon
			stopTracking();
			setDestinationAltAz( 15.0, 0.0001 );
			break;

		case Qt::Key_E: //center on east horizon
			stopTracking();
			setDestinationAltAz( 15.0, 90.0 );
			break;

		case Qt::Key_S: //center on south horizon
			stopTracking();
			setDestinationAltAz( 15.0, 180.0 );
			break;

		case Qt::Key_W: //center on west horizon
			stopTracking();
			setDestinationAltAz( 15.0, 270.0 );
			break;

		case Qt::Key_Z: //center on Zenith
			stopTracking();
			setDestinationAltAz( 90.0, focus()->az()->Degrees() );
			break;

		case Qt::Key_0: //center on Sun
			setClickedObject( data->skyComposite()->findByName("Sun") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_1: //center on Mercury
			setClickedObject( data->skyComposite()->findByName("Mercury") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_2: //center on Venus
			setClickedObject( data->skyComposite()->findByName("Venus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_3: //center on Moon
			setClickedObject( data->skyComposite()->findByName("Moon") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_4: //center on Mars
			setClickedObject( data->skyComposite()->findByName("Mars") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_5: //center on Jupiter
			setClickedObject( data->skyComposite()->findByName("Jupiter") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_6: //center on Saturn
			setClickedObject( data->skyComposite()->findByName("Saturn") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_7: //center on Uranus
			setClickedObject( data->skyComposite()->findByName("Uranus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_8: //center on Neptune
			setClickedObject( data->skyComposite()->findByName("Neptune") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_9: //center on Pluto
			setClickedObject( data->skyComposite()->findByName("Pluto") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Qt::Key_BracketLeft:   // Begin measuring angular distance
			if ( !isAngleMode() ) {
				slotBeginAngularDistance();
			}

			break;

		case Qt::Key_BracketRight:  // End measuring angular distance
			if ( isAngleMode() ) {
				slotEndAngularDistance();
			}

			break;

		case Qt::Key_Escape:        // Cancel angular distance measurement
			if  (isAngleMode() ) {
				slotCancelAngularDistance();
			}
			break;

		case Qt::Key_Comma:  //advance one step backward in time
		case Qt::Key_Less:
			if ( data->clock()->isActive() ) data->clock()->stop();
			data->clock()->setScale( -1.0 * data->clock()->scale() ); //temporarily need negative time step
			data->clock()->manualTick( true );
			data->clock()->setScale( -1.0 * data->clock()->scale() ); //reset original sign of time step
			update();
			qApp->processEvents();
			break;

		case Qt::Key_Period: //advance one step forward in time
		case Qt::Key_Greater:
			if ( data->clock()->isActive() ) data->clock()->stop();
			data->clock()->manualTick( true );
			update();
			qApp->processEvents();
			break;

		case Qt::Key_C: //Center clicked object
			if ( clickedObject() ) slotCenter();
			break;

		case Qt::Key_D: //Details window for Clicked/Centered object
			if ( shiftPressed ) setClickedObject( focusObject() );
			if ( clickedObject() ) slotDetail();
			break;

		case Qt::Key_P: //Show Popup menu for Clicked/Centered object
			if ( shiftPressed ) setClickedObject( focusObject() );
			if ( clickedObject() )
				clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
			break;

		case Qt::Key_O: //Add object to Observing List
			if ( shiftPressed ) setClickedObject( focusObject() );
			if ( clickedObject() )
				ks->observingList()->slotAddObject();
			break;

		case Qt::Key_L: //Toggle User label on Clicked/Centered object
			if ( shiftPressed ) setClickedObject( focusObject() );
			if ( clickedObject() ) {
				if ( isObjectLabeled( clickedObject() ) )
					slotRemoveObjectLabel();
				else
					slotAddObjectLabel();
			}
			break;

		case Qt::Key_T: //Toggle planet trail on Clicked/Centered object (if solsys)
			if ( shiftPressed ) setClickedObject( focusObject() );
			if ( clickedObject() && clickedObject()->isSolarSystem() ) {
				if ( ((KSPlanetBase*)clickedObject())->hasTrail() )
					slotRemovePlanetTrail();
				else
					slotAddPlanetTrail();
			}
			break;

//TIMING
// *** Uncomment and insert timing test code here ***
		case Qt::Key_X:
		{
			QTime t;
			t.start();
			foreach ( SkyObject *star, data->skyComposite()->stars() )
				toScreen( star );
			kDebug() << QString("toScreen() for all stars took %1 ms").arg(t.elapsed());
			t.start();
			foreach ( SkyObject *star, data->skyComposite()->stars() )
				toScreenQuaternion( star );
			kDebug() << QString("toScreenQuaternion() for all stars took %1 ms").arg(t.elapsed());
			break;
		}
//END_TIMING

		case Qt::Key_A:
			Options::setUseAntialias( ! Options::useAntialias() );
			kDebug() << "Use Antialiasing: " << Options::useAntialias();
			forceUpdate();
			break;

		//Test code: create a SkyLine
		case Qt::Key_V:
			kDebug() << "Create a skyline: ";
			SkyLine sl( SkyPoint( 12.34, 33.50 ), SkyPoint( 14.00, 40.00 ) );
			kDebug() << "  " << sl.points().size() << " :: "
					<< sl.point(0)->ra()->toHMSString() << " : " 
					<< sl.point(1)->ra()->toHMSString() << endl;

			SkyPoint p( 11.75, 30.25 );
			sl.setPoint( 1, &p );
			kDebug() << "  " << sl.points().size() << " :: "
					<< sl.point(0)->ra()->toHMSString() << " : " 
					<< sl.point(1)->ra()->toHMSString() << endl;
	}

	setOldFocus( focus() );
	oldfocus()->setAz( focus()->az()->Degrees() );
	oldfocus()->setAlt( focus()->alt()->Degrees() );

	double dHA = data->LST->Hours() - focus()->ra()->Hours();
	while ( dHA < 0.0 ) dHA += 24.0;
	data->HourAngle->setH( dHA );

	if ( arrowKeyPressed ) {
		infoBoxes()->focusObjChanged( i18n( "nothing" ) );
		stopTracking();

		if ( scrollCount > 10 ) {
			setDestination( focus() );
			scrollCount = 0;
		}
	}

	forceUpdate(); //need a total update, or slewing with the arrow keys doesn't work.
}

void SkyMap::stopTracking() {
	if ( ks && Options::isTracking() ) ks->slotTrack();
}

void SkyMap::keyReleaseEvent( QKeyEvent *e ) {
	switch ( e->key() ) {
		case Qt::Key_Left :  //no break; continue to Qt::Key_Down
		case Qt::Key_Right :  //no break; continue to Qt::Key_Down
		case Qt::Key_Up :  //no break; continue to Qt::Key_Down
		case Qt::Key_Down :
			slewing = false;
			scrollCount = 0;

			if ( Options::useAltAz() )
				setDestinationAltAz( focus()->alt()->Degrees(), focus()->az()->Degrees() );
			else
				setDestination( focus() );

			showFocusCoords( true );
			forceUpdate();	// Need a full update to draw faint objects that are not drawn while slewing.
			break;
	}
}

void SkyMap::mouseMoveEvent( QMouseEvent *e ) {
	if ( Options::useHoverLabel() ) {
		//First of all, if the transientObject() pointer is not NULL, then
		//we just moved off of a hovered object.  Begin fading the label.
		if ( transientObject() && ! TransientTimer.isActive() ) {
			fadeTransientLabel();
		}

		//Start a single-shot timer to monitor whether we are currently hovering.
		//The idea is that whenever a moveEvent occurs, the timer is reset.  It
		//will only timeout if there are no move events for HOVER_INTERVAL ms
		HoverTimer.start( HOVER_INTERVAL );
	}

	//Are we dragging an infoBox?
	if ( infoBoxes()->dragBox( e ) ) {
		update();
		return;
	}

	//Are we defining a ZoomRect?
	if ( ZoomRect.center().x() > 0 && ZoomRect.center().y() > 0 ) {
		//cancel operation if the user let go of CTRL
		if ( !( e->modifiers() & Qt::ControlModifier ) ) {
			ZoomRect = QRect(); //invalidate ZoomRect
			update();
		} else {
			//Resize the rectangle so that it passes through the cursor position
			QPoint pcenter = ZoomRect.center();
			int dx = abs(e->x() - pcenter.x());
			int dy = abs(e->y() - pcenter.y());
			if ( dx == 0 || float(dy)/float(dx) > float(height())/float(width()) ) {
				//Size rect by height
				ZoomRect.setHeight( 2*dy );
				ZoomRect.setWidth( 2*dy*width()/height() );
			} else {
				//Size rect by height
				ZoomRect.setWidth( 2*dx );
				ZoomRect.setHeight( 2*dx*height()/width() );
			}
			ZoomRect.moveCenter( pcenter ); //reset center

			update();
			return;
		}
	}

	double dx = ( 0.5*width()  - e->x() )/Options::zoomFactor();
	double dy = ( 0.5*height() - e->y() )/Options::zoomFactor();
	double dyPix = 0.5*height() - e->y();

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	//determine RA, Dec of mouse pointer
	setMousePoint( fromScreen( dx, dy, data->LST, data->geo()->lat() ) );
	mousePoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );


	if ( midMouseButtonDown ) { //zoom according to y-offset
		float yoff = dyPix - y0;

		if (yoff > 10 ) {
			y0 = dyPix;
			if ( ks ) ks->slotZoomIn();
		}
		if (yoff < -10 ) {
			y0 = dyPix;
			if ( ks ) ks->slotZoomOut();
		}
	}

	if ( mouseButtonDown ) {
		// set the mouseMoveCursor and set slewing=true, if they are not set yet
		if (!mouseMoveCursor) setMouseMoveCursor();
		if (!slewing) {
			slewing = true;
			infoBoxes()->focusObjChanged( i18n( "nothing" ) );
			stopTracking(); //toggle tracking off
		}

		//Update focus such that the sky coords at mouse cursor remain approximately constant
		if ( Options::useAltAz() ) {
			mousePoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			clickedPoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
			dms dAz = mousePoint()->az()->Degrees() - clickedPoint()->az()->Degrees();
			dms dAlt = mousePoint()->alt()->Degrees() - clickedPoint()->alt()->Degrees();
			focus()->setAz( focus()->az()->Degrees() - dAz.Degrees() ); //move focus in opposite direction
			focus()->setAlt( focus()->alt()->Degrees() - dAlt.Degrees() );

			if ( focus()->alt()->Degrees() >90.0 ) focus()->setAlt( 90.0 );
			if ( focus()->alt()->Degrees() <-90.0 ) focus()->setAlt( -90.0 );
			focus()->setAz( focus()->az()->reduce() );
			focus()->HorizontalToEquatorial( data->LST, data->geo()->lat() );
		} else {
			dms dRA = mousePoint()->ra()->Degrees() - clickedPoint()->ra()->Degrees();
			dms dDec = mousePoint()->dec()->Degrees() - clickedPoint()->dec()->Degrees();
			focus()->setRA( focus()->ra()->Hours() - dRA.Hours() ); //move focus in opposite direction
			focus()->setDec( focus()->dec()->Degrees() - dDec.Degrees() );

			if ( focus()->dec()->Degrees() >90.0 ) focus()->setDec( 90.0 );
			if ( focus()->dec()->Degrees() <-90.0 ) focus()->setDec( -90.0 );
			focus()->setRA( focus()->ra()->reduce() );
			focus()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		}

		++scrollCount;
		if ( scrollCount > 4 ) {
			showFocusCoords( true );
			scrollCount = 0;
		}

		setOldFocus( focus() );

		double dHA = data->LST->Hours() - focus()->ra()->Hours();
		while ( dHA < 0.0 ) dHA += 24.0;
		data->HourAngle->setH( dHA );

		//redetermine RA, Dec of mouse pointer, using new focus
		setMousePoint( fromScreen( dx, dy, data->LST, data->geo()->lat() ) );
		mousePoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		setClickedPoint( mousePoint() );

		forceUpdate();  // must be new computed
	} else {

		if ( ks ) {
			QString sX, sY, s;
			sX = mousePoint()->az()->toDMSString(true);  //true = force +/- symbol
			sY = mousePoint()->alt()->toDMSString(true); //true = force +/- symbol
			if ( Options::useAltAz() && Options::useRefraction() )
				sY = refract( mousePoint()->alt(), true ).toDMSString(true);

			s = sX + ",  " + sY;
			ks->statusBar()->changeItem( s, 1 );

			sX = mousePoint()->ra()->toHMSString();
			sY = mousePoint()->dec()->toDMSString(true); //true = force +/- symbol
			s = sX + ",  " + sY;
			ks->statusBar()->changeItem( s, 2 );
		}
	}
}

void SkyMap::wheelEvent( QWheelEvent *e ) {
	if ( ks && e->delta() > 0 ) ks->slotZoomIn();
	else if ( ks ) ks->slotZoomOut();
}

void SkyMap::mouseReleaseEvent( QMouseEvent * ) {
	if ( infoBoxes()->unGrabBox() ) {
		update();
		return;
	}

	if ( ZoomRect.isValid() ) {
		//Zoom in on center of Zoom Circle, by a factor equal to the ratio
		//of the sky pixmap's width to the Zoom Circle's diameter
		float factor = float(width()) / float(ZoomRect.width());

		double dx = ( 0.5*width()  - ZoomRect.center().x() )/Options::zoomFactor();
		double dy = ( 0.5*height() - ZoomRect.center().y() )/Options::zoomFactor();

		infoBoxes()->focusObjChanged( i18n( "nothing" ) );
		stopTracking();

		SkyPoint newcenter = fromScreen( dx, dy, data->LST, data->geo()->lat() );

		setFocus( &newcenter );
		setDestination( &newcenter );
		ks->zoom( Options::zoomFactor() * factor );

		setDefaultMouseCursor();
		ZoomRect = QRect(); //invalidate ZoomRect
		forceUpdate();
	} else {
		setDefaultMouseCursor();
		ZoomRect = QRect(); //just in case user Ctrl+clicked + released w/o dragging...
	}

	if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
	if (mouseButtonDown) { //false if double-clicked, because it's unset there.
		mouseButtonDown = false;
		if ( slewing ) {
			slewing = false;

			if ( Options::useAltAz() )
				setDestinationAltAz( focus()->alt()->Degrees(), focus()->az()->Degrees() );
			else
				setDestination( focus() );
		}

		setOldFocus( focus() );
		forceUpdate();	// is needed because after moving the sky not all stars are shown
	}
	if ( midMouseButtonDown ) midMouseButtonDown = false;  // if middle button was pressed unset here

	scrollCount = 0;
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
	//did we Grab an infoBox?
	if ( e->button() == Qt::LeftButton && infoBoxes()->grabBox( e ) ) {
		update(); //refresh without redrawing skymap
		return;
	}

	if ( ( e->modifiers() & Qt::ControlModifier ) && (e->button() == Qt::LeftButton) ) {
		ZoomRect.moveCenter( e->pos() );
		setZoomMouseCursor();
		update(); //refresh without redrawing skymap
		return;
	}

	// if button is down and cursor is not moved set the move cursor after 500 ms
	QTimer t;
	t.singleShot (500, this, SLOT (setMouseMoveCursor()));

	double dx = ( 0.5*width()  - e->x() )/Options::zoomFactor();
	double dy = ( 0.5*height() - e->y() )/Options::zoomFactor();
	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	if ( !midMouseButtonDown && e->button() == Qt::MidButton ) {
		y0 = 0.5*height() - e->y();  //record y pixel coordinate for middle-button zooming
		midMouseButtonDown = true;
	}

	if ( !mouseButtonDown ) {
		if ( e->button() == Qt::LeftButton ) {
			mouseButtonDown = true;
			scrollCount = 0;
		}

		//determine RA, Dec of mouse pointer
		setMousePoint( fromScreen( dx, dy, data->LST, data->geo()->lat() ) );
		mousePoint()->EquatorialToHorizontal( data->LST, data->geo()->lat() );
		setClickedPoint( mousePoint() );

		//Find object nearest to clickedPoint()
		double maxrad = 2500.0/Options::zoomFactor();
		setClickedObject( data->skyComposite()->objectNearest( clickedPoint(), maxrad ) );

		if ( clickedObject() ) {
			setClickedPoint( clickedObject() );

			if ( e->button() == Qt::RightButton ) {
				clickedObject()->showPopupMenu( pmenu, QCursor::pos() );
			}

			if ( ks && e->button() == Qt::LeftButton ) {
				ks->statusBar()->changeItem( clickedObject()->translatedLongName(), 0 );
			}
		} else {
			//Empty sky selected.  If left-click, display "nothing" in the status bar.
	    //If right-click, open "empty" popup menu.
			setClickedObject( NULL );

			switch (e->button()) {
				case Qt::LeftButton:
					if ( ks ) ks->statusBar()->changeItem( i18n( "Empty sky" ), 0 );
					break;
				case Qt::RightButton:
				{
					SkyObject *nullObj = new SkyObject( SkyObject::TYPE_UNKNOWN, clickedPoint()->ra()->Hours(), clickedPoint()->dec()->Degrees() );
					pmenu->createEmptyMenu( nullObj );
					delete nullObj;

					pmenu->popup(  QCursor::pos() );
					break;
				}

				default:
					break;
			}
		}
	}
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
	//Was the event inside an infoBox?  If so, shade the box.
	if ( e->button() == Qt::LeftButton ) {
		if ( infoBoxes()->shadeBox( e ) ) {
			update();
			return;
		}

		double dx = ( 0.5*width()  - e->x() )/Options::zoomFactor();
		double dy = ( 0.5*height() - e->y() )/Options::zoomFactor();
		if (unusablePoint (dx, dy)) return;	// break if point is unusable

		if (mouseButtonDown ) mouseButtonDown = false;
		if ( dx != 0.0 || dy != 0.0 )  slotCenter();
	}
}

void SkyMap::paintEvent( QPaintEvent * )
{
	//If computeSkymap is false, then we just refresh the window using the stored sky pixmap
	//and draw the "overlays" on top.  This lets us update the overlay information rapidly
	//without needing to recompute the entire skymap.
	//use update() to trigger this "short" paint event; to force a full "recompute"
	//of the skymap, use forceUpdate().
	if (!computeSkymap)
	{
		*sky2 = *sky;
		drawOverlays( sky2 );
                QPainter p;
                p.begin( this );
                p.drawImage( 0, 0, sky2->toImage() );
                p.end();
                return ; // exit because the pixmap is repainted and that's all what we want
	}

	//TIMING
// 	QTime t;
// 	t.start();

	QPainter psky;
	setMapGeometry();

//FIXME: What to do about the visibility logic?
// 	//checkSlewing combines the slewing flag (which is true when the display is actually in motion),
// 	//the hideOnSlew option (which is true if slewing should hide objects),
// 	//and clockSlewing (which is true if the timescale exceeds Options::slewTimeScale)
// 	bool checkSlewing = ( ( slewing || ( clockSlewing && data->clock()->isActive() ) )
// 				&& Options::hideOnSlew() );
//
// 	//shortcuts to inform whether to draw different objects
// 	bool drawPlanets( Options::showPlanets() && !(checkSlewing && Options::hidePlanets() ) );
// 	bool drawMW( Options::showMilkyWay() && !(checkSlewing && Options::hideMilkyWay() ) );
// 	bool drawCNames( Options::showCNames() && !(checkSlewing && Options::hideCNames() ) );
// 	bool drawCLines( Options::showCLines() &&!(checkSlewing && Options::hideCLines() ) );
// 	bool drawCBounds( Options::showCBounds() &&!(checkSlewing && Options::hideCBounds() ) );
// 	bool drawGrid( Options::showGrid() && !(checkSlewing && Options::hideGrid() ) );

	psky.begin( sky );
	psky.setRenderHint(QPainter::Antialiasing, (!slewing && Options::useAntialias()) );

	psky.fillRect( 0, 0, width(), height(), QBrush( data->colorScheme()->colorNamed( "SkyColor" ) ) );

	//TIMING
// 	QTime t2;
// 	t2.start();

	//Draw all sky elements
	data->skyComposite()->draw( ks, psky );

	//TIMING
// 	kDebug() << QString("SkyMapComposite::draw() took %1 ms").arg(t2.elapsed());

	//Finish up
	psky.end();

	//TIMING
//	t2.start();

	*sky2 = *sky;
	drawOverlays( sky2 );

	//TIMING
//	kDebug() << QString("drawOverlays() took %1 ms").arg(t2.elapsed());

	//TIMING
//	t2.start();

	QPainter psky2;
	psky2.begin( this );
	psky2.drawPixmap( 0, 0, *sky2 );
	psky2.end();

	//TIMING
//	kDebug() << QString("drawImage() took %1 ms").arg(t2.elapsed());

	computeSkymap = false;	// use forceUpdate() to compute new skymap else old pixmap will be shown

	//TIMING
//	kDebug() << QString("Skymap draw took %1 ms").arg(t.elapsed());
}

