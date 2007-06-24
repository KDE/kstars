/***************************************************************************
                 constellationboundarycomposite.h  -  K Desktop Planetarium
                             -------------------
    begin                : 25 Oct. 2005
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

#include "constellationboundarycomposite.h"
#include "constellationboundarycomponent.h"

#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QBrush>
#include <kstandarddirs.h>

#include <kdebug.h>
#include <klocale.h>

#include "Options.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobject.h"

ConstellationBoundaryComposite::ConstellationBoundaryComposite( SkyComponent *parent )
  : SkyComposite( parent )
{
}

//Constellation boundary data is stored in a series of 
//*.cbound files, one per constellation.  Each file contains 
//the list of RA/Dec points along the constellation's border,
//and a flag indicating whether the segment is duplicated 
//in another constellation.  (Actually all segments have a 
//duplicate somewhere...the choice of calling one the duplicate 
//is entirely arbitrary).
//
//We store the boundary data in a QHash of QPolygonF's (for 
//fast determination of whether a SkyPoint is enclosed, and for 
//drawing a single boundary to the screen).  We also store the 
//non-duplicate segments in the Component's native list of 
//SkyLines (for fast drawing of all boundaries at once).
void ConstellationBoundaryComposite::init( KStarsData *data ) {
	QFile file;
	QString path = KStandardDirs::locate( "appdata", "AND.cbound" );
	path.chop( 11 );
	QDir DataDir( path );
	QStringList filters;
	filters << "*.cbound";
	DataDir.setNameFilters( filters );
	QStringList BoundFiles = DataDir.entryList();

	foreach ( QString bfile, BoundFiles ) {
		ConstellationBoundaryComponent *cbc = 0;

		if ( KSUtils::openDataFile( file, bfile ) ) {
			QString cname = bfile.left(3);
			QPolygonF poly;

			emitProgressText( i18n("Loading constellation boundaries: %1", cname) );
			QTextStream stream( &file );
		
			double ra(0.0), dec(0.0);

			while ( !stream.atEnd() ) {
				QString line;
				bool drawFlag(false), ok(false);

				line = stream.readLine();
				//ignore lines beginning with "#":
				if ( line.at( 0 ) != '#' ) {
					QStringList fields = line.split( " ", QString::SkipEmptyParts );
					if ( fields.count() == 3 ) {
						ra = fields[0].toDouble(&ok);
						if ( ok ) dec = fields[1].toDouble(&ok);
						if ( ok ) drawFlag = fields[2].toInt(&ok);
						
						if ( ok ) {
							poly << QPointF( ra, dec );

							if ( drawFlag ) {
								//If there's no component object, then we are starting a new one; 
								//create it.
								if ( !cbc ) {
									cbc = new ConstellationBoundaryComponent( this, Options::showCBounds );
									cbc->setPen( QPen( QBrush( data->colorScheme()->colorNamed( "CBoundColor" ) ), 1, Qt::SolidLine ) );
								}

								SkyPoint p( ra, dec );
								p.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
								cbc->appendPoint( p );

							//If we have just completed a boundary segment, add it to the composite.
							//reset the component to 0; it will be reassigned by the next visible segment
							} else if ( cbc ) {
								addComponent( cbc );
								cbc = 0;
							}
						}
					}
				}
			}

			//Add the last boundary component
			if ( cbc )
				addComponent( cbc );

			Boundary[cname] = poly;
			
		}

		file.close();
	}
}

QString ConstellationBoundaryComposite::constellation( SkyPoint *p ) {
	QHashIterator<QString, QPolygonF> i(Boundary);

	while (i.hasNext()) {
		i.next();

		QPolygonF poly = i.value();

		if ( poly.containsPoint( QPointF( p->ra()->Hours(), p->dec()->Degrees() ), Qt::OddEvenFill ) )
			return i.key();
	}

	return i18n("Unknown");
}

bool ConstellationBoundaryComposite::inConstellation( const QString &name, SkyPoint *p ) {
	if ( Boundary.contains( name ) ) {
		QPolygonF poly = Boundary.value( name );
		if ( poly.containsPoint( QPointF( p->ra()->Hours(), p->dec()->Degrees() ), Qt::OddEvenFill ) )
			return true;
	}
	
	return false;
}

QPolygonF ConstellationBoundaryComposite::boundary( const QString &name ) const {
	if ( Boundary.contains( name ) )
		return Boundary.value( name );
	else
		return QPolygonF();
}
