/*
    SPDX-FileCopyrightText: 2008 Akarsh Simha <akarshsimha@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later


    Much of the code here is taken from Pablo de Vicente's
    modcalcplanets.h

*/

#pragma once

#include "dms.h"
#include "ui_conjunctions.h"

#include <QFrame>
#include <QMap>
#include <QString>
#include "skycomponents/typedef.h"
#include <memory>

class QSortFilterProxyModel;
class QStandardItemModel;

class GeoLocation;
class KSPlanetBase;
class SkyObject;

//FIXME: URGENT! There's a bug when setting max sep to 0!

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
    void setMode(int);
    void slotGoto();
    void slotFilterType(int);
    void slotClear();
    void slotExport();
    void slotFilterReg(const QString &);

  private:
    void showConjunctions(const QMap<long double, dms> &conjunctionlist, const QString &object1,
                          const QString &object2);

    /**
     * @brief setUpConjunctionOpposition
     * @short set up ui for conj./opp.
     */
    void setUpConjunctionOpposition();

    /**
     * @brief mode
     * @short Represents whether the tool looks for conj/opp.
     */
    enum MODE {
        CONJUNCTION,
        OPPOSITION
    } mode;

    SkyObject_s Object1;
    KSPlanetBase_s Object2; // Second object is always a planet.
    /// To store the names of Planets vs. values expected by KSPlanetBase::createPlanet()
    QHash<int, QString> pNames;

    /// To store Julian Days corresponding to the row index in the output list widget
    QMap<int, long double> outputJDList;
    GeoLocation *geoPlace { nullptr };
    QStandardItemModel *m_Model { nullptr };
    QSortFilterProxyModel *m_SortModel { nullptr };
    int m_index { 0 };
};
