/***************************************************************************
                          elts.h  -  description
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

#ifndef ELTS_H
#define ELTS_H

#include <qptrlist.h>
#include <qvariant.h>
#include <kdialogbase.h>

#include <kapplication.h>
#include <klocale.h>

class QVBoxLayout; 
class QHBoxLayout; 
class QGridLayout; 
class QDateEdit;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QPainter;
class QDateTime;
class QTime;
class SkyObject;
class SkyPoint;
class dms;
class dmsBox;
class eltsCanvas;
class GeoLocation;
class KStars;
class KListBox;

class elts : public KDialogBase
{ 
	Q_OBJECT

public:
	friend class eltsCanvas;
	
	elts( QWidget* parent = 0);
	~elts();

//	void paintEvent(QPaintEvent *);
//	void drawGrid( QPainter *);
//	void initVars(void); 
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
	void processObject( SkyObject *o );

public slots:

	void slotUpdateDateLoc(void);
	void slotClear(void);
	void slotClearBoxes(void);
	void slotAddSource(void);
	void slotBrowseObject(void);
	void slotChooseCity(void);
	void slotAdvanceFocus(void);

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

	QGroupBox *eltsTotalBox;
	QLabel *PixmapLabel1, *dateLabel, *epochLabel, *nameLabel, 
		*raLabel, *decLabel, *latLabel, *longLabel;
	QTabWidget *ctlTabs;
	QWidget *sourceTab, *dateTab;
	QLineEdit *nameBox, *epochName;
	QDateEdit *dateBox;
	dmsBox *latBox, *longBox, *raBox, *decBox;
	QPushButton *browseButton, *cityButton, *clearButton, *addButton, *updateButton;
	eltsCanvas *eltsView;
	KListBox *PlotList;

	QVBoxLayout *dateLocationLayout, *eltsTotalBoxLayout,
		*sourceLeftLayout, *sourceMidLayout;
	QHBoxLayout *sourceLayout, *nameLayout, *coordLayout, 
		*clearAddLayout, *longLatLayout, *updateLayout;
	GeoLocation *geoPlace;
	KStars *ks;
	QPtrList<SkyPoint> pList;
	QPtrList<SkyPoint> deleteList;

};

#endif // ELTS_H
