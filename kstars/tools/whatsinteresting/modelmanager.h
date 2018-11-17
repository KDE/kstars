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

#pragma once

#include "skyobjitem.h"

#include <QList>
#include <QObject>

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
     * @brief Returns model of given type.
     * @return Pointer to SkyObjListModel of given type.
     * @param modelName Name of sky-object model to be returned.
     */
    SkyObjListModel *returnModel(QString modelName);

    int getModelNumber(QString modelName);

    SkyObjListModel *getTempModel() { return tempModel; }

    void loadNGCCatalog();
    void loadICCatalog();
    void loadSharplessCatalog();
    bool isNGCLoaded() { return ngcLoaded; }
    bool isICLoaded() { return icLoaded; }
    bool isSharplessLoaded() { return sharplessLoaded; }

  signals:
    void loadProgressUpdated(double progress);
    void modelUpdated();

  private:
    void loadLists();
    void loadObjectList(QList<SkyObjItem *> &skyObjectList, int type);
    void loadNamedStarList();
    void loadObjectsIntoModel(SkyObjListModel &model, QList<SkyObjItem *> &skyObjectList);

    ObsConditions *m_ObsConditions { nullptr };
    QList<QList<SkyObjItem *>> m_ObjectList;
    QList<SkyObjListModel *> m_ModelList;
    bool showOnlyVisible { true };
    bool showOnlyFavorites { true };
    QList<SkyObjItem *> favoriteGalaxies;
    QList<SkyObjItem *> favoriteNebulas;
    QList<SkyObjItem *> favoriteClusters;
    SkyObjListModel *tempModel { nullptr };
    bool ngcLoaded { false };
    bool icLoaded { false };
    bool sharplessLoaded { false };
};
