/***************************************************************************
                          astrocalc.h  -  description
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

#ifndef ASTROCALC_H_
#define ASTROCALC_H_

#include <QTreeWidgetItem>
#include <QMap>
#include <kdialog.h>

#include "dms.h"

class QSplitter;
class QString;
class QStackedWidget;
class KTextEdit;
class modCalcJD;
class modCalcGeodCoord;
class modCalcGalCoord;
class modCalcSidTime;
class modCalcApCoord;
class modCalcDayLength;
class modCalcAltAz;
class modCalcEquinox;
class modCalcPlanets;
class modCalcEclCoords;
class modCalcAngDist;
class modCalcVlsr;
class ConjunctionsTool;

/** Astrocalc is the base class for the KStars astronomical calculator
 * @author: Pablo de Vicente
 * @version 0.9
 */

class AstroCalc : public KDialog
{

    Q_OBJECT
public:
    /** construtor */
    AstroCalc(QWidget *parent = 0);

    /** destructor */
    ~AstroCalc();

    /**Generate explanatory text for time modules. */
    void genTimeText(void);

    /**Generate explanatory text for coordinate modules. */
    void genCoordText(void);

    /**Generate explanatory text for geodetic modules. */
    void genGeodText(void);

    /**Generate explanatory text for solar system modules. */
    void genSolarText(void);

    /**@returns suggested size of calculator window. */
    QSize sizeHint() const;

public slots:
    /** Determine which item is selected in the navigation panel
      * Generate the corresponding calculator module.
      */
    void slotItemSelection(QTreeWidgetItem *it);

private:
    /** Create widget of type T and put it to widget stack. Widget must
     *  have construtor of type T(QWidget*) */
    template<typename T>
    inline T* addToStack();
    
    /** Add top level item to navigation panel.
        title - name of item
        html  - string to be displayed in splash screen
     */
    QTreeWidgetItem* addTreeTopItem(QTreeWidget* parent, QString title, QString html);

    /** Add item to navigation panel.
        title - name of item
        widget - widget to be selected on click
     */
    QTreeWidgetItem* addTreeItem(QTreeWidgetItem* parent, QString title, QWidget* widget);

    /** Lookup table for help texts */
    QMap<QString, QString>  htmlTable;
    /** Lookup table for widgets */
    QMap<QString, QWidget*> dispatchTable;
    QSplitter *split;
    QTreeWidget *navigationPanel;
    QString previousElection;

    enum typeOfPanel {GenText, TimeText, GeoText, SolarText, CoordText, JD, SidTime, DayLength, Equinox, GeoCoord, Galactic, Apparent, AltAz, Planets, Ecliptic, AngDist, Vlsr};
    typeOfPanel rightPanel;

    QStringList ItemTitles;
    QStackedWidget *acStack;
    KTextEdit *splashScreen;
    modCalcJD *JDFrame;
    modCalcGeodCoord *GeodCoordFrame;
    modCalcGalCoord *GalFrame;
    modCalcSidTime *SidFrame;
    modCalcApCoord *AppFrame;
    modCalcDayLength *DayFrame;
    modCalcAltAz *AltAzFrame;
    modCalcPlanets *PlanetsFrame;
    modCalcEquinox *EquinoxFrame;
    modCalcEclCoords *EclFrame;
    modCalcAngDist *AngDistFrame;
    modCalcVlsr *VlsrFrame;
    ConjunctionsTool *ConjunctFrame;
};

#endif
