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

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QLabel;
class QLineEdit;
class QComboBox;
class QListBox;
class QListBoxItem;
//class QStringList;
class SkyObjectNameListItem;

/**@class FindDialog
	*Dialog window for finding SkyObjects by name.  The dialog contains
	*a QListBox showing the list of named objects, a QLineEdit for filtering
	*the list by name, and a QCombobox for filtering the list by object type.
	*
	*@short Find Object Dialog
	*@author Jason Harris
	*@version 1.0
	*/

class FindDialog : public KDialogBase  {
Q_OBJECT

public:
/**Constructor. Creates all widgets and packs them in QLayouts.  Connects
	*Signals and Slots.  Runs initObjectList().
	*/
	FindDialog( QWidget* parent = 0 );

/**Destructor
	*/
	~FindDialog();

/**@return the currently-selected item from the listbox of named objects
	*/
	SkyObjectNameListItem * currentItem() const { return currentitem; }

public slots:
/**When Text is entered in the QLineEdit, filter the List of objects
	*so that only objects which start with the filter text are shown.
	*/
	void filter();

/**When the selection of the object type QComboBox is changed, filter 
	*the List of objects so that only objects of the selected Type are shown.
	*/
	void filterByType();
	
/**Overloading the Standard KDialogBase slotOk() to show a "sorry" message 
	*box if no object is selected when the user presses Ok.  The window is 
	*not closed in this case.
	*/
	void slotOk();

private slots:
/**Init object list after opening dialog.
	*/
	void init();

/**Set the selected item in the list to the item specified.
	*/
	void updateSelection (QListBoxItem *);

/**Change current filter options.
	*/
	void setFilter( int f );

protected:
/**Process Keystrokes.  The Up and Down arrow keys are used to select the 
	*Previous/Next item in the listbox of named objects.
	*@param e The QKeyEvent pointer 
	*/
	void keyPressEvent( QKeyEvent *e );

private:
/**
	*Automatically select the first item in the list
	*/
	void setListItemEnabled();
	
	QVBoxLayout *vlay;
	QHBoxLayout *hlay;
	QListBox *SearchList;
	QLineEdit *SearchBox;
	QLabel *filterTypeLabel;
	QComboBox *filterType;

	SkyObjectNameListItem *currentitem;
	
	int Filter;
};

#endif
