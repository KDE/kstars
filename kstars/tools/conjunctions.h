/***************************************************************************
                          conjunctions.h  -  Conjunctions Tool
                             -------------------
    begin                : Sun 20th Apr 2008
    copyright            : (C) 2008 Akarsh Simha
    email                : akarshsimha@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 * Much of the code here is taken from Pablo de Vicente's                  *
 * modcalcplanets.h                                                        *
 *                                                                         *
 ***************************************************************************/

#ifndef CONJUNCTIONS_H_
#define CONJUNCTIONS_H_

#include <QTextStream>

#include "ui_conjunctions.h"

class GeoLocation;
class KSPlanetBase;
class dms;

/**
  *@short Predicts conjunctions using KSConjunct in the background
  */

class ConjunctionsTool : public QFrame, public Ui::ConjunctionsDlg {

    Q_OBJECT

public:
    ConjunctionsTool(QWidget *p);
    ~ConjunctionsTool();

public slots:

    void slotLocation();
    void slotCompute();
    void showProgress(int);

private:
    void showConjunctions(QMap<long double, dms> conjunctionlist);

    GeoLocation *geoPlace;
};

#endif
