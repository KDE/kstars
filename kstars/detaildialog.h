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
	*Also shows a piece of the skymap centered on the object.
	*The kind of information displayed depends upon the object type:
	*
	*Stars: Long name, Genetive name, Spectral Type, Magnitude,
  *@author Jason Harris
  */

class DetailDialog : public KDialogBase  {
   Q_OBJECT
public: 
	DetailDialog(SkyObject *o, QDateTime lt, GeoLocation *geo, QWidget *parent=0, const char *name=0);
	~DetailDialog() {}
private:

	class NameBox : public QGroupBox {
	public:
		/**Constructor for stars */
		NameBox( QString pname, QString oname, QString typelabel, QString type,
			QString mag, QWidget *parent, const char *name=0 );
		~NameBox() {}
	private:
		QLabel *PrimaryName, *OtherNames, *TypeLabel, *Type, *MagLabel, *Mag;
		QLabel *SizeLabel, *Size;
		QVBoxLayout *vlay;
		QHBoxLayout *hlayType, *hlayMag;
		QGridLayout *glay;
	};

	class CoordBox : public QGroupBox {
	public:
		CoordBox( SkyObject *o, QDateTime lt, QWidget *parent, const char *name=0 );
		~CoordBox() {}
	private:
		QLabel *RALabel, *DecLabel, *RA, *Dec;
		QLabel *AzLabel, *AltLabel, *Az, *Alt;

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

	QVBoxLayout *vlay;

	long double jd;
	QDateTime ut;
	NameBox *Names;
	CoordBox *Coords;
	RiseSetBox *RiseSet;

};

#endif
