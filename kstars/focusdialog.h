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
#include "focusdialogdlg.h"

class QVBoxLayout;
class QHBoxLayout;
class KLineEdit;
class SkyPoint;
class FocusDialogDlg;

/**@class FocusDialog 
	*@short A small dialog for setting the focus coordinates manually.
	*@author Jason Harris
	*@version 1.0
	*/

class FocusDialog : public KDialogBase {
	Q_OBJECT
public:
	/**Constructor. */
	FocusDialog( QWidget *parent=0 );

	/**Destructor (empty). */
	~FocusDialog();

	/**@return pointer to the SkyPoint described by the entered RA, Dec */
	SkyPoint* point() const { return Point; }

	/**@return suggested size of focus window. */
	QSize sizeHint() const;

	/**@return whether user set the AltAz coords */
	bool usedAltAz() const { return UsedAltAz; }

	void activateAzAltPage();
	long double epochToJd (double epoch);

	double getEpoch (QString eName);

public slots:
	/**If text has been entered in both KLineEdits, enable the Ok button. */
	void checkLineEdits();

	/**Attempt to interpret the text in the KLineEdits as Ra and Dec values.
		*If the point is validated, close the window.
		*/
	void validatePoint();
	void slotOk();

private:
	QVBoxLayout *vlay;
	QHBoxLayout *hlayRA, *hlayDec;
	KLineEdit *editRA, *editDec;
	SkyPoint *Point;
	FocusDialogDlg *fdlg;
	bool UsedAltAz;
};

#endif
