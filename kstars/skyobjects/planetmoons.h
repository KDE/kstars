/*
    SPDX-FileCopyrightText: Vipul Kumar Singh <vipulkrsingh@gmail.com>
    SPDX-FileCopyrightText: Médéric Boquien <mboquien@free.fr>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include <QString>
#include <QVector>

class KSNumbers;
class KSPlanetBase;
class KSSun;
class TrailObject;
class dms;

/**
 * @class PlanetMoons
 *
 * Implements the moons of a planet.
 *
 * TODO: make the moons SkyObjects, rather than just points.
 *
 * @author Vipul Kumar Singh
 * @version 1.0
 */
class PlanetMoons
{
  public:
    /**
     * Constructor.  Assign the name of each moon,
     * and initialize their XYZ positions to zero.
     */
    PlanetMoons() = default;

    /** Destructor.  Delete moon objects */
    virtual ~PlanetMoons();

    /**
     * @return pointer to a moon given the ID number.
     * @param id which moon?
     */
    inline TrailObject *moon(int id) { return Moon[id]; }

    /**
     * @return the name of a moon.
     * @param id which moon?
     */
    QString name(int id) const;

    /**
     * Convert the RA,Dec coordinates of each moon to Az,Alt
     *
     * @param LSTh pointer to the current local sidereal time
     * @param lat pointer to the geographic latitude
     */
    void EquatorialToHorizontal(const dms *LSTh, const dms *lat);

    /**
     * @short Find the positions of each Moon, relative to the planet.
     *
     * We use an XYZ coordinate system, centered on the planet,
     * where the X-axis corresponds to the planet's Equator,
     * the Y-Axis is parallel to the planet's Poles, and the
     * Z-axis points along the line joining the Earth and
     * the planet. Once the XYZ positions are known, this
     * function also computes the RA, Dec positions of each
     * Moon, and sets the inFront bool variable to indicate
     * whether the Moon is nearer to us than the planet or not
     * (this information is used to determine whether the
     * Moon should be drawn on top of the planet, or vice versa).
     *
     * @param num pointer to the KSNumbers object describing
     * the date/time at which to find the positions.
     * @param pla pointer to the planet object
     * @param sunptr pointer to the Sun object
     */
    virtual void findPosition(const KSNumbers *num, const KSPlanetBase *pla, const KSSun *sunptr) = 0;

    /**
     * @return true if the Moon is nearer to Earth than Saturn.
     * @param id which moon? 0=Mimas,1=Enceladus,2=Tethys,3=Dione,4=Rhea,5=Titan,6=Hyperion,7=Lapetus
     */
    inline bool inFront(int id) const { return InFront[id]; }

    /**
     * @return the X-coordinate in the planet-centered coord. system.
     * @param i which moon?
     */
    double x(int i) const { return XP[i]; }

    /**
     * @return the Y-coordinate in the planet-centered coord. system.
     * @param i which moon?
     */
    double y(int i) const { return YP[i]; }

    /**
     * @return the Z-coordinate in the Planet-centered coord. system.
     * @param i which moon?
     */
    double z(int i) const { return ZP[i]; }

    /** @return the number of moons around the planet */
    int nMoons() const { return Moon.size(); }

  protected:
    QVector<TrailObject *> Moon;
    QVector<bool> InFront;
    //the rectangular position, relative to the planet. X-axis is equator of the planet; units are planet Radius
    QVector<double> XP, YP, ZP;

  private:
    PlanetMoons(const PlanetMoons &);
    PlanetMoons &operator=(const PlanetMoons &);
};
