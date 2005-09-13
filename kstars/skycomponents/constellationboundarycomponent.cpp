/***************************************************************************
                          constellationboundarycomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 12/09/2005
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

ConstellationBoundaryComponent::ConstellationBoundaryComponent(SkyComposite *parent)
: SkyComponent(parent)
{
}

// bool KStarsData::readCLineData( void )
void ConstellationBoundaryComponent::init(KStarsData *data)
{
	QFile file;
	if ( KSUtils::openDataFile( file, "cbound.dat" ) ) {
	  QTextStream stream( &file );

		unsigned int nn(0);
		double ra(0.0), dec(0.0);
		QString d1, d2;
		bool ok(false), comment(false);
		
		//read the stream one field at a time.  Individual segments can span
		//multiple lines, so our normal readLine() is less convenient here.
		//Read fields into strings and then attempt to recast them as ints 
		//or doubles..this lets us do error checking on the stream.
		while ( !stream.eof() ) {
			stream >> d1;
			if ( d1.at(0) == '#' ) { 
				comment = true; 
				ok = true; 
			} else {
				comment = false;
				nn = d1.toInt( &ok );
			}
			
			if ( !ok || comment ) {
				d1 = stream.readLine();
				
				if ( !ok ) 
					kdWarning() << i18n( "Unable to parse boundary segment." ) << endl;
				
			} else { 
				CSegment *seg = new CSegment();
				
				for ( unsigned int i=0; i<nn; ++i ) {
					stream >> d1 >> d2;
					ra = d1.toDouble( &ok );
					if ( ok ) dec = d2.toDouble( &ok );
					if ( !ok ) break;
					
					seg->addPoint( ra, dec );
				}
				
				if ( !ok ) {
					//uh oh, this entry was not parsed.  Skip to the next line.
					kdWarning() << i18n( "Unable to parse boundary segment." ) << endl;
					delete seg;
					d1 = stream.readLine();
					
				} else {
					stream >> d1; //this should always equal 2
					
					nn = d1.toInt( &ok );
					//error check
					if ( !ok || nn != 2 ) {
						kdWarning() << i18n( "Bad Constellation Boundary data." ) << endl;
						delete seg;
						d1 = stream.readLine();
					}
				}

				if ( ok ) {
					stream >> d1 >> d2;
					ok = seg->setNames( d1, d2 );
					if ( ok ) csegmentList.append( seg );
				}
			}
		}
	}
}

void ConstellationBoundaryComponent::draw(SkyMap *map, QPainter& psky, double scale)
{
	int Width = int( scale * width() );
	int Height = int( scale * height() );

	psky.setPen( QPen( QColor( data->colorScheme()->colorNamed( "CBoundColor" ) ), 1, SolidLine ) );

	for ( CSegment *seg = data->csegmentList.first(); seg; seg = data->csegmentList.next() ) {
		bool started( false );
		SkyPoint *p = seg->firstNode();
		QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
		if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
			psky.moveTo( o.x(), o.y() );
			started = true;
		}

		for ( p = seg->nextNode(); p; p = seg->nextNode() ) {
			QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );

			if ( ( o.x() >= -1000 && o.x() <= Width+1000 && o.y() >=-1000 && o.y() <= Height+1000 ) ) {
				if ( started ) {
					psky.lineTo( o.x(), o.y() );
				} else {
					psky.moveTo( o.x(), o.y() );
					started = true;
				}
			} else {
				started = false;
			}
		}
	}
}

void ConstellationBoundaryComponent::update(KStarsData*, KSNumbers*, bool needNewCoords) {
	if ( Options::showCBounds() ) {  
		for ( int i=0; i < csegmentList.size(); ++i ) {
			CSegment *seg = csegmentList[i];
			for ( uint j=0; j < seg->nodes().size(); ++j ) {
				SkyPoint *p = seg->node()[j];
				if ( needNewCoords ) p->updateCoords( &num );
				p->EquatorialToHorizontal( LST, geo->lat() );
			}
		}
	}
}
