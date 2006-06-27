/***************************************************************************
                          skyline.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon June 26 2006
    copyright            : (C) 2006 by Jason Harris
    email                : kstarss@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "skyline.h"

dms SkyLine::angularSize() {
	double dalpha = startPoint()->ra()->radians() - endPoint()->ra()->radians();
	double ddelta = startPoint()->dec()->radians() - endPoint()->dec()->radians() ;

	double sa = sin(dalpha/2.);
	double sd = sin(ddelta/2.);

	double hava = sa*sa;
	double havd = sd*sd;

	double aux = havd + cos (endPoint()->dec()->radians()) * cos(startPoint()->dec()->radians()) * hava;

	dms angDist;
	angDist.setRadians( 2 * fabs(asin( sqrt(aux) )) );

	return angDist;
}
