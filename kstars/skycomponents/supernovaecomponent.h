/***************************************************************************
                          supernovaecomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2011/18/06
    copyright            : (C) 2011 by Samikshan Bairagya
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

#ifndef SUPERNOVAE_COMPONENT_H
#define SUPERNOVAE_COMPONENT_H

#include "listcomponent.h"
#include "skyobjects/supernova.h"

#include "ksnumbers.h"

#include <QList>

/**
 * @class SupernovaeComponent This class encapsulates Supernovae.
 *
 * @author Samikshan Bairagya
 *
 * @version 0.1
 */

class Supernova;

class SupernovaeComponent : public ListComponent
{
public:
    SupernovaeComponent(SkyComposite* parent);
    virtual ~SupernovaeComponent();
    virtual bool selected();
    virtual void update(KSNumbers* num = 0);
    virtual SkyObject* findByName(const QString& name);
    virtual SkyObject* objectNearest(SkyPoint* p, double& maxrad);

    /**
     * @note This should actually be implemented in a better manner.
     * Possibly by checking if the host galaxy for the supernova is drawn.
     */
    virtual void draw(SkyPainter* skyp);

    virtual void notifyNewSupernovae();

    /**
     * @short This updates the data file by using supernovae_updates_parser.py
     */
    void updateDataFile();

    /**
     * @note Basically copy pasted from StarComponent::zoomMagnitudeLimit()
     */
    static float zoomMagnitudeLimit();
private:
    void loadData();

    QList<SkyObject*> latest;
};

#endif