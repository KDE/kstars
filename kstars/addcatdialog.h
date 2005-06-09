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
#include "draglistbox.h"

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
	QString name() const { return acd->CatalogName->text(); }

/**@return the filename of the custom catalog.
	*/
	QString filename() const { return acd->CatalogURL->url(); }

private slots:
/**Display contents of the import file.
	*/
	void slotShowDataFile();

/**Create the object catalog file, populate the objectList, 
	*and close the dialog.
	*/
	void slotCreateCatalog();

/**Preview the catalog file as constructed by the current parameters
	*/
	void slotPreviewCatalog();

/**Overridden from KDialogBase to show short help in a dialog rather 
	*than launch KHelpCenter.
	*/
	void slotHelp();

/**Overridden from KDialogBase, so that the entered file can be parsed
	*before window is closed.
	*/
	void slotOk();

private:
/**Attempt to parse the user's data file according to the fields 
	*specified in the Catalog fields list.
	*/
	bool validateDataFile();

/**Write a header line describing the data fields in the catalog, and 
	*defining the catalog name, ID prefix, and coordinate epoch.
	*/
	QString writeCatalogHeader();

	QVBoxLayout *vlay;
	AddCatDialogUI *acd;
	QString CatalogContents;
};

#endif
