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

#include <qgroupbox.h>
#include <qfile.h>
#include <qptrlist.h>
#include <kdialogbase.h>

#include "skyobject.h"
#include "libkdeedu/extdate/extdatetime.h"

class GeoLocation;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QLineEdit;
class QString;
class QStringList;
class QTextEdit;
class QListView;
class KStars;

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
	DetailDialog(SkyObject *o, ExtDateTime lt, GeoLocation *geo, QWidget *parent=0, const char *name=0);
	
/**Destructor (empty)
	*/
	~DetailDialog() {}

public slots:
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

/**Build the General Tab for the current object.
	*@param lt The ExtDateTime to use for time-dependent data
	*@param geo pointer to the Geographic location to use for 
	*location-dependent data
	*/
	void createGeneralTab(ExtDateTime lt, GeoLocation *geo);

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
	void Populate(QListViewItem *parent);

/**For the databases TreeView
	*/
	void forkTree(QListViewItem *parent);

/**Data for the Advanced Tab TreeView is stored in the file advinterface.dat.
	*This function parses advinterface.dat.
	*/
	QString parseADVData(QString link);
	

	SkyObject *selectedObject;
	KStars* ksw;

	// General Tab
	QVBoxLayout *vlay;

	// Links Tab
	QFrame *linksTab;
	QGroupBox *infoBox, *imagesBox;
	QVBoxLayout *infoLayout, *imagesLayout, *topLayout;
	KListBox *infoList, *imagesList; 

	QPushButton *view, *addLink, *editLink, *removeLink;
	QSpacerItem *buttonSpacer;
	QHBoxLayout *buttonLayout;

	// Edit Link Dialog
	QHBoxLayout *editLinkLayout;
	QLabel *editLinkURL;
	QLineEdit *editLinkField;
	QFile file;
	int currentItemIndex;
	QString currentItemURL, currentItemTitle;
	QStringList dataList;

	// Advanced Tab
	QFrame *advancedTab;
	KListView *ADVTree;
	QPushButton *viewTreeItem;
	QLabel *treeLabel;
	QVBoxLayout *treeLayout;
	QSpacerItem *ADVbuttonSpacer;
	QHBoxLayout *ADVbuttonLayout;

	QPtrListIterator<ADVTreeData> * treeIt;

	// Log Tab
	QFrame *logTab;
	QTextEdit *userLog;
	QPushButton *saveLog;
	QVBoxLayout *logLayout;
	QSpacerItem *LOGbuttonSpacer;
	QHBoxLayout *LOGbuttonLayout;

	long double jd;
	ExtDateTime ut;

	class NameBox : public QGroupBox {
	public:
	/**Constructor
		*/
		NameBox( QString pname, QString oname, QString typelabel, QString type,
			QString mag, QString distStr, QString size, 
			QWidget *parent, const char *name=0, bool useSize=true );
	
	/**Destructor (empty)
		*/
		~NameBox() {}
	private:
		QLabel *PrimaryName, *OtherNames, *TypeLabel, *Type, *MagLabel, *Mag;
		QLabel  *DistLabel, *Dist, *SizeLabel, *AngSize;
		QVBoxLayout *vlay;
		QHBoxLayout *hlayType, *hlayMag, *hlayDist, *hlaySize;
		QGridLayout *glay;
	};

	class CoordBox : public QGroupBox {
	public:
	/**Constructor
		*/
		CoordBox( SkyObject *o, ExtDateTime lt, dms *LST, QWidget *parent, const char *name=0 );
	
	/**Destructor (empty)
		*/
		~CoordBox() {}
	private:
		QLabel *RALabel, *DecLabel, *HALabel, *RA, *Dec, *HA;
		QLabel *AzLabel, *AltLabel, *AirMassLabel, *Az, *Alt, *AirMass;

		QVBoxLayout *vlayMain;
		QGridLayout *glayCoords;
	};

	class RiseSetBox : public QGroupBox {
	public:
	/**Constructor
		*/
		RiseSetBox( SkyObject *o, ExtDateTime lt, GeoLocation *geo, QWidget *parent, const char *name=0 );
	
	/**Destructor (empty)
		*/
		~RiseSetBox() {}
	private:
		QLabel *RTime, *TTime, *STime;
		QLabel *RTimeLabel, *TTimeLabel, *STimeLabel;
		QLabel *RAz, *TAlt, *SAz;
		QLabel *RAzLabel, *TAltLabel, *SAzLabel;
		QGridLayout *glay;
		QVBoxLayout *vlay;
	};

	NameBox *Names;
	CoordBox *Coords;
	RiseSetBox *RiseSet;

};

#endif
