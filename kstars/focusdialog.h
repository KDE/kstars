/***************************************************************************
                          focusdialog.h  -  description
                             -------------------
    begin                : Sat Mar 23 2002
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

#ifndef FOCUSDIALOG_H
#define FOCUSDIALOG_H

#include <kdialogbase.h>

class QVBoxLayout;
class QHBoxLayout;
class KLineEdit;
class SkyPoint;

/**
  *@author Jason Harris
  */

class FocusDialog : public KDialogBase  {
	Q_OBJECT
public:
	FocusDialog( QWidget *parent=0 );
	~FocusDialog();

	/**@returns pointer to the SkyPoint described by the entered RA, Dec
		*/
	SkyPoint* point() const { return Point; }

public slots:
	void checkLineEdits();
	void validatePoint();
	void slotOk();

private:
	QVBoxLayout *vlay;
	QHBoxLayout *hlayRA, *hlayDec;
	KLineEdit *editRA, *editDec;
	SkyPoint *Point;
};

#endif
