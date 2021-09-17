/*
    SPDX-FileCopyrightText: 2002 Pablo de Vicente <vicente@oan.es>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ui_modcalcjd.h"

class QTextStream;
class QWidget;

/**
  * Class for KStars module which computes JD, MJD and Date/Time from the
  * any of the other entries.
  *
  * Inherits QVBox
  *
  * @author Pablo de Vicente
  * @version 0.9
  */
class modCalcJD : public QFrame, public Ui::modCalcJdDlg
{
    Q_OBJECT
  public:
    explicit modCalcJD(QWidget *p);
    virtual ~modCalcJD() override = default;

  public slots:
    void slotUpdateCalendar();
    void slotUpdateModJD();
    void slotUpdateJD();
    void showCurrentTime(void);
    void slotRunBatch();
    void slotViewBatch();
    void slotCheckFiles();

  private:
    void processLines(QTextStream &istream, int inputData);
    /** Shows Julian Day in the Box */
    void showJd(long double jd);
    /** Shows the modified Julian Day in the Box */
    void showMjd(long double mjd);
};
