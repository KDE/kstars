/***************************************************************************
                          kssun.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Jan 29 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
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

#include "ksplanet.h"

#include <QString>

/**
 * @class KSSun
 *
 * Child class of KSPlanetBase; encapsulates information about the Sun.
 *
 * @short Provides necessary information about the Sun.
 *
 * @author Mark Hollomon
 * @version 1.0
 */
class KSSun : public KSPlanet
{
  public:
    /**
     * Constructor.
     *
     * Defines constants needed by findPosition(). Sets Ecliptic coordinates appropriate for J2000.
     */
    KSSun();

    KSSun *clone() const override;
    SkyObject::UID getUID() const override;

    virtual ~KSSun() override = default;

    /**
     * Read orbital data from disk
     *
     * @note reimplemented from KSPlanet
     * @note we actually read Earth's orbital data.  The Sun's geocentric
     * ecliptic coordinates are by definition exactly the opposite of the
     * Earth's heliocentric ecliptic coordinates.
     */
    bool loadData() override;

  protected:
    /**
     * Determine geocentric RA, Dec coordinates for the Epoch given in the argument.
     *
     * @p Epoch current Julian Date
     * @p Earth pointer to earth object
   	 */
    bool findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth = nullptr) override;

  private:
    void findMagnitude(const KSNumbers *) override;
};

long double equinox(int year, int d, int m, int angle);
