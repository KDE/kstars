/***************************************************************************
                          objlistpopupmenu.h  -  K Desktop Planetarium
                             -------------------
    begin                : Wed June 9 2010
    copyright            : (C) 2010 by Victor Carbune
    email                : victor.carbune@kdemail.net
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef OBJLISTPOPUPMENU_H_
#define OBJLISTPOPUPMENU_H_

#include <kmenu.h>
#include <kaction.h>

class KStars;
class StarObject;
class SkyObject;
class QLabel;

class ObjListPopupMenu : public KMenu
{
    Q_OBJECT

public:
    /**Default constructor*/
    ObjListPopupMenu();

    /**Destructor (empty)*/
    ~ObjListPopupMenu();

    void init();
    void showAddToSession();
    void showAddVisibleTonight();
    void showCenter();
    void showScope();
    void showDetails();
    void showAVT();
    void showLinks();
    void showRemoveFromSessionPlan();
    void showRemoveFromWishList();

    /**Initialize the popup menus*/
    void initPopupMenu( bool showAddToSession = false, bool showCenter = false, bool showDetails = false, bool showScope = false, bool showRemove = false, bool showLinks = false, bool sessionView = false );

private:
    KStars *ks;
};

#endif
