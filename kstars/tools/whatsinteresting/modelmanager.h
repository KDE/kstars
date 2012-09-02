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

#ifndef MODEL_MANAGER_H
#define MODEL_MANAGER_H

#include "skyobjlistmodel.h"
#include "kstarsdata.h"
#include "obsconditions.h"

/**
 * \class ModelManager
 * \brief Manages models for QML listviews of different types of sky-objects.
 * \author Samikshan Bairagya
 */
class ModelManager
{
public:
    /**
     * \enum ModelType
     * \brief Model type for different types of sky-objects.
     */
    enum ModelType {Planet_Model, Star_Model, Constellation_Model, Galaxy_Model, Cluster_Model, Nebula_Model};

    /**
     * \brief Constructor - Creates models for different sky-object types.
     * \param obs   Pointer to an ObsConditions object.
     */
    ModelManager(ObsConditions *obs);

    /**
     * \brief Destructor
     */
    ~ModelManager();

    /**
     * \brief Updates sky-object list models.
     */
    void updateModels(ObsConditions *obs);

    /**
     * \brief Clears all sky-objects list models.
     */
    void resetModels();

    /**
     * \brief Returns model of given type.
     * \return Pointer to SkyObjListModel of given type.
     * \param type   Type of sky-object model to be returned.
     */
    SkyObjListModel *returnModel (int type);

private:
    ObsConditions *obsconditions;
    SkyObjListModel *planetsModel, *starsModel, *galModel, *conModel, *clustModel, *nebModel;
    QStringList baseCatList, planetaryList, deepSkyList;
    QHash< ModelType, QList <SkyObject *> > initobjects;
};

#endif
