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
  *@version 0.9
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
	*Destructor
	*/
	~FindDialog();

/**
  *Return the currently-selected item in the list
  */
	SkyObjectNameListItem * currentItem() const { return currentitem; }
	
	private:

		QVBoxLayout *vlay;
		QHBoxLayout *hlay;
		QListBox *SearchList;
		QLineEdit *SearchBox;
		QLabel *filterTypeLabel;
		QComboBox *filterType;

		SkyObjectNameListItem *currentitem;
		
		int Filter;
/**
  *Automatically select the first item in the list
  */
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
	*@param i the type to filter with
	*/
	void filterByType();
	
	protected:
/**
	*This is only needed so that the list gets initialized only after the constructor is finished
	*/
		void timerEvent (QTimerEvent *);
	
	private slots:
/**
	*Set the selected item in the list to the item specified.
	*/
		void updateSelection (QListBoxItem *);
/**
	*Change current filter options.
	*/
		void setFilter( int f );
	
};

#endif
