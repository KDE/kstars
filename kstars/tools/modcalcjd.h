/***************************************************************************
                          modcalcjd.h  -  description
                             -------------------
    begin                : Tue Jan 15 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODCALCJD_H
#define MODCALCJD_H

#include <q3vbox.h>

#include <kapplication.h>
#include "modcalcjddlg.h"

class QWidget;
class VBox;
class KStarsDateTime;

/**
  * Class for KStars module which computes JD, MJD and Date/Time from the
  * any of the other entries.
  *
  * Inherits QVBox
  *@author Pablo de Vicente
	*@version 0.9
  */
class modCalcJD : public QFrame, public Ui::modCalcJdDlg
{
Q_OBJECT
public:
	modCalcJD(QWidget *p);
	~modCalcJD();
	
	void computeFromCalendar (void);
	void computeFromMjd (void);
	void computeFromJd (void);
	KStarsDateTime getDateTime (void);

public slots:
		
	void slotComputeTime(void);
	void slotClearTime(void);
	void showCurrentTime(void);

private:

	/** Shows Julian Day in the Box */
	void showJd(long double jd);
	/** Shows the modified Julian Day in the Box */
	void showMjd(long double mjd);
};

#endif
