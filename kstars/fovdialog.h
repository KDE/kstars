/***************************************************************************
                          fovdialog.h  -  description
                             -------------------
    begin                : Fri 05 Sept 2003
    copyright            : (C) 2003 by Jason Harris
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

#ifndef FOVDIALOG_H
#define FOVDIALOG_H

#include <qptrlist.h>
#include <kdialogbase.h>
#include "fov.h"

/**@class FOVDialog Dialog to select a Field-of-View indicator (or create a new one)
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;
class FOVDialogUI;
class NewFOVUI;

class FOVDialog : public KDialogBase
{
	Q_OBJECT
public:
	FOVDialog( QWidget *parent=0 );
	~FOVDialog();
	unsigned int currentItem() const;
	QPtrList<FOV> FOVList;

protected:
	void paintEvent( QPaintEvent * );

private slots:
	void slotNewFOV();
	void slotEditFOV();
	void slotRemoveFOV();
	void slotSelect(QListBoxItem*);

private:
	void initList();

	KStars *ks;
	FOVDialogUI *fov;
};

/**@class NewFOV Dialog for defining a new FOV symbol
	*@author Jason Harris
	*@version 1.0
	*/
class NewFOV : public KDialogBase
{
	Q_OBJECT
public:
	NewFOV( QWidget *parent=0 );
	~NewFOV() {}
	NewFOVUI *ui;

public slots:
	void slotUpdateFOV();
	void slotComputeFOV();

protected:
	void paintEvent( QPaintEvent * );

private:
	FOV f;
};

#endif
