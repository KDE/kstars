/***************************************************************************
                          dmsbox.h  -  description
                             -------------------
    begin                : Wed Dec 19 2002
    copyright            : (C) 2001-2002 by Pablo de Vicente
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

#ifndef DMSBOX_H
#define DMSBOX_H

#include <qhbox.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qstring.h>
#include "dms.h"


class dms;

/**
  *@author Pablo de Vicente
  * This class creates a QHBox with 3 QLineEdit Fields which will contain
  * Degrees (Degrees, Minutes, Seconds) or Hours (Hours, Minutes, Seconds).
  *
  * Inherits QHBox.
  */

class dmsBox : public QHBox  {
public: 
	/**
	 * Constructor for the dmsBox object.
	 * @p parent is the parent QWidget
	 * @p name is the name of the object
	 * @p deg is a boolean. TRUE if it is an object that will contain
	 *  degrees, arcminutes, ...
	 *  FALSE if it will contain hours, minutes, seconds..
	 * @p n is the number of characters of the first field. Defaults to
	 * 3.
	 */
	dmsBox(QWidget *parent, const char *ni, bool deg=TRUE, int n=3);

	~dmsBox();

	void showInHours(dms t);
	/**
	* Fills the QLineEdit fields of the dmsbox object from a dms object
	* showing hours, minutes and seconds.
	*@p t dms object from which to fill the entry fields
	*/
	void showInDegrees(dms t);

	/**
	* Fills the QLineEdit fields of the dmsbox object from a dms object
	* showing degrees, arcminutes and arcseconds.
	*@p t dms object from which to fill the entry fields
	*/

	void show(dms t);

	/**
	*@p s Fills the degrees entry field with string s
	*/
	void setDegree(QString s) { degreeName->setText(s); }

	/**
	*@p s Fills the minutes entry field with string s
	*/
	void setMinute(QString s) { minuteName->setText(s); }

	/**
	*@p s Fills the seconds entry field with string s
	*/
	void setSecond(QString s) { secondName->setText(s); }

	/**
	* returns a dms object constructed from the fields of the dmsbox
	*/
	dms createDms(void);

	/**
	* returns the hour as an integer
	*/
	int hours(void) {return degreeName->text().toInt(); }

	/**
	* returns the degrees field as an integer
	*/
	int degrees(void) {return degreeName->text().toInt(); }

	/**
	* returns the minutes field as an integer
	*/
	int minutes(void) {return minuteName->text().toInt(); }

	/**
	* returns the seconds field as an integer
	*/
	double seconds(void) {return secondName->text().toDouble(); }

	/**
	* returns a boolean indicating if object contains degrees or hours
	**/
	bool degType(void) {return deg;}

	/**
	* Clears the entries of the object
	*/
	void clearFields (void);

private:

	int degree, minute, hour;
	double second;
	int second_int, msecond;
	bool deg;
	QLineEdit *degreeName, *minuteName, *secondName;
	dms degValue;
};

#endif
