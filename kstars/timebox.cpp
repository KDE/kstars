/***************************************************************************
                          timebox.cpp  -  description
                             -------------------
    begin                : Sun Jan 20 2002
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
#include <qhbox.h>
#include <qlineedit.h>
#include <qstring.h>
#include <qwidget.h>
#include <qdatetime.h>

timeBox::timeBox(QWidget *parent, const char *name, bool tt) : QHBox(parent,name) {

	QHBox * dBox = new QHBox(parent,name);
	
	hourName = new QLineEdit( dBox,"HourName");
	minuteName = new QLineEdit( dBox,"MinuteName");
	secondName = new QLineEdit( dBox,"SecondName");
	
	dBox->setSpacing(1);
	setStretchFactor(dBox,0);
	dBox->setMargin(4);

	hourName->setMaxLength(2);
	hourName->setMaximumWidth(20);

	minuteName->setMaxLength(2);
	minuteName->setMaximumWidth(20);

	if (tt) {
		secondName->setMaxLength(2);
		secondName->setMaximumWidth(20);
	}
	else {
		secondName->setMaxLength(6);
		secondName->setMaximumWidth(63);
	}


	if (tt) 
		dBox->setMaximumWidth(68);
	else
		dBox->setMaximumWidth(108);

	timet = tt;
}

void timeBox::showTime (QTime t)
{
	setHour( QString("%1").arg(t.hour(),2) );
	setMinute( QString("%1").arg(t.minute(),2) );
	setSecond( QString("%1").arg(t.second(),2) );

}

void timeBox::showDate (QDate t)
{
	setDay( QString("%1").arg(t.day(),2) );
	setMonth( QString("%1").arg(t.month(),2) );
	setYear( QString("%1").arg(t.year(),6) );

}

QTime timeBox::createTime (void)
{
	QTime t = QTime( hour(), minute(), second() );
	return t;
}

QDate timeBox::createDate (void)
{
	QDate d = QDate( second(), minute(), hour() );
	return	d;
}

void timeBox::clearFields (void) {

	setHour("");
	setMinute("");
	setSecond("");
}

timeBox::~timeBox(){
}
