/***************************************************************************
                          deepskycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/15/08
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

#ifndef DEEPSKYCOMPONENT_H
#define DEEPSKYCOMPONENT_H

/**
	*@class DeepSkyComponent
	*Represents the deep sky objects separated by catalogs. 
	*Custom Catalogs are a standalone component.
	*
	*@author Thomas Kabelmann
	*@version 0.1
	*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class DeepSkyComponent: public ListComponent
{
	public:

		DeepSkyComponent(SkyComponent*, bool (*visibleMethod)());
		
		~DeepSkyComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
	private:
		
		bool readDeepSkyData();
		void drawDeepSkyCatalog( QPainter& psky, QList<DeepSkyObject*>& catalog, 
					QColor& color, bool drawObject, bool drawImage, double scale );

};

#endif
