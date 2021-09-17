/*
    SPDX-FileCopyrightText: 2008 Prakash Mohan <prakash.mohan@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

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
    ObsListPopupMenu();

    virtual ~ObsListPopupMenu() override = default;

    /**
     * Initialize the popup menus.
     * @short initializes the popup menu based on the kind of selection in the observation planner
     * @param sessionView true if we are viewing the session, false if we are viewing the wish list
     * @param multiSelection true if multiple objects were selected, false if a single object was selected
     * @param showScope true if we should show INDI/telescope-related options, false otherwise.
     * @note Showing this popup-menu without a selection may lead to crashes.
     */
    void initPopupMenu(bool sessionView, bool multiSelection, bool showScope);
};
