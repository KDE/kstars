/***************************************************************************
                          modcalcsidtime.cpp  -  description
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

#include "timebox.h"
#include "dmsbox.h"
#include "dms.h"
#include "ksutils.h"
#include "modcalcsidtime.h"
#include "kstars.h"

#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qstring.h>
#include <qbuttongroup.h>
#include <qabstractlayout.h>
#include <kapp.h>
#include <klocale.h>


modCalcSidTime::modCalcSidTime(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {

	rightBox = new QVBox (parentSplit);

// Radio Buttons
	
	QButtonGroup * InputBox = new QButtonGroup (rightBox);
	InputBox->setTitle( i18n("Input Selection") );

	UtRadio = new QRadioButton( i18n( "Universal Time" ), InputBox );
	StRadio = new QRadioButton( i18n( "Sidereal Time" ), InputBox );

	UtRadio->setChecked(TRUE);

	QPushButton * Compute = new QPushButton( i18n( "Compute" ), InputBox );
	QPushButton * Clear = new QPushButton( i18n( "Clear" ), InputBox );
	
// Layout for the Radio Buttons Box

	QVBoxLayout * InputLay = new QVBoxLayout(InputBox);
	QHBoxLayout * hlay = new QHBoxLayout(2);
	QHBoxLayout * hlay2 = new QHBoxLayout(2);
	
	InputLay->setMargin(14);

	hlay->setSpacing(20);
	hlay->setMargin(6);
	hlay2->setMargin(6);
		
	Compute->setFixedHeight(25);
	Compute->setMaximumWidth(100);
	
	Clear->setFixedHeight(25);
	Clear->setMaximumWidth(100);
		
	InputLay->addLayout (hlay);
	InputLay->addLayout (hlay2);
	
	hlay2->addWidget (Compute);
	hlay2->addWidget (Clear);

	hlay->addWidget ( UtRadio);
	hlay->addWidget ( StRadio);
	
	// Input for Longitude and Date
	
	QGroupBox *longdateBox = new QGroupBox (rightBox);
	longdateBox->setTitle( i18n("Input Data") );

	QHBoxLayout * D0Lay = new QHBoxLayout( longdateBox);
	D0Lay->setMargin(20);
	D0Lay->setSpacing(6);
	
	QHBox * l0Box = new QHBox(longdateBox);
	l0Box->setMaximumWidth(150);
	
	QLabel * longLabel = new QLabel(l0Box);
	longLabel->setText( i18n( "Geographical Longitude","Longitude:") );
	longBox = new dmsBox(l0Box,"LongBox",4);

	QHBox * d0Box = new QHBox(longdateBox);
	d0Box->setMaximumWidth(120);

	QLabel * dateLabel = new QLabel(d0Box);
	dateLabel->setText( i18n( "Date:") );
	datBox = new timeBox(d0Box,"dateBox",FALSE);

	D0Lay->addWidget(l0Box);
	D0Lay->addWidget(d0Box);

// Input for Ut	

	QGroupBox *UtimeBox = new QGroupBox (rightBox);
	UtimeBox->setTitle( i18n("Universal Time"));
	
	QHBox *Ut0Box = new QHBox (UtimeBox);
	Ut0Box->setMaximumWidth(110);
	
	QLabel * UtLabel = new QLabel( Ut0Box);
	UtLabel->setText( i18n("Universal Time","UT:") );
	UtBox = new timeBox( Ut0Box, "UtBox" );
		
	QHBoxLayout * UtLay = new QHBoxLayout( UtimeBox);
	UtLay->setSpacing(6);
	UtLay->setMargin(20);
	
	UtLay->addWidget ( Ut0Box);

// Input for St	

	QGroupBox *StimeBox = new QGroupBox (rightBox);
	StimeBox->setTitle( i18n("Sidereal Time"));
	
	QHBox *St0Box = new QHBox (StimeBox);
	St0Box->setMaximumWidth(110);
	
	QLabel * StLabel = new QLabel( St0Box );
	StLabel->setText( i18n("Sidereal Time","ST:") );
	StBox = new timeBox( St0Box, "StBox" );
		
	QHBoxLayout * StLay = new QHBoxLayout( StimeBox);
	StLay->setSpacing(6);
	StLay->setMargin(20);
	
	StLay->addWidget ( St0Box );

//	rightBox->setMaximumWidth(450);
//	rightBox->setMinimumWidth(400);

	showCurrentTimeAndLong();

	rightBox->setMargin(14);
	rightBox->setSpacing(7);
	rightBox->show();

	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputeTime() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearFields() ) ) ;
		
}

modCalcSidTime::~modCalcSidTime(void) {

	delete rightBox;
}

//Del by KDevelop: void modCalcSidTime::slotComputeTime (void)
//Del by KDevelop: {
//Del by KDevelop: 	QTime ut, st;
//Del by KDevelop: 
//Del by KDevelop: 	QDate dt = getDate();
//Del by KDevelop: 	dms longitude = getLongitude();
//Del by KDevelop: 
//Del by KDevelop: 	if(UtRadio->isChecked()) {
//Del by KDevelop: 		ut = getUT();
//Del by KDevelop: 		st = computeUTtoST( ut, dt, longitude );
//Del by KDevelop: 		showST( st );
//Del by KDevelop: 	} else { 
//Del by KDevelop: 		st = getST();
//Del by KDevelop: 		ut = computeSTtoUT( st, dt, longitude );
//Del by KDevelop: 		showUT( ut );
//Del by KDevelop: 	}
//Del by KDevelop: 
//Del by KDevelop: }

void modCalcSidTime::showCurrentTimeAndLong (void)
{
	QDateTime dt = QDateTime::currentDateTime();

	datBox->showDate( dt.date() );
	showUT( dt.time() );
	//longBox->show( KStars::ks->geo()->lng() );
}

QTime modCalcSidTime::computeUTtoST (QTime ut, QDate dt, dms longitude)
{
	QTime lst;

	QDateTime utdt = QDateTime( dt, ut);
	lst = KSUtils::UTtoLST( utdt, longitude);
	return lst;
}

QTime modCalcSidTime::computeSTtoUT (QTime st, QDate dt, dms longitude)
{
	QTime ut;

	QDateTime dtst = QDateTime( dt, st);
	ut = KSUtils::LSTtoUT( dtst, longitude);
	return ut;
}

//Del by KDevelop: void modCalcSidTime::slotClearTime (void)
//Del by KDevelop: {
//Del by KDevelop: 	UtBox->clearFields();
//Del by KDevelop: 	StBox->clearFields();
//Del by KDevelop: }

void modCalcSidTime::showUT ( QTime dt )
{
	UtBox->showTime( dt );
}

void modCalcSidTime::showST ( QTime dt )
{
	StBox->showTime( dt );
}

QTime modCalcSidTime::getUT (void) 
{
	QTime dt;
	dt = UtBox->createTime();
	return dt;
}

QTime modCalcSidTime::getST (void) 
{
	QTime dt;
	dt = StBox->createTime();
	return dt;
}

QDate modCalcSidTime::getDate (void) 
{
	QDate dt;
	dt = datBox->createDate();
	return dt;
}

dms modCalcSidTime::getLongitude (void)
{
	dms longitude;
	longitude = longBox->createDms();
	return longitude;
}

void modCalcSidTime::slotClearFields(){
	UtBox->clearFields();
	StBox->clearFields();
}

void modCalcSidTime::slotComputeTime(){
	QTime ut, st;

	QDate dt = getDate();
	dms longitude = getLongitude();

	if(UtRadio->isChecked()) {
		ut = getUT();
		st = computeUTtoST( ut, dt, longitude );
		showST( st );
	} else {
		st = getST();
		ut = computeSTtoUT( st, dt, longitude );
		showUT( ut );
	}

}
