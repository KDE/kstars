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

#include <kapplication.h>
#include <qwidget.h>
#include <kdialogbase.h>

#include "dms.h"
#include "geolocation.h"

class QSplitter;
class QListView;
class QTextView;
class QListViewItem;
class QVBox;
class QString;
class modCalcJD;
class modCalcGeodCoord;
class modCalcGalCoord;
class modCalcSidTime;
class modCalcPrec;

/** Astrocalc is the base class for the KStars astronomical calculator
 * @author: Pablo de Vicente
 */

class AstroCalc : public KDialogBase
{

Q_OBJECT 
	public:
    /** construtor */
	AstroCalc(QWidget *parent = 0);

    /** destructor */
	~AstroCalc();

		void genTimeText(void);
		void genCoordText(void);
		void genGeodText(void);
		void genJdFrame(void);
		void genSidFrame(void);
		void genGeodCoordFrame(void);
		void genGalFrame(void);
		void genPrecFrame(void);
		void delRightPanel(void);
				
	public slots:
		void slotItemSelection(QListViewItem *it);
		
	private:
		
		QSplitter *split;
		QListView *navigationPanel, *auxiliar;
		QTextView *splashScreen;
//		QListViewItem *timeItem, *coordItem, *jdItem, *stItem, *dayItem;
		QVBox *vbox, *rightBox;
		QString previousElection;

		enum typeOfPanel {GenText, TimeText, GeoText, CoordText, JD, SidTime, GeoCoord, Galactic, Precessor};
		typeOfPanel rightPanel;

		modCalcJD *JDFrame;
		modCalcGeodCoord *GeodCoordFrame;
		modCalcGalCoord *GalFrame;
		modCalcSidTime *SidFrame;
		modCalcPrec *PrecFrame;
};

#endif
