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

#if (QT_VERSION <= 299)
#include <qlist.h>
#else
#include <qptrlist.h>
#endif

#include <kdialogbase.h>
#include <klineedit.h>

#include "skyobject.h"

class QHBoxLayout;
class QVBoxLayout;
class QPushButton;

/**@short dialog for adding custom object catalogs to KStars
  *@author Jason Harris
  *@version 0.9
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

/**@returns text entered into "catalog name" KLineEdit.
	*/
	QString name() const { return catName->text(); }

/**@returns text entered into "catalog filename" KLineEdit.
	*/
	QString filename() const { return catFileName->text(); }

/**@returns QList of SkyObjects as parsed from the custom catalog file.
	*/
	QList<SkyObject> objectList() { return objList; }
private slots:
/**Fill the "catalog filename" KLineEdit according to selection from
	*a OpenFile dialog.
	*/
	void findFile();

/**If both the "catalog name" and "catalog filename" fields are filled,
	*enable the "Ok" button.
	*/
	void checkLineEdits();

/**Attempt to parse SkyObjects from the custom catalog file.
	*If at least on e object is read successfully, close the dialog.
	*/
	void validateFile();

/**Overridden from KDialogBase, so that the entered file can be parsed
	*before window is closed.
	*/
	void slotOk();

private:
	QVBoxLayout *vlay;
	QHBoxLayout *hlay;
	KLineEdit *catFileName, *catName;
	QPushButton *browseForFile;
	QList<SkyObject> objList;

};

#endif
