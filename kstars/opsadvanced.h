/***************************************************************************
                          opsadvanced.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 14 Mar 2004
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

#ifndef OPSADVANCED_H
#define OPSADVANCED_H

#include <qwidget.h>

#include "opsadvancedui.h"
#include "kstars.h"

class OpsAdvanced : public OpsAdvancedUI
{
	Q_OBJECT

public:
	OpsAdvanced( QWidget *parent=0, const char *name=0, WFlags fl = 0 );
	~OpsAdvanced();

private slots:
	void slotChangeTimeScale( float newScale );

private:
	KStars *ksw;
};

#endif  //OPSADVANCED_H

