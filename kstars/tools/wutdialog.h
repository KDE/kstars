/***************************************************************************
                          wutdialog.h  -  K Desktop Planetarium
                             -------------------
    begin                : Die Feb 25 2003
    copyright            : (C) 2003 by Thomas Kabelmann
    email                : tk78@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef WUTDIALOG_H
#define WUTDIALOG_H

#include <qwidget.h>
#include <kdialogbase.h>
#include "kstarsdatetime.h"

#define NCATEGORY 8

class KStars;
class ObjectNameList;
class GeoLocation;
class SkyObject;
class SkyObjectName;
class WUTDialogUI;
class QFrame;

/**@class WUTDialog
	*What's up tonight dialog is a window which lists all skyobjects that will
	*be visible during the next night.
	*@author Thomas Kabelmann
	*@version 1.0
	*/

class WUTDialog : public KDialogBase  {
	Q_OBJECT

	public:

		/**@short Constructor*/
		WUTDialog(KStars *ks);
		/**@short Destructor*/
		~WUTDialog();

	private slots:

		/**@short Load the list of visible objects for selected object type.
			*@p type the object-type classification number
			*@see the SkyObject TYPE enum
			*/
		void slotLoadList(int type);

		/**@short Determine which objects are visible, and store them in
			*an array of lists, classified by object type 
			*/
		void init();

		/**@short display the rise/transit/set times for selected object 
			*/
		void slotDisplayObject(QListBoxItem *item);

		/**@short Apply user's choice of what part of the night should 
			*be examined:
			*@li 0: Evening only (sunset to midnight)
			*@li 1: Morning only (midnight to sunrise)
			*@li 2: All night (sunset to sunrise)
			*/
		void slotEveningMorning( int index );

		/**@short Adjust the date for the WUT tool
			*@note this does NOT affect the date of the sky map 
			*/
		void slotChangeDate();
		
		/**@short Adjust the geographic location for the WUT tool
			*@note this does NOT affect the geographic location for the sky map
			*/
		void slotChangeLocation();
		
		/**@short open the detail dialog for the current object
			*/
		void slotDetails();

		/**@short center the display on the current object
			*/
		void slotCenter();
	private:

		KStars *kstars;
		WUTDialogUI *WUT;
		
		/**@short Initialize all SIGNAL/SLOT connections, used in constructor */
		void makeConnections();
		
		/**@short Initialize catgory list, used in constructor */
		void initCategories();
		
		/**@short Check visibility of object
			*@p oname the name of the object to check
			*@return true if visible
			*/
		bool checkVisibility(SkyObjectName *oname);

		/**@short split the objects in object-type categories */
		void splitObjectList();

		/**@short Append object to the correct object-type list. */
		void appendToList(SkyObjectName *o);

		ObjectNameList *objectList;

		QTime sunRiseTomorrow, sunSetToday, sunRiseToday, moonRise, moonSet;
		KStarsDateTime T0, UT0, Tomorrow, TomorrowUT, Evening, EveningUT;

		GeoLocation *geo;
		int EveningFlag;
		
		struct List {
			QPtrList <SkyObjectName> visibleList[NCATEGORY];
			bool initialized[NCATEGORY];
		} lists;

};

#endif
