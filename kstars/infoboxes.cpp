/***************************************************************************
                          infoboxes.cpp  -  description
                             -------------------
    begin                : Wed Jun 5 2002
    copyright            : (C) 2002 by Jason Harris
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

#include <kglobal.h>
#include <kdebug.h>
#include "infoboxes.h"

InfoBoxes::InfoBoxes( int w, int h ) {
	boxColor = QColor( "white" );
	grabColor = QColor( "red" );
	GrabbedBox = 0;
	GrabPos = QPoint( 0, 0 );
	Visible = true;

	GeoBox   = new InfoBox( 0, 0, "", "" );
	TimeBox  = new InfoBox( 0, 0, "", "", "" );
	FocusBox = new InfoBox( 0, 0, "", "", "" );

	// set default positions
	FocusBox->move( 0, 0 );
	TimeBox->move( w, 0 );
	GeoBox->move( 0, h );
	resize( w, h );
}

InfoBoxes::~InfoBoxes(){
	delete FocusBox;
	delete TimeBox;
	delete GeoBox;
}

void InfoBoxes::resize( int w, int h ) {
	Width = w;
	Height = h;

	if ( GeoBox->rect().right() >= w-2 ) GeoBox->setAnchorRight(true);
	if ( TimeBox->rect().right() >= w-2 ) TimeBox->setAnchorRight(true);
	if ( FocusBox->rect().right() >= w-2 ) FocusBox->setAnchorRight(true);
	if ( GeoBox->rect().bottom() >= h-2 ) GeoBox->setAnchorBottom(true);
	if ( TimeBox->rect().bottom() >= h-2 ) TimeBox->setAnchorBottom(true);
	if ( FocusBox->rect().bottom() >= h-2 ) FocusBox->setAnchorBottom(true);
	
	if ( GeoBox->anchorRight() ) GeoBox->move( w, GeoBox->y() );
	if ( TimeBox->anchorRight() ) TimeBox->move( w, TimeBox->y() );
	if ( FocusBox->anchorRight() ) FocusBox->move( w, FocusBox->y() );
	if ( GeoBox->anchorBottom() ) GeoBox->move( GeoBox->x(), h );
	if ( TimeBox->anchorBottom() ) TimeBox->move( TimeBox->x(), h );
	if ( FocusBox->anchorBottom() ) FocusBox->move( FocusBox->x(), h );
}

void InfoBoxes::drawBoxes( QPainter &p, QColor FGColor, QColor grabColor,
		QColor BGColor, bool fillBG ) {
	if ( isVisible() ) {
		if ( GeoBox->isVisible() ) {
			p.setPen( QPen( FGColor ) );
			if ( GrabbedBox == 1 ) {
				p.setPen( QPen( grabColor ) );
				p.drawRect( GeoBox->x(), GeoBox->y(), GeoBox->width(), GeoBox->height() );
			}
			GeoBox->draw( p, BGColor, fillBG );
		}

		if ( TimeBox->isVisible() ) {
			p.setPen( QPen( FGColor ) );
			if ( GrabbedBox == 2 ) {
				p.setPen( QPen( grabColor ) );
				p.drawRect( TimeBox->x(), TimeBox->y(), TimeBox->width(), TimeBox->height() );
			}
			TimeBox->draw( p, BGColor, fillBG );
		}

		if ( FocusBox->isVisible() ) {
			p.setPen( QPen( FGColor ) );
			if ( GrabbedBox == 3 ) {
				p.setPen( QPen( grabColor ) );
				p.drawRect( FocusBox->x(), FocusBox->y(), FocusBox->width(), FocusBox->height() );
			}
			FocusBox->draw( p, BGColor, fillBG );
		}
	}
}

bool InfoBoxes::grabBox( QMouseEvent *e ) {
	if ( GeoBox->rect().contains( e->pos() ) ) {
		GrabbedBox = 1;
		GrabPos.setX( e->x() - GeoBox->x() );
		GrabPos.setY( e->y() - GeoBox->y() );
		return true;
	} else if ( TimeBox->rect().contains( e->pos() ) ) {
		GrabbedBox = 2;
		GrabPos.setX( e->x() - TimeBox->x() );
		GrabPos.setY( e->y() - TimeBox->y() );
		return true;
	} else if ( FocusBox->rect().contains( e->pos() ) ) {
		GrabbedBox = 3;
		GrabPos.setX( e->x() - FocusBox->x() );
		GrabPos.setY( e->y() - FocusBox->y() );
		return true;
	} else {
		GrabbedBox = 0; //this is probably redundant, because mouseRelease should call unGrabBox()...
		return false;
	}
}

bool InfoBoxes::unGrabBox( void ) {
	if ( GrabbedBox ) {
		GrabbedBox = 0;
		return true;
	} else {
		return false;
	}
}

bool InfoBoxes::dragBox( QMouseEvent *e ) {
	switch( GrabbedBox ) {
		case 1: //GeoBox
			GeoBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
			fixCollisions( GeoBox );
			return true;
			break;
		case 2: //TimeBox
			TimeBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
			fixCollisions( TimeBox );
			return true;
			break;
		case 3: //FocusBox
			FocusBox->move( e->x() - GrabPos.x(), e->y() - GrabPos.y() );
			fixCollisions( FocusBox );
			return true;
			break;
		default: //no box is grabbed; return false
			return false;
			break;
	}
}

bool InfoBoxes::shadeBox( QMouseEvent *e ) {
	if ( GeoBox->rect().contains( e->pos() ) ) {
		GeoBox->toggleShade();
		if ( GeoBox->rect().bottom() > height() ) GeoBox->move( GeoBox->x(), height() - GeoBox->height() );
		if ( GeoBox->rect().right() > width() ) GeoBox->move( width() - GeoBox->width(), GeoBox->y() );
		if ( GeoBox->anchorBottom() ) GeoBox->move( GeoBox->x(), height() - GeoBox->height() );
		if ( GeoBox->anchorRight() ) GeoBox->move( width() - GeoBox->width(), GeoBox->y() );
		fixCollisions( TimeBox );
		fixCollisions( FocusBox );
		return true;
	} else if ( TimeBox->rect().contains( e->pos() ) ) {
		TimeBox->toggleShade();
		if ( TimeBox->rect().bottom() > height() ) TimeBox->move( TimeBox->x(), height() - TimeBox->height() );
		if ( TimeBox->rect().right() > width() ) TimeBox->move( width() - TimeBox->width(), TimeBox->y() );
		if ( TimeBox->anchorBottom() ) TimeBox->move( TimeBox->x(), height() - TimeBox->height() );
		if ( TimeBox->anchorRight() ) TimeBox->move( width() - TimeBox->width(), TimeBox->y() );
		fixCollisions( GeoBox );
		fixCollisions( FocusBox );
		return true;
	} else if ( FocusBox->rect().contains( e->pos() ) ) {
		FocusBox->toggleShade();
		if ( FocusBox->rect().bottom() > height() ) FocusBox->move( FocusBox->x(), height() - FocusBox->height() );
		if ( FocusBox->rect().right() > width() ) FocusBox->move( width() - FocusBox->width(), FocusBox->y() );
		if ( FocusBox->anchorBottom() ) FocusBox->move( FocusBox->x(), height() - FocusBox->height() );
		if ( FocusBox->anchorRight() ) FocusBox->move( width() - FocusBox->width(), FocusBox->y() );
		fixCollisions( TimeBox );
		fixCollisions( GeoBox );
		return true;
	}
	return false;
}

bool InfoBoxes::fixCollisions( InfoBox *target ) {
	int dLeft(0), dRight(0), dUp(0), dDown(0);
	QRect area = QRect( 0, 0, width(), height() );
	QRect t = target->rect();
	QRect Box1, Box2;

//Set Box1 and Box2 to the rects of the other two InfoBoxes, unless
//they are not visible (if so, set a null QRect)
	if ( target == GeoBox ) {
		if ( FocusBox->isVisible() ) Box1 = FocusBox->rect();
		else Box1 = QRect(0,0,0,0);

		if ( TimeBox->isVisible() ) Box2 = TimeBox->rect();
		else Box2 = QRect(0,0,0,0);

	} else if ( target == FocusBox ) {
		if ( GeoBox->isVisible() ) Box1 = GeoBox->rect();
		else Box1 = QRect(0,0,0,0);

		if ( TimeBox->isVisible() ) Box2 = TimeBox->rect();
		else Box2 = QRect(0,0,0,0);

	} else if ( target == TimeBox ) {
		if ( FocusBox->isVisible() ) Box1 = FocusBox->rect();
		else Box1 = QRect(0,0,0,0);

		if ( GeoBox->isVisible() ) Box2 = GeoBox->rect();
		else Box2 = QRect(0,0,0,0);

	} else return false; //none of the Boxes match target!

//Shrink Box1 and Box2 by one pixel in each direction.  This will make the
//Edges of adjacent boxes line up more nicely.
	if ( Box1.width() ) Box1.setCoords( Box1.left()+1, Box1.top()+1, Box1.right()-1, Box1.bottom()-1 );
	if ( Box2.width() ) Box2.setCoords( Box2.left()+1, Box2.top()+1, Box2.right()-1, Box2.bottom()-1 );

//First, make sure target box is within area rect.
	if ( ! area.contains( t ) ) {
		if ( t.x() < area.x() ) target->move( area.x(), t.y() );
		if ( t.y() < area.y() ) target->move( t.x(), area.y() );
		if ( t.right() > area.right() ) target->move( area.right() - t.width(), t.y() );
		if ( t.bottom() > area.bottom() ) target->move( t.x(), area.bottom() - t.height() );

		//Reset t
		t = target->rect();
	}

	QRect upRect = t;
	QRect downRect = t;
	QRect leftRect = t;
	QRect rightRect = t;

//Fix collisions
	if ( t.intersects( Box1 ) || t.intersects( Box2 ) ) {
		//move t to the left one pixel at a time until there is no
		//intersection with Box1 or Box2.
		while ( leftRect.intersects( Box1 ) || leftRect.intersects( Box2 ) ) {
			++dLeft;
			leftRect.moveTopLeft( QPoint( t.x() - dLeft, t.y() ) );
		}
		//If leftRect is outside area, set dLeft to a nonsense large value
		if ( !area.contains( leftRect ) ) { dLeft = 100000; }

		//repeat for right, up and down directions.
		while ( rightRect.intersects( Box1 ) || rightRect.intersects( Box2 ) ) {
			++dRight;
			rightRect.moveTopLeft( QPoint( t.x() + dRight, t.y() ) );
		}
		if ( !area.contains( rightRect ) ) { dRight = 100000; }

		while ( upRect.intersects( Box1 ) || upRect.intersects( Box2 ) ) {
			++dUp;
			upRect.moveTopLeft( QPoint( t.x(), t.y() - dUp ) );
		}
		if ( !area.contains( upRect ) ) { dUp = 100000; }

		while ( downRect.intersects( Box1 ) || downRect.intersects( Box2 ) ) {
			++dDown;
			downRect.moveTopLeft( QPoint( t.x(), t.y() + dDown ) );
		}
		if ( !area.contains( downRect ) ) { dDown = 100000; }

		//find the smallest displacement, and move the target box there.
		//if the smallest displacement is 100000, then the function has failed
		//to find any legal position; return false.
		int dmin = dLeft;
		if ( dRight < dmin ) dmin = dRight;
		if ( dDown < dmin ) dmin = dDown;
		if ( dUp < dmin ) dmin = dUp;

		if ( dmin == 100000 ) return false;
		else if ( dmin == dLeft ) {
			target->move( leftRect.x(), leftRect.y() );
		} else if ( dmin == dRight ) {
			target->move( rightRect.x(), rightRect.y() );
		} else if ( dmin == dUp ) {
			target->move( upRect.x(), upRect.y() );
		} else if ( dmin == dDown ) {
			target->move( downRect.x(), downRect.y() );
		}
	}

	//Set Anchor flags based on new position
	if ( target->rect().right() >= width()-2 ) target->setAnchorRight(true);
	else target->setAnchorRight(false);
	if ( target->rect().bottom() >= height()-2 ) target->setAnchorBottom(true);
	else target->setAnchorBottom(false);

	//Final check to see if we're still inside area (we may not be if target
	//is bigger than area)
	if ( area.contains( target->rect() ) ) return true;
	else return false;
}

void InfoBoxes::timeChanged(QDateTime ut, QDateTime lt, QTime lst, long double julian) {
	TimeBox->setText1( i18n( "Local Time", "LT: " ) + lt.time().toString()
		+ "   " + KGlobal::locale()->formatDate( lt.date(), true ) );	
	TimeBox->setText2( i18n( "Universal Time", "UT: " ) + ut.time().toString()
		+ "   " + KGlobal::locale()->formatDate( ut.date(), true ) );
	QString STString;
	STString = STString.sprintf( "%02d:%02d:%02d   ", lst.hour(), lst.minute(), lst.second() );
	TimeBox->setText3( i18n( "Sidereal Time", "ST: " ) + STString +
		i18n( "Julian Day", "JD: " ) +
		KGlobal::locale()->formatNumber( julian, 2 ) );
}

void InfoBoxes::geoChanged(const GeoLocation *geo) {
	QString name = geo->translatedName() + ", ";
	if ( ! geo->province().isEmpty() ) name += geo->translatedProvince() + ",  ";
	name += geo->translatedCountry();
	GeoBox->setText1( name );

	GeoBox->setText2( i18n( "Longitude", "Long:" ) + " " +
		KGlobal::locale()->formatNumber( geo->lng().Degrees(),3) + "   " +
		i18n( "Latitude", "Lat:" ) + " " +
		KGlobal::locale()->formatNumber( geo->lat().Degrees(),3) );
}

void InfoBoxes::focusObjChanged( const QString &n ) {
	FocusBox->setText1( i18n( "Focused on: " ) + n );
}

void InfoBoxes::focusCoordChanged(const SkyPoint *p) {
	FocusBox->setText2( i18n( "Right Ascension", "RA" ) + ": " + p->ra().toHMSString() +
		"  " + i18n( "Declination", "Dec" ) +  ": " + p->dec().toDMSString(true) );
	FocusBox->setText3( i18n( "Altitude", "Alt" ) + ": " + p->alt().toDMSString(true) +
		"  " + i18n( "Azimuth", "Az" ) + ": " + p->az().toDMSString(true) );
}
#include "infoboxes.moc"
