/***************************************************************************
                          opscatalog.h  -  K Desktop Planetarium
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

#ifndef OPSCATALOG_H
#define OPSCATALOG_H

#include <qwidget.h>
#include <qlistview.h>

#include "opscatalogui.h"
#include "kstars.h"

class OpsCatalog : public OpsCatalogUI
{
	Q_OBJECT

public:
	OpsCatalog( QWidget *parent=0, const char *name=0, WFlags fl = 0 );
	~OpsCatalog();

private slots:
	void updateDisplay();
	void selectCatalog();
	void slotAddCatalog();
	void slotRemoveCatalog();

private:
	QCheckListItem *showMessier, *showMessImages, *showNGC, *showIC;
	KStars *ksw;
};

#endif  //OPSCATALOG_H

