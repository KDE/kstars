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

#if (KDE_VERSION <= 222)
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

/**@short dialog for adding a custom objects catalog to KStars
  *@author Jason Harris
  *@version 0.9
  */

class AddCatDialog : public KDialogBase  {
	Q_OBJECT
public:
	AddCatDialog( QWidget *parent=0 );
	~AddCatDialog();
	QString name() const { return catName->text(); }
	QString filename() const { return catFileName->text(); }
	QList<SkyObject> objectList() { return objList; }
private slots:
	void findFile();
	void checkLineEdits();
	void validateFile();
	void slotOk();

private:
	QVBoxLayout *vlay;
	QHBoxLayout *hlay;
	KLineEdit *catFileName, *catName;
	QPushButton *browseForFile;
	QList<SkyObject> objList;

};

#endif
