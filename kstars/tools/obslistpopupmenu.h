/***************************************************************************
                          obslistpopupmenu.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun July 5 2009
    copyright            : (C) 2008 by Prakash Mohan
    email                : prakash.mohan@kdemail.net
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

#include <QMenu>

/**
 * @class ObsListPopupMenu
 * The Popup Menu for the observing list in KStars. The menu is sensitive to the
 * type of selection in the observing list.
 * @author Prakash Mohan
 * @version 1.0
 */
class ObsListPopupMenu : public QMenu
{
    Q_OBJECT
public:
    /** Default constructor*/
    ObsListPopupMenu();

    /** Destructor (empty)*/
    virtual ~ObsListPopupMenu();

    /** Initialize the popup menus. */
    /**
     * @short initializes the popup menu based on the kind of selection in the observation planner
     * @param sessionView true if we are viewing the session, false if we are viewing the wish list
     * @param multiSelection true if multiple objects were selected, false if a single object was selected
     * @param showScope true if we should show INDI/telescope-related options, false otherwise.
     * @note Showing this popup-menu without a selection may lead to crashes.
     */
    void initPopupMenu( bool sessionView, bool multiSelection, bool showScope );
};

#endif
