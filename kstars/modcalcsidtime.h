/***************************************************************************
                          modcalcsidtime.h  -  description
                             -------------------
    begin                : Wed Jan 23 2002
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

#ifndef MODCALCSIDTIME_H
#define MODCALCSIDTIME_H

#include <qvbox.h>

/**
  * @author Pablo de Vicente
  * Class which implements the KStars calculator module to compute Universal
  * time to/from Sidereal time.
  *
  * Inherits QVBox
  */

class QWidget;
class dms;
class dmsBox;
class timeBox;
class QTime;
class QDate;
class QRadioButton;
class QVBox;


class modCalcSidTime : public QVBox  {

Q_OBJECT

public:

	modCalcSidTime(QWidget *p, const char *n);
	~modCalcSidTime();

	QTime computeUTtoST (QTime u, QDate d, dms l);
	QTime computeSTtoUT (QTime s, QDate d, dms l);

public slots:	
	

	/** No descriptions */
	void slotClearFields();

	/** No descriptions */
	void slotComputeTime();

private:

	void showUT ( QTime d );
	void showST ( QTime d );
	QTime getUT (void);
	QTime getST (void);
	QDate getDate (void);
	dms getLongitude (void);
	
	QRadioButton *UtRadio, *StRadio;
	QVBox *rightBox;
	timeBox *UtBox, *StBox, *datBox;
	dmsBox *longBox;

};

#endif
