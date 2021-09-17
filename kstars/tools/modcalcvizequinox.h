/*
    SPDX-FileCopyrightText: 2007 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "kstarsdatetime.h"
#include "ui_modcalcvizequinox.h"

class QTextStream;

class KPlotObject;

/**
 * @class modCalcEquinox
 *
 * @author Jason Harris
 */
class modCalcEquinox : public QFrame, public Ui::modCalcEquinox
{
    Q_OBJECT

  public:
    explicit modCalcEquinox(QWidget *p);
    virtual ~modCalcEquinox() override = default;

    double dmonth(int imonth);

  private slots:
    void slotCompute();
    void slotCheckFiles();
    void slotRunBatch();
    void slotViewBatch();

  private:
    void processLines(QTextStream &istream);
    void addDateAxes();
    void findSolsticeAndEquinox(uint32_t year);
    qreal FindCorrection(uint32_t year);

  public:
    KStarsDateTime dSpring, dSummer, dAutumn, dWinter;
  private:
    double DMonth[12];
};
