//Added by qt3to4:
#include <QTextStream>
/***************************************************************************
                          constellationlinescomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/08
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

#include <QFile>
#include <QPainter>
#include <QTextStream>

#include "constellationlinescomponent.h"

#include "kstars.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skymap.h"
#include "Options.h"

ConstellationLinesComponent::ConstellationLinesComponent(SkyComponent *parent, bool (*visibleMethod)())
: PointListComponent(parent, visibleMethod)
{
}

ConstellationLinesComponent::~ConstellationLinesComponent() {
}

// bool KStarsData::readCLineData( void )
void ConstellationLinesComponent::init(KStarsData *data)
{
// TODO be sure that stars are already loaded

	//The constellation lines data file (clines.dat) contains lists
	//of abbreviated genetive star names in the same format as they 
	//appear in the star data files (hipNNN.dat).  
	//
	//Each constellation consists of a QPtrList of SkyPoints, 
	//corresponding to the stars at each "node" of the constellation.
	//These are pointers to the starobjects themselves, so the nodes 
	//will automatically be fixed to the stars even as the star 
	//positions change due to proper motions.  In addition, each node 
	//has a corresponding flag that determines whether a line should 
	//connect this node and the previous one.
	
	QFile file;
	if ( KSUtils::openDataFile( file, "clines.dat" ) ) {
	  QTextStream stream( &file );

		while ( !stream.atEnd() ) {
			QString line, name;

			line = stream.readLine();

			//ignore lines beginning with "#":
			if ( line.at( 0 ) != '#' ) {
				name = line.mid( 2 ).trimmed();
				
				//Find the star with this abbreviated genitive name ( name2() )
				SkyObject *o = data->skyComposite()->findByName( name );
				if ( o ) {
					pointList().append( (SkyPoint*)o );
					m_CLineModeList.append( QChar( line.at( 0 ) ) );
				} else {
					kdWarning() << i18n( "No star named %1 found." ).arg(name) << endl;
				}
			}
		}
		file.close();

//		return true;
	} else {
//		return false;
	}
}

void ConstellationLinesComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !visible() ) return;

	SkyMap *map = ks->map();
	float Width = scale * map->width();
	float Height = scale * map->height();

	//Draw Constellation Lines
	psky.setPen( QPen( QColor( ks->data()->colorScheme()->colorNamed( "CLineColor" ) ), 1, Qt::SolidLine ) ); //change to colorGrid
//	int iLast = -1;

	for ( int i=0; i < pointList().size(); ++i ) {
		QPointF o = map->getXY( pointList().at(i), Options::useAltAz(), Options::useRefraction(), scale );
		QPointF oStart(o);

		if ( ( o.x() >= -1000. && o.x() <= Width+1000. && o.y() >=-1000. && o.y() <= Height+1000. ) ) {
			if ( m_CLineModeList.at(i)=='M' ) {
				oStart = o;
			} else if ( m_CLineModeList.at(i)=='D' ) {
//FIXME: I don't think this if() is necessary
//				if ( ks->data()->clineList.at()== (int)(iLast+1) ) {
//					psky.lineTo( o.x(), o.y() );
//				} else {
//					psky.moveTo( o.x(), o.y() );
//				}
				psky.drawLine( oStart, o );
			}
//			iLast = ks->data()->clineList.at();
		}
  }
}
