/*
    SPDX-FileCopyrightText: 2012 Samikshan Bairagya <samikshan@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "catalogobject.h"
#include "skyobjitem.h"
#include "catalogsdb.h"
#include <QList>
#include <QObject>
#include <unordered_map>

class ObsConditions;
class SkyObjListModel;

/**
 * @class ModelManager
 * @brief Manages models for QML listviews of different types of sky-objects.
 *
 * @author Samikshan Bairagya
 */
class ModelManager : public QObject
{
    Q_OBJECT
  public:
    /**
     * @enum ObjectList
     * @brief Model type for different types of sky-objects.
     */
    enum ObjectList
    {
        Planets,
        Stars,
        Constellations,
        Galaxies,
        Clusters,
        Nebulas,
        Satellites,
        Asteroids,
        Comets,
        Supernovas,
        Messier,
        NGC,
        IC,
        Sharpless,
        NumberOfLists
    };

    /**
     * @brief Constructor - Creates models for different sky-object types.
     * @param obs   Pointer to an ObsConditions object.
     */
    explicit ModelManager(ObsConditions *obs);
    ~ModelManager() override;

    /** Updates sky-object list models. */
    void updateAllModels(ObsConditions *obs);

    void updateModel(ObsConditions *obs, QString modelName);

    /** Clears all sky-objects list models. */
    void resetAllModels();

    void setShowOnlyVisibleObjects(bool show) { showOnlyVisible = show; }

    bool showOnlyVisibleObjects() { return showOnlyVisible; }

    void setShowOnlyFavoriteObjects(bool show) { showOnlyFavorites = show; }

    bool showOnlyFavoriteObjects() { return showOnlyFavorites; }

    /**
     * Load objects from the dso db for the catalog with \p name can
     * be used to retreive the object lists later.
     *
     * This is implemented by searching the dso database for objects
     * whichs name starts with a prefix to capture subsets of a catalog.
     */
    void loadCatalog(const QString &name);

    /**
     * @brief Returns model of given type.
     * @return Pointer to SkyObjListModel of given type.
     * @param modelName Name of sky-object model to be returned.
     */
    SkyObjListModel *returnModel(QString modelName);

    int getModelNumber(QString modelName);

    SkyObjListModel *getTempModel() { return tempModel; }

  signals:
    void loadProgressUpdated(double progress);
    void modelUpdated();

  private:
    void loadLists();
    void loadObjectList(QList<SkyObjItem *> &skyObjectList, int type);
    void loadNamedStarList();
    void loadObjectsIntoModel(SkyObjListModel &model, QList<SkyObjItem *> &skyObjectList);

    ObsConditions *m_ObsConditions{ nullptr };
    QList<QList<SkyObjItem *>> m_ObjectList;
    QList<SkyObjListModel *> m_ModelList;
    bool showOnlyVisible{ true };
    bool showOnlyFavorites{ true };
    QList<SkyObjItem *> favoriteGalaxies;
    QList<SkyObjItem *> favoriteNebulas;
    QList<SkyObjItem *> favoriteClusters;
    SkyObjListModel *tempModel{ nullptr };
    std::unordered_map<int, CatalogsDB::CatalogObjectList> m_CatalogMap;
    std::unordered_map<int, std::list<SkyObjItem>> m_CatalogSkyObjItems;
};
