/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <pvicentea@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"
#include "ui_modcalcaltaz.h"

class QDateTime;

class GeoLocation;

/**
  *@author Pablo de Vicente
  */
class modCalcAltAz : public QFrame, public Ui::modCalcAltAz
{
    Q_OBJECT

  public:
    explicit modCalcAltAz(QWidget *p);
    virtual ~modCalcAltAz() override = default;

  public slots:
    void slotCompute();
    void slotNow();
    void slotLocation();
    void slotObject();
    void slotDateTimeChanged(const QDateTime &);

  private:
    GeoLocation *geoPlace;
    dms LST;
};
