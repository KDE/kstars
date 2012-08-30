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

    initobjects[Star_Model] = QList<SkyObject *>();
    initobjects[Galaxy_Model] = QList<SkyObject *>();
    initobjects[Constellation_Model] = QList<SkyObject *>();
    initobjects[Cluster_Model] = QList<SkyObject *>();
    initobjects[Nebula_Model] = QList<SkyObject *>();
    updateModels(obs);
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

void ModelManager::updateModels(ObsConditions *obs)
{
    obsconditions = obs;
    initobjects.clear();
    resetModels();

    KStarsData *data = KStarsData::Instance();

    KSFileReader fileReader;
    if (!fileReader.open("Interesting.dat")) return;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();

        if (line[0] == '#' || line.length() == 0)
            continue;

        SkyObject *o;
        if ((o = data->skyComposite()->findByName(line)))
        {
            //kDebug()<<o->longname()<<o->typeName();
            if     (o->type() == SkyObject::OPEN_CLUSTER ||
                    o->type() == SkyObject::GLOBULAR_CLUSTER ||
                    o->type() == SkyObject::GALAXY_CLUSTER)
            {
                initobjects[Cluster_Model].append(o);
            }
            else if (o->type() == SkyObject::PLANETARY_NEBULA ||
                     o->type() == SkyObject::DARK_NEBULA ||
                     o->type() == SkyObject::GASEOUS_NEBULA)
            {
                initobjects[Nebula_Model].append(o);
            }
            else if (o->type() == SkyObject::STAR)
            {
                initobjects[Star_Model].append(o);
            }
            else if (o->type() == SkyObject::CONSTELLATION)
            {
                initobjects[Constellation_Model].append(o);
            }
            else if (o->type() == SkyObject::GALAXY)
            {
                initobjects[Galaxy_Model].append(o);
            }
        }
    }

    foreach (SkyObject *so, initobjects.value(Star_Model))
    {
        //kDebug()<<so->longname()<<so->typeName();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            starsModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, initobjects.value(Galaxy_Model))
    {
        //kDebug()<<so->longname()<<so->typeName();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            galModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, initobjects.value(Constellation_Model))
    {
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            conModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, initobjects.value(Cluster_Model))
    {
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            clustModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, initobjects.value(Nebula_Model))
    {
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            nebModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (const QString &name, data->skyComposite()->objectNames(SkyObject::PLANET))
    {
        SkyObject *so = data->skyComposite()->findByName(name);
        //kDebug()<<so->name()<<so->mag();
        if (obsconditions->isVisible(data->geo(), data->lst(), so))
        {
            if (so->name() == "Sun") continue;
            planetsModel->addSkyObject(new SkyObjItem(so));
        }
    }
}

void ModelManager::resetModels()
{
    planetsModel->resetModel();
    starsModel->resetModel();
    conModel->resetModel();
    galModel->resetModel();
    clustModel->resetModel();
    nebModel->resetModel();
}

SkyObjListModel* ModelManager::returnModel(int type)
{
    switch(type)
    {
    case 0:    //Planet type
        return planetsModel;
    case 1:    //Star type
        return starsModel;
    case 2:    //Constellation type
        return conModel;
    case 3:    //Galaxy Type
        return galModel;
    case 4:    //Cluster type
        return clustModel;
    case 5:    //Nebula type
        return nebModel;
    default:
        return 0;
    }
}
