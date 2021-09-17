/*
    SPDX-FileCopyrightText: 2016 Artem Fedoskin <afedoskin3@gmail.com>
    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyitem.h"

class RootNode;
class SkyObject;
class SolarSystemSingleComponent;

/**
 * @class PlanetsItem
 * This class handles planets and their moons in SkyMapLite
 *
 * @author Artem Fedoskin
 * @version 1.0
 */
class PlanetsItem : public SkyItem
{
  public:
    /**
     * @short Constructor. Takes lists of pointers to planets(SolarSystemSingleComponent) and their
     * moons (PlanetMoonsComponent) to instantiate PlanetMoonsNodes for each of the planet.
     *
     * @param planets list of pointers to planets
     * @param moons list of pointers to moons
     * @param rootNode parent RootNode that instantiates this object
     */
    explicit PlanetsItem(QList<SolarSystemSingleComponent *> planets,
                /*QList<PlanetMoonsComponent *> moons,*/ RootNode *rootNode = nullptr);

    /**
     * @short returns SolarSystemSingleComponent that corresponds to the planet. Used to determine
     * whether the planet has to be drawn according to its selected() function
     * @param planet
     * @return corresponding SolarSystemSingleComponent
     */
    SolarSystemSingleComponent *getParentComponent(SkyObject *planet);

    /**
     * @short calls update() of all child PlanetMoonsNodes
     * @note see PlanetMoonsNodes::update()
     */
    void update() override;

    /**
     * @short shows this item and labels for all moons (currently only Jupiter moons. Add here labels
     * for moons that are needed to be shown)
     */
    virtual void show() override;

    /**
     * @short hides this item and labels for all moons (currently only Jupiter moons. Add here labels
     * for moons that are needed to be hidden)
     */
    virtual void hide() override;

  private:
    QList<SolarSystemSingleComponent *> m_planetComponents;
    //    QList<PlanetMoonsComponent *> m_moonsComponents;
};
