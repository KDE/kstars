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

#ifndef MODCALCSIDTIME_H_
#define MODCALCSIDTIME_H_

#include <kapplication.h>
#include <QTextStream>
#include "ui_modcalcsidtime.h"
#include "widgets/calcframe.h"

class dms;
class QTime;
class ExtDate;
class GeoLocation;

/**
  * Class which implements the KStars calculator module to compute Universal
  * time to/from Sidereal time.
  *
  * Inherits modCalcSidTimeDlg
  *@author Pablo de Vicente
	*@version 0.9
  */
class modCalcSidTime : public CalcFrame, public Ui::modCalcSidTimeDlg  {

Q_OBJECT

public:

	modCalcSidTime(QWidget *p);
	~modCalcSidTime();

private slots:	
	void slotChangeLocation();
	void slotChangeDate();
	void slotConvertST( const QTime &lt );
	void slotConvertLT( const QTime &st );
	void slotShown();

	void slotUtChecked();
	void slotDateChecked();
	void slotStChecked();
	void slotLongChecked();
	void slotRunBatch();
	void processLines( QTextStream &istream );

private:
	/* Fills the UT, Date boxes with the current time 
	 * and date and the longitude box with the current Geo location 
	 */
	void showCurrentTimeAndLocation();

	QTime computeLTtoST(QTime lt);
	QTime computeSTtoLT(QTime st);

	void sidNoCheck();
	void utNoCheck();

	bool bSyncTime, stInputTime;
	GeoLocation *geo;
};

#endif
