/***************************************************************************
                          modcalcapcoord.h  -  description
                             -------------------
    begin                : Sun May 30 2004
    copyright            : (C) 2004 by Pablo de Vicente
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

#ifndef MODCALCANGDIST_H_
#define MODCALCANGDIST_H_

#include "ui_modcalcangdist.h"

class dms;
class dmsBox;
class SkyPoint;
class QTextStream;

/** Module to compute the angular distance between two points in the sky
  *@author Pablo de Vicente
  *@version 0.9
  */
class modCalcAngDist : public QFrame, public Ui::modCalcAngDistDlg  {

    Q_OBJECT
public:
    /**Constructor. */
    explicit modCalcAngDist(QWidget *p);
    /**Destructor. */
    ~modCalcAngDist();

public slots:
    void slotValidatePositions();
    void slotObjectButton();
    void slotResetTitle();
    void slotRunBatch();

private:
    /** Process Lines **/
    void processLines( QTextStream &istream );

    /**@returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
    SkyPoint getCoords(dmsBox * rBox, dmsBox* dBox, bool *ok);

};

#endif
