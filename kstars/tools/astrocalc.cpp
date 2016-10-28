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
#include <QTreeWidgetItem>
#include <QDialogButtonBox>
#include <QTextEdit>

#include <KLocalizedString>

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
#include "conjunctions.h"

AstroCalc::AstroCalc( QWidget* parent ) :
        QDialog( parent )
{
#ifdef Q_OS_OSX
        setWindowFlags(Qt::Tool| Qt::WindowStaysOnTopHint);
#endif

    // List of messages. Maybe there is better place for it...
    QString message =
        i18n("<QT>"
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
    QString messageTime =
        i18n("<QT>"
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
             "</QT>");
    QString messageCoord =
        i18n("<QT>"
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
             "</QT>");
    QString messageSolar =
        i18n("<QT>"
             "Section with algorithms regarding information "
             "on solar system bodies coordinates and times"
             "<UL><LI>"
             "<B>Planets Coords:</B> Coordinates for the planets, moon and sun "
             "at a given time and from a given position on Earth "
             "</LI></UL>"
             "</QT>");

    QSplitter* split = new QSplitter ( this );

    QVBoxLayout *mainLayout = new QVBoxLayout;
    mainLayout->addWidget(split);
    setLayout(mainLayout);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Close);
    mainLayout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(rejected()), this, SLOT(reject()));

    setWindowTitle( i18n("Calculator") );

    // Create navigation panel
    navigationPanel = new QTreeWidget(split);
    navigationPanel->setColumnCount(1);
    navigationPanel->setHeaderLabels( QStringList(i18n("Calculator modules")) );
    navigationPanel->setSortingEnabled( false );
    //FIXME: Would be better to make the navigationPanel fit its contents,
    //but I wasn't able to make it work
    navigationPanel->setMinimumWidth( 200 );

    acStack = new QStackedWidget( split );

    splashScreen = new QTextEdit( message, acStack );
    splashScreen->setReadOnly( true );
    //FIXME: Minimum size is set to resize calculator to correct size
    //when no modules is loaded. This is simply biggest size of
    //calculator modules. I think it should be set in more cleverly.
    splashScreen->setMinimumSize(640, 550);
    acStack->addWidget( splashScreen );

    // Load icons
    // JM 2016-10-02: Those are missing, disabling the icons for now
    /*
    QIcon jdIcon = QIcon ("jd.png");
    QIcon geodIcon = QIcon ("geodetic.png");
    QIcon solarIcon = QIcon ("geodetic.png");
    // QIcon sunsetIcon = QIcon ("sunset.png"); // Its usage is commented out.
    QIcon timeIcon = QIcon ("sunclock.png");*/
    
    /* Populate the tree widget and widget stack */
    // Time-related entries
    QTreeWidgetItem * timeItem = addTreeTopItem(navigationPanel, i18n("Time Calculators"), messageTime);
    //timeItem->setIcon(0,timeIcon);

    //addTreeItem<modCalcJD>       (timeItem, i18n("Julian Day"))->setIcon(0,jdIcon);
    addTreeItem<modCalcJD>       (timeItem, i18n("Julian Day"));
    addTreeItem<modCalcSidTime>  (timeItem, i18n("Sidereal Time"));
    addTreeItem<modCalcDayLength>(timeItem, i18n("Almanac"));
    addTreeItem<modCalcEquinox>  (timeItem, i18n("Equinoxes & Solstices"));
    //  dayItem->setIcon(0,sunsetIcon);
                                
    // Coordinate-related entries
    QTreeWidgetItem * coordItem = addTreeTopItem(navigationPanel, i18n("Coordinate Converters"), messageCoord);
    addTreeItem<modCalcGalCoord> (coordItem, i18n("Equatorial/Galactic"));
    addTreeItem<modCalcApCoord>  (coordItem, i18n("Apparent Coordinates"));
    addTreeItem<modCalcAltAz>    (coordItem, i18n("Horizontal Coordinates"));
    addTreeItem<modCalcEclCoords>(coordItem, i18n("Ecliptic Coordinates"));
    addTreeItem<modCalcAngDist>  (coordItem, i18n("Angular Distance"));
    addTreeItem<modCalcGeodCoord>(coordItem, i18n("Geodetic Coordinates"));
    addTreeItem<modCalcVlsr>     (coordItem, i18n("LSR Velocity"));

    // Solar System related entries
    QTreeWidgetItem * solarItem = addTreeTopItem(navigationPanel, i18n("Solar System"), messageSolar);
    //solarItem->setIcon(0,solarIcon);
    addTreeItem<modCalcPlanets>  (solarItem, i18n("Planets Coordinates"));
    addTreeItem<ConjunctionsTool>(solarItem, i18n("Conjunctions"));
    
    acStack->setCurrentWidget( splashScreen );
    connect(navigationPanel, SIGNAL(itemClicked(QTreeWidgetItem *, int)),
            this, SLOT(slotItemSelection(QTreeWidgetItem *)));
}

template<typename T>
QWidget* AstroCalc::addToStack()
{
    T* t = new T( acStack );
    acStack->addWidget(t);
    return t;
}

template<typename T>
QTreeWidgetItem* AstroCalc::addTreeItem(QTreeWidgetItem* parent, QString title)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList(title));
    dispatchTable.insert(item,
                         WidgetThunk(this, &AstroCalc::addToStack<T>));
    return item;
}

QTreeWidgetItem* AstroCalc::addTreeTopItem(QTreeWidget* parent, QString title, QString html)
{
    QTreeWidgetItem* item = new QTreeWidgetItem(parent, QStringList(title));
    htmlTable.insert(item, html);
    return item;
}

AstroCalc::~AstroCalc() {}

void AstroCalc::slotItemSelection(QTreeWidgetItem *item)
{
    if ( item == 0)
        return;
    // Lookup in HTML table
    QMap<QTreeWidgetItem*, QString>::iterator iterHTML = htmlTable.find(item);
    if( iterHTML != htmlTable.end() ) {
        splashScreen->setHtml(*iterHTML);
        acStack->setCurrentWidget(splashScreen);
        return;
    }
    // Lookup in frames table
    QMap<QTreeWidgetItem*, WidgetThunk>::iterator iter = dispatchTable.find(item);
    if( iter != dispatchTable.end() ) {
        acStack->setCurrentWidget( iter->eval() );
    }
}

QSize AstroCalc::sizeHint() const
{
    return QSize(640,430);
}

QWidget* AstroCalc::WidgetThunk::eval()
{
    if( widget == 0 ) {
        // This is pointer to member function call. 
        widget = (calc->*func)();
    }
    return widget;
}

