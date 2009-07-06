/***************************************************************************
                          obslistpopupmenu.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun July 5 2009
    copyright            : (C) 2008 by Prakash Mohan
    email                : prak902000@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#ifndef OBSLISTPOPUPMENU_H_
#define OBSLISTPOPUPMENU_H_

#include <kmenu.h>
#include <kaction.h>

class KStars;
class StarObject;
class SkyObject;
class QLabel;

/**@class ObsListPopupMenu
	*The Popup Menu for the observing list in KStars. The menu is sensitive to the 
	*type of selection in the observing list.
    *Author Prakash Mohan, v1.0
	*/
class ObsListPopupMenu : public KMenu
{
    Q_OBJECT
public:
    /**Default constructor*/
    ObsListPopupMenu( KStars *_ks);

    /**Destructor (empty)*/
    ~ObsListPopupMenu();

    /**Initialize the popup menus.
        */
    void initPopupMenu( bool showAddToSession = false, bool showCenter = false, bool showDetails = false, bool showScope = false, bool showRemove = false );

private:
    KStars *ks;
};

#endif
