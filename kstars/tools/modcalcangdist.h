/*
    SPDX-FileCopyrightText: 2004 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcangdist.h"

class dms;
class dmsBox;
class SkyPoint;
class QTextStream;

/**
 * Module to compute the angular distance between two points in the sky and position angle.
 *
 * @author Pablo de Vicente
 * @author Jasem Mutlaq
 * @version 1.0
 */
class modCalcAngDist : public QFrame, public Ui::modCalcAngDistDlg
{
    Q_OBJECT
  public:
    /**Constructor. */
    explicit modCalcAngDist(QWidget *p);

    virtual ~modCalcAngDist() override = default;

  public slots:
    void slotValidatePositions();
    void slotObjectButton();
    void slotResetTitle();
    void slotRunBatch();

  private:
    /** Process Lines **/
    void processLines(QTextStream &istream);

    /** @returns a SkyPoint constructed from the coordinates in the RA and Dec dmsBoxes. */
    SkyPoint getCoords(dmsBox *rBox, dmsBox *dBox, bool *ok);
};
