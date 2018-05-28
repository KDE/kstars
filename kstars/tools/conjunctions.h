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

#pragma once

#include "dms.h"
#include "ui_conjunctions.h"

#include <QFrame>
#include <QMap>
#include <QString>

#include <memory>

class QSortFilterProxyModel;
class QStandardItemModel;

class GeoLocation;
class KSPlanetBase;
class SkyObject;

/**
  * @short Predicts conjunctions using KSConjunct in the background
  */
class ConjunctionsTool : public QFrame, public Ui::ConjunctionsDlg
{
    Q_OBJECT

  public:
    explicit ConjunctionsTool(QWidget *p);
    virtual ~ConjunctionsTool() override = default;

  public slots:

    void slotLocation();
    void slotCompute();
    void showProgress(int);
    void slotFindObject();
    void slotGoto();
    void slotFilterType(int);
    void slotClear();
    void slotExport();
    void slotFilterReg(const QString &);

  private:
    void showConjunctions(const QMap<long double, dms> &conjunctionlist, const QString &object1,
                          const QString &object2);

    SkyObject* Object1 = nullptr;
    std::unique_ptr<KSPlanetBase> Object2; // Second object is always a planet.
    /// To store the names of Planets vs. values expected by KSPlanetBase::createPlanet()
    QHash<int, QString> pNames;
    /// To store Julian Days corresponding to the row index in the output list widget
    QMap<int, long double> outputJDList;
    GeoLocation *geoPlace { nullptr };
    QStandardItemModel *m_Model { nullptr };
    QSortFilterProxyModel *m_SortModel { nullptr };
    int m_index { 0 };
};
