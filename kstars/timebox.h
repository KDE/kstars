/***************************************************************************
                          timebox.h  -  description
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

#ifndef TIMEBOX_H
#define TIMEBOX_H

#include <qhbox.h>
#include <qlineedit.h>
#include <qstring.h>

class QTime;
class QDate;

/**
  * This class creates a QHBox with 3 QLineEdit Fields which will contain
  * either Time (Hour, Minute, Second) or Date (Day, Month, Year).
  *
  * Inherits QHBox
  *@author Pablo de Vicente
	*@version 0.9
  */

class timeBox : public QHBox  {
public: 
	/**
	 * Constructor for the timeBox object.
	 * @p parent is the parent QWidget
	 * @n name is the name of the object
	 * @tt boolean. true means that the object will hold a Time.
	 * false means that the object will hold a Date.
	 */
	timeBox(QWidget *parent, const char *n, bool tt=TRUE);

	~timeBox();

	/**
	* Fills the QLineEdit fields of the timebox object from a QTime object
	* showing hours, minutes and seconds.
	*@p t QTime object from which to fill the entry fields
	*/
	void showTime(QTime t);

	/**
	* Fills the QLineEdit fields of the timebox object from a QTime object
	* showing hours, minutes and seconds.
	*@p t QTime object from which to fill the entry fields
	*/
	void showDate(QDate t);

	/**
	*@p s Fills the Hours entry field with string s
	*/
	void setHour(QString s) { hourName->setText(s); }

	/**
	*@p s Fills the Days entry field with string s
	*/
	void setDay(QString s) { hourName->setText(s); }

	/**
	*@p s Fills the Minutes entry field with string s
	*/
	void setMinute(QString s) { minuteName->setText(s); }

	/**
	*@p s Fills the Month entry field with string s
	*/
	void setMonth(QString s) { minuteName->setText(s); }

	/**
	*@p s Fills the Seconds entry field with string s
	*/
	void setSecond(QString s) { secondName->setText(s); }

	/**
	*@p s Fills the Year entry field with string s
	*/
	void setYear(QString s) { secondName->setText(s); }

	/**
	* returns a QTime object constructed from the fields of the timebox
	*/
	QTime createTime(void);

	/**
	* returns a QDate object constructed from the fields of the timebox
	*/
	QDate createDate(void);

	/**
	* returns the hour as an integer
	*/
	int hour(void) {return hourName->text().toInt(); }

	/**
	* returns the Degrees field as an integer
	*/
	int day(void) {return hourName->text().toInt(); }

	/**
	* returns the Minutes field as an integer
	*/
	int minute(void) {return minuteName->text().toInt(); }

	/**
	* returns the Month field as an integer
	*/
	int month(void) {return minuteName->text().toInt(); }

	/**
	* returns the Seconds field as an integer
	*/
	int second(void) {return secondName->text().toInt(); }

	/**
	* returns the Year field as an integer
	*/
	int year(void) {return secondName->text().toInt(); }

	/**
	* returns a boolean. True indicates that the object holds a 
	* Time Box. False that the object holds a Date Box.
	*/
	bool timeType(void) {return timet;}

	/**
	* Clears all entries.
	*/
	void clearFields (void);
	
private:

	bool timet;
	QLineEdit *hourName, *minuteName, *secondName;
};

#endif
