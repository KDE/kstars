/***************************************************************************
                          pointlistcomponent.h  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/10/01
    copyright            : (C) 2005 by Jason Harris
    email                : kstars@30doradus.org
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

#define NCIRCLE 360 //number of points used to define equator, ecliptic and horizon

#include "skycomponent.h"

#include <QList>

#include <memory>

class SkyPoint;

/**
 * @class PointListComponent
 * An abstract parent class, to be inherited by SkyComponents that store a QList of SkyPoints.
 *
 * @author Jason Harris
 * @version 0.1
 */
class PointListComponent : public SkyComponent
{
  public:
    explicit PointListComponent(SkyComposite *parent);

    ~PointListComponent() override;

    /**
     * @short Update the sky positions of this component.
     *
     * This function usually just updates the Horizontal (Azimuth/Altitude)
     * coordinates of the objects in this component.  However, the precession
     * and nutation must also be recomputed periodically.  Requests to do
     * so are sent through the doPrecess parameter.
     * @p num Pointer to the KSNumbers object
     * @note By default, the num parameter is nullptr, indicating that
     * Precession/Nutation computation should be skipped; this computation
     * is only occasionally required.
     */
    void update(KSNumbers *num = nullptr) override;

    QList<std::shared_ptr<SkyPoint>> &pointList() { return m_PointList; }

  private:
    QList<std::shared_ptr<SkyPoint>> m_PointList;
};
