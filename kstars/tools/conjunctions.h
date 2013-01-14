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
#include <QAbstractTableModel>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>

#include "ui_conjunctions.h"
#include "skyobjects/skyobject.h"
#include "skyobjects/ksplanetbase.h"

class GeoLocation;
class KSPlanetBase;
class dms;
class QListWidgetItem;

/**
  *@short Predicts conjunctions using KSConjunct in the background
  */

class ConjunctionsTool : public QFrame, public Ui::ConjunctionsDlg {

    Q_OBJECT

public:
    explicit ConjunctionsTool(QWidget *p);
    ~ConjunctionsTool();

public slots:

    void slotLocation();
    void slotCompute();
    void showProgress(int);
    void slotFindObject();
    void slotGoto();
    void slotFilterType( int );
    void slotClear();
    void slotExport();
    void slotFilterReg( const QString & );

private:
    SkyObject *Object1;
    KSPlanetBase *Object2;        // Second object is always a planet.

    QHash<int, QString> pNames;   // To store the names of Planets vs. values expected by KSPlanetBase::createPlanet()
    QMap<int, long double> outputJDList; // To store Julian Days corresponding to the row index in the output list widget

    void showConjunctions(const QMap<long double, dms> &conjunctionlist, QString object);

    GeoLocation *geoPlace;

    QStandardItemModel *m_Model;
    QSortFilterProxyModel *m_SortModel;

    int m_index;
};

#endif
