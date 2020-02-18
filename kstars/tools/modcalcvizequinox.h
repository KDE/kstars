/***************************************************************************
                          modcalcvizequinox.h  -  description
                             -------------------
    begin                : Thu 22 Feb 2007
    copyright            : (C) 2007 by Jason Harris
    email                : kstars@30doradus.org
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
