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

#ifndef OPSCOLORS_H
#define OPSCOLORS_H

#include <qwidget.h>
#include <qstringlist.h>

#include "opscolorsui.h"
#include "kstars.h"

class OpsColors : public OpsColorsUI
{
	Q_OBJECT

public:
	OpsColors( QWidget *parent=0, const char *name=0, WFlags fl = 0 );
	~OpsColors();

private slots:
	void newColor( QListBoxItem* item );
	void slotPreset( int i );
	void slotAddPreset();
	void slotRemovePreset();

private:
	bool setColors( QString filename );

	KStars *ksw;

	QStringList PresetFileList;
};

#endif  //OPSCOLORS_H

