/***************************************************************************
                 constellationlinescomposite.h  -  K Desktop Planetarium
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

#include "constellationlinescomposite.h"
#include "constellationlinescomponent.h"

#include <QFile>
#include <QTextStream>
#include <QBrush>

#include <kdebug.h>
#include <klocale.h>

#include "Options.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "skyobject.h"

ConstellationLinesComposite::ConstellationLinesComposite( SkyComponent *parent )
  : SkyComposite( parent )
{
}

void ConstellationLinesComposite::init( KStarsData *data ) {
	//Create the ConstellationLinesComponents.  Each is a series of points
	//connected by line segments.  A single constellation can be composed of
	//any number of these series, and we don't need to know which series
	//belongs to which constellation.

	//The constellation lines data file (clines.dat) contains lists
	//of abbreviated genetive star names in the same format as they
	//appear in the star data files (hipNNN.dat).
	//
	//Each constellation consists of a QList of SkyPoints,
	//corresponding to the stars at each "node" of the constellation.
	//These are pointers to the starobjects themselves, so the nodes
	//will automatically be fixed to the stars even as the star
	//positions change due to proper motions.  In addition, each node
	//has a corresponding flag that determines whether a line should
	//connect this node and the previous one.

	QFile file;
	if ( KSUtils::openDataFile( file, "clines.dat" ) ) {
		emitProgressText( i18n("Loading constellation lines") );
		QTextStream stream( &file );
		ConstellationLinesComponent *clc=0;

		while ( !stream.atEnd() ) {
			QString line, name;
			QChar mode;

			line = stream.readLine();

			//ignore lines beginning with "#":
			if ( line.at( 0 ) != '#' ) {
				//If the first character is "M", we are starting a new series.
				//In this case, add the existing clc component to the composite,
				//then prepare a new one.
				mode = line.at( 0 );
				name = line.mid( 2 ).trimmed();

				//Mode == 'M' starts a new series of line segments, joined end to end
				if ( mode == 'M' ) {
					if ( clc ) addComponent( clc );
					clc = new ConstellationLinesComponent( this, Options::showCLines );
					clc->setPen( QPen( QBrush( data->colorScheme()->colorNamed( "CLineColor" ) ), 1, Qt::SolidLine ) ); 
				}

				SkyObject *star = data->skyComposite()->findStarByGenetiveName( name );
				if ( star && clc ) {
					SkyPoint p( star->ra(), star->dec() );
					p.EquatorialToHorizontal( data->lst(), data->geo()->lat() );
					clc->appendPoint( p );
				} else if ( !star )
					kWarning() << i18n( "No star named %1 found." , name) ;
			}
		}
		file.close();

		//Add the last clc component
		if ( clc ) addComponent( clc );
	}
}

