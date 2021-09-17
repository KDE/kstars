/*
    SPDX-FileCopyrightText: 2001 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksplanetbase.h"
#include "dms.h"

class KSSun;

/**
 * @class KSMoon
 * @short Provides necessary information about the Moon.
 * A subclass of SkyObject that provides information
 * needed for the Moon.  Specifically, KSMoon provides a moon-specific
 * findPosition() function.  Also, there is a method findPhase(), which returns
 * the lunar phase as a floating-point number between 0.0 and 1.0.
 *
 * @author Jason Harris
 * @version 1.0
 */
class KSMoon : public KSPlanetBase
{
  public:
    using KSPlanetBase::findPhase;

    /** Default constructor. Set name="Moon". */
    KSMoon();
    /** Copy constructor */
    KSMoon(const KSMoon &o);

    ~KSMoon() override;

    KSMoon *clone() const override;
    SkyObject::UID getUID() const override;

    /**
     * Determine the phase angle of the moon, and assign the appropriate moon image
     * @param Sun a KSSun pointer with coordinates updated to the time of computation.
     * If not supplied, the sun is retrieved via KStarsData
     * @note Overrides KSPlanetBase::findPhase()
     */
    void findPhase(const KSSun *Sun = nullptr);

    /** @return the illuminated fraction of the Moon as seen from Earth */
    double illum() const { return 0.5 * (1.0 - cos(Phase * dms::PI / 180.0)); }

    /** @return a short string describing the moon's phase */
    QString phaseName() const;

    /** reimplemented from KSPlanetBase */
    bool loadData() override;

    /** @return iPhase, which is used as a key to find the right image file */
    inline short int getIPhase() const { return iPhase; }

    /**
     * Reimplemented from KSPlanetBase, this function employs unique algorithms for
     * estimating the lunar coordinates.  Finding the position of the moon is
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
    bool findGeocentricPosition(const KSNumbers *num, const KSPlanetBase *) override;

    /**
     * @brief updateMag calls findMagnitude() to calculate current magnitude of moon
     * according to current phase. This function is required to perform findMagnitude()
     * from any where in Kstars
     */
    void updateMag() { findMagnitude(nullptr); }

    void initPopupMenu(KSPopupMenu *pmenu) override;

  private:
    void findMagnitude(const KSNumbers *) override;

    static bool data_loaded;
    static int instance_count;

    /**
     * @class MoonLRData
     * @short Moon Longitude and radius data object
     * Encapsulates the Longitude and radius terms of the sums
     * used to compute the moon's position.
     */
    struct MoonLRData
    {
        int nd { 0 };
        int nm { 0 };
        int nm1 { 0 };
        int nf { 0 };
        double Li { 0 };
        double Ri { 0 };
    };

    static QList<MoonLRData> LRData;

    /**
     * @class MoonBData
     * Encapsulates the Latitude terms of the sums used to compute the moon's position.
     * @short Moon Latitude data object
     */
    struct MoonBData
    {
        int nd { 0 };
        int nm { 0 };
        int nm1 { 0 };
        int nf { 0 };
        double Bi { 0 };
    };

    static QList<MoonBData> BData;
    unsigned int iPhase { 0 };
    KSSun *defaultSun=nullptr;
};
