/***************************************************************************
                          opscolors.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 29 2004
    copyright            : (C) 2004 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/
/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OPSCOLORS_H_
#define OPSCOLORS_H_

#include <qstringlist.h>

#include "ui_opscolors.h"

/** @class OpsColors
	*The Colors page allows the user to adjust all of the colors used to 
	*display the night sky.  The colors are presented as a list of 
	*colored rectangles and a description of its assignment in the map.  
	*Clicking on any color opens a KColorDialog for selecting a new color.
	*
	*The user can also load preset color schemes, or create new schemes 
	*from the current set of colors.
	*  
	*@short The Colors page of the Options window.
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;

class OpsColors : public QFrame, public Ui::OpsColors
{
    Q_OBJECT

public:
    explicit OpsColors();
    ~OpsColors();

private slots:
    void newColor( QListWidgetItem* item );
    void slotPreset( int i );
    void slotAddPreset();
    void slotRemovePreset();
    void slotStarColorMode( int );
    void slotStarColorIntensity( int );

private:
    bool setColors( const QString &filename );

    QStringList PresetFileList;    
};

#endif  //OPSCOLORS_H_

