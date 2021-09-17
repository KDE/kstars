/*
    SPDX-FileCopyrightText: 2004 Pablo de Vicente <p.devicente@wanadoo.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

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
    virtual ~modCalcEclCoords() override = default;

  public slots:
    void slotNow(void);
    void slotObject(void);
    void slotDateTimeChanged(const QDateTime &edt);
    void slotCompute(void);

  private:
    KStarsDateTime kdt;
};
