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

ConstellationLinesComponent::ConstellationLinesComponent(SkyComposite *parent)
: SkyComponent(parent)
{
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

		while ( !stream.eof() ) {
			QString line, name;
			QChar *mode;

			line = stream.readLine();

			//ignore lines beginning with "#":
			if ( line.at( 0 ) != '#' ) {
				name = line.mid( 2 ).stripWhiteSpace();
				
				//Find the star with the same abbreviated genitive name ( name2() )
				//increase efficiency by searching the list of named objects, rather than the 
				//full list of all stars.  
				bool starFound( false );
				for ( SkyObjectName *oname = ObjNames.first(); oname; oname = ObjNames.next() ) {
					if ( oname->skyObject()->type() == SkyObject::STAR && 
							 oname->skyObject()->name2() == name ) {
						starFound = true;
						clineList.append( (SkyPoint *)( oname->skyObject() ) );
						
						mode = new QChar( line.at( 0 ) );
						clineModeList.append( mode );
						break;
					}
				}
				
				if ( ! starFound ) 
					kdWarning() << i18n( "No star named %1 found." ).arg(name) << endl;
			}
		}
		file.close();

//		return true;
	} else {
//		return false;
	}
}

void ConstellationLinesComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	//Draw Constellation Lines
	psky.setPen( QPen( QColor( map->data()->colorScheme()->colorNamed( "CLineColor" ) ), 1, Qt::SolidLine ) ); //change to colorGrid
	int iLast = -1;

	for ( SkyPoint *p = map->data()->clineList.first(); p; p = map->data()->clineList.next() ) {
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			if ( map->data()->clineModeList.at(map->data()->clineList.at())->latin1()=='M' ) {
				psky.moveTo( o.x(), o.y() );
			} else if ( map->data()->clineModeList.at(map->data()->clineList.at())->latin1()=='D' ) {
				if ( map->data()->clineList.at()== (int)(iLast+1) ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
				}
			}
			iLast = map->data()->clineList.at();
		}
  }
}
