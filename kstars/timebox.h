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
class ExtDate;

/**@class timeBox
	* This class creates a QHBox with 3 QLineEdit Fields which will contain
	* either Time (Hour, Minute, Second) or Date (Day, Month, Year).
	*
	* Inherits QHBox
	*@author Pablo de Vicente
	*@version 1.0
	*/

class timeBox : public QLineEdit  {
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
	void showDate(ExtDate t);

	/**
	* returns a QTime object constructed from the fields of the timebox
	*/
	QTime createTime(bool *ok=0);

	/**
	* returns a ExtDate object constructed from the fields of the timebox
	*/
	ExtDate createDate(bool *ok=0);


	/**
	*         *@p s Fills the degrees entry field with string s
	*                 */
	void setEntry(QString s) { setText(s); }

	/**
	* returns a boolean. True indicates that the object holds a
	* Time Box. False that the object holds a Date Box.
	*/
	bool timeType(void) const {return timet;}

	/**
	* Clears all entries.
	*/
	void clearFields (void) { setEntry(""); }

private:
	bool timet;
};

#endif
