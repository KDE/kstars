/***************************************************************************
                          modcalceclipticcoords.h  -  description
                             -------------------
    begin                : Fri May 14 2004
    copyright            : (C) 2004 by Pablo de Vicente
    email                : p.devicente@wanadoo.es
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

#include "kstarsdatetime.h"
#include "ui_modcalceclipticcoords.h"

/**
  * Class which implements the KStars calculator module to compute
  * geocentric ecliptic coordinates to/from geocentric equatorial coordinates.
  *
  * Inherits QWidget
  *
  * @author Pablo de Vicente
  */
class modCalcEclCoords : public QFrame, public Ui::modCalcEclCoordsDlg
{
    Q_OBJECT

  public:
    explicit modCalcEclCoords(QWidget *p);
    ~modCalcEclCoords();

  public slots:
    void slotNow(void);
    void slotObject(void);
    void slotDateTimeChanged(const QDateTime &edt);
    void slotCompute(void);

  private:
    KStarsDateTime kdt;
};
