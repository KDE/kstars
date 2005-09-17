/***************************************************************************
                          milkywaycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/07/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "horizoncomponent.h"

#include <QList>
#include <QPoint>
#include <QPainter>
#include <QPointArray>
#include <QFile>

#include "skycomposite.h"
#include "kstarsdata.h"
#include "skymap.h"
#include "skypoint.h" 
#include "dms.h"
#include "Options.h"
#include "ksutils.h"

MilkyWayComponent::MilkyWayComponent(SkyComposite *parent) : SkyComponent(parent)
{
	pts = 0;
}

MilkyWayComponent::~MilkyWayComponent()
{
  for ( int i=0; i<NMWFILES; ++i ) 
    while ( ! MilkyWay[i].isEmpty() ) delete MilkyWay[i].takeFirst();
  
  delete pts;
}

// was bool KStarsData::readMWData( void )
/**Read Milky Way data.  Coordinates for the Milky Way contour are divided into 11
	*files, each representing a simple closed curve that can be drawn with
	*drawPolygon().  The lines in each file are parsed according to column position:
	*@li 0-7     RA [float]
	*@li 9-16    Dec [float]
	*@short read Milky Way contour data.
	*@return true if all MW files were successfully read
	*/
void MilkyWayComponent::init(KStarsData *data)
{
	pts = new QPointArray(2000)

	QFile file;

//	for ( unsigned int i=0; i<11; ++i ) {
	for ( unsigned int i=0; i<NMWFILES; ++i )
	{
		QString snum, fname, szero;
		snum = snum.setNum( i+1 );
		if ( i+1 < 10 ) szero = "0"; else szero = "";
		fname = "mw" + szero + snum + ".dat";

		if ( KSUtils::openDataFile( file, fname ) ) {
			KSFileReader fileReader( file ); // close file is included
			while ( fileReader.hasMoreLines() ) {
				QString line;
				double ra, dec;

				line = fileReader.readLine();

				ra = line.mid( 0, 8 ).toDouble();
				dec = line.mid( 9, 8 ).toDouble();

				SkyPoint *o = new SkyPoint( ra, dec );
				MilkyWay[i].append( o );
			}
		} else {
//			return false;
		}
	}
//	return true;
}

void MilkyWayComponent::update(KStarsData *data, KSNumbers *num, bool needNewCoords)
{
	if (Options::showMilkyWay())
	{
//		for ( unsigned int j=0; j<11; ++j )
		for ( unsigned int j=0; j<NMWFILES; ++j )
		{
		  foreach  ( SkyPoint *p, MilkyWay[j] ) {
			{
				if (needNewCoords) p->updateCoords( num );
				p->EquatorialToHorizontal( LST, map->data()->geo()->lat() );
			}
		}
	}
}

void MilkyWayComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	// TODO add data() to skymap
	if (!Options::showMilkyWay()) return;
	
	int ptsCount = 0;
	int mwmax = int( scale * Options::zoomFactor()/100.);
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	int thick(1);
	if ( ! Options::fillMilkyWay() ) thick=3;

	psky.setPen( QPen( QColor( map->data()->colorScheme()->colorNamed( "MWColor" ) ), thick, SolidLine ) );
	psky.setBrush( QBrush( QColor( map->data()->colorScheme()->colorNamed( "MWColor" ) ) ) );
	bool offscreen, lastoffscreen=false;

	for ( unsigned int j=0; j<11; ++j ) {
		if ( Options::fillMilkyWay() ) {
			ptsCount = 0;
			bool partVisible = false;

			QPoint o = map->getXY( MilkyWay[j].at(0), Options::useAltAz(), Options::useRefraction(), scale );
			if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
			if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;

			for ( SkyPoint *p, MilkyWay[j] ) {
				o = map->getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
				if ( o.x() != -10000000 && o.y() != -10000000 ) pts->setPoint( ptsCount++, o.x(), o.y() );
				if ( o.x() >= 0 && o.x() <= Width && o.y() >= 0 && o.y() <= Height ) partVisible = true;
			}

			if ( ptsCount && partVisible ) {
				psky.drawPolygon( (  const QPointArray ) *pts, false, 0, ptsCount );
			}
		} else {
			QPoint o = map->getXY( MilkyWay[j].at(0), Options::useAltAz(), Options::useRefraction(), scale );
			if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
			else offscreen = false;

			psky.moveTo( o.x(), o.y() );

			for ( SkyPoint *p, MilkyWay[j] ) {
				o = map->getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
				if (o.x()==-10000000 && o.y()==-10000000) offscreen = true;
				else offscreen = false;

				//don't draw a line if the last point's map->getXY was (-10000000, -10000000)
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
