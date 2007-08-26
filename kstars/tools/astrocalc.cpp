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

#include "astrocalc.h"

#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <klocale.h>

#include "dms.h"
#include "modcalcjd.h"
#include "modcalcgeodcoord.h"
#include "modcalcgalcoord.h"
#include "modcalcsidtime.h"
#include "modcalcapcoord.h"
#include "modcalcdaylength.h"
#include "modcalcaltaz.h"
#include "modcalcplanets.h"
#include "modcalceclipticcoords.h"
#include "modcalcangdist.h"
#include "modcalcvizequinox.h"
#include "modcalcvlsr.h"

AstroCalc::AstroCalc( QWidget* parent ) :
	KDialog( parent ), JDFrame(0), GeodCoordFrame(0),
        GalFrame(0), SidFrame(0), AppFrame(0),
        DayFrame(0), AltAzFrame(0), PlanetsFrame(0), EquinoxFrame(0),
	EclFrame(0), AngDistFrame(0)
{
	split = new QSplitter ( this );
	setMainWidget(split);
        setCaption( i18n("Calculator") );
        setButtons( KDialog::Close );

	navigationPanel = new QTreeWidget(split);
	navigationPanel->setColumnCount(1);
	navigationPanel->setHeaderLabels( QStringList(i18n("Calculator modules")) );
	navigationPanel->setSortingEnabled( false );

	QIcon jdIcon = QIcon ("jd.png");
	QIcon geodIcon = QIcon ("geodetic.png");
	QIcon solarIcon = QIcon ("geodetic.png");
	QIcon sunsetIcon = QIcon ("sunset.png");
	QIcon timeIcon = QIcon ("sunclock.png");

	//Populate the tree widget
	QTreeWidgetItem * timeItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Time Calculators")) );
	timeItem->setIcon(0,timeIcon);

	QTreeWidgetItem * jdItem = new QTreeWidgetItem(timeItem,QStringList(i18n("Julian Day")) );
	jdItem->setIcon(0,jdIcon);

	new QTreeWidgetItem(timeItem,QStringList(i18n("Sidereal Time")) );
	new QTreeWidgetItem(timeItem,QStringList(i18n("Almanac")) );
	new QTreeWidgetItem(timeItem,QStringList(i18n("Equinoxes & Solstices")) );
//	dayItem->setIcon(0,sunsetIcon);

	QTreeWidgetItem * coordItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Coordinate Converters")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Equatorial/Galactic")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Apparent Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Horizontal Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Ecliptic Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Angular Distance")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("Geodetic Coordinates")) );
	new QTreeWidgetItem(coordItem,QStringList(i18n("LSR Velocity")) );

	QTreeWidgetItem * solarItem = new QTreeWidgetItem(navigationPanel,QStringList(i18n("Solar System")) );
	solarItem->setIcon(0,solarIcon);
	new QTreeWidgetItem(solarItem,QStringList(i18n("Planets Coordinates")) );

	//FIXME: Would be better to make the navigationPanel fit its contents, 
	//but I wasn't able to make it work
	navigationPanel->setMinimumWidth( 200 );

	//Populate widget stack
	acStack = new QStackedWidget( split );

	QString message = i18n("<QT>"
			"<H2>KStars Astrocalculator</H2>"
			"<P>"
			"The KStars Astrocalculator contains several <B>modules</b> "
			"which perform a variety of astronomy-related calculations.  "
			"The modules are organized into several categories: "
			"<UL>"
			"<LI><B>Time calculators: </B>"
			"Convert between time systems, and predict the timing of celestial events</LI>"
			"<LI><B>Coordinate converters: </B>"
			"Convert between various coordinate systems</LI>"
			"<LI><B>Solar system: </B>"
			"Predict the position of any planet, from a given location on Earth at a given time</LI>"
			"</UL>"
			"</QT>"
	);

	splashScreen = new QTextEdit( message, acStack );
	acStack->addWidget( splashScreen );

	JDFrame = new modCalcJD( acStack );
	acStack->addWidget( JDFrame );
	GeodCoordFrame = new modCalcGeodCoord( acStack );
	acStack->addWidget( GeodCoordFrame );
	GalFrame = new modCalcGalCoord( acStack );
	acStack->addWidget( GalFrame );

	SidFrame = new modCalcSidTime( acStack );
	acStack->addWidget( SidFrame );
	AppFrame = new modCalcApCoord( acStack );
	acStack->addWidget( AppFrame );
	DayFrame = new modCalcDayLength( acStack );
	acStack->addWidget( DayFrame );

	AltAzFrame = new modCalcAltAz( acStack );
	acStack->addWidget( AltAzFrame );
	PlanetsFrame = new modCalcPlanets( acStack );
	acStack->addWidget( PlanetsFrame );
	EquinoxFrame = new modCalcEquinox( acStack );
	acStack->addWidget( EquinoxFrame );
	EclFrame = new modCalcEclCoords( acStack );
	acStack->addWidget( EclFrame );
	AngDistFrame = new modCalcAngDist( acStack );
	acStack->addWidget( AngDistFrame );
	VlsrFrame = new modCalcVlsr( acStack );
	acStack->addWidget( VlsrFrame );

	acStack->setCurrentWidget( splashScreen );

 	connect(navigationPanel, SIGNAL(itemClicked(QTreeWidgetItem *, int)), this,
		SLOT(slotItemSelection(QTreeWidgetItem *)));
}

AstroCalc::~AstroCalc()
{
}

void AstroCalc::slotItemSelection(QTreeWidgetItem *item)
{
	if (item==0L) return;

	//DEBUG
	kDebug() << "Item clicked: " << item->text(0);

	QString s = item->text(0);

	if(!(s.compare(i18n("Time Calculators"))))
		genTimeText();
	if(!(s.compare(i18n("Coordinate Converters"))))
		genCoordText();
	if(!(s.compare(i18n("Solar System"))))
		genSolarText();

	//DEBUG
	kDebug() << "s = " << s;
	kDebug() << acStack << " : " << AltAzFrame;

	if(!(s.compare(i18n("Sidereal Time"))))
		acStack->setCurrentWidget( SidFrame );
	if(!(s.compare(i18n("Julian Day"))))
		acStack->setCurrentWidget( JDFrame );
	if(!(s.compare(i18n("Equatorial/Galactic"))))
		acStack->setCurrentWidget( GalFrame );
	if(!(s.compare(i18n("Apparent Coordinates"))))
		acStack->setCurrentWidget( AppFrame );
	if(!(s.compare(i18n("Horizontal Coordinates"))))
		acStack->setCurrentWidget( AltAzFrame );
	if(!(s.compare(i18n("Ecliptic Coordinates"))))
		acStack->setCurrentWidget( EclFrame );
	if(!(s.compare(i18n("Geodetic Coordinates"))))
		acStack->setCurrentWidget( GeodCoordFrame );
	if(!(s.compare(i18n("Almanac"))))
		acStack->setCurrentWidget( DayFrame );
	if(!(s.compare(i18n("Equinoxes & Solstices"))))
		acStack->setCurrentWidget( EquinoxFrame );
	if(!(s.compare(i18n("Planets Coordinates"))))
		acStack->setCurrentWidget( PlanetsFrame );
	if(!(s.compare(i18n("Angular Distance"))))
		acStack->setCurrentWidget( AngDistFrame );
	if(!(s.compare(i18n("LSR Velocity"))))
		acStack->setCurrentWidget( VlsrFrame );
}

void AstroCalc::genTimeText(void)
{
	splashScreen->setHtml(i18n("<QT>"
			"Section which includes algorithms for computing time ephemeris"
			"<UL><LI>"
			"<B>Julian Day:</B> Julian Day/Calendar conversion"
			"</LI><LI>"
			"<B>Sidereal Time:</B> Sidereal/Universal time conversion"
			"</LI><LI>"
			"<B>Almanac:</B> Rise/Set/Transit timing and position data "
			"for the Sun and Moon"
			"</LI><LI>"
			"<B>Equinoxes & Solstices:</B> Equinoxes, Solstices and duration of the "
			"seasons"
			"</LI></UL>"
			"</QT>"));
	acStack->setCurrentWidget( splashScreen );
}

void AstroCalc::genCoordText(void)
{
	splashScreen->setHtml(i18n("<QT>"
			"Section with algorithms for the conversion of "
			"different astronomical systems of coordinates"
			"<UL><LI>"
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
			"<B>Geodetic Coords:</B> Geodetic/XYZ coordinate conversion"
			"</LI><LI>"
			"<B>LSR Velocity:</B> Computation of the heliocentric, geocentric "
			"and topocentric radial velocity of a source from its LSR velocity"
			"</LI></UL>"
			"</QT>"));
	acStack->setCurrentWidget( splashScreen );
}

void AstroCalc::genSolarText(void)
{
	splashScreen->setHtml(i18n("<QT>"
			"Section with algorithms regarding information "
			"on solar system bodies coordinates and times"
			"<UL><LI>"
			"<B>Planets Coords:</B> Coordinates for the planets, moon and sun "
			" at a given time and from a given position on Earth "
			"</LI></UL>"
			"</QT>"));
	acStack->setCurrentWidget( splashScreen );
}

QSize AstroCalc::sizeHint() const
{
  return QSize(640,430);
}

#include "astrocalc.moc"
