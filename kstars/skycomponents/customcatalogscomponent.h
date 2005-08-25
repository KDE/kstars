/***************************************************************************
                          deepskycomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/17/08
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

#ifndef CUSTOMCATALOGCOMPONENT_H
#define CUSTOMCATALOGCOMPONENT_H

class QList;

/**@class CustomCatalogComponent
*Represents custom catalogs.

*@author Thomas Kabelmann
*@version 0.1
*/

class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;

class CustomCatalogComponent: public SkyComponent
{
	public:

		CustomCatalogComponent(SkyComposite*);
		
		virtual ~CustomCatalogComponent();

		virtual void draw(SkyMap *map, QPainter& psky, double scale);

		virtual void init(KStarsData *data);
	
		virtual void update(KStarsData*, KSNumbers*, bool needNewCoords);

	private:
		
		bool readCustomCatalogs();
		
		bool addCatalog(QString filename);
		
		bool removeCatalog( int i );
		
		CustomCatalog* createCustomCatalog(QString filename, bool showerrs);
		
		bool processCustomDataLine(int lnum, QStringList d, QStringList Columns,
			QString Prefix, QPtrList<SkyObject> &objList, bool showerrs,
			QStringList &errs)
		
		bool parseCustomDataHeader(QStringList lines, QStringList &Columns,
			QString &CatalogName, QString &CatalogPrefix,
			QString &CatalogColor, float &CatalogEpoch, int &iStart,
			bool showerrs, QStringList &errs)

		
		QList<CustomCatalogObject*> deepSkyListOther;

};

#endif
