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

class KStars;
class ObjectNameList;
class GeoLocation;
class SkyObject;
class SkyObjectName;

class QFrame;
class QGroupBox;
class QVBoxLayout;
class QTabBar;
class QFrame;
class KListBox;
class KPushButton;

/**
	* What's up tonight dialog is a window which lists all skyobjects that will
	* be visible next night after sunrise.
  *@author Thomas Kabelmann
  */

class WUTDialog : public KDialogBase  {

	Q_OBJECT

	public:

		WUTDialog(KStars *ks);

		~WUTDialog();

	private slots:

		/** Load list for selected object type. */
		void loadList(int);

		/** initialize the object lists */
		void init();

		/** update the time labels for selected object */
		void updateTimes(QListBoxItem *item);

		/** open the detail dialog */
		void slotDetails();

	private:

		/** Check visibility of object
			* @returns true if visible
			*/
		bool checkVisibility(SkyObjectName *oname);

		/** split the objects in several lists */
		void splitObjectList();

		/** The layouts */
		void createLayout();

		/** The sun box */
		void createSunBox();

		/** The moon box */
		void createMoonBox();

		/** The tab widget */
		void createTabWidget();

		/** All connections */
		void makeConnections();

		/** Append object to the correct list. */
		void appendToList(SkyObjectName *o);

		QFrame *page;

		QVBoxLayout *vlay;

		QGroupBox *sunBox;

		QGroupBox *moonBox;

		// the time labels for sun
		QLabel *sunRiseTimeLabel, *sunSetTimeLabel;

		// the time labels for moon
		QLabel *moonRiseTimeLabel, *moonSetTimeLabel;

		// tab widget
		QTabBar *tabBar;

		// tab widget
		QFrame *frame;

		// listbox in tabwidget
		KListBox *objectListBox;

		// time labels in tabwidget
		QLabel *setTimeLabel, *riseTimeLabel;

		// information button in tabwidget
		KPushButton *detailsButton;

		KStars *kstars;

		ObjectNameList *objectList;

		QTime sunRiseTomorrow, sunSetToday, sunRiseToday, moonRise, moonSet;

		long double today, tomorrow;
		GeoLocation *geo;

		struct List {
			QPtrList <SkyObjectName> visibleList[5];
			bool initialized[5];
		} lists;

};

#endif
