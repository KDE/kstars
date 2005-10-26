/***************************************************************************
                          skymapcomposite.cpp  -  K Desktop Planetarium
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

#include "skymapcomposite.h"

#include "horizoncomponent.h"
#include "milkywaycomponent.h"
#include "coordinategridcomponent.h"

SkyMapComposite::SkyMapComposite(SkyComponent *parent) : SkyComposite(parent)
{
	//Add all components
	// beware the order of adding components
	// first added component will be drawn first
	// so horizon should be one of the last components
	addComponent( new MilkyWayComponent( this, Options::showMilkyWay() ) );
	addComponent( new CoordinateGridComposite( this, Options::showGrid() ) );
	addComponent( new ConstellationBoundaryComponent( this, Options::showCBounds() ) );
	addComponent( new ConstellationLinesComponent( this, Options::showCLines() ) );
	addComponent( new ConstellationNamesComponent( this, Options::showCNames() ) );
	addComponent( new EquatorComponent( this, Options::showEquator() ) );
	addComponent( new EclipticComponent( this, Options::showEcliptic() ) );
	addComponent( new DeepSkyComponent( this, Options::showDeepSky() ) );
	addComponent( new CustomCatalogComponent( this, Options::showOther() ) );
	addComponent( new StarComponent( this, Options::showStars() ) );

	m_SSComposite = new SolarSystemComposite( this, data );
	addComponent( m_SSComposite );

	addComponent( new JupiterMoonsComponent( this, Options::showJupiter() ) );
	addComponent( new HorizonComponent(this) );
}
