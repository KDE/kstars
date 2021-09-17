/*
    SPDX-FileCopyrightText: 2004-2005 Pablo de Vicente <pvicentea@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcplanets.h"

class QTextStream;

class GeoLocation;
class KSPlanetBase;

/**
  *@author Pablo de Vicente
  */
class modCalcPlanets : public QFrame, public Ui::modCalcPlanetsDlg
{
    Q_OBJECT

  public:
    explicit modCalcPlanets(QWidget *p);
    virtual ~modCalcPlanets() override = default;

  public slots:

    void slotLocation();
    void slotComputePosition();
    void slotUtCheckedBatch();
    void slotDateCheckedBatch();
    void slotLongCheckedBatch();
    void slotLatCheckedBatch();
    void slotPlanetsCheckedBatch();
    void slotRunBatch();
    void processLines(QTextStream &istream);
    //void slotInputFile();
    //void slotOutputFile();
    //void slotRunBatch();

  private:
    void showCoordinates(const KSPlanetBase &ksp);
    void showHeliocentricEclipticCoords(const dms &hLong, const dms &hLat, double dist);
    void showGeocentricEclipticCoords(const dms &eLong, const dms &eLat, double r);
    void showEquatorialCoords(const dms &ra, const dms &dec);
    void showTopocentricCoords(const dms &az, const dms &el);
    unsigned int requiredBatchFields();

    // void processLines( QTextStream &istream );

    GeoLocation *geoPlace;
};
