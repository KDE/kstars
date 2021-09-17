/*
    SPDX-FileCopyrightText: 2001 Jason Harris <jharris@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "dms.h"
#include "skypoint.h"

#include <KLocalizedString>

#include <QSharedDataPointer>
#include <QString>
#include <QStringList>

class QPoint;
class GeoLocation;
class KSPopupMenu;

/**
 * @class SkyObject
 * Provides all necessary information about an object in the sky:
 * its coordinates, name(s), type, magnitude, and QStringLists of
 * URLs for images and webpages regarding the object.
 * @short Information about an object in the sky.
 * @author Jason Harris
 * @version 1.0
 */
class SkyObject : public SkyPoint
{
  public:
    /**
     * @short Type for Unique object IDenticator.
     *
     * Each object has unique ID (UID). For different objects UIDs must be different.
     */
    typedef qint64 UID;

    /** @short Kind of UID */
    static const UID UID_STAR;
    static const UID UID_GALAXY;
    static const UID UID_DEEPSKY;
    static const UID UID_SOLARSYS;

    /** Invalid UID. Real sky object could not have such UID */
    static const UID invalidUID;

    /**
     * Constructor.  Set SkyObject data according to arguments.
     * @param t Type of object
     * @param r catalog Right Ascension
     * @param d catalog Declination
     * @param m magnitude (brightness)
     * @param n Primary name
     * @param n2 Secondary name
     * @param lname Long name (common name)
     */
    explicit SkyObject(int t = TYPE_UNKNOWN, dms r = dms(0.0), dms d = dms(0.0), float m = 0.0,
                       const QString &n = QString(), const QString &n2 = QString(), const QString &lname = QString());
    /**
     * Constructor. Set SkyObject data according to arguments. Differs from
     * above function only in data type of RA and Dec.
     * @param t Type of object
     * @param r catalog Right Ascension
     * @param d catalog Declination
     * @param m magnitude (brightness)
     * @param n Primary name
     * @param n2 Secondary name
     * @param lname Long name (common name)
     */
    SkyObject(int t, double r, double d, float m = 0.0, const QString &n = QString(), const QString &n2 = QString(),
              const QString &lname = QString());

    /** Destructor (empty) */
    virtual ~SkyObject() override = default;

    /**
     * @short Create copy of object.
     * This method is virtual copy constructor. It allows for safe
     * copying of objects. In other words, KSPlanet object stored in
     * SkyObject pointer will be copied as KSPlanet.
     *
     * Each subclass of SkyObject MUST implement clone method. There
     * is no checking to ensure this, though.
     *
     *  @return pointer to newly allocated object. Caller takes full responsibility
     *  for deallocating it.
     */
    virtual SkyObject *clone() const;

    /**
     * @enum TYPE
     * The type classification of the SkyObject.
     * @note Keep TYPE_UNKNOWN at 255. To find out how many known
     * types exist, keep the NUMBER_OF_KNOWN_TYPES at the highest
     * non-Unknown value. This is a fake type that can be used in
     * comparisons and for loops.
     */
    enum TYPE
    {
        STAR                  = 0,
        CATALOG_STAR          = 1,
        PLANET                = 2,
        OPEN_CLUSTER          = 3,
        GLOBULAR_CLUSTER      = 4,
        GASEOUS_NEBULA        = 5,
        PLANETARY_NEBULA      = 6,
        SUPERNOVA_REMNANT     = 7,
        GALAXY                = 8,
        COMET                 = 9,
        ASTEROID              = 10,
        CONSTELLATION         = 11,
        MOON                  = 12,
        ASTERISM              = 13,
        GALAXY_CLUSTER        = 14,
        DARK_NEBULA           = 15,
        QUASAR                = 16,
        MULT_STAR             = 17,
        RADIO_SOURCE          = 18,
        SATELLITE             = 19,
        SUPERNOVA             = 20,
        NUMBER_OF_KNOWN_TYPES = 21,
        TYPE_UNKNOWN          = 255
    };
    /**
     * @return A translated string indicating the type name for a given type number
     * @param t The type number
     * @note Note the existence of a SkyObject::typeName( void ) method that is not static and returns the type of this object.
     */
    static QString typeName(const int t);

    /** @return object's primary name. */
    inline virtual QString name(void) const { return hasName() ? Name : unnamedString; }

    /** @return object's primary name, translated to local language. */
    inline QString translatedName() const
    {
        return i18n(
            name()
                .toUtf8()); // FIXME: Hmm... that's funny. How does the string extraction work, if we are UTF8-ing the name first? Does the string extraction change to UTF8?
    }

    /** @return object's secondary name */
    inline QString name2(void) const { return (hasName2() ? Name2 : emptyString); }

    /** @return object's secondary name, translated to local language. */
    inline QString translatedName2() const { return (hasName2() ? i18n(Name2.toUtf8()) : emptyString); }

    /**
     * @return object's common (long) name
     */
    virtual QString longname(void) const { return hasLongName() ? LongName : unnamedObjectString; }

    /**
     * @return object's common (long) name, translated to local language.
     */
    QString translatedLongName() const { return i18n(longname().toUtf8()); }

    /**
     * Set the object's long name.
     * @param longname the object's long name.
     */
    void setLongName(const QString &longname = QString());

    /**
     * @return the string used to label the object on the map
     * In the default implementation, this just returns translatedName()
     * Overridden by StarObject.
     */
    virtual QString labelString() const;

    /**
     * @return object's type identifier (int)
     * @see enum TYPE
     */
    inline int type(void) const { return (int)Type; }

    /**
     * Set the object's type identifier to the argument.
     * @param t the object's type identifier (e.g., "SkyObject::PLANETARY_NEBULA")
     * @see enum TYPE
     */
    inline void setType(int t) { Type = (unsigned char)t; }

    /**
     * @return the type name for this object
     * @note This just calls the static method by the same name, with the appropriate type number. See SkyObject::typeName( const int )
     */
    QString typeName() const;

    /**
     * @return object's magnitude
     */
    inline float mag() const { return sortMagnitude; }

    /**
     * @return the object's position angle.  This is overridden in KSPlanetBase
     * and DeepSkyObject; for all other SkyObjects, this returns 0.0.
     */
    inline virtual double pa() const { return 0.0; }

    /**
     * @return true if the object is a solar system body.
     */
    inline bool isSolarSystem() const { return (type() == 2 || type() == 9 || type() == 10 || type() == 12); }

    /**
     * Initialize the popup menut. This function should call correct
     * initialization function in KSPopupMenu. By overloading the
     * function, we don't have to check the object type when we need
     * the menu.
     */
    virtual void initPopupMenu(KSPopupMenu *pmenu);

    /** Show Type-specific popup menu. Overloading is done in the function initPopupMenu */
    void showPopupMenu(KSPopupMenu *pmenu, const QPoint &pos);

    /**
     * Determine the time at which the point will rise or set.  Because solar system
     * objects move across the sky, it is necessary to iterate on the solution.
     * We compute the rise/set time for the object's current position, then
     * compute the object's position at that time.  Finally, we recompute then
     * rise/set time for the new coordinates.  Further iteration is not necessary,
     * even for the most swiftly-moving object (the Moon).
     * @return the local time that the object will rise
     * @param dt current UT date/time
     * @param geo current geographic location
     * @param rst If true, compute rise time. If false, compute set time.
     * @param exact If true, use a second iteration for more accurate time
     */
    QTime riseSetTime(const KStarsDateTime &dt, const GeoLocation *geo, bool rst, bool exact = true) const;

    /**
     * @return the UT time when the object will rise or set
     * @param dt  target date/time
     * @param geo pointer to Geographic location
     * @param rst Boolean. If true will compute rise time. If false
     * will compute set time.
     * @param exact If true, use a second iteration for more accurate time
     */
    QTime riseSetTimeUT(const KStarsDateTime &dt, const GeoLocation *geo, bool rst, bool exact = true) const;

    /**
     * @return the Azimuth time when the object will rise or set. This function
     * recomputes set or rise UT times.
     * @param dt  target date/time
     * @param geo GeoLocation object
     * @param rst Boolen. If true will compute rise time. If false
     * will compute set time.
     */
    dms riseSetTimeAz(const KStarsDateTime &dt, const GeoLocation *geo, bool rst) const;

    /**
     * The same iteration technique described in riseSetTime() is used here.
     * @return the local time that the object will transit the meridian.
     * @param dt  target date/time
     * @param geo pointer to the geographic location
     */
    QTime transitTime(const KStarsDateTime &dt, const GeoLocation *geo) const;

    /**
     * @return the universal time that the object will transit the meridian.
     * @param dt   target date/time
     * @param geo pointer to the geographic location
     */
    QTime transitTimeUT(const KStarsDateTime &dt, const GeoLocation *geo) const;

    /**
     * @return the altitude of the object at the moment it transits the meridian.
     * @param dt  target date/time
     * @param geo pointer to the geographic location
     */
    dms transitAltitude(const KStarsDateTime &dt, const GeoLocation *geo) const;

    /**
     * The equatorial coordinates for the object on date dt are computed and returned,
     * but the object's internal coordinates are not modified.
     * @return the coordinates of the selected object for the time given by jd
     * @param dt  date/time for which the coords will be computed.
     * @param geo pointer to geographic location (used for solar system only)
     * @note Does not update the horizontal coordinates. Call EquatorialToHorizontal for that.
     */
    SkyPoint recomputeCoords(const KStarsDateTime &dt, const GeoLocation *geo = nullptr) const;

    /**
     * @short Like recomputeCoords, but also calls EquatorialToHorizontal before returning
     */
    SkyPoint recomputeHorizontalCoords(const KStarsDateTime &dt, const GeoLocation *geo) const;

    inline bool hasName() const { return !Name.isEmpty(); }

    inline bool hasName2() const { return !Name2.isEmpty(); }

    inline bool hasLongName() const { return !LongName.isEmpty(); }

    /**
     * @short Given the Image title from a URL file, try to convert it to an image credit string.
     */
    QString messageFromTitle(const QString &imageTitle) const;

    /**
     * @return the pixel distance for offseting the object's name label
     * @note overridden in StarObject, DeepSkyObject, KSPlanetBase
     */
    virtual double labelOffset() const;

    /**
     * @short Return UID for object.
     * This method should be reimplemented in all concrete
     * subclasses. Implementation for SkyObject just returns
     * invalidUID. It's required SkyObject is not an abstract class.
     */
    virtual UID getUID() const;

    // TODO: (Valentin) have another think about onFocus handlers :)    

    /**
     * @brief hashBeenUpdated
     * @return whether the coordinates of the object have been updated
     *
     * This is used for faster filtering.
     */
    bool hashBeenUpdated() { return has_been_updated; }

  private:
    /**
     * Compute the UT time when the object will rise or set. It is an auxiliary
     * procedure because it does not use the RA and DEC of the object but values
     * given as parameters. You may want to use riseSetTimeUT() which is
     * public.  riseSetTimeUT() calls this function iteratively.
     * @param dt     target date/time
     * @param geo    pointer to Geographic location
     * @param righta pointer to Right ascention of the object
     * @param decl   pointer to Declination of the object
     * @param rst    Boolean. If true will compute rise time. If false
     * will compute set time.
     * @return the time at which the given position will rise or set.
     */
    QTime auxRiseSetTimeUT(const KStarsDateTime &dt, const GeoLocation *geo, const dms *righta, const dms *decl,
                           bool riseT) const;

    /**
     * Compute the LST time when the object will rise or set. It is an auxiliary
     * procedure because it does not use the RA and DEC of the object but values
     * given as parameters. You may want to use riseSetTimeLST() which is
     * public.  riseSetTimeLST() calls this function iteratively.
     * @param gLt Geographic latitude
     * @param rga Right ascention of the object
     * @param decl Declination of the object
     * @param rst Boolean. If true will compute rise time. If false
     * will compute set time.
     */
    dms auxRiseSetTimeLST(const dms *gLt, const dms *rga, const dms *decl, bool rst) const;

    /**
     * Compute the approximate hour angle that an object with declination d will have
     * when its altitude is h (as seen from geographic latitude gLat).
     * This function is only used by auxRiseSetTimeLST().
     * @param h pointer to the altitude of the object
     * @param gLat pointer to the geographic latitude
     * @param d pointer to the declination of the object.
     * @return the Hour Angle, in degrees.
     */
    double approxHourAngle(const dms *h, const dms *gLat, const dms *d) const;

    /**
     * Correct for the geometric altitude of the center of the body at the
     * time of rising or setting. This is due to refraction at the horizon
     * and to the size of the body. The moon correction has also to take into
     * account parallax. The value we use here is a rough approximation
     * suggested by J. Meeus.
     *
     * Weather status (temperature and pressure basically) is not taken
     * into account although change of conditions between summer and
     * winter could shift the times of sunrise and sunset by 20 seconds.
     *
     * This function is only used by auxRiseSetTimeLST().
     * @return dms object with the correction.
     */
    dms elevationCorrection(void) const;

    unsigned char Type;
    float
        sortMagnitude; // This magnitude is used for sorting / making decisions about the visibility of an object. Should not be NaN.

  protected:
    /**
     * Set the object's sorting magnitude.
     * @param m the object's magnitude.
     */
    inline void setMag(float m)
    {
        sortMagnitude =
            m < 36.0 ?
                m :
                NaN::
                    f; // Updating faintest sane magnitude to 36.0 (faintest visual magnitude visible with E-ELT, acc. to Wikipedia on Apparent Magnitude.)
    }
    // FIXME: We claim sortMagnitude should not be NaN, but we are setting it to NaN above!! ^

    /**
     * Set the object's primary name.
     * @param name the object's primary name
     */
    inline void setName(const QString &name) { Name = name; }

    /**
     * Set the object's secondary name.
     * @param name2 the object's secondary name.
     */
    inline void setName2(const QString &name2 = QString()) { Name2 = name2; }

    QString Name, Name2, LongName;

    // store often used name strings in static variables
    static QString emptyString;
    static QString unnamedString;
    static QString unnamedObjectString;
    static QString starString;

    // Whether the coordinates of the object have been updated.
    // The default value is chose for compatibility reasons.
    // It primarily matters for objects which are filtered.
    // See `KSAsteroid` for an example.
    bool has_been_updated = true;
};
