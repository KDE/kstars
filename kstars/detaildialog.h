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

class GeoLocation;
class QDateTime;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;
class QFrame;
class QLineEdit;
class QString;
class QStringList;
class QTextEdit;
class QListView;
//class QPtrListIterator;
class KStars;

struct ADVTreeData
{
      QString Name;
      QString Link;
      int Type;
};


/**DetailDialog is a window showing detailed information for a selected object.
	*Also shows a piece of the skymap centered on the object.
	*The kind of information displayed depends upon the object type:
	*
	*Stars: Long name, Genetive name, Spectral Type, Magnitude,
   *In additional to the General tab, there is also a tab for editing/viewing links,
   *a tab that provides advanced access to data, and a log tab.
  *@author Jason Harris, Jasem Mutlaq
  */

class DetailDialog : public KDialogBase  {
   Q_OBJECT
public: 
	DetailDialog(SkyObject *o, QDateTime lt, GeoLocation *geo, QWidget *parent=0, const char *name=0);
	~DetailDialog() {}
private:

    bool readUserFile(int type);
    bool verifyUserData(int type);
    void createGeneralTab(QDateTime lt, GeoLocation *geo);
    void createLinksTab();
    void createAdvancedTab();
    void createLogTab();

    void Populate(QListViewItem *parent);
    void forkTree(QListViewItem *parent);
    QString parseADVData(QString link);
    

	class NameBox : public QGroupBox {
	public:
		/**Constructor for stars */
		NameBox( QString pname, QString oname, QString typelabel, QString type,
			QString mag, QString distStr, QString size, QWidget *parent, const char *name=0 );
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
		CoordBox( SkyObject *o, QDateTime lt, dms *LST, QWidget *parent, const char *name=0 );
		~CoordBox() {}
	private:
		QLabel *RALabel, *DecLabel, *HALabel, *RA, *Dec, *HA;
		QLabel *AzLabel, *AltLabel, *AirMassLabel, *Az, *Alt, *AirMass;

		QVBoxLayout *vlayMain;
		QGridLayout *glayCoords;
	};

	class RiseSetBox : public QGroupBox {
	public:
		RiseSetBox( SkyObject *o, QDateTime lt, GeoLocation *geo, QWidget *parent, const char *name=0 );
		~RiseSetBox() {}
	private:
		QLabel *RTime, *TTime, *STime;
		QLabel *RTimeLabel, *TTimeLabel, *STimeLabel;
		QLabel *RAz, *TAlt, *SAz;
		QLabel *RAzLabel, *TAltLabel, *SAzLabel;
		QGridLayout *glay;
		QVBoxLayout *vlay;
	};

/*
	class ProperMotionBox : public QGroupBox {
		Q_OBJECT
	public:
		ProperMotionBox( QWidget *parent, const char *name=0 );
		~ProperMotionBox() {}
	private:
		QLabel *deltaRA, *deltaDec;
	};
*/
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
	QDateTime ut;
	NameBox *Names;
	CoordBox *Coords;
	RiseSetBox *RiseSet;

  public slots:
  void viewLink();
  void unselectImagesList();
  void unselectInfoList();
  void updateLists();
  void editLinkDialog();
  void removeLinkDialog();
  void viewADVData();
  void saveLogData();

};

#endif
