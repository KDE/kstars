/***************************************************************************
                          astrocalc.cpp  -  description
                             -------------------
    begin                : wed dec 19 16:20:11 CET 2002
    copyright            : (C) 2001-2002 by Pablo de Vicente
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

#include "astrocalc.h"
#include "dms.h"
#include "dmsbox.h"
#include "ksutils.h"
#include "modcalcjd.h"
#include "modcalcgeodcoord.h"
#include "geolocation.h"
#include "modcalcgalcoord.h"
#include "modcalcsidtime.h"
#include "modcalcprec.h"
#include <qvbox.h>
#include <qstring.h>
#include <qsplitter.h>
#include <qlistview.h>
#include <qtextview.h>
#include <kapp.h>
#include <klocale.h>
#include <kmenubar.h>


//AstroCalc::AstroCalc(QWidget *parent) : KMainWindow( parent)
//{
//	setCaption("Astronomical calculator");

//	QPopupMenu *filemenu = new QPopupMenu;
//	filemenu->insertItem( i18n("&Quit"), kapp, SLOT(quit()) );

//	QString about = i18n("KStars Calculator\n\n"
//	"(C) 2002 The KStars developers\n");

//	QPopupMenu *helpmenu = helpMenu(about);
//	KMenuBar * menu = menuBar();menu->insertItem( i18n("&File"), filemenu);
//	menu->insertSeparator();
//	menu->insertItem( i18n("&Help"), helpmenu);
AstroCalc::AstroCalc( QWidget* parent ) : KDialogBase( KDialogBase::Plain, i18n( "KStars Calculator" ), Ok|Close|Cancel, Ok, parent ) {

	QFrame *page = plainPage();

//	vbox = new QVBox (this);

	split = new QSplitter ( page );

	navigationPanel = new QListView (split,"NavigationPanel");
	splashScreen = new QTextView (i18n("<H2>KStars Astrocalculator</H2>"),"",split);
	rightPanel = GenText;

	navigationPanel->addColumn("Section");
	navigationPanel->setRootIsDecorated(1);

	QPixmap jdIcon = QPixmap ("jd.png");
	QPixmap geodIcon = QPixmap ("geodetic.png");
	QPixmap sunsetIcon = QPixmap ("sunset.png");
	QPixmap timeIcon = QPixmap ("sunclock.png");
			
	QListViewItem * timeItem = new QListViewItem(navigationPanel,i18n("Time calculator"));
	timeItem->setPixmap(0,timeIcon);
	
	QListViewItem * jdItem = new QListViewItem(timeItem,i18n("Julian Day"));
	jdItem->setPixmap(0,jdIcon);

	QListViewItem * stItem = new QListViewItem(timeItem,i18n("Sidereal Time"));
//	QListViewItem * dayItem = new QListViewItem(timeItem,i18n("Day duration"));
//	dayItem->setPixmap(0,sunsetIcon);
	
	QListViewItem * coordItem = new QListViewItem(navigationPanel,i18n("Coordinates conversor"));
	QListViewItem * galItem = new QListViewItem(coordItem,i18n("Equatorial/Galactic"));
	QListViewItem * precItem = new QListViewItem(coordItem,i18n("Precession"));
	
	QListViewItem * geoItem = new QListViewItem(navigationPanel,i18n("Earth coordinates"));
	geoItem->setPixmap(0,geodIcon);

	QListViewItem * cartItem = new QListViewItem(geoItem,i18n("Geodetic coordinates"));

	connect(navigationPanel, SIGNAL(clicked(QListViewItem *)), this,
		SLOT(slotItemSelection(QListViewItem *)));
	
//	splashScreen->setMaximumWidth(450);
	splashScreen->setMinimumWidth(420);
//	splashScreen->setMaximumHeight(380);
	splashScreen->setMinimumHeight(360);
	splashScreen->setMinimumSize(420, 360);
	navigationPanel->setMaximumWidth(180);
	navigationPanel->setMinimumWidth(160);
	navigationPanel->show();
	
//	page->resize(600,430);
	setMinimumSize(620, 430);
	resize(640,430);
	
//	setCentralWidget(vbox);
	
}

AstroCalc::~AstroCalc()
{
}

void AstroCalc::slotItemSelection(QListViewItem *item)
{
	QString election;
	
	if (item==0L) return;
	
	election = item->text(0);
	if(!(election.compare(i18n("Time calculator"))))
		genTimeText();
	if(!(election.compare(i18n("Sidereal Time"))))
		genSidFrame();
	if(!(election.compare(i18n("Coordinates conversor"))))
		genCoordText();
	if(!(election.compare(i18n("Julian Day"))))
		genJdFrame();
	if(!(election.compare(i18n("Equatorial/Galactic"))))
		genGalFrame();
	if(!(election.compare(i18n("Precession"))))
		genPrecFrame();
	if(!(election.compare(i18n("Earth coordinates"))))
		genGeodText();
	if(!(election.compare(i18n("Geodetic coordinates"))))
		genGeodCoordFrame();
		
		previousElection = election;
			
}

void AstroCalc::genTimeText(void)
{

	delRightPanel();
	splashScreen = new QTextView ("","",split);
	splashScreen->show();
		
	splashScreen->setText(i18n("<QT>"
														 "Section which includes algorithms for computing time ephemeris"
														 "<UL><LI>"
														 "<B>Julian Day:</B>  Julian Day/Calendar conversion"
														 "</LI><LI>"
														 "<B>Sidereal Time:</B> Sidereal/Universal time conversion"
														 "</LI><LI>"
														 "<B>Day duration:</B> Sunrise, Sunset and noon time and "
														 "positions for those events"
														 "</LI></UL>"
														 "</QT>"));
	
	rightPanel=TimeText;
														 	
}

void AstroCalc::genCoordText(void)
{
	delRightPanel();
	splashScreen = new QTextView ("","",split);
	splashScreen->show();
	
	splashScreen->setText(i18n("<QT>"
														 "Section with algorithms for the conversion of"
														 "different astronomical systems of coordinates"
														 "<UL><LI>"
														 "<B>Precessor:</B> Precession of coordinates between epochs"
														 "</LI><LI>"
														 "<B>Galactic:</B> Galactic/Equatorial coordinates conversion"
														 "</LI><LI>"
														 "<B>Apparent:</B> Computation of current equatorial coordinates"
														 "          from a given epoch\n"
														 "</LI><LI>"
														 "<B>Ecliptic:</B> Ecliptic/Equatorial coordinates conversion"
														 "</LI><LI>"
														 "<B>Horizontal:</B> Computation of azimuth and elevation for a"
														 "given source, time, and location on the Earth"
														 "</LI></UL>"
														 "</QT>"));

	rightPanel=CoordText;

}

void AstroCalc::genGeodText(void)
{
	delRightPanel();
	splashScreen = new QTextView ("","",split);
	splashScreen->show();
	
	splashScreen->setText(i18n("<QT>"
														 "Section with algorithms for the conversion of"
														 "systems of coordinates for the Earth"
														 "<UL><LI>"
														 "<B>Geodetic Coords:</B> Geodetic/XYZ coordinate conversion"
														 "</LI></UL>"
														 "</QT>"));

	rightPanel=GeoText;
}

void AstroCalc::delRightPanel(void)
{
	if (rightPanel <= 3)
		delete splashScreen;
	else if (rightPanel == JD)
		delete JDFrame;
	else if (rightPanel == SidTime)
		delete SidFrame;
	else if (rightPanel == GeoCoord)
		delete GeodCoordFrame;
	else if (rightPanel == Galactic)
		delete GalFrame;
	else if (rightPanel == Precessor)
		delete PrecFrame;

}

void AstroCalc::genJdFrame(void)
{
	delRightPanel();
	JDFrame = new modCalcJD(split,"JulianDay");
	rightPanel = JD;
}

void AstroCalc::genSidFrame(void)
{
	delRightPanel();
	SidFrame = new modCalcSidTime(split,"SiderealTime");
	rightPanel = SidTime;
}


void AstroCalc::genGeodCoordFrame(void)
{
	delRightPanel();
	GeodCoordFrame = new modCalcGeodCoord(split,"GeoCoord");
	rightPanel = GeoCoord;
}
void AstroCalc::genGalFrame(void)
{
	delRightPanel();
	GalFrame = new modCalcGalCoord(split,"Galactic");
	rightPanel = Galactic;
}

void AstroCalc::genPrecFrame(void)
{
	delRightPanel();
	PrecFrame = new modCalcPrec(split,"Precession");
	rightPanel = Precessor;
}

