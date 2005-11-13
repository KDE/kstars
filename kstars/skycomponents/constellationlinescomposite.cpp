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

ConstellationLinesComposite::ConstellationLinesComposite( SkyComponent *parent, KStarsData *data ) 
  : SkyComposite( parent, data ) 
{
	//Create the ConstellationLinesComponents.  Each is a series of points 
	//connected by line segments.  A single constellation can be composed of 
	//any number of these series, and we don't need to know which series 
	//belongs to which constellation.

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
			QChar mode;
			ConstellationLinesComponent *clc=0;

			line = stream.readLine();

			//ignore lines beginning with "#":
			if ( line.at( 0 ) != '#' ) {
				//If the first character is "M", we are starting a new series.
				//In this case, add the existing clc component to the composite, 
				//then prepare a new one.
				mode = line.at( 0 );
				if ( mode == 'M' ) {
					if ( clc ) addComponent( clc );
					clc = new ConstellationLinesComponent( this, Options::showCLines() );
				}

				name = line.mid( 2 ).trimmed();
				SkyPoint *p = data->skyComposite()->findStarByGenetiveName( name );

				if ( p && clc )
					clc->pointList().append( p );
				else if ( !p ) 
					kdWarning() << i18n( "No star named %1 found." ).arg(name) << endl;
			}
		}
		file.close();

		//Add the last clc component
		if ( clc ) addComponent( clc );
	}
}

