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

#ifndef MODCALCVIZEQUINOX_H_
#define MODCALCVIZEQUINOX_H_

#include "ui_modcalcvizequinox.h"
#include "kstarsdatetime.h"

class QTextStream;

/**
  *@class modCalcEquinox
  *@author Jason Harris
  */
class modCalcEquinox : public QFrame, public Ui::modCalcEquinox  {

    Q_OBJECT

public:
    modCalcEquinox(QWidget *p);
    ~modCalcEquinox();

    KStarsDateTime dSpring, dSummer, dAutumn, dWinter;
    double dmonth(int imonth);

private slots:
    void slotCompute();
    void slotCheckFiles();
    void slotRunBatch();
    void slotViewBatch();

private:
    void processLines( QTextStream &istream );
    void addDateAxes();
    KStarsDateTime findEquinox( int year, bool Spring, KPlotObject *po );
    KStarsDateTime findSolstice( int year, bool Summer );
    double DMonth[12];
};

#endif
