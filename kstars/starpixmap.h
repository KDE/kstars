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


/**
  *@author Thomas Kabelmann
  */


#include <qpixmap.h>

class StarPixmap {

	public:
		StarPixmap (bool useNightColors = false, int colorIntensity = 4);
		~StarPixmap();
		
		QPixmap* getPixmap (QChar *color, int s, int z);
		
		void setNightColors();
		void setDefaultColors();
		void setIntensity (int colorIntensity);
		bool useNightColors() { return night; }
		int Intensity() { return intensity; }
		
	private:
		
		QPixmap starPixmaps[10][24];	// the preloaded starpixmaps 10 colors/ 24 sizes
		bool night;
		int size, zoomlevel, intensity;

		void loadPixmaps (bool useNightColors = false, int colorIntensity = 4);
};

#endif
