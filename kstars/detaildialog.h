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
#include <kdialogbase.h>

#include "skyobject.h"

class GeoLocation;
class QDateTime;
class QLabel;
class QHBoxLayout;
class QVBoxLayout;

/**DetailDialog is a window showing detailed information for a selected object.
	*Currently, the DetailDialog shows information regarding the object's names,
	*type, magnitude, coordinates and rise/transit/set events.
	*@short Show a dialog with detailed information about an object.
	*@author Jason Harris
	*@version 0.9
  */

class DetailDialog : public KDialogBase  {
   Q_OBJECT
public: 
/**Constructor.*/
	DetailDialog(SkyObject *o, QDateTime lt, GeoLocation *geo, QWidget *parent=0, const char *name=0);

/**Destructor (empty)*/
	~DetailDialog() {}
private:

/**NameBox is an internal class that encapsulates the object's name, type, 
	*and magnitude information.
	*/
	class NameBox : public QGroupBox {
	public:
	/**Constructor*/
		NameBox( QString pname, QString oname, QString typelabel, QString type,
			QString mag, QWidget *parent, const char *name=0 );

	/**Destructor (empty)*/
		~NameBox() {}
	
	private:
		QLabel *PrimaryName, *OtherNames, *TypeLabel, *Type, *MagLabel, *Mag;
		QLabel *SizeLabel, *Size;
		QVBoxLayout *vlay;
		QHBoxLayout *hlayType, *hlayMag;
		QGridLayout *glay;
	};

/**CoordBox is an internbal class that encapsulates the object's coordinate data.
	*/
	class CoordBox : public QGroupBox {
	public:
	/**Constructor*/
		CoordBox( SkyObject *o, QDateTime lt, QWidget *parent, const char *name=0 );

	/**Destructor (empty)*/
		~CoordBox() {}
	
	private:
		QLabel *RALabel, *DecLabel, *RA, *Dec;
		QLabel *AzLabel, *AltLabel, *Az, *Alt;

		QVBoxLayout *vlayMain;
		QGridLayout *glayCoords;
	};

/**RiseSetBox is an internal class that encapsulates data regarding the object's 
	*rise/transit/set events.
	*/
	class RiseSetBox : public QGroupBox {
	public:
	/**Constructor*/
		RiseSetBox( SkyObject *o, QDateTime lt, GeoLocation *geo, QWidget *parent, const char *name=0 );

	/**Destructor (empty)*/
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

	QVBoxLayout *vlay;

	long double jd;
	QDateTime ut;
	NameBox *Names;
	CoordBox *Coords;
	RiseSetBox *RiseSet;

};

#endif
