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
#include <QtConcurrent>

ModelManager::ModelManager(ObsConditions * obs)
{
    m_ObsConditions = obs;

    m_ModelList = QList < SkyObjListModel *>();
    m_ObjectList = QList< QList <SkyObjItem *> >();

    favoriteClusters = QList <SkyObjItem *>();
    favoriteNebulas = QList <SkyObjItem *>();
    favoriteGalaxies = QList <SkyObjItem *>();

    for(int i=0;i<NumberOfLists;i++){
        m_ModelList.append(new SkyObjListModel());
        m_ObjectList.append(QList<SkyObjItem *>());
    }

    QtConcurrent::run(this, &ModelManager::loadLists);
}

ModelManager::~ModelManager()
{
    qDeleteAll(m_ModelList);
    foreach(QList<SkyObjItem *> list, m_ObjectList)
       qDeleteAll(list);
}

void ModelManager::loadLists()
{
    KStarsData * data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> listStars;
    listStars.append(data->skyComposite()->objectLists(SkyObject::STAR));
    for(int i = 0; i < listStars.size(); i++)
    {
        QPair<QString, const SkyObject *> pair = listStars.value(i);
        const StarObject * star = dynamic_cast<const StarObject *>(pair.second);
        if(star->hasLatinName())
            m_ObjectList[Stars].append(new SkyObjItem(star->clone()));
    }
    QString prevName;
    for(int i = 0; i < m_ObjectList[Stars].size(); i++)
    {
        SkyObjItem *star=m_ObjectList[Stars].at(i);
        if(prevName==star->getName()){
            m_ObjectList[Stars].removeAt(i);
            i--;
        }
        prevName = star->getName();
    }

    KSFileReader fileReader;
    if (!fileReader.open("Interesting.dat")) return;

    while (fileReader.hasMoreLines())
    {
        QString line = fileReader.readLine();

        if (line.length() == 0 || line[0] == '#')
            continue;

        SkyObject * o;
        if ((o = data->skyComposite()->findByName(line)))
        {
            //qDebug()<<o->longname()<<o->typeName();
            switch(o->type())
            {
                case SkyObject::OPEN_CLUSTER:
                case SkyObject::GLOBULAR_CLUSTER:
                case SkyObject::GALAXY_CLUSTER:
                    favoriteClusters.append(new SkyObjItem(o));
                    break;
                case SkyObject::PLANETARY_NEBULA:
                case SkyObject::DARK_NEBULA:
                case SkyObject::GASEOUS_NEBULA:
                    favoriteNebulas.append(new SkyObjItem(o));
                    break;
                case SkyObject::GALAXY:
                    favoriteGalaxies.append(new SkyObjItem(o));
                    break;
            }
        }
    }

    loadObjectList(m_ObjectList[Asteroids], SkyObject::ASTEROID);
    loadObjectList(m_ObjectList[Comets], SkyObject::COMET);
    loadObjectList(m_ObjectList[Satellites], SkyObject::SATELLITE);
    loadObjectList(m_ObjectList[Constellations], SkyObject::CONSTELLATION);
    loadObjectList(m_ObjectList[Planets], SkyObject::PLANET);

    loadObjectList(m_ObjectList[Galaxies], SkyObject::GALAXY);

    loadObjectList(m_ObjectList[Clusters], SkyObject::OPEN_CLUSTER);
    loadObjectList(m_ObjectList[Clusters], SkyObject::GLOBULAR_CLUSTER);
    loadObjectList(m_ObjectList[Clusters], SkyObject::GALAXY_CLUSTER);

    loadObjectList(m_ObjectList[Nebulas], SkyObject::PLANETARY_NEBULA);
    loadObjectList(m_ObjectList[Nebulas], SkyObject::GASEOUS_NEBULA);
    loadObjectList(m_ObjectList[Nebulas], SkyObject::DARK_NEBULA);


    for(int i=1;i<=110;i++)
    {
           SkyObject * o;
           if ((o = data->skyComposite()->findByName("M " + QString::number(i))))
               m_ObjectList[Messier].append(new SkyObjItem(o));
    }
/**
   Note:  The sharpless catalog did not have good wikipedia articles
    for(int i=1;i<=350;i++)
    {
           SkyObject * o;
           if ((o = data->skyComposite()->findByName("Sh2 " + QString::number(i))))
               m_SharplessModel->addSkyObject(new SkyObjItem(o));
    }
**/
}

void ModelManager::updateAllModels(ObsConditions * obs)
{
    m_ObsConditions = obs;
    resetAllModels();

    for(int i=0;i<NumberOfLists;i++)
        loadObjectsIntoModel(*m_ModelList[i], m_ObjectList[i]);
}

void ModelManager::updateModel(ObsConditions * obs, QString modelName){
    m_ObsConditions = obs;
    SkyObjListModel * model = returnModel(modelName);
    if(model){
        model->resetModel();
        if(showOnlyFavorites && modelName=="galaxies")
            loadObjectsIntoModel(*m_ModelList[getModelNumber(modelName)], favoriteGalaxies);
        else if(showOnlyFavorites && modelName=="nebulas")
            loadObjectsIntoModel(*m_ModelList[getModelNumber(modelName)], favoriteNebulas);
        else if(showOnlyFavorites && modelName=="clusters")
            loadObjectsIntoModel(*m_ModelList[getModelNumber(modelName)], favoriteClusters);
        else
            loadObjectsIntoModel(*m_ModelList[getModelNumber(modelName)], m_ObjectList[getModelNumber(modelName)]);
    }
}

void ModelManager::loadObjectList(QList<SkyObjItem *> & skyObjectList, int type){
    KStarsData * data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> objects = data->skyComposite()->objectLists( type );

    for(int i = 0; i < objects.size(); i++)
    {
        QPair<QString, const SkyObject *> pair = objects.value(i);
        const SkyObject * listObject = dynamic_cast<const SkyObject *>(pair.second);
        if(listObject->name() != "Sun")
            skyObjectList.append(new SkyObjItem(listObject->clone()));
    }
}

void ModelManager::loadObjectsIntoModel(SkyObjListModel & model, QList<SkyObjItem *> &skyObjectList){
    KStarsData * data = KStarsData::Instance();

    foreach (SkyObjItem * soitem, skyObjectList)
    {
        bool isVisible = (showOnlyVisible) ? (m_ObsConditions->isVisible(data->geo(), data->lst(), soitem->getSkyObject())) : true;
        if(isVisible)
            model.addSkyObject(soitem);
    }
}

void ModelManager::resetAllModels()
{
    foreach(SkyObjListModel * model, m_ModelList)
        model->resetModel();
}

int ModelManager::getModelNumber(QString modelName){
    if(modelName=="planets")
        return Planets;
    if(modelName=="stars")
        return Stars;
    if(modelName=="constellations")
        return Constellations;
    if(modelName=="galaxies")
        return Galaxies;
    if(modelName=="clusters")
        return Clusters;
    if(modelName=="nebulas")
        return Nebulas;
    if(modelName=="asteroids")
        return Asteroids;
    if(modelName=="comets")
        return Comets;
    if(modelName=="satellites")
        return Satellites;
    if(modelName=="messier")
        return Messier;
    if(modelName=="sharpless")
        return Sharpless;
    else return -1;
}

SkyObjListModel * ModelManager::returnModel(QString modelName)
{
        int modelNumber = getModelNumber(modelName);
        if(modelNumber > -1 && modelNumber < NumberOfLists)
            return m_ModelList[modelNumber];
        else
            return new SkyObjListModel();
}
