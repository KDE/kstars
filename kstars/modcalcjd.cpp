/***************************************************************************
                          modcalcjd.cpp  -  description
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

#include "modcalcjd.h"
#include "modcalcjd.moc"
#include "timebox.h"
#include "ksutils.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qvbox.h>
#include <qgroupbox.h>
#include <qradiobutton.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qpushbutton.h>
#include <qdatetime.h>
#include <qstring.h>
#include <qbuttongroup.h>
#include <qlistview.h>
#include <qtextview.h>
#include <klocale.h>
#include <kglobal.h>

#include <kapplication.h>

modCalcJD::modCalcJD(QWidget *parentSplit, const char *name) : QVBox(parentSplit,name) {
	
	rightBox = new QVBox (parentSplit);
	
// Boxes in the page

// Radio Buttons
	
	QButtonGroup * InputBox = new QButtonGroup (rightBox);
	InputBox->setTitle( i18n("Input Selection") );

	JdRadio = new QRadioButton( i18n( "Julian day" ), InputBox );
	MjdRadio = new QRadioButton( i18n( "Modified Julian day" ), InputBox );
	DateRadio = new QRadioButton( i18n( "Date" ), InputBox );

	DateRadio->setChecked(TRUE);

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
		
	InputLay->addLayout (hlay, 0);
	InputLay->addLayout (hlay2, 0);
	
	hlay2->addWidget (Compute, 0, 0);
	hlay2->addWidget (Clear, 0, 0);

	
	hlay->addWidget ( JdRadio, 0, 0);
	hlay->addWidget ( MjdRadio, 0, 0);
	hlay->addWidget ( DateRadio, 0, 0);


// Input for Jd	

	QGroupBox *JdBox = new QGroupBox (rightBox);
	JdBox->setTitle( i18n("Julian Day"));
	
	QLabel * JdLabel = new QLabel( JdBox, "JdLabel" );
	JdLabel->setText( i18n("Julian Day","JD:") );
	JdName = new QLineEdit( JdBox, "JdName" );
		
	QHBoxLayout * JdLay = new QHBoxLayout( JdBox );
	JdLay->setSpacing(10);
	JdLay->setMargin(20);
	
	JdLay->addWidget ( JdLabel );
	JdLay->addWidget ( JdName );

// MJD Box	
	
	QGroupBox *MjdBox = new QGroupBox (rightBox,"MjdBox");
	MjdBox->setTitle( i18n("Modified Julian Day") );
	
	QLabel * MjdLabel = new QLabel( MjdBox, "JdLabel" );
	MjdLabel->setText( i18n( "Modified Julian Day","MJD:") );
	MjdName = new QLineEdit( MjdBox, "MjdName" );

	QHBoxLayout * MjdLay = new QHBoxLayout( MjdBox );
	MjdLay->setSpacing(10);
	MjdLay->setMargin(20);
	
	MjdLay->addWidget ( MjdLabel );
	MjdLay->addWidget ( MjdName );

// Input for Date and Time
	
	QGroupBox *DateBox = new QGroupBox (rightBox,"DateBox");
	DateBox->setTitle( i18n("Date && Time") );

	QVBoxLayout * D0Lay = new QVBoxLayout( DateBox );

	QHBox * datetimeBox = new QHBox(DateBox,"datetimeBox");

	QHBox * d0Box = new QHBox(datetimeBox,"datetimeBox");
	d0Box->setMaximumWidth(100);

	QLabel * timeLabel = new QLabel(d0Box,"timeLabel");
	timeLabel->setText( i18n( "Universal time","UT:") );
	timBox = new timeBox(d0Box,"timeBox");

	QHBox * d1Box = new QHBox(datetimeBox,"datetimeBox");
	d1Box->setMaximumWidth(140);

	QLabel * dateLabel = new QLabel(d1Box,"dateLabel");
	dateLabel->setText( i18n( "Universal time","Date:") );
	datBox = new timeBox(d1Box,"dateBox",FALSE);

	QPushButton *Now = new QPushButton( i18n( "Now" ), DateBox );
	showCurrentTime();
		
// Layout for the Calendar Box	

	Now->setFixedHeight(25);
	Now->setMaximumWidth(100);
		
	D0Lay->setMargin(20);
	D0Lay->setSpacing(10);
	D0Lay->addWidget(datetimeBox);
	D0Lay->addWidget(Now,0);

	rightBox->setMaximumWidth(550);
	rightBox->setMinimumWidth(400);
	rightBox->setMargin(14);
	rightBox->setSpacing(6);
	rightBox->show();

	connect( Compute, SIGNAL(clicked() ), this, SLOT( slotComputeTime() ) ) ;
	connect( Clear, SIGNAL(clicked() ), this, SLOT( slotClearTime() ) ) ;
	connect( Now, SIGNAL(clicked() ), this, SLOT( showCurrentTime() ) ) ;
		
}

modCalcJD::~modCalcJD(void)
{
	delete rightBox;
}

void modCalcJD::slotComputeTime (void)
{
	
	if(DateRadio->isChecked()) {
		computeFromCalendar();
	} else if (JdRadio->isChecked()) {
		JdName->setFocus();
		computeFromJd();
	} else if (MjdRadio->isChecked()) {
		MjdName->setFocus();
		computeFromMjd();
	}

}

void modCalcJD::computeFromCalendar (void)
{
	long double julianDay, modjulianDay;
	
	julianDay = KSUtils::UTtoJulian( getQDateTime() );
//	JdName->setText(QString("%1").arg(julianDay,13,'f',5));
	JdName->setText(KGlobal::locale()->formatNumber( (double)julianDay, 5 ) );

	modjulianDay = julianDay - 2400000.5;
	//MjdName->setText(QString("%1").arg(modjulianDay,13,'f',5));
	MjdName->setText(KGlobal::locale()->formatNumber( (double)modjulianDay, 5 ) );
}

void modCalcJD::computeFromMjd (void)
{
	long double julianDay, modjulianDay;

	//modjulianDay = MjdName->text().toDouble();
	modjulianDay = KGlobal::locale()->readNumber( MjdName->text() );
	julianDay = 	2400000.5 + modjulianDay;
	JdName->setText(KGlobal::locale()->formatNumber( julianDay, 5 ) );
	//JdName->setText(QString("%1").arg(julianDay,13,'f',5));
	computeFromJd();
	
}
void modCalcJD::computeFromJd (void)
{
	long double julianDay, modjulianDay;

	QDateTime dt;

	//julianDay = JdName->text().toDouble();
	julianDay = KGlobal::locale()->readNumber( JdName->text() );
	dt = KSUtils::JDtoDateTime( julianDay );

	datBox->showDate( dt.date() );
	timBox->showTime( dt.time() );

	modjulianDay = julianDay - 2400000.5;
	MjdName->setText(KGlobal::locale()->formatNumber( modjulianDay, 5 ) );
	//MjdName->setText(QString("%1").arg(modjulianDay,13,'f',5));
}


void modCalcJD::slotClearTime (void)
{
	JdName->setText ("");
	MjdName->setText ("");
	datBox->clearFields();
	timBox->clearFields();
}

void modCalcJD::showCurrentTime (void)
{
	QDateTime dt = QDateTime::currentDateTime();
	
	datBox->showDate( dt.date() );
	timBox->showTime( dt.time() );
}

QDateTime modCalcJD::getQDateTime (void)
{
	QDateTime dt ( datBox->createDate() , timBox->createTime() );

	return dt;
}

/** Gets Julian Day in the Box */
long double modCalcJD::getJd(void)
{
		long double julianDay;
		
		julianDay = JdName->text().toDouble();
		return julianDay;
}

/** Shows Julian Day in the Box */
void modCalcJD::showJd(long double julianDay)
{
	JdName->setText(QString("%1").arg(julianDay,13,'f',5));
}
