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
	 */
	dmsBox(QWidget *parent, const char *ni, bool deg=TRUE);

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
	void setDMS(QString s) { dmsName->setText(s); }

	/**
	* returns a dms object constructed from the fields of the dmsbox
	*/
	dms createDms(void);

	/**
	* returns a boolean indicating if object contains degrees or hours
	**/
	bool degType(void) {return deg;}

	/**
	* Clears the entries of the object
	*/
	void clearFields (void) { setDMS(""); }

private:

	int degree, minute, hour;
	double second;
	int second_int, msecond;
	bool deg;
	QLineEdit *dmsName;
	dms degValue;
};

#endif
