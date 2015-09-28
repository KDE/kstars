/*  Artificial Horizon Manager
    Copyright (C) 2015 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
*/

#ifndef HORIZONMANAGER_H
#define HORIZONMANAGER_H

#include <QAbstractTableModel>

#include <QDialog>

#include "ui_horizonmanager.h"

class QSortFilterProxyModel;
class QStandardItemModel;
class KStars;
class ArtificialHorizonComponent;
class SkyPoint;
class LineList;

class HorizonManagerUI : public QFrame, public Ui::HorizonManager
{
    Q_OBJECT

    public:
    /** @short Constructor
     */
        explicit HorizonManagerUI( QWidget *parent );
};


/**
 *@class HorizonManager
 *@short Manages adding/removing and editing regions and points associated with
 * user-customized artificial horizons.
 *
 *@version 1.0
 *@author Jasem Mutlaq
 */
class HorizonManager : public QDialog
{
    Q_OBJECT
public:
    /**
     *@short Constructor.
     */
    explicit HorizonManager( QWidget *ks );

    /**
     *@short Destructor.
     */
    ~HorizonManager();

    void clearFields ();
    void showRegion( const int regionID );

    bool validatePolygon(int regionID);

    void deleteRegion( int regionID );

public slots:
    /**
     *@short Add region
     */
    void slotAddRegion();

    /**
     *@short Delete region
     */
    void slotRemoveRegion();

    void addSkyPoint(SkyPoint *p);
    void slotAddPoint();
    void slotRemovePoint();

    void clearPoints();

    void setSelectPoints(bool);


private slots:
    void slotSaveChanges();
    void slotSetShownRegion( QModelIndex idx );

private:
    //void insertFlag( bool isNew, int row = 0 );

    HorizonManagerUI *ui;

    QStandardItemModel *m_RegionsModel;

    QSortFilterProxyModel *m_PointsSortModel;
    QSortFilterProxyModel *m_RegionsSortModel;

    ArtificialHorizonComponent *horizonComponent;

    QMap<QString, LineList*> regionMap;

    bool selectPoints;
};

#endif
