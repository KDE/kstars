/***************************************************************************
                          opsguides.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun 6 Feb 2005
    copyright            : (C) 2005 by Jason Harris
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

#ifndef OPSGUIDES_H
#define OPSGUIDES_H

#include "opsguidesui.h"

class OpsGuides : public OpsGuidesUI
{
	Q_OBJECT

public:
	OpsGuides( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
	~OpsGuides();

private slots:
	void slotToggleConstellOptions();
	void slotToggleMilkyWayOptions();
};

#endif // OPSGUIDES_H
