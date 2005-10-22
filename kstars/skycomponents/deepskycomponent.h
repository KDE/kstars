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

class QList;

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

class DeepSkyComponent: public SkyComponent
{
	public:

		DeepSkyComponent(SkyComposite*);
		
		~DeepSkyComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		
		bool readDeepSkyData();
		void drawDeepSkyCatalog( QPainter& psky, QList<DeepSkyObject*>& catalog, 
					QColor& color, bool drawObject, bool drawImage, double scale );

		/** List of all deep sky objects */
		QList<DeepSkyObject*> deepSkyList;
		/** List of all deep sky objects per type, to speed up drawing the sky map */
		QList<DeepSkyObject*> deepSkyListMessier;
		/** List of all deep sky objects per type, to speed up drawing the sky map */
		QList<DeepSkyObject*> deepSkyListNGC;
		/** List of all deep sky objects per type, to speed up drawing the sky map */
		QList<DeepSkyObject*> deepSkyListIC;
		/** List of all deep sky objects per type, to speed up drawing the sky map */
		QList<DeepSkyObject*> deepSkyListOther;

};

#endif
