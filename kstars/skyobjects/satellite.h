/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#pragma once

#include "skyobject.h"

#include <QString>

class KSPopupMenu;

/**
 * @class Satellite
 * Represents an artificial satellites.
 *
 * @author Jérôme SONRIER
 * @version 0.1
 */
class Satellite : public SkyObject
{
    public:
        /** @short Constructor */
        Satellite(const QString &name, const QString &line1, const QString &line2);

        /**
         * @return a clone of this object
         * @note See SkyObject::clone()
         */
        Satellite *clone() const override;

        /** @short Destructor */
        virtual ~Satellite() override = default;

        /** @short Update satellite position */
        int updatePos();

        /**
         * @return True if the satellite is visible (above horizon, in the sunlight and sun at least 12° under horizon)
         */
        bool isVisible();

        /** @return True if the satellite is selected */
        bool selected();

        /** @short Select or not the satellite */
        void setSelected(bool selected);

        /** @return Satellite velocity in km/s */
        double velocity() const;

        /** @return Satellite altitude in km */
        double altitude() const;

        /** @return Satellite range from observer in km */
        double range() const;

        /** @return Satellite international designator */
        QString id() const;

        /** @return Satellite TLE */
        QString tle() const;

        /**
         * @brief sgp4ErrorString Get error string associated with sgp4 calculation failure
         * @param code error code as returned from sgp4() function
         * @return error string
         */
        QString sgp4ErrorString(int code);

        void initPopupMenu(KSPopupMenu *pmenu) override;

    private:
        /** @short Compute non time dependent parameters */
        void init();

        /** @short Compute satellite position */
        int sgp4(double tsince);

        /** @return Arcsine of the argument */
        double arcSin(double arg);

        /**
         * Provides the difference between UT (approximately the same as UTC)
         * and ET (now referred to as TDT).
         * This function is based on a least squares fit of data from 1950
         * to 1991 and will need to be updated periodically.
         */
        double deltaET(double year);

        /** @return arg1 mod arg2 */
        double Modulus(double arg1, double arg2);

        // TLE
        /// Satellite Number
        int m_number { 0 };
        /// Security Classification
        QChar m_class;
        /// International Designator
        QString m_id;
        /// Complete TLE
        QString m_tle;
        /// Epoch Year
        double m_epoch_year { 0 };
        /// Epoch (Day of the year and fractional portion of the day)
        double m_epoch { 0 };
        /// First Time Derivative of the Mean Motion
        double m_first_deriv { 0 };
        /// Second Time Derivative of Mean Motion
        double m_second_deriv { 0 };
        /// BSTAR drag term (decimal point assumed)
        double m_bstar { 0 };
        /// Ephemeris type
        int m_ephem_type { 0 };
        /// Element number
        int m_elem_number { 0 };
        /// Inclination [Radians]
        double m_inclination { 0 };
        /// Right Ascension of the Ascending Node [Radians]
        double m_ra { 0 };
        /// Eccentricity
        double m_eccentricity { 0 };
        /// Argument of Perigee [Radians]
        double m_arg_perigee { 0 };
        /// Mean Anomaly [Radians]
        double m_mean_anomaly { 0 };
        /// Mean Motion [Radians per minutes]
        double m_mean_motion { 0 };
        /// Revolution number at epoch [Revs]
        int m_nb_revolution { 0 };
        /// TLE epoch converted to julian date
        double m_tle_jd { 0 };

        // Satellite
        /// True if the satellite is visible
        bool m_is_visible { false };
        /// True if the satellite is in the shadow of the earth
        bool m_is_eclipsed { false };
        /// True if the satellite is selected
        bool m_is_selected { false };
        /// Satellite velocity in km/s
        double m_velocity { 0 };
        /// Satellite altitude in km
        double m_altitude { 0 };
        /// Satellite range from observer in km
        double m_range { 0 };

        // Near Earth
        bool isimp { false };
        double aycof { 0 }, con41 { 0 }, cc1 { 0 }, cc4 { 0 }, cc5 { 0 }, d2 { 0 }, d3 { 0 }, d4 { 0 };
        double delmo { 0 }, eta { 0 }, argpdot { 0 }, omgcof { 0 }, sinmao { 0 }, t { 0 }, t2cof { 0 };
        double t3cof { 0 }, t4cof { 0 }, t5cof { 0 }, x1mth2 { 0 }, x7thm1 { 0 }, mdot { 0 };
        double nodedot { 0 }, xlcof { 0 }, xmcof { 0 }, nodecf { 0 };

        // Deep Space
        int irez { 0 };
        double d2201 { 0 }, d2211 { 0 }, d3210 { 0 }, d3222 { 0 }, d4410 { 0 }, d4422 { 0 }, d5220 { 0 };
        double d5232 { 0 }, d5421 { 0 }, d5433 { 0 }, dedt { 0 }, del1 { 0 }, del2 { 0 }, del3 { 0 };
        double didt { 0 }, dmdt { 0 }, dnodt { 0 }, domdt { 0 }, e3 { 0 }, ee2 { 0 }, peo { 0 };
        double pgho { 0 }, pho { 0 }, pinco { 0 }, plo { 0 }, se2 { 0 }, se3 { 0 }, sgh2 { 0 };
        double sgh3 { 0 }, sgh4 { 0 }, sh2 { 0 }, sh3 { 0 }, si2 { 0 }, si3 { 0 }, sl2 { 0 }, sl3 { 0 };
        double sl4 { 0 }, gsto { 0 }, xfact { 0 }, xgh2 { 0 }, xgh3 { 0 }, xgh4 { 0 }, xh2 { 0 };
        double xh3 { 0 }, xi2 { 0 }, xi3 { 0 }, xl2 { 0 }, xl3 { 0 }, xl4 { 0 }, xlamo { 0 }, zmol { 0 };
        double zmos { 0 }, atime { 0 }, xli { 0 }, xni { 0 };

        char method;
};
