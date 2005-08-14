/***************************************************************************
                          starcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/14/08
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

#ifndef STARCOMPONENT_H
#define STARCOMPONENT_H

class QList;

/**@class StarComponent
*Represents the stars on the sky map. For optimization reasons the stars are
*not separate objects and are stored in a list.

*@author Thomas Kabelmann
*@version 0.1
*/

#define NHIPFILES 127

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class StarComponent: public SkyComponent
{
	public:

		StarComponent(SkyComposite*);
		
		virtual ~StarComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	protected:

		virtual void drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale);
		
	private:
		
		// some helper methods
		bool openStarFile(int i);
		bool readStarData();
		void processStar(QString *line, bool reloadMode);

		QList<StarObject*> *starList;
		KSFileReader *starFileReader;

};

#endif
