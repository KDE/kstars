/***************************************************************************
                          opssolarsystem.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 22 Aug 2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OPSSOLARSYSTEM_H
#define OPSSOLARSYSTEM_H

#include <opssolarsystemui.h>

class KStars;

/**@class OpsSolarSystem
	*The Solar System page for the Options window.  This page allows the user
	*to modify display of solar system bodies in KStars, including the 
	*major planets, the Sun and Moon, and the comets and asteroids.
	*@short The Solar System page of the Options window.
	*@author Jason Harris
	*@version 1.0
	*/
class OpsSolarSystem : public OpsSolarSystemUI
{
	Q_OBJECT

public:
	OpsSolarSystem( QWidget *parent=0, const char *name=0, WFlags fl = 0 );
	~OpsSolarSystem();

private slots:
	void slotAsteroidWidgets(bool on);
	void slotCometWidgets(bool on);
	void slotSelectPlanets();

private:
	KStars *ksw;
};

#endif
