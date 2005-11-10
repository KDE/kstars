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
	*@note this Component is similar to ListComponent, but 
	*the deep sky objects are stored in four separate QLists.
	*@author Thomas Kabelmann
	*@version 0.1
	*/

#define NNGCFILES 14

#include "skycomponent.h"

class QColor;
class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;
class DeepSkyObject;

class DeepSkyComponent: public SkyComponent
{
	Q_OBJECT
	public:

		DeepSkyComponent( SkyComponent*, bool (*vMethodDeepSky)(), bool (*vMethodMess)(), 
				bool (*vMethodNGC)(), bool (*vMethodIC)(), 
				bool (*vMethodOther)(), bool (*vMethodImages)() );
		
		~DeepSkyComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void init(KStarsData *data);

		bool (*visibleMessier)();
		bool (*visibleNGC)();
		bool (*visibleIC)();
		bool (*visibleOther)();
		bool (*visibleImages)();

	signals:
		void progressText( const QString &s );

	private:
		
		bool readDeepSkyData();
		void drawDeepSkyCatalog( QPainter& psky, SkyMap *map, QList<DeepSkyObject*>& catalog, 
					QColor& color, QColor& color2, bool drawObject, bool drawImage, double scale );

		QList<DeepSkyObject*> m_MessierList;
		QList<DeepSkyObject*> m_NGCList;
		QList<DeepSkyObject*> m_ICList;
		QList<DeepSkyObject*> m_OtherList;
};

#endif
