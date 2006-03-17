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

#ifndef ASTROCALC_H
#define ASTROCALC_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <QTreeWidgetItem>
#include <kdialog.h>
#include <kapplication.h>

#include "dms.h"

class QSplitter;
class QString;
class QStackedWidget;
class QTextEdit;
class modCalcJD;
class modCalcGeodCoord;
class modCalcGalCoord;
class modCalcSidTime;
class modCalcPrec;
class modCalcApCoord;
class modCalcDayLength;
class modCalcAzel;
class modCalcEquinox;
class modCalcPlanets;
class modCalcEclCoords;
class modCalcAngDist;
class modCalcVlsr;

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
		/**Determine which item is selected in the function menu QListBox.
			*Generate the corresponding calculator module.
			*/
		void slotItemSelection(QTreeWidgetItem *it);
		
	private:
		
		QSplitter *split;
		QTreeWidget *navigationPanel;
		QString previousElection;

		enum typeOfPanel {GenText, TimeText, GeoText, SolarText, CoordText, JD, SidTime, DayLength, Equinox, GeoCoord, Galactic, Precessor, Apparent, Azel, Planets, Ecliptic, AngDist, Vlsr};
		typeOfPanel rightPanel;

		QStringList ItemTitles;
		QStackedWidget *acStack;
		QTextEdit *splashScreen;
		modCalcJD *JDFrame;
		modCalcGeodCoord *GeodCoordFrame;
		modCalcGalCoord *GalFrame;
		modCalcSidTime *SidFrame;
		modCalcPrec *PrecFrame;
		modCalcApCoord *AppFrame;
		modCalcDayLength *DayFrame;
		modCalcAzel *AzelFrame;
		modCalcPlanets *PlanetsFrame;
		modCalcEquinox *EquinoxFrame;
		modCalcEclCoords *EclFrame;
		modCalcAngDist *AngDistFrame;
		modCalcVlsr *VlsrFrame;
};

#endif
