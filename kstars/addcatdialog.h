/***************************************************************************
                          addcatdialog.h  -  description
                             -------------------
    begin                : Sun Mar 3 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ADDCATDIALOG_H
#define ADDCATDIALOG_H

#include <qglobal.h>

#include <qptrlist.h>

#include <kdialogbase.h>
#include <klineedit.h>
#include <kurlrequester.h>
#include "deepskyobject.h"
#include "addcatdialogui.h"

class QHBoxLayout;
class QVBoxLayout;

/**@class AddCatDialog
	*@short Dialog for adding custom object catalogs to KStars
	*@author Jason Harris
	*@version 1.0
	*/

class AddCatDialog : public KDialogBase  {
	Q_OBJECT
public:
/**Default constructor
	*/
	AddCatDialog( QWidget *parent=0 );

/**Destructor (empty)
	*/
	~AddCatDialog();

/**@return the name for the custom catalog.
	*/
	QString name() const { return acd->catName->text(); }

/**@return the filename of the custom catalog.
	*/
	QString filename() const { return acd->catFileName->lineEdit()->text(); }

/**@return QPtrList of SkyObjects as parsed from the custom catalog file.
	*/
	QPtrList<DeepSkyObject> objectList() { return objList; }

private slots:
/**If both the "catalog name" and "catalog filename" fields are filled,
	*enable the "Ok" button.
	*/
	void slotCheckLineEdits();

/**Attempt to parse SkyObjects from the custom catalog file.
	*If at least on e object is read successfully, close the dialog.
	*/
	void slotValidateFile();

/**Overridden from KDialogBase to show short help in a dialog rather 
	*than launch KHelpCenter.
	*/
	void slotHelp();

/**Overridden from KDialogBase, so that the entered file can be parsed
	*before window is closed.
	*/
	void slotOk();

private:
	QVBoxLayout *vlay;
	AddCatDialogUI *acd;
	QPtrList<DeepSkyObject> objList;
};

#endif
