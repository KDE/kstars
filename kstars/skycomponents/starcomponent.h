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

/**@class StarComponent
*Represents the stars on the sky map. For optimization reasons the stars are
*not separate objects and are stored in a list.

*@author Thomas Kabelmann
*@version 0.1
*/

#define NHIPFILES 127

#include <QObject>

#include "listcomponent.h"
#include "starpixmap.h"

class SkyComponent;
class KStars;
class KStarsData;
class SkyMap;
class KSNumbers;
class KSFileReader;

class StarComponent: public QObject, public ListComponent
{
	Q_OBJECT

	public:

		StarComponent(SkyComponent*, bool (*visibleMethod)());
		
		virtual ~StarComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void init(KStarsData *data);

		KStarsData *data() { return m_Data; }

/**@return the current setting of the color mode for stars (0=real colors, 
	*1=solid red, 2=solid white or 3=solid black).
	*/
	int starColorMode( void ) const { return starpix->mode(); }

/**@short Set the color mode for stars (0=real colors, 1=solid red, 2=solid
	*white or 3=solid black).
	*/
	void setStarColorMode( int mode ) { starpix->setColorMode( mode ); }

/**@short Retrieve the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*@return the current setting of the color intensity setting for stars.
	*/
	int starColorIntensity( void ) const { return starpix->intensity(); }

/**@short Sets the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*/
	void setStarColorIntensity( int value ) { starpix->setIntensity( value ); }

	public slots:
		void setFaintMagnitude( float newMagnitude );

	signals:
		void progressText( const QString & );

	protected:
		virtual void drawNameLabel(QPainter &psky, SkyObject *obj, int x, int y, double scale);
		
	private:
		// some helper methods
		bool openStarFile(int i);
		void processStar( const QString &line );

		KStarsData *m_Data;

		StarPixmap *starpix;	// the pixmap of the stars

		KSFileReader *starFileReader;
		float m_FaintMagnitude;
};

#endif
