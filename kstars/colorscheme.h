/***************************************************************************
                          colorscheme.h  -  description
                             -------------------
    begin                : Wed May 8 2002
    copyright            : (C) 2002 by Jason Harris
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

#ifndef COLORSCHEME_H
#define COLORSCHEME_H

#include <qmap.h>

class QStringList;
class KConfig;

/**Implements a color scheme for kstars.  Contains a QMap with entries for 
	*each modifiable color, keyed by the color's ID string.  Also contains 
	*integers that specify the star-color mode (real color, white, red or black)
	*and the star color intensity, if real colors are used.
	*@short describes a KStars color scheme
	*@version 0.9
	*@author Jason Harris
	*/

class ColorScheme {

	public: 
	
	/**Constructor
		*/
		ColorScheme();

	/**Copy constructor
		*/
		ColorScheme( const ColorScheme &cs );

	/**Destructor (empty)
		*/
		~ColorScheme();

	/**Return a string describing the RGB color value of the color named 'name'.
		*Return white ("#FFFFFF") if no color named 'name' found.
		*@param name the ID string of the color to be returned. 
		*/
		QString colorNamed( QString name ) const;

	/**Return a string describing the RGB coor value of the color at index 
		*position i.  Return white ("#FFFFFF") if no color at position i found.
		*@param i the index of the color in the QMap.
		*/
		QString colorAt( int i ) const;

	/**Return the long name of the color at position i.
		*@param i the index of the color in the QMap.
		*/
		QString nameAt( int i ) const;

	/**Return the key ID string of the color at position i.
		*@param i the index of the color in the QMap.
		*/
		QString keyAt( int i ) const;

	/**Add a color to the color palette QMap.  If a color with the same 
		*key already exists, it is overwritten.
		*@short Add or modify a color in the color scheme
		*@param key the key ID for the new color.
		*@param color the RGB string for the new color.
		*/
		void setColor( QString key, QString color );

	/**@short Read in a color scheme from a file.
		*@param filename the filename from which to read the color scheme.
		*/
		bool load( QString filename );

	/**@short Save the current color scheme to a file whose name is based on 
		*the 'name' argument.
		*@param name The name of the color scheme.  The filename is derived from this.
		*/
		bool save( QString name );

	/**@short Copy the color scheme given as an argument.
		*@param cs the color scheme to copy.
		*/
		void copy( const ColorScheme &cs );

	/**@short Load the color scheme from the KStars configuration object.
		*/
		void loadFromConfig( KConfig* );

	/**@short Save the current color scheme to the KStars configuration object.
		*/
		void saveToConfig( KConfig* );

	/**@returns the number of colors in the color scheme.
		*/
#if (QT_VERSION < 300)
		unsigned int numberOfColors() const { return (int)Palette.count(); }
#else
		unsigned int numberOfColors() const { return (int)Palette.size(); }
#endif

	/**@returns the star color mode. { 0 = real colors; 1 = solid red; 
		*2 = solid black; 3 = solid white }
		*/
		int starColorMode() const { return StarColorMode; }

	/**@returns the star color intensity parameter.  The higher the value, 
		*the more saturated the star colors will appear.  This only has meaning 
		*when StarColorMode = 0 {real colors}.
		*/
		int starColorIntensity() const { return StarColorIntensity; }

	/**@short Set the star color mode.
		*@param mode the new star color mode value.
		*/
		void setStarColorMode( int mode ) { StarColorMode = mode; }

	/**@short Set the star color intensity.
		*@param intens the new star color intensity value.
		*/
		void setStarColorIntensity( int intens) { StarColorIntensity = intens; }

	private:

		int StarColorMode, StarColorIntensity;

		QStringList KeyName, Name, Default;

		QMap<QString,QString> Palette;

};

#endif
