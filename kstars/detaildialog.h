/***************************************************************************
                          detaildialog.h  -  description
                             -------------------
    begin                : Sun May 5 2002
    copyright            : (C) 2002 by Jason Harris
    email                : kstars@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef DETAILDIALOG_H
#define DETAILDIALOG_H

#include <qfile.h>
#include <qlabel.h>
#include <q3ptrlist.h>
//Added by qt3to4:
#include <QPixmap>
#include <QMouseEvent>
#include <QFocusEvent>
#include <QHBoxLayout>
#include <kdialogbase.h>
#include <ktextedit.h>

#include "skyobject.h"
//UI headers
#include "details_data.h"
#include "details_position.h"
#include "details_links.h"
#include "details_database.h"
#include "details_log.h"

class GeoLocation;
class QHBoxLayout;
class QLineEdit;
class QFile;
class QPixmap;
class QString;
class QStringList;
class KStars;
class KStarsDateTime;

class DataWidget;
class PositionWidget;
class LinksWidget;
class DatabaseWidget;
class LogWidget;

struct ADVTreeData
{
	QString Name;
	QString Link;
	int Type;
};

/**@class DetailDialog is a window showing detailed information for a selected object.
	*The window is split into four Tabs: General, Links, Advanced and Log.
	*The General Tab displays some type-specific data about the object, as well as its 
	*present coordinates and Rise/Set/Transit times for the current date.  The Type-specific 
	*data are:
	*@li Stars: common name, genetive name, Spectral type, magnitude, distance
	*@li Solar System: name, object type (planet/comet/asteroid), Distance, magnitude (TBD), 
	*angular size (TBD)
	*@li Deep Sky: Common name, other names, object type, magnitude, angular size 
	*
	*The Links Tab allows the user to manage the list of Image and Information links 
	*listed in the object's popup menu.  The Advanced Tab allows the user to query 
	*a number of professional-grade online astronomical databases for data on the object.
	*The Log tab allows the user to attach their own text notes about the object.
	*@author Jason Harris, Jasem Mutlaq
	*@version 1.0
	*/

class DetailDialog : public KDialogBase  {
   Q_OBJECT
public: 
/**Constructor
	*/
	DetailDialog( SkyObject *o, const KStarsDateTime &ut, GeoLocation *geo, QWidget *parent=0 );
	
/**Destructor (empty)
	*/
	~DetailDialog() {}

	QPixmap* thumbnail() { return Thumbnail; }

public slots:
/**@short Add this object to the observing list.
	*/
	void addToObservingList();

/**@short Center this object in the display.
	*/
	void centerMap();

/**@short Center this object in the telescope.
	*/
	void centerTelescope();

/**@short Display thumbnail image for the object
	*/
	void showThumbnail();

/**@short Update thumbnail image for the object
	*/
	void updateThumbnail();

/**@short View the selected image or info URL in the web browser.
	*/
	void viewLink();

/**@short Unselect the currently selected item in the Images list
	*@note used when an item is selected in the Info list
	*/
	void unselectImagesList();

/**@short Unselect the currently selected item in the Info list
	*@note used when an item is selected in the Images list
	*/
	void unselectInfoList();

/**@short Rebuild the Image and Info URL lists for this object.  
	*@note used when an item is added to either list.
	*/
	void updateLists();

/**@short Open a dialog to edit a URL in either the Images or Info lists, 
	*and update the user's *url.dat file.
	*/
	void editLinkDialog();

/**@short remove a URL entry from either the Images or Info lists, and 
	*update the user's *url.dat file.
	*/
	void removeLinkDialog();

/**Open the web browser to the selected online astronomy database, 
	*with a query to the object of this Detail Dialog.
	*/
	void viewADVData();

/**Save the User's text in the Log Tab to the userlog.dat file.
	*/
	void saveLogData();

private:

/**Read in the user's customized URL file (either images or info webpages),
	*and store the file's lines in a QStringList.
	*@param type 0=Image URLs; 1=Info URLs
	*/
	bool readUserFile(int type);

/**Parse the QStringList containing the User's URLs.
	*@param type 0=Image URLs; 1=Info URLs
	*/
	bool verifyUserData(int type);

/**Build the General Data Tab for the current object.
	*/
	void createGeneralTab();

/**Build the Position Tab for the current object.
	*/
	void createPositionTab( const KStarsDateTime &ut, GeoLocation *geo );

/**Build the Links Tab, populating the image and info lists with the 
	*known URLs for the current Object.
	*/
	void createLinksTab();

/**Build the Advanced Tab
	*/
	void createAdvancedTab();

/**Build the Log Tab
	*/
	void createLogTab();


/**Populate the TreeView of known astronomical databases in the Advanced Tab
	*/
	void populateADVTree(Q3ListViewItem *parent);

/**For the databases TreeView
	*/
	void forkTree(Q3ListViewItem *parent);

/**Data for the Advanced Tab TreeView is stored in the file advinterface.dat.
	*This function parses advinterface.dat.
	*/
	QString parseADVData( const QString &link );
	

	SkyObject *selectedObject;
	KStars* ksw;

	// Edit Link Dialog
	QHBoxLayout *editLinkLayout;
	QLabel *editLinkURL;
	QLineEdit *editLinkField;
	QFile file;
	QPixmap *Thumbnail;
	int currentItemIndex;
	QString currentItemURL, currentItemTitle;
	QStringList dataList;

	Q3PtrListIterator<ADVTreeData> * treeIt;

	DataWidget *Data;
	PositionWidget *Pos;
	LinksWidget *Links;
	DatabaseWidget *Adv;
	LogWidget *Log;

};

class DataWidget : public QFrame, public Ui::DetailsData {
	Q_OBJECT
	public: 
		DataWidget( QWidget *parent=0 );
};

class PositionWidget : public QFrame, public Ui::DetailsPosition {
	Q_OBJECT
	public: 
		PositionWidget( QWidget *parent=0 );
};

class LinksWidget : public QFrame, public Ui::DetailsLinks {
	Q_OBJECT
	public: 
		LinksWidget( QWidget *parent=0 );
};

class DatabaseWidget : public QFrame, public Ui::DetailsDatabase {
	Q_OBJECT
	public: 
		DatabaseWidget( QWidget *parent=0 );
};

class LogWidget : public QFrame, public Ui::DetailsLog {
	Q_OBJECT
	public: 
		LogWidget( QWidget *parent=0 );
};

#endif
