/***************************************************************************
                          linelistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2006/11/01
    copyright            : (C) 2006 by Jason Harris
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

#ifndef LINELISTCOMPONENT_H
#define LINELISTCOMPONENT_H

#define NCIRCLE 360   //number of segments used to define equator, ecliptic and horizon

#include <QList>
#include <QPen>

#include "skycomponent.h"

class KStars;
class SkyLine;

/**
 *@class LineListComponent
 *An abstract parent class, to be inherited by SkyComponents that store a QList
 *of SkyLines.
 *
 *@author Jason Harris
 *@version 0.1
 */
class LineListComponent : public SkyComponent
{
	public:
	
		LineListComponent( SkyComponent *parent, bool (*visibleMethod)() );
		
		virtual ~LineListComponent();
		
		enum { NoLabel = 0, LeftEdgeLabel = 1, RightEdgeLabel = 2, UnknownLabel };

		const QString& label() const { return Label; }
		void setLabel( const QString &label ) { Label = label; }

		int labelPosition() const { return LabelPosition; }
		void setLabelPosition( int Pos ) { LabelPosition = Pos; }

		const QPen& pen() const { return Pen; }
		void setPen( const QPen &p ) { Pen = p; }

		/**Draw the list of objects on the SkyMap*/
		virtual void draw(KStars *ks, QPainter& psky, double scale);
		
		/**Draw the object, if it is exportable to an image
		*@see isExportable()
		*/
		void drawExportable(KStars *ks, QPainter& psky, double scale);
		
		/**
			*@short Update the sky positions of this component.
			*
			*This function usually just updates the Horizontal (Azimuth/Altitude)
			*coordinates of the objects in this component.  However, the precession
			*and nutation must also be recomputed periodically.  Requests to do
			*so are sent through the doPrecess parameter.
			*@p data Pointer to the KStarsData object
			*@p num Pointer to the KSNumbers object
			*@note By default, the num parameter is NULL, indicating that 
			*Precession/Nutation computation should be skipped; this computation 
			*is only occasionally required.
			*/
		virtual void update( KStarsData *data, KSNumbers *num=0 );
		
		QList<SkyLine*>& lineList() { return m_LineList; }

	private:
		SkyComponent *Parent;
		QList<SkyLine*> m_LineList;
		int LabelPosition;
		QString Label;
		QPen Pen;
};

#endif
