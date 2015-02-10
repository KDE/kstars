/***************************************************************************
                          modcalcLSRVel.h  -  description
                             -------------------
    begin                : dom mar 13 2005
    copyright            : (C) 2005 by Pablo de Vicente
    email                : pvicentea@wanadoo.es
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef MODCALCVLSR_H_
#define MODCALCVLSR_H_

#include <QTextStream>

#include "ui_modcalcvlsr.h"

class QWidget;
class SkyPoint;
class GeoLocation;
class dms;

/**
  *@author Pablo de Vicente
  *Module to compute the heliocentric radial velocity, geocentric radial velocity and 
  *topocentric radial velocity for a source, given its coordinates, its Vlsr and the date and
  *location on the Earth.
  */
class modCalcVlsr : public QFrame, public Ui::modCalcVlsrDlg  {

    Q_OBJECT

public:
    explicit modCalcVlsr(QWidget *p);
    ~modCalcVlsr();

private slots:
    void slotNow();
    void slotLocation();
    void slotFindObject();
    void slotCompute();

    void slotUtChecked();
    void slotDateChecked();
    void slotRaChecked();
    void slotDecChecked();
    void slotEpochChecked();
    void slotLongChecked();
    void slotLatChecked();
    void slotHeightChecked();
    void slotVlsrChecked();
    void slotInputFile();
    void slotOutputFile();
    void slotRunBatch();

private:

    /** @returns a SkyPoint constructed from the coordinates in the
     * RA and Dec dmsBoxes. */
    SkyPoint skypoint();

    /* Constructs the a GeoLocation object (geoPlace) from the calling classes.
     * This is for using as Geolocation the location setup in KStars 
     * */
    void initGeo(void);

    /* Method to process the lines from a file
     */
    void processLines( QTextStream &istream );

    GeoLocation *geoPlace;
    int velocityFlag;
};



#endif
