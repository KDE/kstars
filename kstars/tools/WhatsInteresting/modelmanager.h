/***************************************************************************
                          modelmanager.h  -  K Desktop Planetarium
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

//#include <QStandardItemModel>
#include "skyobjlistmodel.h"
#include "kstarsdata.h"

class ModelManager
{
public:
    enum LIST_TYPE {BaseList, PlanetaryObjects, DeepSkyObjects, Planets, Satellites,
                    Stars, Galaxies, Constellations, Star_Clusters, Nebulae};
    ModelManager();
    ~ModelManager();
    void updateModels();

    //void setModelToListView( QStandardItemModel* myModel );
    //QStandardItemModel* returnModel ( int TYPE );

    SkyObjListModel* returnModel ( LIST_TYPE Type );
    QStringList returnCatListModel ( LIST_TYPE Type );
    bool isVisible(GeoLocation* geo, dms* lst, SkyObject* so);

private:
    SkyObjListModel *planetsModel, *starsModel, *galModel, *conModel, *starClustModel, *nebModel;
    QStringList baseCatList, planetaryList, deepSkyList;
    QHash < SkyObject::TYPE , QList < SkyObject * > > initobjects;
};