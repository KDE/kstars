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

/**
  *@author Jason Harris
  */

class ColorScheme {
public: 
	ColorScheme();
//	ColorScheme( const ColorScheme &cs );
	~ColorScheme();
	QString colorNamed( QString name );
	QString colorAt( int i ) const;
	QString nameAt( int i ) const;
	QString keyAt( int i ) const;
	void setColor( QString key, QString color );
	bool load( QString filename );
	bool save( QString name );
	void copy( ColorScheme cs );
	void loadFromConfig( KConfig* );
	void saveToConfig( KConfig* );
	unsigned int numberOfColors() const { return (int)Palette.size(); }
	int starColorMode() const { return StarColorMode; }
	int starColorIntensity() const { return StarColorIntensity; }
	void setStarColorMode( int mode ) { StarColorMode = mode; }
	void setStarColorIntensity( int intens) { StarColorIntensity = intens; }
private:
	int StarColorMode, StarColorIntensity;
	QStringList KeyName, Name, Default;
	QMap<QString,QString> Palette;
};

#endif
