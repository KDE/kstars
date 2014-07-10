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
    m_ObsConditions = obs;
    m_PlanetsModel = new SkyObjListModel();
    m_StarsModel = new SkyObjListModel();
    m_GalModel = new SkyObjListModel();
    m_ConModel = new SkyObjListModel();
    m_ClustModel = new SkyObjListModel();
    m_NebModel = new SkyObjListModel();

    m_InitObjects[Star_Model] = QList<SkyObject *>();
    m_InitObjects[Galaxy_Model] = QList<SkyObject *>();
    m_InitObjects[Constellation_Model] = QList<SkyObject *>();
    m_InitObjects[Cluster_Model] = QList<SkyObject *>();
    m_InitObjects[Nebula_Model] = QList<SkyObject *>();
    updateModels(obs);
}

ModelManager::~ModelManager()
{
    delete m_PlanetsModel;
    delete m_StarsModel;
    delete m_GalModel;
    delete m_ConModel;
    delete m_ClustModel;
    delete m_NebModel;
}

void ModelManager::updateModels(ObsConditions *obs)
{
    m_ObsConditions = obs;
    m_InitObjects.clear();
    resetModels();

    KStarsData *data = KStarsData::Instance();

    KSFileReader fileReader;
    if (!fileReader.open("Interesting.dat")) return;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();

        if (line.length() == 0 || line[0] == '#')
            continue;

        SkyObject *o;
        if ((o = data->skyComposite()->findByName(line)))
        {
            //qDebug()<<o->longname()<<o->typeName();
            switch(o->type())
            {
                case SkyObject::OPEN_CLUSTER:
                case SkyObject::GLOBULAR_CLUSTER:
                case SkyObject::GALAXY_CLUSTER:
                    m_InitObjects[Cluster_Model].append(o);
                    break;
                case SkyObject::PLANETARY_NEBULA:
                case SkyObject::DARK_NEBULA:
                case SkyObject::GASEOUS_NEBULA:
                    m_InitObjects[Nebula_Model].append(o);
                    break;
                case SkyObject::STAR:
                    m_InitObjects[Star_Model].append(o);
                    break;
                case SkyObject::CONSTELLATION:
                    m_InitObjects[Constellation_Model].append(o);
                    break;
                case SkyObject::GALAXY:
                    m_InitObjects[Galaxy_Model].append(o);
                    break;
            }
        }
    }

    foreach (SkyObject *so, m_InitObjects.value(Star_Model))
    {
        //qDebug()<<so->longname()<<so->typeName();
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            m_StarsModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, m_InitObjects.value(Galaxy_Model))
    {
        //qDebug()<<so->longname()<<so->typeName();
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            m_GalModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, m_InitObjects.value(Constellation_Model))
    {
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            m_ConModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, m_InitObjects.value(Cluster_Model))
    {
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            m_ClustModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (SkyObject *so, m_InitObjects.value(Nebula_Model))
    {
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            m_NebModel->addSkyObject(new SkyObjItem(so));
        }
    }

    foreach (const QString &name, data->skyComposite()->objectNames(SkyObject::PLANET))
    {
        SkyObject *so = data->skyComposite()->findByName(name);
        //qDebug()<<so->name()<<so->mag();
        if (m_ObsConditions->isVisible(data->geo(), data->lst(), so))
        {
            if (so->name() == "Sun") continue;
            m_PlanetsModel->addSkyObject(new SkyObjItem(so));
        }
    }
}

void ModelManager::resetModels()
{
    m_PlanetsModel->resetModel();
    m_StarsModel->resetModel();
    m_ConModel->resetModel();
    m_GalModel->resetModel();
    m_ClustModel->resetModel();
    m_NebModel->resetModel();
}

SkyObjListModel* ModelManager::returnModel(int type)
{
    switch(type)
    {
    case 0:    //Planet type
        return m_PlanetsModel;
    case 1:    //Star type
        return m_StarsModel;
    case 2:    //Constellation type
        return m_ConModel;
    case 3:    //Galaxy Type
        return m_GalModel;
    case 4:    //Cluster type
        return m_ClustModel;
    case 5:    //Nebula type
        return m_NebModel;
    default:
        return 0;
    }
}
