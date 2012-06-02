/***************************************************************************
                          modelmanager.cpp  -  K Desktop Planetarium
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

#include "modelmanager.h"
//#include "skyobjectitem.h"
#include "kstarsdata.h"
#include "skymapcomposite.h"
#include "skyobject.h"

ModelManager::ModelManager()
{
    planetsModel = new SkyObjListModel();
    starsModel = new SkyObjListModel();
    galModel = new SkyObjListModel();
    conModel = new SkyObjListModel();
    starClustModel = new SkyObjListModel();
    nebModel = new SkyObjListModel();
    updateModels();
}

void ModelManager::updateModels()
{
    KStarsData *data = KStarsData::Instance();

    baseCatList<<"Planetary Objects"<<"Deep-sky Objects" ;
    planetaryList<<"Planets"<<"Satellites";
    deepSkyList<<"Stars"<<"Galaxies"<<"Constellations"<<"Star Clusters"<<"Nebulae";

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANET ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 7)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            planetsModel->addSkyObject(o);
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::STAR ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 2)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            starsModel->addSkyObject(o);
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::GALAXY ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 2)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            galModel->addSkyObject(o);
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::CONSTELLATION ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 2)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            conModel->addSkyObject(o);
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::OPEN_CLUSTER ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 2)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            starClustModel->addSkyObject(o);
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANETARY_NEBULA ) ) 
    {
        SkyObject *o = data->skyComposite()->findByName( name );
        kDebug()<<o->name()<<o->mag();
        if (o->mag() < 2)
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            nebModel->addSkyObject(o);
        }
    }



}

SkyObjListModel* ModelManager::returnModel(ModelManager::LIST_TYPE Type)
{
    if (Type == Planets)
        return planetsModel;
    else if (Type == Stars)
        return starsModel;
    else if (Type == Galaxies)
        return galModel;
    else if (Type == Constellations)
        return conModel;
    else if (Type == Star_Clusters)
        return starClustModel;
    else if (Type == Nebulae)
        return nebModel;

    return (new SkyObjListModel());
}

QStringList ModelManager::returnCatListModel(ModelManager::LIST_TYPE Type)
{
    if (Type == BaseList)
        return baseCatList;
    else if ( Type == PlanetaryObjects )
        return planetaryList;
    else
        return deepSkyList;
}
