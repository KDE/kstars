/***************************************************************************
                          starpixmap.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Sep 19 2001
    copyright            : (C) 2001 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef STARPIXMAP_H
#define STARPIXMAP_H

#include <qpixmap.h>

/**@class StarPixmap
	*Stores a two-dimensional array of star images, indexed by both size and color.
	*Based on a star's spectral type, brightness, and the current zoom level, the appropriate
	*image is selected from the array in the SkyMap::drawSymbol() function.
	*A two-dimensional array of QPixmap star images
	*@author Thomas Kabelmann
	*@version 1.0
	*/

class StarPixmap {
	public:
	/**Constructor.  Build the array of images, based on a color mode and a colorintensity
		*value.
		*@param colorMode the color mode: 0 (real colors), 1 (solid red), 2 (solid black), or 3 (solid white).
		*@param colorIntensity The thickness of the real-color circle drawn in mode 0 (it does nothing in the other color modes).
		*/
		StarPixmap (int colorMode=0, int colorIntensity = 4);

	/**Destructor (empty)*/
		~StarPixmap() {}

	/**Retrieve the pixmap from the array indexed by color and size
		*@param color the spectral type (one of O,B,A,F,G,K,M,N,P)
		*@param s the size index
		*/
		QPixmap* getPixmap (QChar *color, int s);

	/**Change the Color mode.  Regenerate the star image array.
		*@param newMode the new Color Mode to use.
		*/
		void setColorMode( int newMode );

	/**Change the color Intensity.  Regenerate the star image array.
		*@param newIntensity the new colorIntensity value.
		*/
		void setIntensity ( int newIntensity );

	/**@return the current color mode*/
		int mode() const { return colorMode; }

	/**@return the current colorIntensity value*/
		int intensity() const { return colorIntensity; }

	private:

		QPixmap starPixmaps[10][26];	// the preloaded starpixmaps 10 colors/ 24 sizes
		int size, colorMode, colorIntensity;

	/**Construct the array of star images*/
		void loadPixmaps ( int colorMode = 0, int colorIntensity = 4 );
};

#endif
