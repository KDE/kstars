/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"
#include "ui_modcalcgalcoord.h"

/**
 * Class which implements the KStars calculator module to compute
 * Galactic coordinates to/from Equatorial coordinates.
 *
 * @author Pablo de Vicente
 * @version 0.9
 */
class modCalcGalCoord : public QFrame, public Ui::modCalcGalCoordDlg
{
    Q_OBJECT

  public:
    explicit modCalcGalCoord(QWidget *p);
    virtual ~modCalcGalCoord() override = default;

  public slots:

    void slotComputeCoords();
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
    void processLines(QTextStream &is);

    dms galLong, galLat, raCoord, decCoord;
    QString epoch;
    bool galInputCoords { false };
};
