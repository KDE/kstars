/***************************************************************************
                          finddialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed Jul 4 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/




#ifndef FINDDIALOG_H
#define FINDDIALOG_H

#include <kdialogbase.h>

#include "skyobject.h"
#include "skyobjectname.h"

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QComboBox;
class QListBox;
class QListBoxItem;
class QStringList;

/**
	*Dialog window for finding SkyObjects by name.  The dialog contains
	*a QListBox showing the list of named objects, a QLineEdit for filtering
	*the list by name, and a QCombobox for filtering the list by object type.
	*
	*@short Find Object Dialog
  *@author Jason Harris
  *@version 0.4
  */

class FindDialog : public KDialogBase  {
	Q_OBJECT
	
	public:
/**
	*Constructor. Creates all widgets and packs them in QLayouts.  Connects
	*Signals and Slots.  Runs initObjectList().
	*/
	FindDialog( QWidget* parent = 0 );
/**
	*Destructor (empty).
	*/
	~FindDialog();
/**
	*Initialize QList of Objects for the QListBox.  Loop through each
	*catalog, and add each named object to the List.
	*/

	SkyObjectNameListItem * currentItem() { return currentitem; }
	
	private:
		void initObjectList( void );
		QVBoxLayout *vlay;
		QHBoxLayout *hlay;
		QListBox *SearchList;
		QLineEdit *SearchBox;
		QLabel *filterTypeLabel;
		QComboBox *filterType;
		QList <SkyObject> *filteredList;

	private:
		SkyObjectNameListItem *currentitem;
		
		void setListItemEnabled();
	
public slots:
/**
	*When Text is entered in the QLineEdit, filter the List of objects
	*so that only objects which start with the filter text are shown.
	*/	
	void filter();
/**
	*When the selection of the QComboBox is changed, filter the List of
	*objects so that only objects of the selected Type are shown.
	*/
	void filterByType( int i );
	
	protected:
		void timerEvent (QTimerEvent *);
	
	private slots:
		void updateSelection (QListBoxItem *);
	
};

#endif
