/***************************************************************************
                          modcalcapcoord.h  -  description
                             -------------------
    begin                : Wed Apr 10 2002
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

#ifndef MODCALCAPCOORD_H_
#define MODCALCAPCOORD_H_

#include "ui_modcalcapcoord.h"

class QTextStream;

/** Module to compute the equatorial coordinates for a given date and time
 * from a given epoch or equinox
  *@author Pablo de Vicente
	*@version 0.9
  */
class modCalcApCoord : public QFrame, public Ui::modCalcApCoordDlg  {

    Q_OBJECT
public:
    /**Constructor. */
    modCalcApCoord(QWidget *p);
    /**Destructor. */
    ~modCalcApCoord();

    /** Process Lines **/
    //	void processLines( const QFile * f );
    void processLines( QTextStream &istream );

private slots:
    void slotCompute();
    void slotObject();

    /**Fill the Time and Date fields with the current values from the CPU clock. */
    void showCurrentTime();

    void slotUtCheckedBatch();
    void slotDateCheckedBatch();
    void slotRaCheckedBatch();
    void slotDecCheckedBatch();
    void slotEpochCheckedBatch();
    void slotRunBatch();
};

#endif
