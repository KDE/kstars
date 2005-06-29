/***************************************************************************
                          observinglist.h  -  K Desktop Planetarium
                             -------------------
    begin                : 29 Nov 2004
    copyright            : (C) 2004 by Jeff Woods, Jason Harris
    email                : jcwoods@bellsouth.net, jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBSERVINGLIST_H
#define OBSERVINGLIST_H

#include <kdialogbase.h>
#include "skyobject.h"

class KStars;
class ObservingListUI;
class QStringList;

/**@class ObservingList
	*Tool window for managing a custom list of objects.  The window
	*displays the Name, RA, Dec, mag, and type of each object in the list.
	*
	*By selecting an object in the list, you can perform a number of functions
	*on that object:
	*+ Center it in the display (TBD)
	*+ Examine its Details Window (TBD)
	*+ Point the telescope at it (TBD)
	*+ Attach a custom icon or name label (TBD)
	*+ Attach a trail (solar system only) (TBD)
	*+ Open the AltVsTime tool (TBD)
	*
	*The user can also save/load their observing lists, and can export 
	*list data (TBD: as HTML table?  CSV format?  plain text?)
	*
	*The observing notes associated with the selected object are displayed 
	*below the list. (TBD)
	*
	*TODO: 
	*+ Implement a "shaded" state, in which the UI is compressed to
	*  make it easier to float on the KStars window.  Displays only
	*  object names, and single-letter action buttons, and no user log.
	*+ Implement an InfoBox version (the ultimate shaded state)
	*
	*@short Tool for managing a custom list of objects
	*@author Jeff Woods, Jason Harris
	*@version 1.0
	*/

class ObservingList : public KDialogBase
{
  Q_OBJECT

public:
/**@short Cunstructor
	*/
	ObservingList( KStars *_ks, QWidget* parent = 0 );
/**@short Destuctor (empty)
	*/
	~ObservingList() {}

/**@return true if the object is in the observing list
	*@p o pointer to the object to test.
	*/
	bool contains( const SkyObject *o );

	SkyObject* first() { return obsList.first(); }
	SkyObject* next() { return obsList.next(); }
	uint count() const { return obsList.count(); }

/**@return true if the window is in its default "large" state.
	*/
	bool isLarge() const { return bIsLarge; }

/**@short If the current list has unsaved changes, ask the user about saving it.
	*@note also clears the list in preparation of opening a new one
	*/
	void saveCurrentList();

public slots:
/**@short about to switch the visible table in the widget stack
	*@p tab pointer to the table which is about to be shown
	*/
	void slotPrepTable( QWidget *tab );
 
/**@short add a new object to list
	*@p o pointer to the object to add to the list
	*/
	void slotAddObject( SkyObject *o=NULL );
 
/**@short Remove skyobjects which are highlighted in the 
	*observing list tool from the observing list.
	*/
	void slotRemoveObjects();

/**@short Remove skyobject from the observing list.
	*@p o pointer to the SkyObject to be removed.
	*Use SkyMap::clickedObject() if o is NULL (default)
	*/
	void slotRemoveObject( SkyObject *o=NULL );

/**@short center the selected object in the display
	*/
	void slotCenterObject();

/**@short slew the telescope to the selected object
	*/
	void slotSlewToObject();

/**@short Show the details window for the selected object
	*/
	void slotDetails();

/**@short Show the details window for the selected object
	*/
	void slotAVT();

/**@short Tasks needed when changing the selected object
	*Save the user log of the previous selected object, 
	*find the new selected object in the obsList, and 
	*show the notes associated with the new selected object
	*/
	void slotNewSelection();

/**@short load an observing list from disk.
	*/
	void slotOpenList();

/**@short save the current observing list to disk.
	*/
	void slotSaveList();

/**@short save the current observing list to disk, specify filename.
	*/
	void slotSaveListAs();

/**@short construct a new observing list using the wizard.
	*/
	void slotWizard();

/**@short toggle between the large and small window states
	*/
	void slotToggleSize();

/**@short Save the user log text to a file.
	*@note the log is attached to the current object in obsList.
	*/
	void saveCurrentUserLog();

protected slots:
	void slotClose();

private:
	void syncTableSelection( bool syncFullTable=true );

	KStars *ks;
	ObservingListUI *ui;
	QPtrList<SkyObject> obsList;
	QPtrList<SkyObject> SelectedObjects;
	SkyObject *LogObject, *oCurrent;
	uint noNameStars;

	bool isModified, bIsLarge;
	QString ListName, FileName;
};

#endif // OBSERVINGLIST_H
