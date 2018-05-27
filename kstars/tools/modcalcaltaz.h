/***************************************************************************
                          modcalcaltaz.h  -  description
                             -------------------
    begin                : sáb oct 26 2002
    copyright            : (C) 2002 by Pablo de Vicente
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
