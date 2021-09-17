/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "modelmanager.h"

#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "obsconditions.h"
#include "skymapcomposite.h"
#include "skyobjitem.h"
#include "skyobjlistmodel.h"
#include "starobject.h"
#include "catalogsdb.h"

#include <QtConcurrent>

ModelManager::ModelManager(ObsConditions *obs)
{
    m_ObsConditions = obs;

    tempModel = new SkyObjListModel();

    m_ModelList  = QList<SkyObjListModel *>();
    m_ObjectList = QList<QList<SkyObjItem *>>();

    favoriteClusters = QList<SkyObjItem *>();
    favoriteNebulas  = QList<SkyObjItem *>();
    favoriteGalaxies = QList<SkyObjItem *>();

    for (int i = 0; i < NumberOfLists; i++)
    {
        m_ModelList.append(new SkyObjListModel());
        m_ObjectList.append(QList<SkyObjItem *>());
    }

    QtConcurrent::run(this, &ModelManager::loadLists);
}

ModelManager::~ModelManager()
{
    qDeleteAll(m_ModelList);
    foreach (QList<SkyObjItem *> list, m_ObjectList)
        qDeleteAll(list);
    delete tempModel;
}

void ModelManager::loadLists()
{
    if (KStars::Closing)
        return;

    emit loadProgressUpdated(0);
    KStarsData *data = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> listStars;
    listStars.append(data->skyComposite()->objectLists(SkyObject::STAR));
    for (int i = 0; i < listStars.size(); i++)
    {
        QPair<QString, const SkyObject *> pair = listStars.value(i);
        const StarObject *star                 = dynamic_cast<const StarObject *>(pair.second);
        if (star != nullptr && star->hasLatinName())
            m_ObjectList[Stars].append(new SkyObjItem((SkyObject *)(star)));
    }
    QString prevName;
    for (int i = 0; i < m_ObjectList[Stars].size(); i++)
    {
        SkyObjItem *star = m_ObjectList[Stars].at(i);
        if (prevName == star->getName())
        {
            m_ObjectList[Stars].removeAt(i);
            i--;
        }
        prevName = star->getName();
    }

    KSFileReader fileReader;
    if (!fileReader.open("Interesting.dat"))
        return;

    while (fileReader.hasMoreLines())
    {
        if (KStars::Closing)
            return;

        QString line = fileReader.readLine();

        if (line.length() == 0 || line[0] == '#')
            continue;

        SkyObject *o;
        if ((o = data->skyComposite()->findByName(line)))
        {
            //qDebug()<<o->longname()<<o->typeName();
            switch (o->type())
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

    emit loadProgressUpdated(0.20);

    loadObjectList(m_ObjectList[Asteroids], SkyObject::ASTEROID);
    emit loadProgressUpdated(0.30);
    loadObjectList(m_ObjectList[Comets], SkyObject::COMET);
    emit loadProgressUpdated(0.40);
    loadObjectList(m_ObjectList[Satellites], SkyObject::SATELLITE);
    loadObjectList(m_ObjectList[Supernovas], SkyObject::SUPERNOVA);
    emit loadProgressUpdated(0.50);
    loadObjectList(m_ObjectList[Constellations], SkyObject::CONSTELLATION);
    emit loadProgressUpdated(0.55);
    loadObjectList(m_ObjectList[Planets], SkyObject::PLANET);
    emit loadProgressUpdated(0.60);

    loadObjectList(m_ObjectList[Galaxies], SkyObject::GALAXY);
    emit loadProgressUpdated(0.70);

    loadObjectList(m_ObjectList[Clusters], SkyObject::OPEN_CLUSTER);
    loadObjectList(m_ObjectList[Clusters], SkyObject::GLOBULAR_CLUSTER);
    loadObjectList(m_ObjectList[Clusters], SkyObject::GALAXY_CLUSTER);
    emit loadProgressUpdated(0.80);

    loadObjectList(m_ObjectList[Nebulas], SkyObject::PLANETARY_NEBULA);
    loadObjectList(m_ObjectList[Nebulas], SkyObject::SUPERNOVA_REMNANT);
    loadObjectList(m_ObjectList[Nebulas], SkyObject::GASEOUS_NEBULA);
    loadObjectList(m_ObjectList[Nebulas], SkyObject::DARK_NEBULA);

    emit loadProgressUpdated(0.90);
    emit loadProgressUpdated(1);
}

void ModelManager::updateAllModels(ObsConditions *obs)
{
    m_ObsConditions = obs;
    resetAllModels();

    for (int i = 0; i < NumberOfLists; i++)
        loadObjectsIntoModel(*m_ModelList[i], m_ObjectList[i]);
}

void ModelManager::updateModel(ObsConditions *obs, QString modelName)
{
    m_ObsConditions        = obs;
    SkyObjListModel *model = returnModel(modelName);
    const auto modelNumber = getModelNumber(modelName);

    if (modelNumber > -1 && model)
    {
        model->resetModel();
        if (showOnlyFavorites && modelName == "galaxies")
            loadObjectsIntoModel(*m_ModelList[modelNumber], favoriteGalaxies);
        else if (showOnlyFavorites && modelName == "nebulas")
            loadObjectsIntoModel(*m_ModelList[modelNumber], favoriteNebulas);
        else if (showOnlyFavorites && modelName == "clusters")
            loadObjectsIntoModel(*m_ModelList[modelNumber], favoriteClusters);
        else
            loadObjectsIntoModel(*m_ModelList[modelNumber], m_ObjectList[modelNumber]);
        emit modelUpdated();
    }
}

void ModelManager::loadObjectList(QList<SkyObjItem *> &skyObjectList, int type)
{
    if (KStars::Closing)
        return;

    KStarsData *data                                   = KStarsData::Instance();
    QVector<QPair<QString, const SkyObject *>> objects = data->skyComposite()->objectLists(type);

    for (int i = 0; i < objects.size(); i++)
    {
        if (KStars::Closing)
            return;

        QPair<QString, const SkyObject *> pair = objects.value(i);
        const SkyObject *listObject            = pair.second;
        if (listObject->name() != i18n("Sun"))
            skyObjectList.append(new SkyObjItem(const_cast<SkyObject *>(listObject)));
    }
    QString prevName;
    for (int i = 0; i < skyObjectList.size(); i++)
    {
        if (KStars::Closing)
            return;

        SkyObjItem *obj = skyObjectList.at(i);
        if (prevName == obj->getName())
        {
            skyObjectList.removeAt(i);
            i--;
        }
        prevName = obj->getName();
    }
}

void ModelManager::loadObjectsIntoModel(SkyObjListModel &model, QList<SkyObjItem *> &skyObjectList)
{
    KStarsData *data = KStarsData::Instance();

    foreach (SkyObjItem *soitem, skyObjectList)
    {
        bool isVisible =
            (showOnlyVisible) ? (m_ObsConditions->isVisible(data->geo(), data->lst(), soitem->getSkyObject())) : true;
        if (isVisible)
            model.addSkyObject(soitem);
    }
}

void ModelManager::resetAllModels()
{
    foreach (SkyObjListModel *model, m_ModelList)
        model->resetModel();
}

int ModelManager::getModelNumber(QString modelName)
{
    if (modelName == "planets")
        return Planets;
    if (modelName == "stars")
        return Stars;
    if (modelName == "constellations")
        return Constellations;
    if (modelName == "galaxies")
        return Galaxies;
    if (modelName == "clusters")
        return Clusters;
    if (modelName == "nebulas")
        return Nebulas;
    if (modelName == "asteroids")
        return Asteroids;
    if (modelName == "comets")
        return Comets;
    if (modelName == "supernovas")
        return Supernovas;
    if (modelName == "satellites")
        return Satellites;
    if (modelName == "messier")
        return Messier;
    if (modelName == "ngc")
        return NGC;
    if (modelName == "ic")
        return IC;
    if (modelName == "sharpless")
        return Sharpless;
    else
        return -1;
}

SkyObjListModel *ModelManager::returnModel(QString modelName)
{
    int modelNumber = getModelNumber(modelName);
    if (modelNumber > -1 && modelNumber < NumberOfLists)
        return m_ModelList[modelNumber];
    else
        return tempModel;
}

void ModelManager::loadCatalog(const QString &name)
{
    const auto id = getModelNumber(name);
    if (m_CatalogMap.count(id) > 0)
        return;

    const std::unordered_map<QString, QString> search_prefixes{
        { "ngc", "NGC " }, { "ic", "IC " }, { "messier", "M " }, { "sharpless", "Sh2 " }
    };

    CatalogsDB::DBManager manager{ CatalogsDB::dso_db_path() };

    const auto &prefix = search_prefixes.at(name);
    const int offset   = prefix.size();

    m_CatalogMap[id] = std::get<2>(manager.general_master_query(
        QString("name LIKE '%1'").arg(prefix + "%"),
        QString("CAST(SUBSTR(name,%1) AS INT)").arg(offset)));

    auto &lst = m_CatalogSkyObjItems[id];

    for (auto &obj : m_CatalogMap[id])
    {
        obj.updateCoordsNow(KStarsData::Instance()->updateNum());
        lst.emplace_back(&obj);
    }

    auto &p_lst = m_ObjectList[id];
    for (auto &obj : lst)
        p_lst.append(&obj);

    updateModel(m_ObsConditions, name);
    emit loadProgressUpdated(1);
};
