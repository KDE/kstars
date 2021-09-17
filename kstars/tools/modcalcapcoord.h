/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcapcoord.h"

class QTextStream;

/**
 * Compute the equatorial coordinates for a given date and time
 * from a given epoch or equinox
 *
 * @author Pablo de Vicente
 * @version 0.9
 */
class modCalcApCoord : public QFrame, public Ui::modCalcApCoordDlg
{
    Q_OBJECT
  public:
    /** Constructor. */
    explicit modCalcApCoord(QWidget *p);

    /** Process Lines **/
    //	void processLines( const QFile * f );
    void processLines(QTextStream &istream);

  private slots:
    void slotCompute();
    void slotObject();

    /** Fill the Time and Date fields with the current values from the CPU clock. */
    void showCurrentTime();

    void slotUtCheckedBatch();
    void slotDateCheckedBatch();
    void slotRaCheckedBatch();
    void slotDecCheckedBatch();
    void slotEpochCheckedBatch();
    void slotRunBatch();
};
