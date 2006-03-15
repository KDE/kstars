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

#include "listcomponent.h"

class SkyComponent;
class KStars;
class KStarsData;
class SkyMap;
class KSNumbers;
class KSFileReader;

class StarComponent: public ListComponent
{
	public:

		StarComponent(SkyComponent*, bool (*visibleMethod)());
		
		virtual ~StarComponent();

		virtual void draw(KStars *ks, QPainter& psky, double scale);

		virtual void init(KStarsData *data);

		KStarsData *data() { return m_Data; }

/**@return the current setting of the color mode for stars (0=real colors, 
	*1=solid red, 2=solid white or 3=solid black).
	*/
	int starColorMode( void ) const { return m_ColorMode; }

/**@short Set the color mode for stars (0=real colors, 1=solid red, 2=solid
	*white or 3=solid black).
	*/
	void setStarColorMode( int mode ) { m_ColorMode = mode; }

/**@short Retrieve the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*@return the current setting of the color intensity setting for stars.
	*/
	int starColorIntensity( void ) const { return m_ColorIntensity; }

/**@short Sets the color-intensity value for stars.
	*
	*When using the "realistic colors" mode for stars, stars are rendered as 
	*white circles with a colored border.  The "color intensity" setting modulates
	*the relative thickness of this colored border, so it effectively adjusts
	*the color-saturation level for star images.
	*/
	void setStarColorIntensity( int value ) { m_ColorIntensity = value; }

	float faintMagnitude() const { return m_FaintMagnitude; }
	void setFaintMagnitude( float newMagnitude );

	private:
		// some helper methods
		bool openStarFile(int i);

	/**
		*Parse a line from a stars data file, construct a StarObject from the data,
		*and add it to the StarComponent.
		*
		*Each line is parsed according to the column
		*position in the line:
		*@li 0-1      RA hours [int]
		*@li 2-3      RA minutes [int]
		*@li 4-8      RA seconds [float]
		*@li 10       Dec sign [char; '+' or '-']
		*@li 11-12    Dec degrees [int]
		*@li 13-14    Dec minutes [int]
		*@li 15-18    Dec seconds [float]
		*@li 20-28    dRA/dt (milli-arcsec/yr) [float]
		*@li 29-37    dDec/dt (milli-arcsec/yr) [float]
		*@li 38-44    Parallax (milli-arcsec) [float]
		*@li 46-50    Magnitude [float]
		*@li 51-55    B-V color index [float]
		*@li 56-57    Spectral type [string]
		*@li 59       Multiplicity flag (1=true, 0=false) [int]
		*@li 61-64    Variability range of brightness (magnitudes; bank if not variable) [float]
		*@li 66-71    Variability period (days; blank if not variable) [float]
		*@li 72-END   Name(s) [string].  This field may be blank.  The string is the common
		*             name for the star (e.g., "Betelgeuse").  If there is a colon, then
		*             everything after the colon is the genetive name for the star (e.g.,
		*             "alpha Orionis").
		*
		*@param line pointer to the line of data to be processed as a StarObject
		*/
		void processStar( const QString &line );

		KStarsData *m_Data;
		KSFileReader *starFileReader;
		float m_FaintMagnitude;
		int m_ColorMode, m_ColorIntensity;
};

#endif
