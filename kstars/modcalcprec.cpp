/***************************************************************************
                          modcalcprec.cpp  -  description
                             -------------------
    begin                : Sun Jan 27 2002
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

#include "modcalcprec.h"
#include "modcalcprec.moc"
#include "dmsbox.h"
#include "ksutils.h"
#include "skypoint.h"
#include "dms.h"

#include <klocale.h>
#include <qdatetime.h>
#include <klineedit.h>
#include <kapplication.h>

modCalcPrec::modCalcPrec(QWidget *parentSplit, const char *name) : modCalcPrecDlg(parentSplit,name) {

		this->show();
	
}

modCalcPrec::~modCalcPrec(){
	
}

SkyPoint modCalcPrec::getEquCoords (void) {
	dms raCoord, decCoord;

	raCoord = ra0Box->createDms(FALSE);
	decCoord = dec0Box->createDms();

	SkyPoint sp = SkyPoint (raCoord, decCoord);

	return sp;
}

QString modCalcPrec:: showCurrentEpoch () {

	double epoch = setCurrentEpoch();
	QString eName = QString("%1").arg(epoch,7,'f',2);

	return eName;
}

double modCalcPrec::setCurrentEpoch () {

	QDateTime dt = QDateTime::currentDateTime();
	long double jd = KSUtils::UTtoJD( dt );
	double epoch = JdtoEpoch(jd);

	return epoch;
}

double modCalcPrec::getEpoch (QString eName) {
	
	double epoch = eName.toDouble();

	return epoch;
}

void modCalcPrec::slotClearCoords (void) {
	
	ra0Box->clearFields();
	dec0Box->clearFields();
	rafBox->clearFields();
	decfBox->clearFields();
	epoch0Name->setText("");
	epochfName->setText("");
	
}

void modCalcPrec::slotComputeCoords (void) {

	SkyPoint sp;
	
	sp = getEquCoords();

	double epoch0 = getEpoch( epoch0Name->text() );
	double epochf = getEpoch( epochfName->text() );

	long double jd0 = epochToJd ( epoch0 );
	long double jdf = epochToJd ( epochf );

	sp.precessFromAnyEpoch(jd0, jdf);

	showEquCoords( sp );

}

void modCalcPrec::showEquCoords ( SkyPoint sp ) {
	rafBox->show( sp.ra(),FALSE );
	decfBox->show( sp.dec() );
}

double modCalcPrec::JdtoEpoch (long double jd) {

	long double Jd2000 = 2451545.00;
	return ( 2000.0 - ( Jd2000 - jd ) / 365.2425);
}

long double modCalcPrec::epochToJd (double epoch) {

	double yearsTo2000 = 2000.0 - epoch;

	if (epoch == 1950.0) {
		return 2433282.4235;
	} else if ( epoch == 2000.0 ) {
		return J2000;
	} else {
		return ( J2000 - yearsTo2000 * 365.2425 );
	}

}
