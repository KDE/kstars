/*
    SPDX-FileCopyrightText: 2001 Thomas Kabelmann <tk78@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

//#define PROFILE_UPDATECOORDS

#include "skyobject.h"

#include <QString>

struct DeepStarData;
class KSNumbers;
class KSPopupMenu;
struct StarData;

/**
 * @class StarObject
 *
 * This is a subclass of SkyObject.  It adds the Spectral type, and flags
 * for variability and multiplicity.
 * For stars, the primary name (n) is the latin name (e.g., "Betelgeuse").  The
 * secondary name (n2) is the genetive name (e.g., "alpha Orionis").
 * @short subclass of SkyObject specialized for stars.
 *
 * @author Thomas Kabelmann
 * @version 1.0
 */
class StarObject : public SkyObject
{
  public:
    /**
     * @short returns the reindex interval (in centuries!) for the given
     * magnitude of proper motion (in milliarcsec/year).  ASSUMING a
     * 25 arc-minute margin for proper motion.
     */
    static double reindexInterval(double pm);

    /**
     * Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and spectral type.
     *
     * @param r Right Ascension
     * @param d Declination
     * @param m magnitude
     * @param n common name
     * @param n2 genetive name
     * @param sptype Spectral Type
     * @param pmra Proper motion in RA direction [mas/yr]
     * @param pmdec Proper motion in Dec direction [mas/yr]
     * @param par Parallax angle [mas]
     * @param mult Multiplicity flag (false=dingle star; true=multiple star)
     * @param var Variability flag (true if star is a known periodic variable)
     * @param hd Henry Draper Number
     */
    explicit StarObject(dms r = dms(0.0), dms d = dms(0.0), float m = 0.0, const QString &n = QString(),
                        const QString &n2 = QString(), const QString &sptype = "--", double pmra = 0.0,
                        double pmdec = 0.0, double par = 0.0, bool mult = false, bool var = false, int hd = 0);
    /**
     * Constructor.  Sets sky coordinates, magnitude, latin name, genetive name, and
     * spectral type.  Differs from above function only in data type of RA and Dec.
     *
     * @param r Right Ascension
     * @param d Declination
     * @param m magnitude
     * @param n common name
     * @param n2 genetive name
     * @param sptype Spectral Type
     * @param pmra Proper motion in RA direction [mas/yr]
     * @param pmdec Proper motion in Dec direction [mas/yr]
     * @param par Parallax angle [mas]
     * @param mult Multiplicity flag (false=dingle star; true=multiple star)
     * @param var Variability flag (true if star is a known periodic variable)
     * @param hd Henry Draper Number
     */
    StarObject(double r, double d, float m = 0.0, const QString &n = QString(), const QString &n2 = QString(),
               const QString &sptype = "--", double pmra = 0.0, double pmdec = 0.0, double par = 0.0, bool mult = false,
               bool var = false, int hd = 0);

    StarObject *clone() const override;
    UID getUID() const override;

    /** Copy constructor */
    StarObject(const StarObject &o);

    /** Destructor. (Empty) */
    ~StarObject() override = default;

    /**
     * @short  Initializes a StarObject to given data
     *
     * This is almost like the StarObject constructor itself, but it avoids
     * setting up name, gname etc for unnamed stars. If called instead of the
     * constructor, this method will be much faster for unnamed stars
     *
     * @param  stardata  Pointer to starData object containing required data (except name and gname)
     * @return Nothing
     */
    void init(const StarData *stardata);

    /**
     * @short  Initializes a StarObject to given data
     *
     * @param  stardata  Pointer to deepStarData object containing the available data
     * @return Nothing
     */
    void init(const DeepStarData *stardata);

    /**
     * @short  Sets the name, genetive name, and long name
     *
     * @param  name  Common name
     * @param  name2 Genetive name
     */
    void setNames(const QString &name, const QString &name2);

    /** @return true if the star has a name ("star" doesn't count) */
    inline bool hasName() const { return (!Name.isEmpty() && Name != starString); }

    /** @return true if the star has a latin name ("star" or HD... doesn't count) */
    inline bool hasLatinName() const
    {
        return (!Name.isEmpty() && Name != starString && Name != gname(false) && Name != gname(true) &&
                !Name.startsWith("HD "));
    }

    /** If star is unnamed return "star" otherwise return the name */
    inline QString name(void) const override { return hasName() ? Name : starString; }

    /** If star is unnamed return "star" otherwise return the longname */
    inline QString longname(void) const override { return hasLongName() ? LongName : starString; }

    /**
     * Returns entire spectral type string
     * @return Spectral Type string
     */
    QString sptype(void) const;

    /** Returns just the first character of the spectral type string. */
    char spchar() const;

    /**
     * Returns the genetive name of the star.
     * @return genetive name of the star
     */
    QString gname(bool useGreekChars = true) const;

    /**
     * Returns the greek letter portion of the star's genetive name.
     * Returns empty string if star has no genetive name defined.
     * @return greek letter portion of genetive name
     */
    QString greekLetter(bool useGreekChars = true) const;

    /** @return the genitive form of the star's constellation. */
    QString constell(void) const;

    /**
     * Determine the current coordinates (RA, Dec) from the catalog
     * coordinates (RA0, Dec0), accounting for both precession and nutation.
     *
     * @param num pointer to KSNumbers object containing current values of
     * time-dependent variables.
     * @param includePlanets does nothing in this implementation (see KSPlanetBase::updateCoords()).
     * @param lat does nothing in this implementation (see KSPlanetBase::updateCoords()).
     * @param LST does nothing in this implementation (see KSPlanetBase::updateCoords()).
     * @param forceRecompute defines whether the data should be recomputed forcefully.
     */
    void updateCoords(const KSNumbers *num, bool includePlanets = true, const CachingDms *lat = nullptr,
                      const CachingDms *LST = nullptr, bool forceRecompute = false) override;

    /**
     * @short Fills ra and dec with the coordinates of the star with the proper
     * motion correction but without precision and its friends.  It is used
     * in StarComponent to re-index all the stars.
     * @note In the Hipparcos catalog, from which most of the proper
     * motion data in KStars is obtained, the RA correction already
     * has the cos(delta) factor incorporated into it. See
     * https://heasarc.gsfc.nasa.gov/W3Browse/all/hipparcos.html
     *
     * @return true if we changed the coordinates, false otherwise
     * NOTE: ra and dec both in degrees.
     */
    bool getIndexCoords(const KSNumbers *num, CachingDms &ra, CachingDms &dec);
    bool getIndexCoords(const KSNumbers *num, double *ra, double *dec);

    /** @short added for JIT updates from both StarComponent and ConstellationLines */
    void JITupdate();

    /** @short returns the magnitude of the proper motion correction in milliarcsec/year */
    inline double pmMagnitude() const
    {
        return sqrt(pmRA() * pmRA() + pmDec() * pmDec());
    }

    /**
     * @short returns the square of the magnitude of the proper motion correction in (milliarcsec/year)^2
     * @note In the Hipparcos catalog, from which most of the proper
     * motion data in KStars is obtained, the RA correction already
     * has the cos(delta) factor incorporated into it. See
     * https://heasarc.gsfc.nasa.gov/W3Browse/all/hipparcos.html
     * @note This method is faster when the square root need not be taken
     */
    inline double pmMagnitudeSquared() const
    {
        return (pmRA() * pmRA() + pmDec() * pmDec());
    }

    /**
     * @short Set the Ra and Dec components of the star's proper motion, in milliarcsec/year.
     * Note that the RA component should already have been multiplied by cos(dec).
     * @param pmra the new RA proper motion
     * @param pmdec the new Dec proper motion
     */
    inline void setProperMotion(double pmra, double pmdec)
    {
        PM_RA  = pmra;
        PM_Dec = pmdec;
    }

    /** @return the RA component of the star's proper motion, in mas/yr (multiplied by cos(dec)) */
    inline double pmRA() const { return PM_RA; }

    /** @return the Dec component of the star's proper motion, in mas/yr */
    inline double pmDec() const { return PM_Dec; }

    /** @short set the star's parallax angle, in milliarcsec */
    inline void setParallax(double plx) { Parallax = plx; }

    /** @return the star's parallax angle, in milliarcsec */
    inline double parallax() const { return Parallax; }

    /** @return the star's distance from the Sun in parsecs, as computed from the parallax. */
    inline double distance() const { return 1000. / parallax(); }

    /**
     * @short set the star's multiplicity flag (i.e., is it a binary or multiple star?)
     * @param m true if binary/multiple star system
     */
    inline void setMultiple(bool m) { Multiplicity = m; }

    /** @return whether the star is a binary or multiple starobject */
    inline bool isMultiple() const { return Multiplicity; }

    /** @return the star's HD index */
    inline int getHDIndex() const { return HD; }

    /**
     * @short set the star's variability flag
     *
     * @param v true if star is variable
     */
    inline void setVariable(bool v) { Variability = v; }

    /** @return whether the star is a binary or multiple starobject */
    inline bool isVariable() const { return Variability; }

    /** @short returns the name, the magnitude or both. */
    QString nameLabel(bool drawName, bool drawMag) const;

    QString labelString() const override;

    /**
     * @return the pixel distance for offseting the star's name label
     * This takes the zoom level and the star's brightness into account.
     */
    double labelOffset() const override;

    /** @return the Visual magnitude of the star */
    inline float getVMag() const { return V; }

    /** @return the blue magnitude of the star */
    inline float getBMag() const { return B; }

    /**
     * @return the B - V color index of the star, or a nonsense number
     * larger than 30 if it's not well defined
     */
    inline float getBVIndex() const { return ((B < 30.0 && V < 30.0) ? B - V : 99.9); }

    void initPopupMenu(KSPopupMenu *pmenu) override;

    quint64 updateID { 0 };
    quint64 updateNumID { 0 };

#ifdef PROFILE_UPDATECOORDS
    static double updateCoordsCpuTime;
    static unsigned int starsUpdated;
#endif

  protected:
    // DEBUG EDIT. For testing proper motion, uncomment this, and related blocks
    // See starobject.cpp for further info.
    //    static QVector<SkyPoint *> Trail;
    //    bool testStar;
    // END DEBUG

  private:
    double PM_RA { 0 };
    double PM_Dec { 0 };
    double Parallax { 0 };
    bool Multiplicity { false };
    bool Variability { false };
    char SpType[2];
    int HD { 0 };
    // B and V magnitudes, separately. NOTE 1) This is kept separate from mag for a reason.
    // See init( const DeepStarData *); 2) This applies only to deep stars at the moment
    float B { 0 };
    float V { 0 };
};
