/*
    SPDX-FileCopyrightText: 2018 Valentin Boettcher <valentin@boettcher.cf (do not hesitate to contact)>
    matrix               : @hiro98@tchncs.de

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "ksplanetbase.h"

class KSSun;
class KSMoon;
class KSPlanet;

/**
 * @class KSEarthShadow
 * @short A class that manages the calculation of the
 * earths shadow (in moon distance) as a 'virtual' skyobject.
 * KSMoon is responsible for coordinating this object. While a
 * rather unusual measure, this method ensures that unnecessary
 * calculations are avoided.
 *
 * @author Valentin Boettcher
 * @version 1.0
 */
class KSEarthShadow : public KSPlanetBase
{
    public:
        /**
         * @param moon - an instance of KSMoon
         * @param sun - an instance of KSSun
         * @param earth - an instance of KSPlanet
         * @note The three parameters must be supplied to avoid initialization order
         * problems. This class may be generalized to any three bodies if it becomes
         * necessary in the future.
         * This class is relatively cheap, so it's save to create new instances instead
         * of reusing an existing one.
         */
        KSEarthShadow(const KSMoon *moon, const KSSun *sun, const KSPlanet * earth);

        /**
         * @brief The ECLIPSE_TYPE enum describes the quality of an eclipse.
         */
        enum ECLIPSE_TYPE
        {
            PARTIAL, FULL_PENUMBRA, FULL_UMBRA, NONE
        };

        /**
         * @short The earths shadow on the moon appears only at new moon
         * so calculating it on other occasions is rather pointless.
         * @return whether to update the shadow or not
         */
        bool shouldUpdate();


        /**
         * @brief isInEclipse - a slim version of getEclipseType()
         * @return Whether the earth shadow eclipses the moon.
         */
        bool isInEclipse();

        /**
         * @brief eclipse
         * @return The eclipse type. @see KSEarthShadow::ECLIPSE_TYPE
         */
        ECLIPSE_TYPE getEclipseType();

        bool findGeocentricPosition(const KSNumbers *, const KSPlanetBase * Earth = nullptr) override;

        /**
         * @short Update the Coordinates of the shadow.
         * In truth it finds the sun and calls KSEarthShadow::updateCoords(const KSSun *)
         */
        void updateCoords(const KSNumbers *num, bool includePlanets = true, const CachingDms *lat = nullptr,
                          const CachingDms *LST = nullptr, bool forceRecompute = false) override;

        /**
         * @short Update the RA/DEC of the shadow.
         */
        void updateCoords();

        /**
         * @short Updates umbra and penumbra radius from the positions of the three bodies.
         */
        void calculateShadowRadius();

        /**
         * @return The angular radius of the umbra.
         */
        double getUmbraAngSize() const
        {
            return m_umbra_ang;
        }

        /**
         * @return The angular radius of the penumbra.
         */
        double getPenumbraAngSize() const
        {
            return m_penumbra_ang;
        }

        /**
         * @brief angSize
         * @return the angular size (penumbra) in arc minutes
         */
        double findAngularSize() final override
        {
            calculateShadowRadius();
            return m_penumbra_ang;
        }

        // Some Compatibility Nonsense
        void findMagnitude(const KSNumbers *) override {} // Empty
        bool loadData() override
        {
            return true;
        }
        void findPhase() override {}

    private:
        double m_umbra_ang { 0 }; // Radius!
        double m_penumbra_ang { 0 }; // Radius!

        const KSSun* m_sun { nullptr };
        const KSMoon* m_moon { nullptr };
        const KSPlanet* m_earth { nullptr };

        void findSun();
        void findMoon();
        void findEarth();
};
