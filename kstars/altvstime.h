/***************************************************************************
                          altvstime.h  -  description
                             -------------------
    begin                : Mon Dec 23 2002
    copyright            : (C) 2002 by Pablo de Vicente
    email                : vicente@oan.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef ALTVSTIME_H
#define ALTVSTIME_H

#include <qptrlist.h>
#include <qvariant.h>
#include <kdialogbase.h>
#include <klocale.h>

#include "kstarsplotwidget.h"
#include "altvstimeui.h"

class QPainter;
class QDateTime;
class QTime;
class SkyObject;
class SkyPoint;
class dms;
class GeoLocation;
class KStars;

class AVTPlotWidget : public KStarsPlotWidget
{
	Q_OBJECT
public:
/**Constructor
	*/
	AVTPlotWidget( double x1=0.0, double x2=1.0, double y1=0.0, double y2=1.0, QWidget *parent=0, const char* name=0 );

	void setSunRiseSetTimes( double sr, double ss ) { SunRise = sr; SunSet = ss; }

protected:
	void mouseMoveEvent( QMouseEvent *e );
	void mousePressEvent( QMouseEvent *e );
	void paintEvent( QPaintEvent *e );

private: 
	double SunRise, SunSet;
};

class AltVsTime : public KDialogBase
{
	Q_OBJECT

public:
	AltVsTime( QWidget* parent = 0);
	~AltVsTime();

//	void paintEvent(QPaintEvent *);
//	void drawGrid( QPainter *);
//	void initVars(void);
	void setLSTLimits();
	void showCurrentDate (void);
	QDateTime getQDate (void);
	void showLongLat(void);
//	int UtMinutes(void);
//	QSize sizeHint() const;
	void computeSunRiseSetTimes();
	long double computeJdFromCalendar (void);
	double QDateToEpoch( const QDate &d );
	double getEpoch (QString eName);
	long double epochToJd (double epoch);
	void processObject( SkyObject *o, bool forceAdd=false );
	double findAltitude( SkyPoint *p, double hour );
	int currentPlotListItem() const;
	QPtrList<SkyPoint>* skyPointList() { return &pList; }

public slots:

	void slotUpdateDateLoc(void);
	void slotClear(void);
	void slotClearBoxes(void);
	void slotAddSource(void);
	void slotBrowseObject(void);
	void slotChooseCity(void);
	void slotAdvanceFocus(void);
	void slotHighlight(void);

private:
	AVTPlotWidget *View;
	AltVsTimeUI *avtUI;
	QVBoxLayout *topLayout;

	GeoLocation *geo;
	KStars *ks;
	QPtrList<SkyPoint> pList;
	QPtrList<SkyPoint> deleteList;

	int DayOffset;
	bool dirtyFlag;
};

#endif // ALTVSTIME_H
