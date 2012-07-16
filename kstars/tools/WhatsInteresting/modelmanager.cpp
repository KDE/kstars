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

#include "ksfilereader.h"
#include "modelmanager.h"
#include "kstarsdatetime.h"
#include "skymapcomposite.h"
#include "skyobject.h"

ModelManager::ModelManager(ObsConditions *obs)
{
    obsconditions = obs;
    planetsModel = new SkyObjListModel();
    starsModel = new SkyObjListModel();
    galModel = new SkyObjListModel();
    conModel = new SkyObjListModel();
    starClustModel = new SkyObjListModel();
    nebModel = new SkyObjListModel();

    initobjects["Star"] = QList<SkyObject *>();
    initobjects["Galaxy"] = QList<SkyObject *>();
    initobjects["Constellation"] = QList<SkyObject *>();
    initobjects["Open Cluster"] = QList<SkyObject *>();
    initobjects["Planetary Nebula"] = QList<SkyObject *>();
    updateModels();
}

ModelManager::~ModelManager()
{
    delete planetsModel;
    delete starsModel;
    delete galModel;
    delete conModel;
    delete starClustModel;
    delete nebModel;
}

void ModelManager::updateModels()
{
    KStarsData *data = KStarsData::Instance();

    //double TLM = obsconditions->getTrueMagLim();
    baseCatList<<"Planetary Objects"<<"Stars"<<"Constellations"<<"Deep-sky Objects" ;
    planetaryList<<"Planets"<<"Satellites";
    deepSkyList<<"Galaxies"<<"Star Clusters"<<"Nebulae";

    KSFileReader fileReader;
    if ( !fileReader.open("Interesting.dat") ) return;

    while ( fileReader.hasMoreLines() )
    {
        QString line;
        //SkyObject::TYPE sotype;
        QString sotype;
        line = fileReader.readLine();

        SkyObject *o;
        if ((o = data->skyComposite()->findByName( line )))
        {
            sotype = o->typeName();
            QList<SkyObject *> solist = initobjects[sotype];
            solist.append(o);
            initobjects[sotype] = solist;
        }
    }

    foreach(SkyObject *so, initobjects.value("Star"))
    {
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            starsModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value("Galaxy"))
    {
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            galModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value("Constellation"))
    {
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            conModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value("Open Cluster"))
    {
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            starClustModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value("Planetary Nebula"))
    {
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            nebModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach ( const QString &name, data->skyComposite()->objectNames( SkyObject::PLANET ) ) 
    {
        SkyObject *so = data->skyComposite()->findByName( name );
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
//             SkyObjectItem *planetItem = new SkyObjectItem(o);
//             planetItem->setText(o->name());
            planetsModel->addSkyObject(new SkyObjItem(so));
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

SkyObjListModel* ModelManager::returnModel(QString Type)
{
    if (Type == "Planet")
        return planetsModel;
    else if (Type == "Star")
        return starsModel;
    else if (Type == "Galaxy")
        return galModel;
    else if (Type == "Constellation")
        return conModel;
//     else if (Type == "Star_Clusters")
//         return starClustModel;
//     else if (Type == "Nebulae")
//         return nebModel;

    return (new SkyObjListModel());
}

QStringList ModelManager::returnCatListModel(ModelManager::LIST_TYPE Type)
{
    switch (Type)
    {
        case BaseList:
            return baseCatList;
        case PlanetaryObjects:
            return planetaryList;
        default:
            return deepSkyList;
    }
    return deepSkyList;
}
