/***************************************************************************
                          kspopupmenu.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sat Feb 27 2003
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


#ifndef KSPOPUPMENU_H
#define KSPOPUPMENU_H

class KStars;
class StarObject;
class SkyObject;
class QLabel;

#include <kpopupmenu.h>

class KSPopupMenu : public KPopupMenu
{
Q_OBJECT
public:
	KSPopupMenu( QWidget *parent = 0, const char *name = 0 );
	~KSPopupMenu();

/**Initialize the popup menus. Adds name and type labels, and possibly
	*Rise/Set/Transit labels, Center/Track item, and Show Details item.
	*@short initialize the right-click popup menu
	*@param obj pointer to the skyobject which the menu describes
	*@param name1 The primary object name
	*@param name2 The (optional) secondary object name
	*@param type a string identifying the object type
	*@param showRiseSet if true, the Rise/Set/Transit labels are added
	*@param showCenterTrack if true, the Center/Track item is added
	*@param showDetails if true, the Show-Details item is added
	*@param showTrail if true, the add/remove planet trail item is added
	*@param addTrail if true, the add/remove planet trail item reads "Add Trail"
	*/
	void initPopupMenu( SkyObject *obj, QString name1, QString name2, QString type,
		bool showRiseSet=true, bool showCenterTrack=true,
		bool showDetails=true, bool showTrail=false, bool addTrail=false );

	void addLinksToMenu( SkyObject *obj, bool showDSS=true, bool allowCustom=true );
	void createStarMenu( StarObject *star );
	void createDeepSkyObjectMenu( SkyObject *obj );
	void createCustomObjectMenu( SkyObject *obj );
	void createPlanetMenu( SkyObject *p );
	void createEmptyMenu( SkyObject *nullObj=0 );
	void setRiseSetLabels( SkyObject *obj );
	bool addINDI(void);

private:
	KStars *ksw;

	QLabel *pmTitle, *pmTitle2, *pmType;
	QLabel *pmRiseTime, *pmSetTime, *pmTransitTime;

};

#endif
