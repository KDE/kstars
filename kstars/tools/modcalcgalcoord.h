/***************************************************************************
                          modcalcgal.h  -  description
                             -------------------
    begin                : Thu Jan 17 2002
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

#ifndef MODCALCGALCOORD_H_
#define MODCALCGALCOORD_H_

#include "ui_modcalcgalcoord.h"

/**
  * Class which implements the KStars calculator module to compute
  * Galactic coordinates to/from Equatorial coordinates.
  *
  *@author Pablo de Vicente
	*@version 0.9
  */
class modCalcGalCoord : public QFrame, public Ui::modCalcGalCoordDlg  {

    Q_OBJECT

public:

    modCalcGalCoord(QWidget *p);
    ~modCalcGalCoord();

public slots:

    void slotComputeCoords ();
    void slotObject();

    void slotGalLatCheckedBatch();
    void slotGalLongCheckedBatch();
    void slotRaCheckedBatch();
    void slotDecCheckedBatch();
    void slotEpochCheckedBatch();
    void slotRunBatch();

private:
    void equCheck();
    void galCheck();
    void processLines( QTextStream &is );

    dms galLong, galLat, raCoord, decCoord;
    QString epoch;
    bool galInputCoords;
};
#endif

