/***************************************************************************
                          modcalcequinox.cpp  -  description
                             -------------------
    begin                : dom apr 18 2002
    copyright            : (C) 2004 by Pablo de Vicente
    email                : p.devicentea@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "modcalcequinox.h"

#include "modcalcequinox.moc"
#include "dms.h"
#include "dmsbox.h"
#include "ksutils.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "kssun.h"

#include <qcombobox.h>
#include <qdatetimeedit.h>
#include <qstring.h>
#include <qtextstream.h>
#include <kfiledialog.h>
#include <kmessagebox.h>


modCalcEquinox::modCalcEquinox(QWidget *parentSplit, const char *name) : modCalcEquinoxDlg (parentSplit,name) {

	showCurrentYear();
	this->show();
}

modCalcEquinox::~modCalcEquinox(){
}

int modCalcEquinox::getYear (QString eName)
{
	int equinoxYear = eName.toInt();
	return equinoxYear;
}

void modCalcEquinox::showCurrentYear (void)
{
	QDateTime dt = QDateTime::currentDateTime();
	yearEdit->setText( QString( "%1").arg( dt.date().year() ) );
}

void modCalcEquinox::slotComputeEquinoxesAndSolstices (void)
{
	long double julianDay = 0., jdf = 0.;
	float deltaJd;
	KStarsData *kd = (KStarsData*) parent()->parent()->parent();
	KSSun *Sun = new KSSun(kd);
	
	int year0 = getYear( yearEdit->text() );
//	if ( equinoxSolsticesComboBox->currentText() == i18n() ) 
	
	if (equinoxSolsticesComboBox->currentItem() == 0 ) {
		julianDay = Sun->springEquinox(year0);
		jdf = Sun->summerSolstice(year0);
	}
	else if(equinoxSolsticesComboBox->currentItem() == 1) {
		julianDay = Sun->summerSolstice(year0);
		jdf = Sun->autumnEquinox(year0);
	}
	else if (equinoxSolsticesComboBox->currentItem() == 2 ) {
		julianDay = Sun->autumnEquinox(year0);
		jdf = Sun->winterSolstice(year0);
	}
	else if(equinoxSolsticesComboBox->currentItem() == 3) {
		julianDay = Sun->winterSolstice(year0);
		jdf = Sun->springEquinox(year0);
	}
	
	deltaJd = (float) (jdf - julianDay);
	showStartDateTime(julianDay);
	showSeasonDuration(deltaJd);
	
}

void modCalcEquinox::slotClear(void){
	yearEdit->setText("");
	seasonDuration->setText("");
}

void modCalcEquinox::showStartDateTime(double jd)
{
	startDateTimeEquinox->setDateTime( KSUtils::JDtoUT( jd ) );
}
void modCalcEquinox::showSeasonDuration(float deltaJd)
{
	seasonDuration->setText( QString( "%1").arg( deltaJd ) );
}

