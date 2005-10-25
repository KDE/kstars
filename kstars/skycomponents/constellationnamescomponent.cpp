/***************************************************************************
                          constellationnamescomponent.cpp  -  K Desktop Planetarium
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
#include "constellationnamescomponent.h"

#include <QFile>
#include <QTextStream>
#include <QString>

#include "ksutils.h"
#include "skyobject.h"

ConstellationNamesComponent::ConstellationNamesComponent(SkyComponent *parent, bool (*visibleMethod)())
: ListComponent(parent, visibleMethod)
{
}

ConstellationNamesComponent::~ConstellationNamesComponent() {
	while ( ! cnameList.isEmpty() ) delete cnameList.takeFirst();
}

void ConstellationNamesComponent::init(KStarsData *data)
{
	QFile file;
	QString cnameFile = "cnames.dat";

	if ( KSUtils::openDataFile( file, cnameFile ) ) {
		QTextStream stream( &file );

		while ( !stream.eof() ) {
			QString line, name, abbrev;
			int rah, ram, ras, dd, dm, ds;
			QChar sgn;

			line = stream.readLine();

			rah = line.mid( 0, 2 ).toInt();
			ram = line.mid( 2, 2 ).toInt();
			ras = line.mid( 4, 2 ).toInt();

			sgn = line.at( 6 );
			dd = line.mid( 7, 2 ).toInt();
			dm = line.mid( 9, 2 ).toInt();
			ds = line.mid( 11, 2 ).toInt();

			abbrev = line.mid( 13, 3 );
			name  = line.mid( 17 ).stripWhiteSpace();

			dms r; r.setH( rah, ram, ras );
			dms d( dd, dm,  ds );

			if ( sgn == "-" ) { d.setD( -1.0*d.Degrees() ); }

			SkyObject *o = new SkyObject( SkyObject::CONSTELLATION, r, d, 0.0, name, abbrev );
			cnameList.append( o );
			ObjNames.append( o );
		}
		file.close();
	}
}

void ConstellationNamesComponent::draw(KStars *ks, QPainter& psky, double scale)
{
	if ( !Options::showCNames() ) return;
	
	SkyMap *map = ks->map();
	int Width = int( scale * map->width() );
	int Height = int( scale * map->height() );

	//Draw Constellation Names
	psky.setPen( QColor( ks->data()->colorScheme()->colorNamed( "CNameColor" ) ) );
	foreach ( SkyPoint *p, cnameList ) {
		if ( map->checkVisibility( p, fov(), XRange ) ) {
			QPoint o = getXY( p, Options::useAltAz(), Options::useRefraction(), scale );
			if (o.x() >= 0 && o.x() <= Width && o.y() >=0 && o.y() <= Height ) {
				if ( Options::useLatinConstellNames() ) {
					int dx = 5*p->name().length();
					psky.drawText( o.x()-dx, o.y(), p->name() );  // latin constellation names
				} else if ( Options::useLocalConstellNames() ) {
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
