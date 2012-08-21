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

#include "skyobjlistmodel.h"
#include "kstarsdata.h"
#include "obsconditions.h"

class ModelManager
{
public:
    enum ModelType {Planet_Model, Star_Model, Constellation_Model, Galaxy_Model, Cluster_Model, Nebula_Model};
    ModelManager(ObsConditions *obs);
    ~ModelManager();
    void updateModels(ObsConditions *obs);
    void resetModels();

    SkyObjListModel *returnModel (int type);

private:
    ObsConditions *obsconditions;
    SkyObjListModel *planetsModel, *starsModel, *galModel, *conModel, *clustModel, *nebModel;
    QStringList baseCatList, planetaryList, deepSkyList;
    QHash< ModelType, QList <SkyObject *> > initobjects;
};
