/*
    SPDX-FileCopyrightText: 2005 Pablo de Vicente <pvicentea@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcvlsr.h"

#include <QTextStream>

class QWidget;
class SkyPoint;
class GeoLocation;
class dms;

/**
 * Module to compute the heliocentric radial velocity, geocentric radial velocity and
 * topocentric radial velocity for a source, given its coordinates, its Vlsr and the date and
 * location on the Earth.
 *
 * @author Pablo de Vicente
 */
class modCalcVlsr : public QFrame, public Ui::modCalcVlsrDlg
{
    Q_OBJECT

  public:
    explicit modCalcVlsr(QWidget *p);
    virtual ~modCalcVlsr() override = default;

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
    /**
     * @returns a SkyPoint constructed from the coordinates in the
     * RA and Dec dmsBoxes. */
    SkyPoint skypoint();

    /**
     * Constructs the a GeoLocation object (geoPlace) from the calling classes.
     * This is for using as Geolocation the location setup in KStars
     */
    void initGeo(void);

    /** Method to process the lines from a file */
    void processLines(QTextStream &istream);

    GeoLocation *geoPlace { nullptr };
    int velocityFlag { 0 };
};
