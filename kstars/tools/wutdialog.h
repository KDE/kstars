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
#include <qdatetime.h>

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

		WUTDialog(KStars *ks);
		~WUTDialog();

	private slots:

		/** Load list for selected object type. */
		void slotLoadList(int);

		/** initialize the object lists */
		void init();

		/** update the time labels for selected object */
		void slotDisplayObject(QListBoxItem *item);

		void slotEveningMorning( int index );

		void slotChangeDate();
		void slotChangeLocation();
		/** open the detail dialog */
		void slotDetails();

	private:

		KStars *kstars;
		WUTDialogUI *WUT;
		
		/** Init All connections, used in constructor */
		void makeConnections();
		
		/** Init catgory list, used in constructor */
		void initCategories();
		
		/** Check visibility of object
			* @returns true if visible
			*/
		bool checkVisibility(SkyObjectName *oname);

		/** split the objects in several lists */
		void splitObjectList();

		/** Append object to the correct list. */
		void appendToList(SkyObjectName *o);

		ObjectNameList *objectList;

		QTime sunRiseTomorrow, sunSetToday, sunRiseToday, moonRise, moonSet;

		QDateTime Today;
		long double JDToday, JDTomorrow;
		GeoLocation *geo;
		int EveningFlag;
		
		struct List {
			QPtrList <SkyObjectName> visibleList[NCATEGORY];
			bool initialized[NCATEGORY];
		} lists;

};

#endif
