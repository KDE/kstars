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
				if ( focus()->alt()->Degrees() > 90.0 ) focus()->setAlt( 89.9999 );
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
				if ( focus()->alt()->Degrees() < -90.0 ) focus()->setAlt( -89.9999 );
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
			setClickedObject( NULL );
			setFoundObject( NULL );//no longer tracking foundObject
			ksw->options()->isTracking = false;
			if ( ksw->data()->PlanetTrail.count() ) ksw->data()->PlanetTrail.clear();
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
			ksw->options()->isTracking = false; //break tracking on slew
			ksw->actionCollection()->action("track_object")->setIconSet( BarIcon( "decrypted" ) );
			setClickedObject( NULL );
			setFoundObject( NULL );//no longer tracking foundObject
			
			//Clear the planet trail list
			if ( ksw->data()->PlanetTrail.count() ) ksw->data()->PlanetTrail.clear();
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
		setDestination( focus() );
		slewing = false;
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
		SkyObject *solarminobj = ksw->data()->PC->findClosest(clickedPoint(), r);

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
				if ( r < rsolar_min ) {
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
			if ( ! o->catalog().isEmpty() && ksw->options()->drawOther ) checkObject = true;

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
				QList<SkyObject> cat = ksw->data()->CustomCatalogs[ ksw->options()->CatalogName[j] ];

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
			QList<SkyObject> cat;

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

void SkyMap::drawBoxes( QPixmap *pm ) {
	QPainter p;
	p.begin( pm );
	ksw->infoBoxes()->drawBoxes( p,
			ksw->options()->colorScheme()->colorNamed( "BoxTextColor" ),
			ksw->options()->colorScheme()->colorNamed( "BoxGrabColor" ),
			ksw->options()->colorScheme()->colorNamed( "BoxBGColor" ), false );
	p.end();
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
	drawPlanetTrail( psky );
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

void SkyMap::drawMilkyWay( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int ptsCount = 0;
	int mwmax = int( scale * pixelScale[ options->ZoomLevel ]/100.);
	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	//Draw Milky Way (draw this first so it's in the "background")
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "MWColor" ) ), 1, SolidLine ) ); //change to colorGrid
		psky.setBrush( QBrush( QColor( options->colorScheme()->colorNamed( "MWColor" ) ) ) );
		bool offscreen, lastoffscreen=false;

		for ( register unsigned int j=0; j<11; ++j ) {
			if ( options->fillMilkyWay ) {
				ptsCount = 0;
				bool partVisible = false;

				QPoint o = getXY( data->MilkyWay[j].at(0), options->useAltAz, options->useRefraction, scale );
				if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;

				for ( SkyPoint *p = data->MilkyWay[j].first(); p; p = data->MilkyWay[j].next() ) {
					o = getXY( p, options->useAltAz, options->useRefraction, scale );
					if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;
				}

				if ( ptsCount && partVisible ) {
					psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
	 	  	}
			} else {
	      QPoint o = getXY( data->MilkyWay[j].at(0), options->useAltAz, options->useRefraction, scale );
				if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
				else offscreen = false;

				psky.moveTo( o.x(), o.y() );
  	
				for ( register unsigned int i=1; i<data->MilkyWay[j].count(); ++i ) {
					o = getXY( ksw->data()->MilkyWay[j].at(i), options->useAltAz, options->useRefraction, scale );
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
}

void SkyMap::drawCoordinateGrid( QPainter& psky, double scale )
{
	KStarsOptions* options = ksw->options();
	QPoint cur;
	
	//Draw coordinate grid
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "GridColor" ) ), 1, DotLine ) ); //change to GridColor

		//First, the parallels
		for ( register double Dec=-80.; Dec<=80.; Dec += 20. ) {
			bool newlyVisible = false;
			sp->set( 0.0, Dec );
			if ( options->useAltAz ) sp->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			QPoint o = getXY( sp, options->useAltAz, options->useRefraction, scale );
			QPoint o1 = o;
			cur = o;
			psky.moveTo( o.x(), o.y() );

			double dRA = 1./15.; //180 points along full circle of RA
			for ( register double RA=dRA; RA<24.; RA+=dRA ) {
				sp->set( RA, Dec );
				if ( options->useAltAz ) sp->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );

				if ( checkVisibility( sp, guideFOV, guideXmax ) ) {
					o = getXY( sp, options->useAltAz, options->useRefraction, scale );

					//When drawing on the printer, the psky.pos() point does NOT get updated
					//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
					int dx = cur.x() - o.x();
					int dy = cur.y() - o.y();
					cur = o;
				
					if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
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
			if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
				psky.lineTo( o1.x(), o1.y() );
			} else {
				psky.moveTo( o1.x(), o1.y() );	
			}
		}

    //next, the meridians
		for ( register double RA=0.; RA<24.; RA += 2. ) {
			bool newlyVisible = false;
			SkyPoint *sp1 = new SkyPoint( RA, -90. );
			if ( options->useAltAz ) sp1->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			QPoint o = getXY( sp1, options->useAltAz, options->useRefraction, scale );
			cur = o;
			psky.moveTo( o.x(), o.y() );

			double dDec = 1.;
			for ( register double Dec=-89.; Dec<=90.; Dec+=dDec ) {
				sp1->set( RA, Dec );
				if ( options->useAltAz ) sp1->EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );

				if ( checkVisibility( sp1, guideFOV, guideXmax ) ) {
					o = getXY( sp1, options->useAltAz, options->useRefraction, scale );
				
					//When drawing on the printer, the psky.pos() point does NOT get updated
					//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
					int dx = cur.x() - o.x();
					int dy = cur.y() - o.y();
					cur = o;
				
					if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
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
}

void SkyMap::drawEquator( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	//Draw Equator (currently can't be hidden on slew)
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "EqColor" ) ), 1, SolidLine ) );

		SkyPoint *p = data->Equator.first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		QPoint o1 = o;
		QPoint last = o;
		QPoint cur = o;
		psky.moveTo( o.x(), o.y() );
		bool newlyVisible = false;

		//start loop at second item
		for ( p = data->Equator.next(); p; p = data->Equator.next() ) {
			if ( checkVisibility( p, guideFOV, guideXmax ) ) {
				o = getXY( p, options->useAltAz, options->useRefraction, scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;
				
				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
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
		if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }
}

void SkyMap::drawEcliptic( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Ecliptic (currently can't be hidden on slew)
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "EclColor" ) ), 1, SolidLine ) ); //change to colorGrid

		SkyPoint *p = data->Ecliptic.first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		QPoint o1 = o;
		QPoint last = o;
		QPoint cur = o;
		psky.moveTo( o.x(), o.y() );

		bool newlyVisible = false;
		//Start loop at second item
		for ( p = data->Ecliptic.next(); p; p = data->Ecliptic.next() ) {
			if ( checkVisibility( p, guideFOV, guideXmax ) ) {
				o = getXY( p, options->useAltAz, options->useRefraction, scale );

				//When drawing on the printer, the psky.pos() point does NOT get updated
				//when lineTo or moveTo are called.  Grrr.  Need to store current position in QPoint cur.
				int dx = cur.x() - o.x();
				int dy = cur.y() - o.y();
				cur = o;
				
				if ( abs(dx) < guidemax*scale && abs(dy) < guidemax*scale ) {
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
		if ( abs(dx) < Width && abs(dy) < Height ) {
			psky.lineTo( o1.x(), o1.y() );
		} else {
			psky.moveTo( o1.x(), o1.y() );
		}
  }
}

void SkyMap::drawConstellationLines( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	//Draw Constellation Lines
	if ( true ) {
		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "CLineColor" ) ), 1, SolidLine ) ); //change to colorGrid
		int iLast = -1;

		for ( SkyPoint *p = data->clineList.first(); p; p = data->clineList.next() ) {
			QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );

			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
				if ( data->clineModeList.at(data->clineList.at())->latin1()=='M' ) {
					psky.moveTo( o.x(), o.y() );
				} else if ( data->clineModeList.at(data->clineList.at())->latin1()=='D' ) {
					if ( data->clineList.at()== (int)(iLast+1) ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
  	      }
				}
				iLast = data->clineList.at();
			}
		}
  }
}

void SkyMap::drawConstellationNames( QPainter& psky, QFont& stdFont, double scale ) {
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	//Draw Constellation Names
	//Don't draw names if slewing
	if ( true ) {
		psky.setFont( stdFont );
		psky.setPen( QColor( options->colorScheme()->colorNamed( "CNameColor" ) ) );
		for ( SkyObject *p = data->cnameList.first(); p; p = data->cnameList.next() ) {
			if ( checkVisibility( p, FOV, Xmax ) ) {
				QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
				if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
					if ( options->useLatinConstellNames ) {
						int dx = 5*p->name().length();
						psky.drawText( o.x()-dx, o.y(), p->name() );  // latin constellation names
					} else if ( options->useLocalConstellNames ) {
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
}

void SkyMap::drawStars( QPainter& psky, double scale ) {
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	bool checkSlewing = ( ( slewing || ( clockSlewing && ksw->getClock()->isActive() ) )
				&& options->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool hideFaintStars( checkSlewing && options->hideStars );

	//Draw Stars
	if ( options->drawSAO ) {
		float maglim;
		float maglim0 = options->magLimitDrawStar;
		float zoomlim = 7.0 + float( options->ZoomLevel )/4.0;

		if ( maglim0 < zoomlim ) maglim = maglim0;
		else maglim = zoomlim;

	  //Only draw bright stars if slewing
		if ( hideFaintStars && maglim > options->magLimitHideStar ) maglim = options->magLimitHideStar;
		
		for ( StarObject *curStar = data->starList.first(); curStar; curStar = data->starList.next() ) {
			// break loop if maglim is reached
			if ( curStar->mag() > maglim ) break;

			if ( checkVisibility( curStar, FOV, Xmax ) ) {
				QPoint o = getXY( curStar, options->useAltAz, options->useRefraction, scale );

				// draw star if currently on screen
				if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
					// but only if the star is bright enough.
					float mag = curStar->mag();
					float sizeFactor = 2.0;
					int size = int( sizeFactor*(zoomlim - mag) ) + 1;
					if (size>23) size=23;

					if ( size > 0 ) {
						psky.setPen( QColor( options->colorScheme()->colorNamed( "SkyColor" ) ) );
						drawSymbol( psky, curStar->type(), o.x(), o.y(), size, 1.0, 0, curStar->color(), scale );

						// now that we have drawn the star, we can display some extra info
						if ( !checkSlewing && (curStar->mag() <= options->magLimitDrawStarInfo )
								&& (options->drawStarName || options->drawStarMagnitude ) ) {
							// collect info text
							QString sTmp = "";
							if ( options->drawStarName ) {
								if (curStar->name() != "star") sTmp = curStar->name() + " ";	// see line below
							}
							if ( options->drawStarMagnitude ) {
								sTmp += QString().sprintf("%.1f", curStar->mag() );
							}
							
							if ( ! sTmp.isEmpty() ) {
								int offset = int( scale * (3 + int(0.5*(5.0-mag)) + int(0.5*( options->ZoomLevel - 6)) ));

								psky.setPen( QColor( options->colorScheme()->colorNamed( "SNameColor" ) ) );
								psky.drawText( o.x()+offset, o.y()+offset, sTmp );
							}
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawDeepSkyCatalog( QPainter& psky, QList<SkyObject>& catalog, QColor& color, 
			bool drawObject, bool drawImage, double scale )
{
	KStarsOptions* options = ksw->options();
	QImage ScaledImage;

	int Width = int( scale * width() );
	int Height = int( scale * height() );
  
	// Set color once
	psky.setPen( color );
	psky.setBrush( NoBrush );
	QColor colorHST  = options->colorScheme()->colorNamed( "HSTColor" );
	
	//Draw Deep-Sky Objects
	for ( SkyObject *obj = catalog.first(); obj; obj = catalog.next() ) {

		if ( drawObject || drawImage ) {
			if ( checkVisibility( obj, FOV, Xmax ) ) {
				QPoint o = getXY( obj, options->useAltAz, options->useRefraction, scale );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
					int PositionAngle = findPA( obj, o.x(), o.y() );

					//Draw Image
					if ( drawImage && options->ZoomLevel >3 ) {
						QFile file;

						//readImage reads image from disk, or returns pointer to existing image.
						QImage *image=obj->readImage();
						if ( image ) {
							int w = int( obj->a()*dms::PI*scale*pixelScale[ options->ZoomLevel ]/10800.0 );
							int h = int( w*image->height()/image->width() ); //preserve image's aspect ratio
							int dx = int( 0.5*w );
							int dy = int( 0.5*h );
							ScaledImage = image->smoothScale( w, h );
							
							//store current xform translation:
							QPoint oldXForm = psky.xForm( QPoint(0,0) );
							
							psky.translate( o.x(), o.y() );
							psky.rotate( double( PositionAngle ) );  //rotate the coordinate system
							psky.drawImage( -dx, -dy, ScaledImage );
							psky.resetXForm();
							
							//restore old xform
							psky.translate( oldXForm.x(), oldXForm.y() );
						}
					}

					//Draw Symbol
					if ( drawObject ) {
						//change color if extra images are available
            // most objects don't have those, so we only change colors temporarily
            // for the few exceptions. Changing color is expensive!!!
            bool bColorChanged = false;
						if ( obj->isCatalogM() && obj->ImageList.count() > 1 ) {
							psky.setPen( colorHST );
              bColorChanged = true;
            }
						else if ( (!obj->isCatalogM()) && obj->ImageList.count() ) {
							psky.setPen( colorHST );
              bColorChanged = true;
            }

						float majorAxis = obj->a();
						// if size is 0.0 set it to 1.0, this are normally stars (type 0 and 1)
						// if we use size 0.0 the star wouldn't be drawn
						if ( majorAxis == 0.0 && obj->type() < 2 ) {
							majorAxis = 1.0;
						}

						//scale parameter is included in drawSymbol, so we don't place it here in definition of Size
						int Size = int( majorAxis*dms::PI*pixelScale[ options->ZoomLevel ]/10800.0 );

						// use star draw function
						drawSymbol( psky, obj->type(), o.x(), o.y(), Size, obj->e(), PositionAngle, 0, scale );
            // revert temporary color change
            if ( bColorChanged ) {
							psky.setPen( color );
            }
					}
				}
			} else { //Object failed checkVisible(); delete it's Image pointer, if it exists.
				if ( obj->image() ) {
					obj->deleteImage();
				}
			}
		}
	}

}

void SkyMap::drawDeepSkyObjects( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );

	QImage ScaledImage;

	bool checkSlewing = ( ( slewing || ( clockSlewing && ksw->getClock()->isActive() ) )
				&& options->hideOnSlew );

//shortcuts to inform wheter to draw different objects
	bool drawMess( options->drawDeepSky && ( options->drawMessier || options->drawMessImages ) && !(checkSlewing && options->hideMess) );
	bool drawNGC( options->drawDeepSky && options->drawNGC && !(checkSlewing && options->hideNGC) );
	bool drawOther( options->drawDeepSky && options->drawOther && !(checkSlewing && options->hideOther) );
	bool drawIC( options->drawDeepSky && options->drawIC && !(checkSlewing && options->hideIC) );

	// calculate color objects once, outside the loop
	QColor colorMess = options->colorScheme()->colorNamed( "MessColor" );
	QColor colorIC  = options->colorScheme()->colorNamed( "ICColor" );
	QColor colorNGC  = options->colorScheme()->colorNamed( "NGCColor" );

	// draw Messier catalog
	if ( drawMess ) {
		bool drawObject = options->drawMessier;
		bool drawImage = options->drawMessImages;
		drawDeepSkyCatalog( psky, data->deepSkyListMessier, colorMess, drawObject, drawImage, scale );
	}

	// draw NGC Catalog
	if ( drawNGC ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
		drawDeepSkyCatalog( psky, data->deepSkyListNGC, colorNGC, drawObject, drawImage, scale );
	}

	// draw IC catalog
	if ( drawIC ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
	drawDeepSkyCatalog( psky, data->deepSkyListIC, colorIC, drawObject, drawImage, scale );
	}
  
	// draw the rest
	if ( drawOther ) {
		bool drawObject = true;
		bool drawImage = options->drawMessImages;
		//Use NGC color for now...
		drawDeepSkyCatalog( psky, data->deepSkyListOther, colorNGC, drawObject, drawImage, scale );
	}
  
	//Draw Custom Catalogs
	for ( register unsigned int i=0; i<options->CatalogCount; ++i ) { //loop over custom catalogs
		if ( options->drawCatalog[i] ) {

			psky.setBrush( NoBrush );
			psky.setPen( QColor( options->colorScheme()->colorNamed( "NGCColor" ) ) );

			QList<SkyObject> cat = data->CustomCatalogs[ options->CatalogName[i] ];

			for ( SkyObject *obj = cat.first(); obj; obj = cat.next() ) {

				if ( checkVisibility( obj, FOV, Xmax ) ) {
					QPoint o = getXY( obj, options->useAltAz, options->useRefraction, scale );

					if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {

						if ( obj->type()==0 || obj->type()==1 ) {
							StarObject *starobj = (StarObject*)obj;
							float zoomlim = 7.0 + float( options->ZoomLevel )/4.0;
							float mag = starobj->mag();
							float sizeFactor = 2.0;
							int size = int( sizeFactor*(zoomlim - mag) ) + 1;
							if (size>23) size=23;
							if ( size > 0 ) drawSymbol( psky, starobj->type(), o.x(), o.y(), size, starobj->color(), scale );
						} else {
							int size = pixelScale[ options->ZoomLevel ]/pixelScale[0];
							if (size>8) size = 8;
							drawSymbol( psky, obj->type(), o.x(), o.y(), size, scale );
						}
					}
				}
			}
		}
	}
}

void SkyMap::drawAttachedLabels( QPainter &psky, double scale ) {
	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	//Set color to contrast well with background
	QColor csky( ksw->options()->colorScheme()->colorNamed( "SkyColor" ) );
	int h,s,v;
	csky.hsv( &h, &s, &v );
	if ( v > 128 ) psky.setPen( QColor( "black" ) );
	else psky.setPen( QColor( "white" ) );

	for ( SkyObject *obj = ksw->data()->ObjLabelList.first(); obj; obj = ksw->data()->ObjLabelList.next() ) {
		if ( checkVisibility( obj, FOV, Xmax ) ) {
			QPoint o = getXY( obj, ksw->options()->useAltAz, ksw->options()->useRefraction, scale );
			if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
				int offset = int( scale * (3 + int(0.5*( ksw->options()->ZoomLevel - 6)) ));
				if ( obj->type() == 0 ) offset +=  + int(0.5*(5.0-obj->mag()));
				
				psky.drawText( o.x() + offset, o.y() + offset, obj->translatedName() );
			}
		}
	}
}

void SkyMap::drawPlanetTrail( QPainter& psky, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	if ( data->PlanetTrail.count() ) {
		QColor tcolor1 = QColor( options->colorScheme()->colorNamed( "EclColor" ) );
		QColor tcolor2 = QColor( options->colorScheme()->colorNamed( "SkyColor" ) );
		
		SkyPoint *p = data->PlanetTrail.first();
		QPoint o = getXY( p, options->useAltAz, options->useRefraction, scale );
		bool firstPoint(false);
		int i = 0;
		int n = data->PlanetTrail.count();
		
		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo(o.x(), o.y());
			firstPoint = true;
		}
		
		for ( p = data->PlanetTrail.next(); p; p = data->PlanetTrail.next() ) {
			//Define interpolated color
			QColor tcolor = QColor( 
						(i*tcolor1.red()   + (n-i)*tcolor2.red())/n,
						(i*tcolor1.green() + (n-i)*tcolor2.green())/n,
						(i*tcolor1.blue()  + (n-i)*tcolor2.blue())/n );
			psky.setPen( tcolor );
			++i;
			
			o = getXY( p, options->useAltAz, options->useRefraction, scale );
			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
					
					if ( firstPoint ) {
						psky.lineTo( o.x(), o.y() );
					} else {
						psky.moveTo( o.x(), o.y() );
						firstPoint = true;
					}
			}
		}
	}
}


void SkyMap::drawSolarSystem( QPainter& psky, bool drawPlanets, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	//Draw Sun
	if ( options->drawSun && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Sun"), QColor( "Yellow" ), 8, 2.4, 3, 1, scale );
	}

	//Draw Moon
	if ( options->drawMoon && drawPlanets ) {
		drawPlanet(psky, data->Moon, QColor( "White" ), 8, 2.5, -1, 1, scale );
	}

	//Draw Mercury
	if ( options->drawMercury && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Mercury"), QColor( "SlateBlue1" ), 4, 0.04, 4, 1, scale );
	}

	//Draw Venus
	if ( options->drawVenus && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Venus"), QColor( "LightGreen" ), 4, 0.05, 2, 1, scale );
	}

	//Draw Mars
	if ( options->drawMars && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Mars"), QColor( "Red" ), 4, 0.00555, 2, 1, scale );
	}

	//Draw Jupiter
	if ( options->drawJupiter && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Jupiter"), QColor( "Goldenrod" ), 4, 0.05, 2, 1, scale );
		
		//Draw Jovian moons
		psky.setPen( QPen( QColor( "white" ) ) );
		if ( options->ZoomLevel > 5 ) {
			QFont pfont = psky.font();
			QFont moonFont = psky.font();
			moonFont.setPointSize( pfont.pointSize() - 2 );
			psky.setFont( moonFont );
			
			for ( unsigned int i=0; i<5; ++i ) {
				QPoint o = getXY( data->jmoons->pos(i), options->useAltAz, options->useRefraction, scale );
				if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
					psky.drawEllipse( o.x()-1, o.y()-1, 2, 2 );
					
					//Draw Moon name labels if at high zoom
					if ( options->ZoomLevel > 15 ) {
						int offset = int(3*scale);
						psky.drawText( o.x() + offset, o.y() + offset, data->jmoons->name(i) );
						
					}
				}
			}
			
			//reset font
			psky.setFont( pfont );
		} 
	}

	//Draw Saturn
	if ( options->drawSaturn && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Saturn"), QColor( "LightYellow2" ), 4, 0.05, 2, 2, scale );
	}

	//Draw Uranus
	if ( options->drawUranus && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Uranus"), QColor( "LightSeaGreen" ), 4, 0.007, 2, 1, scale );
	}

	//Draw Neptune
	if ( options->drawNeptune && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Neptune"), QColor( "SkyBlue" ), 4, 0.0035, 2, 1, scale );
	}

	//Draw Pluto
	if ( options->drawPluto && drawPlanets ) {
		drawPlanet(psky, data->PC->findByName("Pluto"), QColor( "gray" ), 4, 0.001, 4, 1, scale );
	}

	//Draw Asteroids
	if ( options->drawAsteroids && drawPlanets ) {
		for ( KSAsteroid *ast = data->asteroidList.first(); ast; ast = data->asteroidList.next() ) {
			psky.setPen( QPen( QColor( "gray" ) ) );
			psky.setBrush( QBrush( QColor( "gray" ) ) );
			//if ( options->ZoomLevel > 3 ) {
				QPoint o = getXY( ast, options->useAltAz, options->useRefraction, scale );

				if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
					
					int size = int( 0.05 * scale * pixelScale[ ksw->options()->ZoomLevel ]/pixelScale[0] );
					if ( size < 2 ) size = 2;
					int x1 = o.x() - size/2;
					int y1 = o.y() - size/2;
					psky.drawEllipse( x1, y1, size, size );
				
					//draw Name
					if ( ksw->options()->drawAsteroidName ) {
						int offset( int( 0.5*size ) );
						if ( offset < 2 ) offset = 2;

						psky.setPen( QColor( ksw->options()->colorScheme()->colorNamed( "PNameColor" ) ) );
						psky.drawText( o.x()+offset, o.y()+offset, ast->translatedName() );
					}
				}
			//}
		}
	}

	//Draw Comets
	if ( options->drawComets && drawPlanets ) {
		for ( KSComet *com = data->cometList.first(); com; com = data->cometList.next() ) {
			psky.setPen( QPen( QColor( "cyan4" ) ) );
			psky.setBrush( QBrush( QColor( "cyan4" ) ) );
			//if ( options->ZoomLevel > 3 ) {
				QPoint o = getXY( com, options->useAltAz, options->useRefraction, scale );

				if ( ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) ) {
					
					int size = int( 0.05 * scale * pixelScale[ ksw->options()->ZoomLevel ]/pixelScale[0] );
					if ( size < 2 ) size = 2;
					int x1 = o.x() - size/2;
					int y1 = o.y() - size/2;
					psky.drawEllipse( x1, y1, size, size );
					
					//draw Name
					if ( ksw->options()->drawCometName ) {
						int offset( int( 0.5*size ) );
						if ( offset < 2 ) offset = 2;

						psky.setPen( QColor( ksw->options()->colorScheme()->colorNamed( "PNameColor" ) ) );
						psky.drawText( o.x()+offset, o.y()+offset, com->translatedName() );
					}
				}
			//}
		}
	}
}

void SkyMap::drawPlanet( QPainter &psky, KSPlanetBase *p, QColor c,
		int sizemin, double mult, int zoommin, int resize_mult, double scale ) {
	
	int Width = int( scale * width() );
	int Height = int( scale * height() );
	sizemin = int( sizemin * scale );
	
	psky.setPen( c );
	psky.setBrush( c );
	QPoint o = getXY( p, ksw->options()->useAltAz, ksw->options()->useRefraction, scale );
	
	if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) {
		//Image size must be modified to account for possibility that rotated image's
		//size is bigger than original image size.
		int size = int( mult * scale * pixelScale[ ksw->options()->ZoomLevel ]/pixelScale[0] * p->image()->width()/p->image0()->width() );
		if ( size < sizemin ) size = sizemin;
		int x1 = o.x() - size/2;
		int y1 = o.y() - size/2;

                                             //Only draw planet image if:
		if ( ksw->options()->drawPlanetImage &&  //user wants them,
				ksw->options()->ZoomLevel > zoommin &&  //zoomed in enough,
				!p->image()->isNull() &&             //image loaded ok,
				size < Width ) {                   //and size isn't too big.

			if (resize_mult != 1) {
				size *= resize_mult;
				x1 = o.x() - size/2;
				y1 = o.y() - size/2;
			}

			//Determine position angle of planet (assuming that it is aligned with
			//the Ecliptic, which is only roughly correct).
			//Displace a point along +Ecliptic Latitude by 1 degree
      SkyPoint test, test2;
			KSNumbers num( ksw->data()->CurrentDate );

			dms newELat( p->ecLat()->Degrees() + 1.0 );
			test.setFromEcliptic( num.obliquity(), p->ecLong(), &newELat );
			if ( ksw->options()->useAltAz ) test.EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			
			double dx = test.ra()->Degrees() - p->ra()->Degrees();
			double dy = p->dec()->Degrees() - test.dec()->Degrees();
			
			double pa;
			if ( dy ) {
				pa = atan( dx/dy )*180.0/dms::PI;
			} else {
				pa = 90.0;
				if ( dx > 0 ) pa = -90.0;
			}

			p->setPA( pa );

			//rotate Planet image, if necessary
			//image angle is PA plus the North angle.
			
			//Find North angle:
			test.set( p->ra()->Hours(), p->dec()->Degrees() + 1.0 );
			if ( ksw->options()->useAltAz ) test.EquatorialToHorizontal( ksw->LSTh(), ksw->geo()->lat() );
			QPoint t = getXY( &test, ksw->options()->useAltAz, ksw->options()->useRefraction, scale );
			dx = double( o.x() - t.x() );  //backwards to get counterclockwise angle
			dy = double( t.y() - o.y() );
			double north;
			if ( dy ) {
				north = atan( dx/dy )*180.0/dms::PI;
			} else {
				north = 90.0;
				if ( dx > 0 ) north = -90.0;
			}
			
			//rotate Image
			
			p->rotateImage( p->pa() + north );
			
			psky.drawImage( x1, y1,  p->image()->smoothScale( size, size ));

		} else { //Otherwise, draw a simple circle.

			psky.drawEllipse( x1, y1, size, size );
		}

		//draw Name
		if ( ksw->options()->drawPlanetName ) {
			int offset( int( 0.5*size ) );
			if ( offset < sizemin ) offset = sizemin;

			psky.setPen( QColor( ksw->options()->colorScheme()->colorNamed( "PNameColor" ) ) );
			psky.drawText( o.x()+offset, o.y()+offset, p->translatedName() );
		}
	}
}

	//Label the clicked Object...commenting out for now
	//I'd like the text to "fade out", but this will require masking the skymap image
  //and updating just the portion of the label on a more rapid timescale than the rest
  //It's a lot of work for something completely superfluous, but I don't think the label
  //will work unless it's not persistent, and I don't want it to just disappear.
  //Maybe I'll do a tooltip window instead.
/*
	if ( labelClickedObject && clickedObject() ) {
		if ( checkVisibility( clickedObject(), FOV, Xmax, options->useAltAz, isPoleVisible ) ) {
			QPoint o = getXY( clickedObject(), options->useAltAz, options->useRefraction );
			if (o.x() >= 0 && o.x() <= Width() && o.y() >=0 && o.y() <= height() ) {
				int dx(20), dy(20);
				if ( clickedObject()->a() ) {
					dx = int( clickedObject()->a()*dms::PI*pixelScale[ ksw->options()->ZoomLevel ]/10800.0 );
					dy = int( dx*clickedObject()->e() );
				}
				psky.setBrush( QColor( "White" ) );
				psky.drawText( o.x()+dy, o.y()+dy, clickedObject()->name() );  // latin constellation names
			}
		}
	}
*/

void SkyMap::drawHorizon( QPainter& psky, QFont& stdFont, double scale )
{
	KStarsData* data = ksw->data();
	KStarsOptions* options = ksw->options();

	int Width = int( scale * width() );
	int Height = int( scale * height() );
	
	QList<QPoint> points;
	points.setAutoDelete(true);
	//Draw Horizon
	//The horizon should not be corrected for atmospheric refraction, so getXY has doRefract=false...
	if (options->drawHorizon || options->drawGround) {
		QPoint OutLeft(0,0), OutRight(0,0);

		psky.setPen( QPen( QColor( options->colorScheme()->colorNamed( "HorzColor" ) ), 1, SolidLine ) ); //change to colorGrid
		psky.setBrush( QColor ( options->colorScheme()->colorNamed( "HorzColor" ) ) );
		int ptsCount = 0;

		int maxdist = pixelScale[ options->ZoomLevel ]/4;

		for ( SkyPoint *p = data->Horizon.first(); p; p = data->Horizon.next() ) {
			QPoint *o = new QPoint();
			*o = getXY( p, options->useAltAz, false, scale );  //false: do not refract the horizon
			bool found = false;

			//Use the QList of points to pre-sort visible horizon points
//			if ( o->x() > -1*maxdist && o->x() < width() + maxdist ) {
			if ( o->x() > -100 && o->x() < Width + 100 && o->y() > -100 && o->y() < Height + 100 ) {
				if ( options->useAltAz ) {
					register unsigned int j;
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
				if ( ( OutRight.x() == 0 || o->x() < OutRight.x() ) && o->x() >  + 100 ) {
					OutRight.setX( o->x() );
					OutRight.setY( o->y() );
				}
				// delete non stored points to avoid memory leak
				delete o;
			}
		}

		//Add left-edge and right-edge points based on interpolating the first/last onscreen points
		//to the nearest offscreen points.
		if ( options->useAltAz && points.count() > 0 ) {
     //Interpolate from first sorted onscreen point to x=-100,
     //using OutLeft to determine the slope
			int xtotal = ( points.at( 0 )->x() - OutLeft.x() );
			int xx = ( points.at( 0 )->x() + 100 ) / xtotal;
			int yp = xx*OutRight.y() + (1-xx)*points.at( 0 )->y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.insert( 0, LeftEdge ); //Prepend LeftEdge to the beginning of points

    	//Interpolate from the last sorted onscreen point to ()+100,
			//using OutRight to determine the slope.
			xtotal = ( OutRight.x() - points.at( points.count() - 1 )->x() );
			xx = ( Width + 100 - points.at( points.count() - 1 )->x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*points.at( points.count() - 1 )->y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( Width+100, yp );
			points.append( RightEdge );

//If there are no horizon points, then either the horizon doesn't pass through the screen
//or we're at high zoom, and horizon points lie on either side of the screen.
		} else if ( options->useAltAz && OutLeft.y() !=0 && OutRight.y() !=0 &&
            !( OutLeft.y() > Height + 100 && OutRight.y() > Height + 100 ) &&
            !( OutLeft.y() < -100 && OutRight.y() < -100 ) ) {

     //It's possible at high zoom that /no/ horizon points are onscreen.  In this case,
     //interpolate between OutLeft and OutRight directly to construct the horizon polygon.
			int xtotal = ( OutRight.x() - OutLeft.x() );
			int xx = ( OutRight.x() + 100 ) / xtotal;
			int yp = xx*OutLeft.y() + (1-xx)*OutRight.y();  //interpolated left-edge y value
			QPoint *LeftEdge = new QPoint( -100, yp );
			points.append( LeftEdge );

			xx = ( Width + 100 - OutLeft.x() ) / xtotal;
			yp = xx*OutRight.y() + (1-xx)*OutLeft.y(); //interpolated right-edge y value
			QPoint *RightEdge = new QPoint( Width+100, yp );
			points.append( RightEdge );
 		}
		
		if ( points.count() ) {
//		Fill the pts array with sorted horizon points, Draw Horizon Line
			pts->setPoint( 0, points.at(0)->x(), points.at(0)->y() );
			if ( options->drawHorizon ) psky.moveTo( points.at(0)->x(), points.at(0)->y() );

			for ( register unsigned int i=1; i<points.count(); ++i ) {
				pts->setPoint( i, points.at(i)->x(), points.at(i)->y() );

				if ( options->drawHorizon ) {
					if ( !options->useAltAz && ( abs( points.at(i)->x() - psky.pos().x() ) > maxdist ||
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

//DUMP_HORIZON
/*
			if (dumpHorizon) {
				//First, make sure output file doesn't exist yet (so this only happens once)
				QFile dumpFile( "horizon.xy" );
				if ( !dumpFile.exists() && dumpFile.open( IO_WriteOnly ) ) {
					QTextStream t( &dumpFile );
					for ( register uint i=0; i < points.count(); ++i ) {
						t << points.at(i)->x() << " " << points.at(i)->y() << endl;
					}
					dumpFile.close();
				}

				dumpHorizon = false;
			}
*/
//END_DUMP_HORIZON

//		Finish the Ground polygon by adding a square bottom edge, offscreen
			if ( options->useAltAz && options->drawGround ) {
				ptsCount = points.count();
				pts->setPoint( ptsCount++, Width+100, Height+100 );   //bottom right corner
				pts->setPoint( ptsCount++, -100, Height+100 );         //bottom left corner
 	
				psky.drawPolygon( ( const QPointArray ) *pts, false, 0, ptsCount );

//  remove all items in points list
				for ( register unsigned int i=0; i<points.count(); ++i ) {
					points.remove(i);
				}

//	Draw compass heading labels along horizon
				SkyPoint *c = new SkyPoint;
				QPoint cpoint;
				psky.setPen( QColor ( options->colorScheme()->colorNamed( "CompassColor" ) ) );
				psky.setFont( stdFont );

		//North
				c->setAz( 359.99 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "North", "N" ) );
				}
 	
		//NorthEast
				c->setAz( 45.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northeast", "NE" ) );
				}

		//East
				c->setAz( 90.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "East", "E" ) );
				}

		//SouthEast
				c->setAz( 135.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southeast", "SE" ) );
				}

		//South
				c->setAz( 179.99 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "South", "S" ) );
				}

		//SouthWest
				c->setAz( 225.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Southwest", "SW" ) );
				}

		//West
				c->setAz( 270.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "West", "W" ) );
				}

		//NorthWest
				c->setAz( 315.0 );
				c->setAlt( 0.0 );
				if ( !options->useAltAz ) c->HorizontalToEquatorial( ksw->LSTh(), ksw->geo()->lat() );
				cpoint = getXY( c, options->useAltAz, false, scale );
				cpoint.setY( cpoint.y() + int(scale*20) );
				if (cpoint.x() > 0 && cpoint.x() < Width && cpoint.y() > 0 && cpoint.y() < Height ) {
					psky.drawText( cpoint.x(), cpoint.y(), i18n( "Northwest", "NW" ) );
				}

				delete c;
			}
		}
	}  //endif drawing horizon
}

void SkyMap::drawTargetSymbol( QPainter &psky, int style ) {
	//Draw this last so it is never "behind" other things.
	psky.setPen( QPen( QColor( ksw->options()->colorScheme()->colorNamed("TargetColor" ) ) ) );
	psky.setBrush( NoBrush );
	int pxperdegree = int(pixelScale[ ksw->options()->ZoomLevel ]/57.3);
	
	switch ( style ) {
		case 1: { //simple circle, one degree in diameter.
			int size = pxperdegree;
			psky.drawEllipse( width()/2-size/2, height()/2-size/2, size, size );
			break;
		}
		case 2: { //case 1, fancy crosshairs
			int s1 = pxperdegree/2;
			int s2 = pxperdegree;
			int s3 = 2*pxperdegree;
			
			int x0 = width()/2;  int y0 = height()/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;
			
			//Draw radial lines
			psky.drawLine( x1, y0, x3, y0 );
			psky.drawLine( x0+s2, y0, x0+s1/2, y0 );
			psky.drawLine( x0, y1, x0, y3 );
			psky.drawLine( x0, y0+s1/2, x0, y0+s2 );
			
			//Draw circles at 0.5 & 1 degrees
			psky.drawEllipse( x1, y1, s1, s1 );
			psky.drawEllipse( x2, y2, s2, s2 );
			
			break;
		}
		
		case 3: { //Bullseye
			int s1 = pxperdegree/2;
			int s2 = pxperdegree;
			int s3 = 2*pxperdegree;
			
			int x0 = width()/2;  int y0 = height()/2;
			int x1 = x0 - s1/2;  int y1 = y0 - s1/2;
			int x2 = x0 - s2/2;  int y2 = y0 - s2/2;
			int x3 = x0 - s3/2;  int y3 = y0 - s3/2;
		
			psky.drawEllipse( x1, y1, s1, s1 );
			psky.drawEllipse( x2, y2, s2, s2 );
			psky.drawEllipse( x3, y3, s3, s3 );
			
			break;
		}
		
		case 4: { //Rectangle
			int s = pxperdegree;
			int x1 = width()/2 - s/2;
			int y1 = height()/2 - s/2;
			
			psky.drawRect( x1, y1, s, s );
			break;
		}
		
	}
}

