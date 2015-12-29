/***************************************************************************
                          ksmoon.h  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Aug 26 2001
    copyright            : (C) 2001 by Jason Harris
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

#ifndef KSMOON_H_
#define KSMOON_H_

#include "ksplanetbase.h"
#include "dms.h"

/** @class KSMoon
	*A subclass of SkyObject that provides information
	*needed for the Moon.  Specifically, KSMoon provides a moon-specific
	*findPosition() function.  Also, there is a method findPhase(), which returns
	*the lunar phase as a floating-point number between 0.0 and 1.0.
	*@short Provides necessary information about the Moon.
	*@author Jason Harris
	*@version 1.0
	*/

class KSSun;

class KSMoon : public KSPlanetBase  {
public:
    using KSPlanetBase::findPhase;

    /** Default constructor. Set name="Moon". */
    KSMoon();
    /** Copy constructor */
    KSMoon(const KSMoon& o);

    virtual KSMoon* clone() const;
    virtual SkyObject::UID getUID() const;

    /** Destructor (empty). */
    ~KSMoon();

    /**
     *Determine the phase angle of the moon, and assign the appropriate
     *moon image
     * @param Sun a KSSun pointer with coordinates updated to the time of computation. If not supplied, the findByName() method will be used to find the sun.
     *@note Overrides KSPlanetBase::findPhase()
     */
    virtual void findPhase( const KSSun *Sun = 0 );

    /** @return the illuminated fraction of the Moon as seen from Earth */
    double illum() const { return 0.5*(1.0 - cos( Phase * dms::PI / 180.0 ) ); }

    /** @return a short string describing the moon's phase */
    QString phaseName() const;

    /** reimplemented from KSPlanetBase */
    virtual bool loadData();

    /** @return iPhase, which is used as a key to find the right image file */
    inline short int getIPhase() const { return iPhase; }

    /** Reimplemented from KSPlanetBase, this function employs unique algorithms for
     *  estimating the lunar coordinates.  Finding the position of the moon is
     * much more difficult than the other planets.  For one thing, the Moon is
     * a lot closer, so we can detect smaller deviations in its orbit.  Also,
     * the Earth has a significant effect on the Moon's orbit, and their
     * interaction is complex and nonlinear.  As a result, the positions as
     * calculated by findPosition() are only accurate to about 10 arcseconds
     * (10 times less precise than the planets' positions!)
     * @short moon-specific coordinate finder
     * @param num KSNumbers pointer for the target date/time
     * @note we don't use the Earth pointer here
     */
    virtual bool findGeocentricPosition( const KSNumbers *num, const KSPlanetBase* );

    /**
     * @brief updateMag calls findMagnitude() to calculate current magnitude of moon
     * according to current phase. This function is required to perform findMagnitude() from any where in Kstars
     */
    void updateMag() { findMagnitude(NULL); }

private:
    virtual void initPopupMenu( KSPopupMenu* pmenu );
    virtual void findMagnitude(const KSNumbers*);

    static bool data_loaded;
    static int instance_count;

    /** @class MoonLRData
     * Encapsulates the Longitude and radius terms of the sums
     * used to compute the moon's position.
     * @short Moon Longitude and radius data object
     * @author Mark Hollomon
     * @version 1.0
     */
    struct MoonLRData {
        int nd;
        int nm;
        int nm1;
        int nf;
        double Li;
        double Ri;
    };

    static QList<MoonLRData> LRData;

    /** @class MoonBData
     * Encapsulates the Latitude terms of the sums
     * used to compute the moon's position.
     * @short Moon Latitude data object
     * @author Mark Hollomon
     * @version 1.0
     */
    struct MoonBData {
        int nd;
        int nm;
        int nm1;
        int nf;
        double Bi;
    };

    static QList<MoonBData> BData;
    unsigned int iPhase;
};

#endif
