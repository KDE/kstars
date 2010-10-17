/***************************************************************************
                          modcalcazel.h  -  description
                             -------------------
    begin                : mier abr 20 2004
    copyright            : (C) 2004-2005 by Pablo de Vicente
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

#ifndef MODCALCPLANETS_H_
#define MODCALCPLANETS_H_

#include <QTextStream>

#include "ui_modcalcplanets.h"

class GeoLocation;
class KSPlanetBase;

/**
  *@author Pablo de Vicente
  */
class modCalcPlanets : public QFrame, public Ui::modCalcPlanetsDlg  {

    Q_OBJECT

public:
    modCalcPlanets(QWidget *p);
    ~modCalcPlanets();

public slots:

    void slotLocation();
    void slotComputePosition();
    void slotUtCheckedBatch();
    void slotDateCheckedBatch();
    void slotLongCheckedBatch();
    void slotLatCheckedBatch();
    void slotPlanetsCheckedBatch();
    void slotRunBatch();
    void processLines( QTextStream &istream );
    //void slotInputFile();
    //void slotOutputFile();
    //void slotRunBatch();

private:

    void showCoordinates( const KSPlanetBase &ksp );
    void showHeliocentricEclipticCoords( const dms& hLong, const dms& hLat, double dist);
    void showGeocentricEclipticCoords( const dms& eLong, const dms& eLat, double r);
    void showEquatorialCoords( const dms& ra, const dms& dec);
    void showTopocentricCoords( const dms& az, const dms& el);
    unsigned int requiredBatchFields();

    // void processLines( QTextStream &istream );

    GeoLocation *geoPlace;
};

#endif
