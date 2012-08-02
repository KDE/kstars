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
    clustModel = new SkyObjListModel();
    nebModel = new SkyObjListModel();

    initobjects["Star"] = QList<SkyObject *>();
    initobjects["Galaxy"] = QList<SkyObject *>();
    initobjects["Constellation"] = QList<SkyObject *>();
    initobjects["Cluster"] = QList<SkyObject *>();
    initobjects["Nebula"] = QList<SkyObject *>();
    updateModels();
}

ModelManager::~ModelManager()
{
    delete planetsModel;
    delete starsModel;
    delete galModel;
    delete conModel;
    delete clustModel;
    delete nebModel;
}

void ModelManager::updateModels()
{
    KStarsData *data = KStarsData::Instance();

    //double TLM = obsconditions->getTrueMagLim();
    baseCatList << "Planets" << "Stars" << "Constellations" << "Deep-sky Objects" ;
    deepSkyList << "Galaxies" << "Clusters" << "Nebulae";

    KSFileReader fileReader;
    if ( !fileReader.open("Interesting.dat") ) return;

    while ( fileReader.hasMoreLines() )
    {
        QString soTypeName;
        QString line = fileReader.readLine();

        SkyObject *o;
        if ((o = data->skyComposite()->findByName( line )))
        {
            if (o->type() == 3 || o->type() == 4 || o->type() == 14 )
            {
                kDebug()<<"Cluster object";
                initobjects["Cluster"].append(o);
            }
            else if (o->type() == 5 || o->type() == 6 || o->type() == 15)
            {
                kDebug()<<"Nebulae object";
                initobjects["Nebula"].append(o);
            }
            else
            {
                soTypeName = o->typeName();
                QList<SkyObject *> solist = initobjects[soTypeName];
                solist.append(o);
                initobjects[soTypeName] = solist;
            }
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

    foreach(SkyObject *so, initobjects.value("Cluster"))
    {
        kDebug()<<"Cluster object"<<so->name()<<so->typeName();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            clustModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach(SkyObject *so, initobjects.value("Nebula"))
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

SkyObjListModel* ModelManager::returnModel(int type)
{
    switch(type)
    {
    case 0:
        return planetsModel;
    case 1:
        return starsModel;
    case 2:
        return conModel;
    case 3:
        return galModel;
    case 4:
        return clustModel;
    case 5:
        return nebModel;
    default:
        return (new SkyObjListModel());
    }
}
