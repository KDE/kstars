/***************************************************************************
                          skymapdraw.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Mar 2 2003
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

//This file contains drawing functions SkyMap class.

#include <stdlib.h> // abs
#include <math.h> //log10()
#include <iostream>

#include <QPainter>
#include <QPixmap>

#include "skymap.h"
#include "Options.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "ksnumbers.h"
#include "skyobject.h"
#include "deepskyobject.h"
#include "starobject.h"
#include "ksplanetbase.h"
#include "ksasteroid.h"
#include "kscomet.h"
#include "ksmoon.h"
#include "jupitermoons.h"
#include "infoboxes.h"
#include "simclock.h"
#include "csegment.h"
#include "devicemanager.h"
#include "indimenu.h"
#include "indiproperty.h"
#include "indielement.h"
#include "indidevice.h"
#include "observinglist.h"

void SkyMap::drawOverlays( QPixmap *pm ) {
	if ( ksw ) { //only if we are drawing in the GUI window
		QPainter p;
		p.begin( pm );
		p.setRenderHint(QPainter::Antialiasing, true);

		showFocusCoords( true );
		drawBoxes( p );

		//draw FOV symbol
		ksw->data()->fovSymbol.draw( p, (float)(Options::fOVSize() * Options::zoomFactor()/57.3/60.0) );
		drawTelescopeSymbols( p );
		drawObservingList( p );
		drawZoomBox( p );
		if ( transientObject() ) drawTransientLabel( p );
		if (isAngleMode()) {
			updateAngleRuler();
			drawAngleRuler( p );
		}
	}
}

void SkyMap::drawAngleRuler( QPainter &p ) {
	p.setPen( QPen( data->colorScheme()->colorNamed( "AngularRuler" ), 1.0, Qt::DotLine ) );
	p.drawLine( beginRulerPoint, endRulerPoint );
}

void SkyMap::drawZoomBox( QPainter &p ) {
	//draw the manual zoom-box, if it exists
	if ( ZoomRect.isValid() ) {
		p.setPen( QPen( Qt::white, 1.0, Qt::DotLine ) );
		p.drawRect( ZoomRect.x(), ZoomRect.y(), ZoomRect.width(), ZoomRect.height() );
	}
}

void SkyMap::drawTransientLabel( QPainter &p ) {
	if ( transientObject() ) {
		p.setPen( TransientColor );

		if ( checkVisibility( transientObject() ) ) {
			QPointF o = getXY( transientObject(), Options::useAltAz(), Options::useRefraction(), 1.0 );
			if ( o.x() >= 0. && o.x() <= width() && o.y() >= 0. && o.y() <= height() ) {
				drawNameLabel( p, transientObject(), o.x(), o.y(), 1.0 );
			}
		}
	}
}

void SkyMap::drawBoxes( QPainter &p ) {
	if ( ksw ) { //only if we are drawing in the GUI window
		ksw->infoBoxes()->drawBoxes( p,
				data->colorScheme()->colorNamed( "BoxTextColor" ),
				data->colorScheme()->colorNamed( "BoxGrabColor" ),
				data->colorScheme()->colorNamed( "BoxBGColor" ), Options::boxBGMode() );
	}
}

void SkyMap::drawObservingList( QPainter &psky, double scale ) {
	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "ObsListColor" ) ), 1 ) );

	if ( ksw && ksw->observingList()->obsList().size() ) {
		foreach ( SkyObject* obj, ksw->observingList()->obsList() ) {
			if ( checkVisibility( obj ) ) {
				QPointF o = getXY( obj, Options::useAltAz(), Options::useRefraction() );

				// label object if it is currently on screen
				if (o.x() >= 0. && o.x() <= width() && o.y() >=0. && o.y() <= height() ) {
					if ( Options::obsListSymbol() ) {
						float size = 20.*scale;
						float x1 = o.x() - 0.5*size;
						float y1 = o.y() - 0.5*size;
						psky.drawArc( QRectF(x1, y1, size, size), -60*16, 120*16 );
						psky.drawArc( QRectF(x1, y1, size, size), 120*16, 120*16 );
					}
					if ( Options::obsListText() ) {
						drawNameLabel( psky, obj, o.x(), o.y(), scale );
					}
				}
			}
		}
	}
}

void SkyMap::drawNameLabel( QPainter &psky, SkyObject *obj, float x, float y, double scale ) {
	int size(0);

	QFont stdFont( psky.font() );
	QFont smallFont( stdFont );
	smallFont.setPointSize( stdFont.pointSize() - 2 );
	if ( Options::zoomFactor() < 10.*MINZOOM ) {
		psky.setFont( smallFont );
	} else {
		psky.setFont( stdFont );
	}

	//Stars
	if ( obj->type() == SkyObject::STAR ) {
		((StarObject*)obj)->drawLabel( psky, x, y, Options::zoomFactor(), true, false, scale );
		psky.setFont( stdFont );
		return;

	//Solar system
	} else if ( obj->type() == SkyObject::PLANET
						|| obj->type() == SkyObject::ASTEROID
						|| obj->type() == SkyObject::COMET ) {
		KSPlanetBase *p = (KSPlanetBase*)obj;
		size = int( p->angSize() * scale * dms::PI * Options::zoomFactor()/10800.0 );
		int minsize = 4;
		if ( p->type() == SkyObject::ASTEROID || p->type() == SkyObject::COMET )
			minsize = 2;
		if ( p->name() == "Sun" || p->name() == "Moon" )
			minsize = 8;
		if ( size < minsize )
			size = minsize;
		if ( p->name() == "Saturn" )
			size = int(2.5*size);

	//Other
	} else {
		//Calculate object size in pixels
		float majorAxis = ((DeepSkyObject*)obj)->a();
		if ( majorAxis == 0.0 && obj->type() == 1 ) majorAxis = 1.0; //catalog stars
		size = int( majorAxis * scale * dms::PI * Options::zoomFactor()/10800.0 );
	}

	float offset = 0.5*size + 4.;
	psky.drawText( QPointF(x+offset, y+offset), obj->translatedName() );

	//Reset font
	psky.setFont( stdFont );
}

void SkyMap::drawTelescopeSymbols(QPainter &psky) {
 
/* NOTE We're using TelescopeComponent now. Remove this later on

	if ( ksw ) { //ksw doesn't exist in non-GUI mode!
		INDI_P *eqNum;
		INDI_P *portConnect;
		INDI_E *lp;
		INDIMenu *devMenu = ksw->getINDIMenu();
		bool useJ2000 (false), useAltAz(false);
		SkyPoint indi_sp;

		if (!Options::indiCrosshairs() || devMenu == NULL)
			return;

		psky.setPen( QPen( QColor( data->colorScheme()->colorNamed("TargetColor" ) ) ) );
		psky.setBrush( Qt::NoBrush );
		float pxperdegree = Options::zoomFactor()/57.3;

		//fprintf(stderr, "in draw telescope function with mgrsize of %d\n", devMenu->mgr.size());
		for ( int i=0; i < devMenu->mgr.size(); i++ )
		{
			for ( int j=0; j < devMenu->mgr.at(i)->indi_dev.size(); j++ )
			{
				useAltAz = false;
				useJ2000 = false;

				// make sure the dev is on first
				if (devMenu->mgr.at(i)->indi_dev.at(j)->isOn())
				{
				        portConnect = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("CONNECTION");

					if (!portConnect)
					 return;

					 if (portConnect->state == PS_BUSY)
					  return;

					eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_EOD_COORD");
					
					if (eqNum == NULL)
					{
						eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("EQUATORIAL_COORD");
						if (eqNum == NULL)
						{
							eqNum = devMenu->mgr.at(i)->indi_dev.at(j)->findProp("HORIZONTAL_COORD");
							if (eqNum == NULL) continue;
							else
								useAltAz = true;
						}
						else
							useJ2000 = true;
					}

					// make sure it has RA and DEC properties
					if ( eqNum)
					{
						//fprintf(stderr, "Looking for RA label\n");
                                                if (useAltAz)
						{
						lp = eqNum->findElement("AZ");
						if (!lp)
							continue;

						dms azDMS(lp->value);
						
						lp = eqNum->findElement("ALT");
						if (!lp)
							continue;

						dms altDMS(lp->value);

						indi_sp.setAz(azDMS);
						indi_sp.setAlt(altDMS);

						}
						else
						{

						lp = eqNum->findElement("RA");
						if (!lp)
							continue;

						// express hours in degrees on the celestial sphere
						dms raDMS(lp->value);
						raDMS.setD ( raDMS.Degrees() * 15.0);

						lp = eqNum->findElement("DEC");
						if (!lp)
							continue;

						dms decDMS(lp->value);


						//kDebug() << "the KStars RA is " << raDMS.toHMSString() << endl;
						//kDebug() << "the KStars DEC is " << decDMS.toDMSString() << "\n****************" << endl;

						indi_sp.setRA(raDMS);
						indi_sp.setDec(decDMS);

						if (useJ2000)
						{
							indi_sp.setRA0(raDMS);
							indi_sp.setDec0(decDMS);
							indi_sp.apparentCoord( (double) J2000, ksw->data()->ut().djd());
						}
							
						if ( Options::useAltAz() ) indi_sp.EquatorialToHorizontal( ksw->LST(), ksw->geo()->lat() );

						}

						QPointF P = getXY( &indi_sp, Options::useAltAz(), Options::useRefraction() );

						float s1 = 0.5*pxperdegree;
						float s2 = pxperdegree;
						float s3 = 2.0*pxperdegree;

						float x0 = P.x();        float y0 = P.y();
						float x1 = x0 - 0.5*s1;  float y1 = y0 - 0.5*s1;
						float x2 = x0 - 0.5*s2;  float y2 = y0 - 0.5*s2;
						float x3 = x0 - 0.5*s3;  float y3 = y0 - 0.5*s3;

						//Draw radial lines
						psky.drawLine( QPointF(x1, y0), QPointF(x3, y0) );
						psky.drawLine( QPointF(x0+s2, y0), QPointF(x0+0.5*s1, y0) );
						psky.drawLine( QPointF(x0, y1), QPointF(x0, y3) );
						psky.drawLine( QPointF(x0, y0+0.5*s1), QPointF(x0, y0+s2) );
						//Draw circles at 0.5 & 1 degrees
						psky.drawEllipse( QRectF(x1, y1, s1, s1) );
						psky.drawEllipse( QRectF(x2, y2, s2, s2) );

						psky.drawText( QPointF(x0+s2+2., y0), QString(devMenu->mgr.at(i)->indi_dev.at(j)->label) );

					}
				}
			}
		}
	}
*/
}

void SkyMap::exportSkyImage( QPaintDevice *pd ) {
	QPainter p;
	p.begin( pd );

	//scale image such that it fills 90% of the x or y dimension on the paint device
	double xscale = double(p.device()->width()) / double(width());
	double yscale = double(p.device()->height()) / double(height());
	double scale = (xscale < yscale) ? xscale : yscale;

	int pdWidth = int( scale * width() );
	int pdHeight = int( scale * height() );
	int x1 = int( 0.5*(p.device()->width()  - pdWidth) );
	int y1 = int( 0.5*(p.device()->height()  - pdHeight) );

	p.setClipRect( QRect( x1, y1, pdWidth, pdHeight ) );
	p.setClipping( true );

	//Fill background with sky color
	p.fillRect( x1, y1, pdWidth, pdHeight, QBrush( data->colorScheme()->colorNamed( "SkyColor" ) ) );

	if ( x1 || y1 ) p.translate( x1, y1 );

	data->skyComposite()->draw( ksw, p, scale );

	p.end();
}

void SkyMap::setMapGeometry() {
	m_Guidemax = Options::zoomFactor()/10.0;

	isPoleVisible = false;
	if ( Options::useAltAz() ) {
		XRange = 1.2*fov()/cos( focus()->alt()->radians() );
		Ymax = fabs( focus()->alt()->Degrees() ) + fov();
	} else {
		XRange = 1.2*fov()/cos( focus()->dec()->radians() );
		Ymax = fabs( focus()->dec()->Degrees() ) + fov();
	}
	if ( Ymax >= 90. ) isPoleVisible = true;

	//at high zoom, double FOV for guide lines so they don't disappear.
	guideFOV = fov();
	guideXRange = XRange;
	if ( Options::zoomFactor() > 10.*MINZOOM ) { guideFOV *= 2.0; guideXRange *= 2.0; }
}
