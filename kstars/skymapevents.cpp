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

#include <kiconloader.h>
#include <kstatusbar.h>
#include <kshortcut.h> //KKey class
#include <stdlib.h>
#include <math.h> //using fabs()

#include "infoboxes.h"
#include "kstars.h"
#include "skymap.h"
#include "ksutils.h"
#include "ksfilereader.h"
#include "kspopupmenu.h"

void SkyMap::resizeEvent( QResizeEvent * )
{
		computeSkymap = true; // skymap must be new computed
		if ( testWState(WState_AutoMask) ) updateMask();
		
		// avoid phantom positions of infoboxes
		if ( isVisible() || width() == ksw->width() || height() == ksw->height() ) {
			ksw->infoBoxes()->resize( width(), height() );
		}
}

void SkyMap::keyPressEvent( QKeyEvent *e ) {
	QString s;
	float step = 2.0;
	if ( e->state() & ShiftButton ) step = 4.0;

	//If the resume key was pressed, we process it here
	if ( ! ksw->data()->resumeKey.isNull() && e->key() == ksw->data()->resumeKey.keyCodeQt() ) {
		//kdDebug() << "resumeKey pressed; resuming DCOP." << endl;
		ksw->resumeDCOP();
		return;
	}
	
	switch ( e->key() ) {
		case Key_Left :
			if ( ksw->options()->useAltAz ) {
				focus()->setAz( focus()->az()->Degrees() - step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				focus()->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra()->Hours() + 0.05*step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				focus()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			}

//			setDestination( focus() );
			slewing = true;
			++scrollCount;
			break;

		case Key_Right :
			if ( ksw->options()->useAltAz ) {
				focus()->setAz( focus()->az()->Degrees() + step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				focus()->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
			} else {
				focus()->setRA( focus()->ra()->Hours() - 0.05*step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				focus()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			}

			slewing = true;
			++scrollCount;
			break;

		case Key_Up :
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt()->Degrees() + step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				if ( focus()->alt()->Degrees() > 90.0 ) focus()->setAlt( 90.0 );
				focus()->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec()->Degrees() + step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				if (focus()->dec()->Degrees() > 90.0) focus()->setDec( 90.0 );
				focus()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			}

			slewing = true;
			++scrollCount;
			break;

		case Key_Down:
			if ( ksw->options()->useAltAz ) {
				focus()->setAlt( focus()->alt()->Degrees() - step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				if ( focus()->alt()->Degrees() < -90.0 ) focus()->setAlt( -90.0 );
				focus()->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
			} else {
				focus()->setDec( focus()->dec()->Degrees() - step * pixelScale[0]/pixelScale[ ksw->options()->ZoomLevel ] );
				if (focus()->dec()->Degrees() < -90.0) focus()->setDec( -90.0 );
				focus()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			}

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
			setFoundObject( NULL );
			setDestinationAltAz( 15.0, 0.0 );
			break;

		case Key_E: //center on east horizon
			setClickedObject( NULL );
			setFoundObject( NULL );
			setDestinationAltAz( 15.0, 90.0 );
			break;

		case Key_S: //center on south horizon
			setClickedObject( NULL );
			setFoundObject( NULL );
			setDestinationAltAz( 15.0, 180.0 );
			break;

		case Key_W: //center on west horizon
			setClickedObject( NULL );
			setFoundObject( NULL );
			setDestinationAltAz( 15.0, 270.0 );
			break;

		case Key_Z: //center on Zenith
			setClickedObject( NULL );
			setFoundObject( NULL );
			setDestinationAltAz( 90.0, focus()->az()->Degrees() );
			break;

		case Key_0: //center on Sun
			setClickedObject( ksw->data()->PC->findByName("Sun") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_1: //center on Mercury
			setClickedObject( ksw->data()->PC->findByName("Mercury") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_2: //center on Venus
			setClickedObject( ksw->data()->PC->findByName("Venus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_3: //center on Moon
			setClickedObject( ksw->data()->Moon );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_4: //center on Mars
			setClickedObject( ksw->data()->PC->findByName("Mars") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_5: //center on Jupiter
			setClickedObject( ksw->data()->PC->findByName("Jupiter") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_6: //center on Saturn
			setClickedObject( ksw->data()->PC->findByName("Saturn") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_7: //center on Uranus
			setClickedObject( ksw->data()->PC->findByName("Uranus") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_8: //center on Neptune
			setClickedObject( ksw->data()->PC->findByName("Neptune") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

		case Key_9: //center on Pluto
			setClickedObject( ksw->data()->PC->findByName("Pluto") );
			setClickedPoint( clickedObject() );
			slotCenter();
			break;

//DUMP_HORIZON
/*
		case Key_X: //Spit out coords of horizon polygon
			dumpHorizon = true;
			break;
*/
//END_DUMP_HORIZON

//TIMING

		case Key_T: //loop through all objects, get Sin, Cos, Rad
		{

      QFile file;
     	if ( KSUtils::openDataFile( file, "Cities.dat" ) ) {
        KSFileReader fileReader( file );
        int nCount = 0;
        while (fileReader.hasMoreLines()) {
          QString line = fileReader.readLine();
          nCount++;
    			kdDebug() << "Line " << nCount << " : " << line;
        }
      }

/*
			QTime t1;
			t1.start();
      for (int i=0;i<10;i++) {
      	if ( KSUtils::openDataFile( file, "Cities.dat" ) ) {
          QString sAll( file.readAll() );
          QStringList lines = QStringList::split( "\n", sAll );
          int nSize = lines.size();
          for ( int i=0; i<nSize; i++ ) {
            QString& line = lines[i];
          }
      		file.close();
        }
      }
			kdDebug() << "time taken for reading city data via read all (10 times): (msec): " << t1.elapsed() << endl;

			QTime t2;
			t2.start();
      for (int i=0;i<10;i++) {
      	if ( KSUtils::openDataFile( file, "Cities.dat" ) ) {
      		QTextStream stream( &file );
        	while ( !stream.eof() ) {
      			QString line = stream.readLine();
      		}
      		file.close();
      	}
      }
			kdDebug() << "time taken for reading city data old code (10 times): (msec): " << t2.elapsed() << endl;

			QTime t3;
			t3.start();
      for (int i=0;i<1;i++) {
      	if ( KSUtils::openDataFile( file, "ngcic.dat" ) ) {
          QString sAll( file.readAll() );
          QStringList lines = QStringList::split( "\n", sAll );
          int nSize = lines.size();
          for ( int i=0; i<nSize; i++ ) {
            QString& line = lines[i];
          }
      		file.close();
        }
      }
			kdDebug() << "time taken for reading deep sky data via read all (1 times): (msec): " << t3.elapsed() << endl;

			QTime t4;
			t4.start();
      for (int i=0;i<1;i++) {
      	if ( KSUtils::openDataFile( file, "ngcic.dat" ) ) {
      		QTextStream stream( &file );
        	while ( !stream.eof() ) {
      			QString line = stream.readLine();
      		}
      		file.close();
      	}
      }
			kdDebug() << "time taken for reading deep sky data old code  (1 times): (msec): " << t4.elapsed() << endl;

*/
			
			break;
		}

//END_TIMING
	}

	setOldFocus( focus() );
	oldfocus()->setAz( focus()->az()->Degrees() );
	oldfocus()->setAlt( focus()->alt()->Degrees() );

	double dHA = ksw->LSTh()->Hours() - focus()->ra()->Hours();
	while ( dHA < 0.0 ) dHA += 24.0;
	ksw->data()->HourAngle->setH( dHA );

	if ( slewing ) {
		if ( ksw->options()->isTracking ) {
			ksw->slotTrack();  //toggle tracking off
		}

		if ( scrollCount > 10 ) {
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
	//Are we dragging an infoBox?
	if ( ksw->infoBoxes()->dragBox( e ) ) {
		update();
		return;
	}

	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->options()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->options()->ZoomLevel ];
	double dyPix = 0.5*height() - e->y();

	if (unusablePoint (dx, dy)) return;	// break if point is unusable

	//determine RA, Dec of mouse pointer
	setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->LSTh(), ksw->geo()->lat() ) );

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
			if ( ksw->options()->isTracking ) ksw->slotTrack(); //toggle tracking off
		}

		//Update focus such that the sky coords at mouse cursor remain approximately constant
		if ( ksw->options()->useAltAz ) {
			mousePoint()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			clickedPoint()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			dms dAz = mousePoint()->az()->Degrees() - clickedPoint()->az()->Degrees();
			dms dAlt = mousePoint()->alt()->Degrees() - clickedPoint()->alt()->Degrees();
			focus()->setAz( focus()->az()->Degrees() - dAz.Degrees() ); //move focus in opposite direction
			focus()->setAlt( focus()->alt()->Degrees() - dAlt.Degrees() );

			if ( focus()->alt()->Degrees() >90.0 ) focus()->setAlt( 89.9999 );
			if ( focus()->alt()->Degrees() <-90.0 ) focus()->setAlt( -89.9999 );
			focus()->setAz( focus()->az()->reduce() );
			focus()->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
		} else {
			dms dRA = mousePoint()->ra()->Degrees() - clickedPoint()->ra()->Degrees();
			dms dDec = mousePoint()->dec()->Degrees() - clickedPoint()->dec()->Degrees();
			focus()->setRA( focus()->ra()->Hours() - dRA.Hours() ); //move focus in opposite direction
			focus()->setDec( focus()->dec()->Degrees() - dDec.Degrees() );

			if ( focus()->dec()->Degrees() >90.0 ) focus()->setDec( 90.0 );
			if ( focus()->dec()->Degrees() <-90.0 ) focus()->setDec( -90.0 );
			focus()->setRA( focus()->ra()->reduce() );
			focus()->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
		}

		++scrollCount;
		if ( scrollCount > 4 ) {
			ksw->showFocusCoords();
			scrollCount = 0;
		}

		setOldFocus( focus() );

		double dHA = ksw->LSTh()->Hours() - focus()->ra()->Hours();
		while ( dHA < 0.0 ) dHA += 24.0;
		ksw->data()->HourAngle->setH( dHA );

		//redetermine RA, Dec of mouse pointer, using new focus
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, ksw->LSTh(), ksw->geo()->lat() ) );
		setClickedPoint( mousePoint() );

		Update();  // must be new computed
	} else {

		QString sRA, sDec, s;
		sRA = mousePoint()->ra()->toHMSString();
		sDec = mousePoint()->dec()->toDMSString(true); //true = force +/- symbol
		s = sRA + ",  " + sDec;
		ksw->statusBar()->changeItem( s, 1 );
	}
}

void SkyMap::wheelEvent( QWheelEvent *e ) {
	if ( e->delta() > 0 ) ksw->slotZoomIn();
	else ksw->slotZoomOut();
}

void SkyMap::mouseReleaseEvent( QMouseEvent * ) {
	if ( ksw->infoBoxes()->unGrabBox() ) {
		update();
		return;
	}

	if (mouseMoveCursor) setDefaultMouseCursor();	// set default cursor
	if (mouseButtonDown) { //false if double-clicked, becuase it's unset there.
		mouseButtonDown = false;
		if ( slewing ) {
			setDestination( focus() );
			slewing = false;
		}
		
		setOldFocus( focus() );
		Update();	// is needed because after moving the sky not all stars are shown
	}
	if ( midMouseButtonDown ) midMouseButtonDown = false;  // if middle button was pressed unset here

	scrollCount = 0;
}

void SkyMap::mousePressEvent( QMouseEvent *e ) {
//did we Grab an infoBox?
	if ( e->button() == LeftButton && ksw->infoBoxes()->grabBox( e ) ) {
		update();
		return;
	}

// if button is down and cursor is not moved set the move cursor after 500 ms
	QTimer t;
	t.singleShot (500, this, SLOT (setMouseMoveCursor()));

	double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->options()->ZoomLevel ];
	double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->options()->ZoomLevel ];
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
		setMousePoint( dXdYToRaDec( dx, dy, ksw->options()->useAltAz, 
				ksw->LSTh(), ksw->geo()->lat(), ksw->options()->useRefraction ) );
		setClickedPoint( mousePoint() );

		double r0 = 200.0/pixelScale[ ksw->options()->ZoomLevel ];  //the maximum search radius
		double rmin = r0;

		//Search stars database for nearby object.
		double rstar_min = r0;
		double starmag_min = 20.0;      //absurd initial value
		int istar_min = -1;

		if ( ksw->options()->drawSAO ) { //Can only click on a star if it's being drawn!
			
			//test RA and dec to see if this star is roughly nearby
			
			for ( register unsigned int i=0; i<ksw->data()->starList.count(); ++i ) {
				SkyObject *test = (SkyObject *)ksw->data()->starList.at(i);

				double dRA = test->ra()->Hours() - clickedPoint()->ra()->Hours();
				double dDec = test->dec()->Degrees() - clickedPoint()->dec()->Degrees();
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
		
		if ( ksw->options()->drawPlanets )
			solarminobj = ksw->data()->PC->findClosest(clickedPoint(), r);

		if ( r < r0 ) {
			rsolar_min = r;
		} else {
			solarminobj = NULL;
		}

		//Moon
		if ( ksw->options()->drawMoon ) {
			double dRA = ksw->data()->Moon->ra()->Hours() - clickedPoint()->ra()->Hours();
			double dDec = ksw->data()->Moon->dec()->Degrees() - clickedPoint()->dec()->Degrees();
			double f = 15.0*cos( ksw->data()->Moon->dec()->radians() );
			r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
			if (r < rsolar_min) {
				solarminobj= ksw->data()->Moon;
				rsolar_min = r;
			}
		}

		//Asteroids
		if ( ksw->options()->drawAsteroids ) {
			for ( KSAsteroid *ast = ksw->data()->asteroidList.first(); ast; ast = ksw->data()->asteroidList.next() ) {
				//test RA and dec to see if this object is roughly nearby
				double dRA = ast->ra()->Hours() - clickedPoint()->ra()->Hours();
				double dDec = ast->dec()->Degrees() - clickedPoint()->dec()->Degrees();
				double f = 15.0*cos( ast->dec()->radians() );
				double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if ( r < rsolar_min && ast->mag() < ksw->options()->magLimitAsteroid ) {
					solarminobj = ast;
					rsolar_min = r;
				}
			}
		}
		
		//Comets
		if ( ksw->options()->drawComets ) {
			for ( KSComet *com = ksw->data()->cometList.first(); com; com = ksw->data()->cometList.next() ) {
				//test RA and dec to see if this object is roughly nearby
				double dRA = com->ra()->Hours() - clickedPoint()->ra()->Hours();
				double dDec = com->dec()->Degrees() - clickedPoint()->dec()->Degrees();
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

		for ( SkyObject *o = ksw->data()->deepSkyList.first(); o; o = ksw->data()->deepSkyList.next() ) {
			bool checkObject = false;
			if ( o->isCatalogM() &&
					( ksw->options()->drawMessier || ksw->options()->drawMessImages ) ) checkObject = true;
			if ( o->isCatalogNGC() && ksw->options()->drawNGC ) checkObject = true;
			if ( o->isCatalogIC() && ksw->options()->drawIC ) checkObject = true;
			if ( o->catalog().isEmpty() && ksw->options()->drawOther ) checkObject = true;

			if ( checkObject ) {
				//test RA and dec to see if this object is roughly nearby
				double dRA = o->ra()->Hours() - clickedPoint()->ra()->Hours();
				double dDec = o->dec()->Degrees() - clickedPoint()->dec()->Degrees();
				double f = 15.0*cos( o->dec()->radians() );
				double r = f*f*dRA*dRA + dDec*dDec; //no need to take sqrt, we just want to ID smallest value.
				if ( o->isCatalogM() && r < rmess_min) {
					imess_min = ksw->data()->deepSkyList.at();
					rmess_min = r;
				}
				if ( o->isCatalogNGC() && r < rngc_min) {
					ingc_min = ksw->data()->deepSkyList.at();
					rngc_min = r;
				}
				if ( o->isCatalogIC() && r < ric_min) {
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

		for ( register unsigned int j=0; j<ksw->options()->CatalogCount; ++j ) {
			if ( ksw->options()->drawCatalog[j] ) {
				QPtrList<SkyObject> cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[j] ];

				for ( register unsigned int i=0; i<cat.count(); ++i ) {
					//test RA and dec to see if this object is roughly nearby
					SkyObject *test = (SkyObject *)cat.at(i);
					double dRA = test->ra()->Hours()-clickedPoint()->ra()->Hours();
					double dDec = test->dec()->Degrees()-clickedPoint()->dec()->Degrees();
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
			QPtrList<SkyObject> cat;

			switch (icat) {
				case 0: //star
					starobj = (StarObject *)ksw->data()->starList.at(istar_min);
					setClickedObject( (SkyObject *)ksw->data()->starList.at(istar_min) );
					
					setClickedPoint( clickedObject() );

					if ( e->button() == RightButton ) {
						pmenu->createStarMenu( starobj );
						pmenu->popup( QCursor::pos() );
					}
					break;

				case 1: //Deep-Sky Objects
					setClickedObject( (SkyObject *)ksw->data()->deepSkyList.at(jmin) );
					setClickedPoint( clickedObject());

					if (e->button() == RightButton ) {
						pmenu->createSkyObjectMenu( clickedObject() );
						pmenu->popup( QCursor::pos() );
					}
					break;

				case 2: //Custom Catalog Object
					cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[icust_cat] ];
					setClickedObject( (SkyObject *)cat.at(icust_min) );
					setClickedPoint( clickedObject() );

					if ( e->button() == RightButton ) {
						pmenu->createCustomObjectMenu( clickedObject() );
						pmenu->popup( QCursor::pos() );
					}
					break;

				case 3: //solar system object
					setClickedObject(solarminobj);
					if ( clickedObject() != NULL ) setClickedPoint( clickedObject() );

					if ( e->button() == RightButton ) {
						pmenu->createPlanetMenu( clickedObject() );
						pmenu->popup( QCursor::pos() );
					}
					break;
			}

			if ( e->button() == LeftButton ) {
				//Display name in status bar
				ksw->statusBar()->changeItem( i18n(clickedObject()->longname().latin1()), 0 );

			}
		} else {
			//Empty sky selected.  If left-click, display "nothing" in the status bar.
	    //If right-click, open "empty" popup menu.
			setClickedObject( NULL );

			switch (e->button()) {
				case LeftButton:
					ksw->statusBar()->changeItem( i18n( "Empty sky" ), 0 );
					break;
				case RightButton:
					pmenu->createEmptyMenu();
					pmenu->popup(  QCursor::pos() );
					break;
				default:
					break;
			}
		}
	}
}

void SkyMap::mouseDoubleClickEvent( QMouseEvent *e ) {
	//Was the event inside an infoBox?  If so, shade the box.
	if ( e->button() == LeftButton ) {
		if ( ksw->infoBoxes()->shadeBox( e ) ) {
			update();
			return;
		}

		double dx = ( 0.5*width()  - e->x() )/pixelScale[ ksw->options()->ZoomLevel ];
		double dy = ( 0.5*height() - e->y() )/pixelScale[ ksw->options()->ZoomLevel ];
		if (unusablePoint (dx, dy)) return;	// break if point is unusable

		if (mouseButtonDown ) mouseButtonDown = false;
		if ( dx != 0.0 || dy != 0.0 )  slotCenter();
	}
}

void SkyMap::paintEvent( QPaintEvent * )
{
	KStarsOptions* options = ksw->options();

// if the skymap should be only repainted and constellations need not to be new computed; call this with update() (default)
	if (!computeSkymap)
	{
		QPixmap *sky2 = new QPixmap( *sky );
		drawBoxes( sky2 );
		bitBlt( this, 0, 0, sky2 );
		delete sky2;
		return ; // exit because the pixmap is repainted and that's all what we want
	}

// if the sky should be recomputed (this is not every paintEvent call needed, explicitly call with Update())
	QPainter psky;

	guidemax = pixelScale[ options->ZoomLevel ]/10;
	FOV = fov();
	isPoleVisible = false;
	if ( options->useAltAz ) {
		Xmax = 1.2*FOV/cos( focus()->alt()->radians() );
		Ymax = fabs( focus()->alt()->Degrees() ) + FOV;
	} else {
		Xmax = 1.2*FOV/cos( focus()->dec()->radians() );
		Ymax = fabs( focus()->dec()->Degrees() ) + FOV;
	}
	if ( Ymax >= 90. ) isPoleVisible = true;

	//at high zoom, double FOV for guide lines so they don't disappear.
	guideFOV = fov();
	guideXmax = Xmax;
	if ( options->ZoomLevel > 4 ) { guideFOV *= 2.0; guideXmax *= 2.0; }

//checkSlewing combines the slewing flag (which is true when the display is actually in motion),
//the hideOnSlew option (which is true if slewing should hide objects),
//and clockSlewing (which is true if the timescale exceeds options()->slewTimeScale)
	bool checkSlewing = ( ( slewing || ( clockSlewing && ksw->getClock()->isActive() ) )
				&& options->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool drawPlanets( options->drawPlanets && !(checkSlewing && options->hidePlanets) );
	bool drawMW( options->drawMilkyWay && !(checkSlewing && options->hideMW) );
	bool drawCNames( options->drawConstellNames && !(checkSlewing && options->hideCNames) );
	bool drawCLines( options->drawConstellLines &&!(checkSlewing && options->hideCLines) );
	bool drawGrid( options->drawGrid && !(checkSlewing && options->hideGrid) );

	psky.begin( sky );
	psky.fillRect( 0, 0, width(), height(), QBrush( options->colorScheme()->colorNamed( "SkyColor" ) ) );

	QFont stdFont = psky.font();
	QFont smallFont = psky.font();
	smallFont.setPointSize( stdFont.pointSize() - 2 );

	if ( drawMW ) drawMilkyWay( psky );
	if ( drawGrid ) drawCoordinateGrid( psky );
	if ( ksw->options()->drawEquator ) drawEquator( psky );
	if ( options->drawEcliptic ) drawEcliptic( psky );
	if ( drawCLines ) drawConstellationLines( psky );
	if ( drawCNames ) drawConstellationNames( psky, stdFont );
	
	// stars and planets use the same font size
	if ( ksw->options()->ZoomLevel < 6 ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}
	
	//drawing to screen, so leave scale parameter at its default value of 1.0
	drawStars( psky );
	drawDeepSkyObjects( psky );
	drawSolarSystem( psky, drawPlanets );
	drawAttachedLabels( psky );
	drawHorizon( psky, stdFont );
	
	//Draw a Field-of-View indicator
	drawTargetSymbol( psky, options->targetSymbol );

	//Finish up
	psky.end();
	
	QPixmap *sky2 = new QPixmap( *sky );
	drawBoxes( sky2 );
	bitBlt( this, 0, 0, sky2 );
	delete sky2;
	
	computeSkymap = false;	// use Update() to compute new skymap else old pixmap will be shown
}

