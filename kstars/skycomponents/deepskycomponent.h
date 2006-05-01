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

#include <QObject>
#include "skycomponent.h"

class QColor;
class SkyComposite;
class KStarsData;
class SkyMap;
class KSNumbers;
class DeepSkyObject;
class SkyPoint;

class DeepSkyComponent: public SkyComponent
{
	public:

		DeepSkyComponent( SkyComponent*, bool (*vMethodDeepSky)(), bool (*vMethodMess)(), 
				bool (*vMethodNGC)(), bool (*vMethodIC)(), 
				bool (*vMethodOther)(), bool (*vMethodImages)() );
		
		~DeepSkyComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

	/**
		*@short Read the ngcic.dat deep-sky database.
		*Parse all lines from the deep-sky object catalog files. Construct a DeepSkyObject
		*from the data in each line, and add it to the DeepSkyComponent.
		*
		*Each line in the file is parsed according to column position:
		*@li 0        IC indicator [char]  If 'I' then IC object; if ' ' then NGC object
		*@li 1-4      Catalog number [int]  The NGC/IC catalog ID number
		*@li 6-8      Constellation code (IAU abbreviation)
		*@li 10-11    RA hours [int]
		*@li 13-14    RA minutes [int]
		*@li 16-19    RA seconds [float]
		*@li 21       Dec sign [char; '+' or '-']
		*@li 22-23    Dec degrees [int]
		*@li 25-26    Dec minutes [int]
		*@li 28-29    Dec seconds [int]
		*@li 31       Type ID [int]  Indicates object type; see TypeName array in kstars.cpp
		*@li 33-36    Type details [string] (not yet used)
		*@li 38-41    Magnitude [float] can be blank
		*@li 43-48    Major axis length, in arcmin [float] can be blank
		*@li 50-54    Minor axis length, in arcmin [float] can be blank
		*@li 56-58    Position angle, in degrees [int] can be blank
		*@li 60-62    Messier catalog number [int] can be blank
		*@li 64-69    PGC Catalog number [int] can be blank
		*@li 71-75    UGC Catalog number [int] can be blank
		*@li 77-END   Common name [string] can be blank
		*@return true if data file is successfully read.
		*/
		virtual void init(KStarsData *data);

		/**
			*@short Update the sky positions of this component.
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates of the objects in this component.  If the KSNumbers* 
			*argument is not NULL, this function also recomputes precession and
			*nutation for the date in KSNumbers.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@note By default, the num parameter is NULL, indicating that 
			*Precession/Nutation computation should be skipped; this computation 
			*is only occasionally required.
			*/
		virtual void update( KStarsData *data, KSNumbers *num=0 );
		
		bool (*visibleMessier)();
		bool (*visibleNGC)();
		bool (*visibleIC)();
		bool (*visibleOther)();
		bool (*visibleImages)();

		/**
			*@short Search the children of this SkyComponent for 
			*a SkyObject whose name matches the argument
			*@p name the name to be matched
			*@return a pointer to the SkyObject whose name matches
			*the argument, or a NULL pointer if no match was found.
			*/
		virtual SkyObject* findByName( const QString &name );

		virtual SkyObject* objectNearest( SkyPoint *p, double &maxrad );
		QList<DeepSkyObject*>& objectList() { return m_DeepSkyList; }

		void clear();

	private:
		void drawDeepSkyCatalog( QPainter& psky, SkyMap *map, QList<DeepSkyObject*>& catalog, 
					QColor& color, QColor& color2, bool drawObject, bool drawImage, double scale );

		QList<DeepSkyObject*> m_DeepSkyList;
		QList<DeepSkyObject*> m_MessierList;
		QList<DeepSkyObject*> m_NGCList;
		QList<DeepSkyObject*> m_ICList;
		QList<DeepSkyObject*> m_OtherList;
};

#endif
