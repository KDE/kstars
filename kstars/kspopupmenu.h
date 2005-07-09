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

#include <kpopupmenu.h>

/**@class KSPopupMenu
	*The KStars Popup Menu.  The menu is sensitive to the 
	*object type of the object which was clicked to invoke the menu.
	*Items in the menu include name and type data; rise/transit/set times;
	*actions such as Center, Details, Telescope actions, and Label;
	*and Image and Information URL links.
	*@author Jason Harris
	*@version 1.0
	*/

class KStars;
class StarObject;
class SkyObject;
class QLabel;

class KSPopupMenu : public KPopupMenu
{
Q_OBJECT
public:
/**Default constructor*/
	KSPopupMenu( QWidget *parent = 0, const char *name = 0 );
	
/**Destructor (empty)*/
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
	*if false, it reads "Remove Trail".
	*@param showAngularDistance if true, the Angular Distance item is added.
	*@param showObsList if true, the Add to List/Remove from List item is added.
 */
	void initPopupMenu( SkyObject *obj, QString name1, QString name2, QString type,
			bool showRiseSet=true, bool showCenterTrack=true,
			bool showDetails=true, bool showTrail=false, 
			bool addTrail=false, bool showAngularDistance=true,
			bool showObsList=true );

/**Add an item to the popup menu for each of the URL links associated with 
	*this object.  URL links appear in two categories: images and information pages.
	*For some objects, a link to Digitized Sky Survey images will automatically be added
	*in addition to the object's normal image links.  Also, for some objects, an
	*"Add link..." item will be included, which allows the user to add their own cutsom 
	*URLs for this object.
	*@param obj pointer to the skyobject which the menu describes
	*@param showDSS if TRUE, include DSS Image links
	*@param allowCustom if TRUE, include the "Add Link..." item
	*/
	void addLinksToMenu( SkyObject *obj, bool showDSS=true, bool allowCustom=true );
	
/**@short Create a popup menu for a star.  
	*
	*Stars get the following labels: a primary name and/or a genetive name, 
	*a spectral type, an object type ("star"), and rise/transit/set times.  
	*Stars get a "Center & Track" item, an Angular Distance item, and a 
	*"Detailed Info" item.  Named stars get an "Attach Label" item and an 
	*"Add Link..." item, and may have image/info links; all stars get DSS 
	*image links.  Stars do not get an "Add Trail" item.
	*@param star pointer to the star which the menu describes
	*/
	void createStarMenu( StarObject *star );
	
/**@short Create a popup menu for a deep-sky object.  
	*
	*DSOs get the following labels: 
	*a common name and/or a catalog name, an object type, and rise/transit/set
	*times.  DSOs get a "Center & Track" item, an Angular Distance item, an
	*"Attach Label" item, and a "Detailed Info" item.  
	*They may have image/info links, and also get the DSS Image links and the 
	*"Add Link..." item.  They do not get an "Add Trail" item.
	*@param obj pointer to the object which the menu describes
	*/
	void createDeepSkyObjectMenu( SkyObject *obj );
	
/**@short Create a popup menu for a custom-catalog object.
	*
	*For now, this behaves essentially like the createDeepSkyObjectMenu() 
	*function, except that adding custom links is (temporarily?) disabled.
	*@param obj pointer to the custom catalog object which the menu describes
	*/
	void createCustomObjectMenu( SkyObject *obj );
	
/**@short Create a popup menu for a solar system body. 
	*
	*Solar System bodies get a name label, a type label ("solar system"),
	*and rise/set/transit time labels. They also get Center&Track, 
	*Angular Distance, Detailed Info, Attach Label, and Add Trail items.  
	*They can have image/info links, and also get the "Add Link..." item.
	*@note despite the name "createPlanetMenu", this function is used for 
	*comets and asteroids as well.
	*@param p the solar system object which the menu describes.
	*/
	void createPlanetMenu( SkyObject *p );

/**@short Create a popup menu for empty sky.
	*
	*The popup menu when right-clicking on nothing is still useful.
	*Instead of a name label, it shows "Empty Sky".  The rise/set/transit 
	*times of the clicked point on the sky are also shown.  You also get
	*the Center & Track and Angular Distance items, and the DSS image links.
	*@param nullObj pointer to dummy SkyObject, just to hold the clicked 
	*coordinates for passing to initPopupMenu().
	*/
	void createEmptyMenu( SkyObject *nullObj=0 );
	
/**Set the rise/transit/set time labels for the object.  Compute these times 
	*for the object for the current date and location.  If the object is 
	*circumpolar or never rises, the rise and set labels will indicate this
	*(but the transit time should always be valid).
	*@param obj the skyobject whose r/t/s times are to be displayed.
	*/
	void setRiseSetLabels( SkyObject *obj );
	
/**Add a submenu for INDI controls (Slew, Track, Sync, etc).
	*@return true if a valid INDI menu was added.
	*/
	bool addINDI(void);

private:
	KStars *ksw;
	QLabel *pmTitle, *pmTitle2, *pmType, *pmConstellation;
	QLabel *pmRiseTime, *pmSetTime, *pmTransitTime;

};

#endif
