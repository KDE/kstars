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

#include <qvbox.h>

/**
  *@author Pablo de Vicente
  * Class for KStars module which computes JD, MJD and Date/Time from the 
  * any of the other entries.
  *
  * Inherits QVBox
  */

class QWidget;
class VBox;
class QLineEdit;
class QRadioButton;
class QDateTime;
class timeBox;


class modCalcJD : public QVBox
{
Q_OBJECT
public:
	modCalcJD(QWidget *p, const char *n);
	~modCalcJD();
	
	long double getJD(QDateTime t);
	void computeFromCalendar (void);
	void computeFromMjd (void);
	void computeFromJd (void);
	QDateTime getQDateTime (void);

public slots:
		
	void slotComputeTime(void);
	void slotClearTime(void);
	void showCurrentTime(void);

private:

  /** Shows Julian Day in the Box */
  void showJd(long double jd);
  long double getJd(void);
	
	QVBox *rightBox;
	QLineEdit *JdName, *MjdName, *DayName, *MonthName, *YearName;
	QRadioButton *JdRadio, *MjdRadio, *DateRadio;
	timeBox *timBox, *datBox;
		
};

#endif
