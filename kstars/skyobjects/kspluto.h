/*
    SPDX-FileCopyrightText: 2001 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef KSPLUTO_H_
#define KSPLUTO_H_

#include "ksasteroid.h"

/** @class KSPluto
	*A subclass of KSAsteroid that represents the planet Pluto.  Now, we
	*are certainly NOT claiming that Pluto is an asteroid.  However, the
	*findPosition() routines of KSPlanet do not work properly for Pluto.
	*We had been using a unique polynomial expansion for Pluto, but even
	*this fails spectacularly for dates much more remote than a few
	*hundred years.  We consider it better to instead treat Pluto's
	*orbit much more simply, using elliptical orbital elements as we do
	*for asteroids.  In order to improve the long-term accuracy of Pluto's
	*position, we are also including linear approximations of the evolution
	*of each orbital element with time.
	*
	*The orbital element data (including the time-derivatives) come from
	*the NASA/JPL website:  https://ssd.jpl.nasa.gov/?planets#elem
	*
	*@short Provides necessary information about Pluto.
	*@author Jason Harris
	*@version 1.0
	*/

class KSPluto : public KSAsteroid
{
  public:
    /** Constructor.  Calls KSAsteroid constructor with name="Pluto", and fills
        	*in orbital element data (which is hard-coded for now).
        	*@param fn filename of Pluto's image
        	*@param pSize physical diameter of Pluto, in km
        	*/
    explicit KSPluto(const QString &fn = QString(), double pSize = 0);

    KSPluto *clone() const override;

    /**Destructor (empty) */
    ~KSPluto() override;

  protected:
    /** A custom findPosition() function for Pluto.  Computes the values of the
        	*orbital elements on the requested date, and calls KSAsteroid::findGeocentricPosition()
        	*using those elements.
        	*@param num time-dependent values for the desired date
        	*@param Earth planet Earth (needed to calculate geocentric coords)
        	*@return true if position was successfully calculated.
        	*/
    bool findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *Earth = nullptr) override;

  private:
    void findMagnitude(const KSNumbers *) override;

    //The base orbital elements for J2000 (these don't change with time)
    double a0, e0;
    dms i0, w0, M0, N0;

    //Rates-of-change for each orbital element
    double a1, e1, i1, w1, M1, N1;
};

#endif
