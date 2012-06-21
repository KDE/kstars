/***************************************************************************
                          wiview.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2012/26/05
    copyright            : (C) 2012 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "QtDeclarative/QDeclarativeView"
#include "QModelIndex"
#include "skyobject.h"
#include "modelmanager.h"
#include "skyobjlistmodel.h"

class WIView : public QObject
{
    Q_OBJECT
public:
    enum TYPE {PLANET, SATELLITE, STAR, GALAXY, CONSTELLATION, STAR_CLUSTER, NEBULA};
    enum DetailViewType {PLANETARY};

    WIView(QObject *parent = 0);
    ~WIView();
    void manageViews(int TYPE);    //display view
    void loadDetailsView( int detailsViewType, SkyObject *so);

public slots:
//     void loadModel ( int TYPE );
//     void skyObjectItemClicked(QModelIndex index);
    void onCatListItemClicked(QString);
    void onSoListItemClicked(QString name, QString type);
private:
    QObject *baseObj, *catListObj, *soListObj;
    QDeclarativeContext *ctxt;
    QDeclarativeView *baseListView;
    QDeclarativeView *planetaryListView;
    QDeclarativeView *deepSkyListView;
    QDeclarativeView *skyObjListView;

    QDeclarativeView *planetDetailsView;
    QDeclarativeView *satelliteDetailsView;
    QDeclarativeView *deepSkyDetailsView;

    ModelManager *m;
};