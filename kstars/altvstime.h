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

#include <qlayout.h>
#include <qptrlist.h>
#include <qvariant.h>
#include <kdialogbase.h>

#include <kapplication.h>
#include <klocale.h>

#include "kstarsplotwidget.h"

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QDateEdit;
class QGroupBox;
class QLabel;
class QPushButton;
class QTabWidget;
class QPainter;
class QDateTime;
class QTime;
class SkyObject;
class SkyPoint;
class dms;
class dmsBox;
class GeoLocation;
class KStars;
class KLineEdit;
class KListBox;

class AVTPlotWidget : public KStarsPlotWidget
{
	Q_OBJECT
public:
/**Constructor
	*/
	AVTPlotWidget( double x1=0.0, double x2=1.0, double y1=0.0, double y2=1.0, QWidget *parent=0, const char* name=0 );

protected:
	void mouseMoveEvent( QMouseEvent *e );
	void mousePressEvent( QMouseEvent *e );
	void paintEvent( QPaintEvent *e );
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
	dms getLongitude (void);
	dms getLatitude (void);
	double getTZ( void );
	void initGeo(void);
	void showLongLat(void);
//	int UtMinutes(void);
//	QSize sizeHint() const;
	long double computeJdFromCalendar (void);
	double QDateToEpoch( const QDate &d );
	double getEpoch (QString eName);
	long double epochToJd (double epoch);
	void processObject( SkyObject *o, bool forceAdd=false );
	double findAltitude( SkyPoint *p, double hour );
	int currentPlotListItem() const { return PlotList->currentItem(); }
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
	// xnticks, ynticks = number of minor ticks of X and Y axis
	// xmticks, ymticks = number of major ticks of X and Y axis
//	int xnticks, xmticks, ynticks, ymticks;

	// xmin, xmax = Minimum and maximum X values = 0 24x60 (mnutes)
	// ymin, ymax = Minimum and maximum Y values = 0 90x60  (arcmin)
//	int xmin, xmax, ymin, ymax;

//	int xwidth, ywidth;

	// Value of the interval tick for x and y

//	int xticksep, xmticksep, yticksep, ymticksep;
//	int yticksize, xticksize, xmticksize;

	QGroupBox *AVTTotalBox;
	QLabel *PixmapLabel1, *dateLabel, *epochLabel, *nameLabel,
		*raLabel, *decLabel, *latLabel, *longLabel;
	QTabWidget *ctlTabs;
	QWidget *sourceTab, *dateTab;
	KLineEdit *nameBox, *epochName;
	QDateEdit *dateBox;
	dmsBox *latBox, *longBox, *raBox, *decBox;
	QPushButton *browseButton, *cityButton, *clearFieldsButton, *clearButton, *addButton, *updateButton;
	AVTPlotWidget *View;
	KListBox *PlotList;

	QVBoxLayout *topLayout, *dateLocationLayout, *AVTTotalBoxLayout,
		*sourceLeftLayout, *sourceMidLayout;
	QHBoxLayout *sourceLayout, *nameLayout, *coordLayout,
		*clearAddLayout, *longLatLayout, *updateLayout;
	GeoLocation *geoPlace;
	KStars *ks;
	QPtrList<SkyPoint> pList;
	QPtrList<SkyPoint> deleteList;

	int DayOffset;
	bool dirtyFlag;
};

#endif // ALTVSTIME_H
