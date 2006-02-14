/***************************************************************************
                          astrocalc.cpp  -  description
                             -------------------
    begin                : wed dec 19 16:20:11 CET 2002
    copyright            : (C) 2001-2005 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include <QSplitter>
#include <QTreeWidget>
#include <QTextEdit>
//#include <q3listview.h>
//#include <q3textview.h>
//#include <QPixmap>
#include <klocale.h>

#include "astrocalc.h"
#include "dms.h"
#include "modcalcjd.h"
#include "modcalcgeodcoord.h"
#include "modcalcgalcoord.h"
#include "modcalcsidtime.h"
#include "modcalcprec.h"
#include "modcalcapcoord.h"
#include "modcalcdaylength.h"
#include "modcalcazel.h"
#include "modcalcplanets.h"
#include "modcalceclipticcoords.h"
#include "modcalcangdist.h"
#include "modcalcequinox.h"
#include "modcalcvlsr.h"

AstroCalc::AstroCalc( QWidget* parent ) :
	KDialogBase( parent, "starscalculator", true, i18n("Calculator"), Close ),
	JDFrame(0), GeodCoordFrame(0), GalFrame(0), SidFrame(0), PrecFrame(0),
	AppFrame(0), DayFrame(0), AzelFrame(0), PlanetsFrame(0), EquinoxFrame(0),
	EclFrame(0), AngDistFrame(0)
{
	split = new QSplitter ( this );
	setMainWidget(split);

	navigationPanel = new QTreeWidget(split);
	navigationPanel->setColumnCount(1);
	navigationPanel->setHeaderLabels( QStringList(i18n("Section")) );
	navigationPanel->setSortingEnabled( false );

	splashScreen = new QTextEdit (i18n("<H2>KStars Astrocalculator</H2>"),split);

	splashScreen->setMaximumWidth(550);
	splashScreen->setMinimumWidth(400);

	rightPanel = GenText;

	QIcon jdIcon = QIcon ("jd.png");
	QIcon geodIcon = QIcon ("geodetic.png");
	QIcon solarIcon = QIcon ("geodetic.png");
	QIcon sunsetIcon = QIcon ("sunset.png");
	QIcon timeIcon = QIcon ("sunclock.png");

	QTreeWidgetItem * timeItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Time Calculators")) );
	timeItem->setIcon(0,timeIcon);

	QTreeWidgetItem * jdItem = new QTreeWidgetItem(timeItem,QStringList(i18n("Julian Day")) );
	jdItem->setIcon(0,jdIcon);

	new QTreeWidgetItem(timeItem,QStringList(i18n("Sidereal Time")) );
	new QTreeWidgetItem(timeItem,QStringList(i18n("Day Duration")) );
	new QTreeWidgetItem(timeItem,QStringList(i18n("Equinoxes & Solstices")) );
//	dayItem->setIcon(0,sunsetIcon);

	QTreeWidgetItem * coordItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Coordinate Converters")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Equatorial/Galactic")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Precession")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Apparent Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Horizontal Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Ecliptic Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Angular Distance")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("LSR Velocity")) );

	QTreeWidgetItem * geoItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Earth Coordinates")) );
	geoItem->setIcon(0,geodIcon);
	/*QListViewItem * cartItem = */new QTreeWidgetItem(geoItem,QStringList(i18n("Geodetic Coordinates")) );

	QTreeWidgetItem * solarItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Solar System")) );
	solarItem->setIcon(0,solarIcon);
	/*QListViewItem * planetsItem = */new QTreeWidgetItem(solarItem,QStringList(i18n("Planets Coordinates")) );

	connect(navigationPanel, SIGNAL(clicked(QTreeWidgetItem *)), this,
		SLOT(slotItemSelection(QTreeWidgetItem *)));
}

AstroCalc::~AstroCalc()
{
}

void AstroCalc::slotItemSelection(QTreeWidgetItem *item)
{
	QString election;

	if (item==0L) return;

	election = item->text(0);
	if(!(election.compare(i18n("Time Calculators"))))
		genTimeText();
	if(!(election.compare(i18n("Sidereal Time"))))
		genSidFrame();
	if(!(election.compare(i18n("Coordinate Converters"))))
		genCoordText();
	if(!(election.compare(i18n("Julian Day"))))
		genJdFrame();
	if(!(election.compare(i18n("Equatorial/Galactic"))))
		genGalFrame();
	if(!(election.compare(i18n("Precession"))))
		genPrecFrame();
	if(!(election.compare(i18n("Apparent Coordinates"))))
		genAppFrame();
	if(!(election.compare(i18n("Horizontal Coordinates"))))
		genAzelFrame();
	if(!(election.compare(i18n("Ecliptic Coordinates"))))
		genEclFrame();
	if(!(election.compare(i18n("Earth Coordinates"))))
		genGeodText();
	if(!(election.compare(i18n("Geodetic Coordinates"))))
		genGeodCoordFrame();
	if(!(election.compare(i18n("Day Duration"))))
		genDayFrame();
	if(!(election.compare(i18n("Equinoxes & Solstices"))))
		genEquinoxFrame();
	if(!(election.compare(i18n("Solar System"))))
		genSolarText();
	if(!(election.compare(i18n("Planets Coordinates"))))
		genPlanetsFrame();
	if(!(election.compare(i18n("Angular Distance"))))
		genAngDistFrame();
	if(!(election.compare(i18n("LSR Velocity"))))
		genVlsrFrame();

		previousElection = election;

}

void AstroCalc::genTimeText(void)
{

	delRightPanel();
	splashScreen = new QTextEdit (QString(),split);
	splashScreen->setMaximumWidth(550);
	splashScreen->setMinimumWidth(400);
	splashScreen->show();
		
	splashScreen->setText(i18n("<QT>"
														 "Section which includes algorithms for computing time ephemeris"
														 "<UL><LI>"
														 "<B>Julian Day:</B> Julian Day/Calendar conversion"
														 "</LI><LI>"
														 "<B>Sidereal Time:</B> Sidereal/Universal time conversion"
														 "</LI><LI>"
														 "<B>Day duration:</B> Sunrise, Sunset and noon time and "
														 "positions for those events"
														 "</LI><LI>"
														"<B>Equinoxes & Solstices:</B> Equinoxes, Solstices and duration of the "
														"seasons"
														 "</LI></UL>"
														 "</QT>"));
	
	rightPanel=TimeText;
														 	
}

void AstroCalc::genCoordText(void)
{
	delRightPanel();
	splashScreen = new QTextEdit (QString(),split);
	splashScreen->setMaximumWidth(550);
	splashScreen->setMinimumWidth(400);
	splashScreen->show();
	
	splashScreen->setText(i18n("<QT>"
														 "Section with algorithms for the conversion of "
														 "different astronomical systems of coordinates"
														 "<UL><LI>"
														 "<B>Precessor:</B> Precession of coordinates between epochs"
														 "</LI><LI>"
														 "<B>Galactic:</B> Galactic/Equatorial coordinates conversion"
														 "</LI><LI>"
														 "<B>Apparent:</B> Computation of current equatorial coordinates"
														 " from a given epoch"
														 "</LI><LI>"
														 "<B>Ecliptic:</B> Ecliptic/Equatorial coordinates conversion"
														 "</LI><LI>"
														 "<B>Horizontal:</B> Computation of azimuth and elevation for a "
														 "given source, time, and location on the Earth"
														 "</LI><LI>"
														 "<B>Angular Distance:</B> Computation of angular distance between "
														 "two objects whose positions are given in equatorial coordinates"
														 "</LI><LI>"
														 "<B>LRS Velocity:</B> Computation of the heliocentric, geocentric "
														 "and topocentric radial velocity of a source from its LSR velocity"
														 "</LI></UL>"
														 "</QT>"));

	rightPanel=CoordText;

}

void AstroCalc::genGeodText(void)
{
	delRightPanel();
	splashScreen = new QTextEdit (QString(),split);
	splashScreen->setMaximumWidth(550);
	splashScreen->setMinimumWidth(400);
	splashScreen->show();
	
	splashScreen->setText(i18n("<QT>"
														 "Section with algorithms for the conversion of "
														 "systems of coordinates for the Earth"
														 "<UL><LI>"
														 "<B>Geodetic Coords:</B> Geodetic/XYZ coordinate conversion"
														 "</LI></UL>"
														 "</QT>"));

	rightPanel=GeoText;
}

void AstroCalc::genSolarText(void)
{
	delRightPanel();
	splashScreen = new QTextEdit (QString(),split);
	splashScreen->setMaximumWidth(550);
	splashScreen->setMinimumWidth(400);
	splashScreen->show();
	
	splashScreen->setText(i18n("<QT>"
														 "Section with algorithms regarding information "
														 "on solar system bodies coordinates and times"
														 "<UL><LI>"
														 "<B>Planets Coords:</B> Coordinates for the planets, moon and sun "
														 " at a given time and from a given position on Earth "
														 "</LI></UL>"
														 "</QT>"));

	rightPanel=SolarText;
}


void AstroCalc::delRightPanel(void)
{
	if (rightPanel <= CoordText)
		delete splashScreen;
	else if (rightPanel == JD)
		delete JDFrame;
	else if (rightPanel == SidTime)
		delete SidFrame;
	else if (rightPanel == DayLength)
		delete DayFrame;
	else if (rightPanel == Equinox)
		delete EquinoxFrame;
	else if (rightPanel == GeoCoord)
		delete GeodCoordFrame;
	else if (rightPanel == Galactic)
		delete GalFrame;
	else if (rightPanel == Ecliptic)
		delete EclFrame;
	else if (rightPanel == Precessor)
		delete PrecFrame;
	else if (rightPanel == Apparent)
		delete AppFrame;
	else if (rightPanel == Azel)
		delete AzelFrame;
	else if (rightPanel == Planets)
		delete PlanetsFrame;
	else if (rightPanel == AngDist)
		delete AngDistFrame;
	else if (rightPanel == Vlsr)
		delete VlsrFrame;

}

void AstroCalc::genJdFrame(void)
{
	delRightPanel();
	JDFrame = new modCalcJD(split);
	rightPanel = JD;
}

void AstroCalc::genSidFrame(void)
{
	delRightPanel();
	SidFrame = new modCalcSidTime(split);
	rightPanel = SidTime;
}

void AstroCalc::genDayFrame(void)
{
	delRightPanel();
	DayFrame = new modCalcDayLength(split);
	rightPanel = DayLength;
}

void AstroCalc::genEquinoxFrame(void)
{
	delRightPanel();
	EquinoxFrame = new modCalcEquinox(split);
	rightPanel = Equinox;
}

void AstroCalc::genGeodCoordFrame(void)
{
	delRightPanel();
	GeodCoordFrame = new modCalcGeodCoord(split);
	rightPanel = GeoCoord;
}

void AstroCalc::genGalFrame(void)
{
	delRightPanel();
	GalFrame = new modCalcGalCoord(split);
	rightPanel = Galactic;
}

void AstroCalc::genEclFrame(void)
{
	delRightPanel();
	EclFrame = new modCalcEclCoords(split);
	rightPanel = Ecliptic;
}

void AstroCalc::genPrecFrame(void)
{
	delRightPanel();
	PrecFrame = new modCalcPrec(split);
	rightPanel = Precessor;
}

void AstroCalc::genAppFrame(void)
{
	delRightPanel();
	AppFrame = new modCalcApCoord(split);
	rightPanel = Apparent;
}

void AstroCalc::genAzelFrame(void)
{
	delRightPanel();
	AzelFrame = new modCalcAzel(split);
	rightPanel = Azel;
}

void AstroCalc::genPlanetsFrame(void)
{
	delRightPanel();
	PlanetsFrame = new modCalcPlanets(split);
	rightPanel = Planets;
}

void AstroCalc::genAngDistFrame(void)
{
	delRightPanel();
	AngDistFrame = new modCalcAngDist(split);
	rightPanel = AngDist;
}

void AstroCalc::genVlsrFrame(void)
{
	delRightPanel();
	VlsrFrame = new modCalcVlsr(split);
	rightPanel = Vlsr;
}

QSize AstroCalc::sizeHint() const
{
  return QSize(640,430);
}

#include "astrocalc.moc"
