/*
    SPDX-FileCopyrightText: 2009 Jerome SONRIER <jsid@emor3j.fr.eu.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "satellite.h"

#include "ksplanetbase.h"
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif
#include "kstarsdata.h"
#include "kssun.h"
#include "Options.h"
#include "skymapcomposite.h"

#include <QDebug>

#include <cmath>
#include <typeinfo>

// Define some constants
//  WGS-72 constants
#define RADIUSEARTHKM 6378.135          // Earth radius (km)
#define XKE           0.07436691613317  // 60.0 / sqrt(RADIUSEARTHKM^3/MU)
#define J2            0.001082616       // The second gravitational zonal harmonic of the Earth
#define J4            -0.00000165597    // The fourth gravitational zonal harmonic of the Earth
#define J3OJ2         -2.34506972e-3    // J3 / J2

// Mathematical constants
#define TWOPI   6.2831853071795864769 // 2*PI
#define PIO2    1.5707963267948966192 // PI/2
#define X2O3    .66666666666666666667 // 2/3
#define DEG2RAD 1.745329251994330e-2  // Deg -> Rad

// Other constants
#define MINPD   1440                     // Minutes per day
#define MEANALT 0.84                     // Mean altitude (km)
#define SR      6.96000e5                // Solar radius - km (IAU 76)
#define AU      1.49597870691e8          // Astronomical unit - km (IAU 76)
#define XPDOTP  229.1831180523293        // 1440.0 / (2.0 * pi)
#define SS      1.0122292801892716288    // Parameter for the SGP4 density function
#define QZMS2T  1.8802791590152706439e-9 // (( 120.0 - 78.0) / RADIUSEARTHKM )^4
#define F       3.35281066474748e-3      // Flattening factor
#define MFACTOR 7.292115e-5

Satellite::Satellite(const QString &name, const QString &line1, const QString &line2)
{
    //m_name          = name;
    m_number      = line1.midRef(2, 5).toInt();
    m_class       = line1.at(7);
    m_id          = line1.mid(9, 8);
    m_epoch       = line1.midRef(18, 14).toDouble();
    m_first_deriv = line1.midRef(33, 10).toDouble() / (XPDOTP * MINPD);
    m_second_deriv =
        line1.midRef(44, 6).toDouble() * (1.0e-5 / pow(10.0, line1.midRef(51, 1).toDouble())) / (XPDOTP * MINPD * MINPD);
    m_bstar         = line1.midRef(53, 6).toDouble() * 1.0e-5 / pow(10.0, line1.midRef(60, 1).toDouble());
    m_ephem_type    = line1.midRef(62, 1).toInt();
    m_elem_number   = line1.midRef(64, 4).toInt();
    m_inclination   = line2.midRef(8, 8).toDouble() * DEG2RAD;
    m_ra            = line2.midRef(17, 8).toDouble() * DEG2RAD;
    m_eccentricity  = line2.midRef(26, 7).toDouble() * 1.0e-7;
    m_arg_perigee   = line2.midRef(34, 8).toDouble() * DEG2RAD;
    m_mean_anomaly  = line2.midRef(43, 8).toDouble() * DEG2RAD;
    m_mean_motion   = line2.midRef(52, 11).toDouble() * TWOPI / MINPD;
    m_nb_revolution = line2.midRef(63, 5).toInt();
    m_tle           = name + "\n" + line1 + "\n" + line2;

    setName(name);
    setName2(name);
    setLongName(name + " (" + m_id + ')');
    setType(SkyObject::SATELLITE);
    setMag(0.0);

    m_is_selected = Options::selectedSatellites().contains(name);

    // Convert TLE epoch to Julian date
    double day = modf(m_epoch * 1.e-3, &m_epoch_year) * 1.e3;
    if (m_epoch_year < 57.)
        m_epoch_year += 2000.;
    else
        m_epoch_year += 1900.;
    double year = m_epoch_year - 1.;
    long i      = year / 100;
    long A      = i;
    i           = A / 4;
    long B      = 2 - A + i;
    i           = 365.25 * year;
    i += 30.6001 * 14;
    m_tle_jd = i + 1720994.5 + B + day;

    init();
}

Satellite *Satellite::clone() const
{
    Q_ASSERT(typeid(this) == typeid(static_cast<const Satellite *>(this))); // Ensure we are not slicing a derived class
    return new Satellite(*this);
}

void Satellite::init()
{
    double ao, cosio, sinio, cosio2, omeosq, posq, rp, rteosq, eccsq, con42, cnodm, snodm, cosim, sinim, cosomm, sinomm,
           cc1sq, cc2, cc3, coef, coef1, cosio4, day, em, emsq, eeta, etasq, gam, inclm, nm, perige, pinvsq, psisq,
           qzms24, rtemsq, s1, s2, s3, s4, s5, s6, s7, sfour, ss1(0), ss2(0), ss3(0), ss4(0), ss5(0), ss6(0), ss7(0),
           sz1(0), sz2(0), sz3(0), sz11(0), sz12(0), sz13(0), sz21(0), sz22(0), sz23(0), sz31(0), sz32(0), sz33(0), tc,
           temp, temp1, temp2, temp3, tsi, xpidot, xhdot1, z1, z2, z3, z11, z12, z13, z21, z22, z23, z31, z32, z33, ak, d1,
           del, adel, po, ds70, ts70, tfrac, c1, thgr70, fk5r, c1p2p;
    //    double dndt;

    // Init near earth variables
    isimp   = false;
    aycof   = 0.;
    con41   = 0.;
    cc1     = 0.;
    cc4     = 0.;
    cc5     = 0.;
    d2      = 0.;
    d3      = 0.;
    d4      = 0.;
    delmo   = 0.;
    eta     = 0.;
    argpdot = 0.;
    omgcof  = 0.;
    sinmao  = 0.;
    t       = 0.;
    t2cof   = 0.;
    t3cof   = 0.;
    t4cof   = 0.;
    t5cof   = 0.;
    x1mth2  = 0.;
    x7thm1  = 0.;
    mdot    = 0.;
    nodedot = 0.;
    xlcof   = 0.;
    xmcof   = 0.;
    nodecf  = 0.;

    // Init deep space variables
    irez  = 0;
    d2201 = 0.;
    d2211 = 0.;
    d3210 = 0.;
    d3222 = 0.;
    d4410 = 0.;
    d4422 = 0.;
    d5220 = 0.;
    d5232 = 0.;
    d5421 = 0.;
    d5433 = 0.;
    dedt  = 0.;
    del1  = 0.;
    del2  = 0.;
    del3  = 0.;
    didt  = 0.;
    dmdt  = 0.;
    dnodt = 0.;
    domdt = 0.;
    e3    = 0.;
    ee2   = 0.;
    peo   = 0.;
    pgho  = 0.;
    pho   = 0.;
    pinco = 0.;
    plo   = 0.;
    se2   = 0.;
    se3   = 0.;
    sgh2  = 0.;
    sgh3  = 0.;
    sgh4  = 0.;
    sh2   = 0.;
    sh3   = 0.;
    si2   = 0.;
    si3   = 0.;
    sl2   = 0.;
    sl3   = 0.;
    sl4   = 0.;
    gsto  = 0.;
    xfact = 0.;
    xgh2  = 0.;
    xgh3  = 0.;
    xgh4  = 0.;
    xh2   = 0.;
    xh3   = 0.;
    xi2   = 0.;
    xi3   = 0.;
    xl2   = 0.;
    xl3   = 0.;
    xl4   = 0.;
    xlamo = 0.;
    zmol  = 0.;
    zmos  = 0.;
    atime = 0.;
    xli   = 0.;
    xni   = 0.;

    method = 'n';

    m_is_visible = false;

    // Divisor for divide by zero check on inclination
    const double temp4 = 1.5e-12;

    /*----- Initializes variables for sgp4 -----*/
    // Calculate auxiliary epoch quantities
    eccsq  = m_eccentricity * m_eccentricity;
    omeosq = 1.0 - eccsq;
    rteosq = sqrt(omeosq);
    cosio  = cos(m_inclination);
    cosio2 = cosio * cosio;

    // Un-kozai the mean motion
    ak            = pow(XKE / m_mean_motion, X2O3);
    d1            = 0.75 * J2 * (3.0 * cosio2 - 1.0) / (rteosq * omeosq);
    del           = d1 / (ak * ak);
    adel          = ak * (1.0 - del * del - del * (1.0 / 3.0 + 134.0 * del * del / 81.0));
    del           = d1 / (adel * adel);
    m_mean_motion = m_mean_motion / (1.0 + del);

    ao     = pow(XKE / m_mean_motion, X2O3);
    sinio  = sin(m_inclination);
    po     = ao * omeosq;
    con42  = 1.0 - 5.0 * cosio2;
    con41  = -con42 - (2.0 * cosio2);
    posq   = po * po;
    rp     = ao * (1.0 - m_eccentricity);
    method = 'n';

    // Find sidereal time
    ts70  = m_tle_jd - 2433281.5 - 7305.0;
    ds70  = floor(ts70 + 1.0e-8);
    tfrac = ts70 - ds70;
    // find greenwich location at epoch
    c1     = 1.72027916940703639e-2;
    thgr70 = 1.7321343856509374;
    fk5r   = 5.07551419432269442e-15;
    c1p2p  = c1 + TWOPI;
    gsto   = fmod(thgr70 + c1 * ds70 + c1p2p * tfrac + ts70 * ts70 * fk5r, TWOPI);
    if (gsto < 0.0)
        gsto = gsto + TWOPI;

    if ((omeosq >= 0.0) || (m_mean_motion >= 0.0))
    {
        if (rp < (220.0 / RADIUSEARTHKM + 1.0))
            isimp = true;
        sfour  = SS;
        qzms24 = QZMS2T;
        perige = (rp - 1.0) * RADIUSEARTHKM;

        // For perigees below 156 km, s and qoms2t are altered
        if (perige < 156.0)
        {
            sfour = perige - 78.0;
            if (perige < 98.0)
                sfour = 20.0;
            qzms24 = pow(((120.0 - sfour) / RADIUSEARTHKM), 4.0);
            sfour  = sfour / RADIUSEARTHKM + 1.0;
        }
        pinvsq = 1.0 / posq;

        tsi   = 1.0 / (ao - sfour);
        eta   = ao * m_eccentricity * tsi;
        etasq = eta * eta;
        eeta  = m_eccentricity * eta;
        psisq = fabs(1.0 - etasq);
        coef  = qzms24 * pow(tsi, 4.0);
        coef1 = coef / pow(psisq, 3.5);
        cc2   = coef1 * m_mean_motion *
                (ao * (1.0 + 1.5 * etasq + eeta * (4.0 + etasq)) +
                 0.375 * J2 * tsi / psisq * con41 * (8.0 + 3.0 * etasq * (8.0 + etasq)));
        cc1 = m_bstar * cc2;
        cc3 = 0.0;
        if (m_eccentricity > 1.0e-4)
            cc3 = -2.0 * coef * tsi * J3OJ2 * m_mean_motion * sinio / m_eccentricity;
        x1mth2 = 1.0 - cosio2;
        cc4    = 2.0 * m_mean_motion * coef1 * ao * omeosq *
                 (eta * (2.0 + 0.5 * etasq) + m_eccentricity * (0.5 + 2.0 * etasq) -
                  J2 * tsi / (ao * psisq) *
                  (-3.0 * con41 * (1.0 - 2.0 * eeta + etasq * (1.5 - 0.5 * eeta)) +
                   0.75 * x1mth2 * (2.0 * etasq - eeta * (1.0 + etasq)) * cos(2.0 * m_arg_perigee)));
        cc5 = 2.0 * coef1 * ao * omeosq * (1.0 + 2.75 * (etasq + eeta) + eeta * etasq);

        cosio4 = cosio2 * cosio2;
        temp1  = 1.5 * J2 * pinvsq * m_mean_motion;
        temp2  = 0.5 * temp1 * J2 * pinvsq;
        temp3  = -0.46875 * J4 * pinvsq * pinvsq * m_mean_motion;
        mdot   = m_mean_motion + 0.5 * temp1 * rteosq * con41 +
                 0.0625 * temp2 * rteosq * (13.0 - 78.0 * cosio2 + 137.0 * cosio4);
        argpdot = -0.5 * temp1 * con42 + 0.0625 * temp2 * (7.0 - 114.0 * cosio2 + 395.0 * cosio4) +
                  temp3 * (3.0 - 36.0 * cosio2 + 49.0 * cosio4);
        xhdot1  = -temp1 * cosio;
        nodedot = xhdot1 + (0.5 * temp2 * (4.0 - 19.0 * cosio2) + 2.0 * temp3 * (3.0 - 7.0 * cosio2)) * cosio;
        xpidot  = argpdot + nodedot;
        omgcof  = m_bstar * cc3 * cos(m_arg_perigee);
        xmcof   = 0.0;
        if (m_eccentricity > 1.0e-4)
            xmcof = -X2O3 * coef * m_bstar / eeta;
        nodecf = 3.5 * omeosq * xhdot1 * cc1;
        t2cof  = 1.5 * cc1;
        // Do not divide by zero
        if (fabs(1.0 + cosio) > 1.5e-12)
            xlcof = -0.25 * J3OJ2 * sinio * (3.0 + 5.0 * cosio) / (1.0 + cosio);
        else
            xlcof = -0.25 * J3OJ2 * sinio * (3.0 + 5.0 * cosio) / temp4;
        aycof  = -0.5 * J3OJ2 * sinio;
        delmo  = pow((1.0 + eta * cos(m_mean_anomaly)), 3);
        sinmao = sin(m_mean_anomaly);
        x7thm1 = 7.0 * cosio2 - 1.0;

        // Deep space initialization
        if ((TWOPI / m_mean_motion) >= 225.0)
        {
            method = 'd';
            isimp  = true;
            tc     = 0.0;
            inclm  = m_inclination;

            // Init deep space common variables
            // Define some constants
            const double zes    = 0.01675;
            const double zel    = 0.05490;
            const double c1ss   = 2.9864797e-6;
            const double c1l    = 4.7968065e-7;
            const double zsinis = 0.39785416;
            const double zcosis = 0.91744867;
            const double zcosgs = 0.1945905;
            const double zsings = -0.98088458;

            int lsflg;
            double a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, betasq, cc, ctem, stem, x1, x2, x3, x4, x5, x6, x7, x8,
                   xnodce, xnoi, zcosg, zcosgl, zcosh, zcoshl, zcosi, zcosil, zsing, zsingl, zsinh, zsinhl, zsini, zsinil,
                   zx, zy;

            nm     = m_mean_motion;
            em     = m_eccentricity;
            snodm  = sin(m_ra);
            cnodm  = cos(m_ra);
            sinomm = sin(m_arg_perigee);
            cosomm = cos(m_arg_perigee);
            sinim  = sin(m_inclination);
            cosim  = cos(m_inclination);
            emsq   = em * em;
            betasq = 1.0 - emsq;
            rtemsq = sqrt(betasq);

            // Initialize lunar solar terms
            peo    = 0.0;
            pinco  = 0.0;
            plo    = 0.0;
            pgho   = 0.0;
            pho    = 0.0;
            day    = m_tle_jd - 2433281.5 + 18261.5 + tc / 1440.0;
            xnodce = fmod(4.5236020 - 9.2422029e-4 * day, TWOPI);
            stem   = sin(xnodce);
            ctem   = cos(xnodce);
            zcosil = 0.91375164 - 0.03568096 * ctem;
            zsinil = sqrt(1.0 - zcosil * zcosil);
            zsinhl = 0.089683511 * stem / zsinil;
            zcoshl = sqrt(1.0 - zsinhl * zsinhl);
            gam    = 5.8351514 + 0.0019443680 * day;
            zx     = 0.39785416 * stem / zsinil;
            zy     = zcoshl * ctem + 0.91744867 * zsinhl * stem;
            zx     = atan2(zx, zy);
            zx     = gam + zx - xnodce;
            zcosgl = cos(zx);
            zsingl = sin(zx);

            // Solar terms
            zcosg = zcosgs;
            zsing = zsings;
            zcosi = zcosis;
            zsini = zsinis;
            zcosh = cnodm;
            zsinh = snodm;
            cc    = c1ss;
            xnoi  = 1.0 / nm;

            for (lsflg = 1; lsflg <= 2; ++lsflg)
            {
                a1  = zcosg * zcosh + zsing * zcosi * zsinh;
                a3  = -zsing * zcosh + zcosg * zcosi * zsinh;
                a7  = -zcosg * zsinh + zsing * zcosi * zcosh;
                a8  = zsing * zsini;
                a9  = zsing * zsinh + zcosg * zcosi * zcosh;
                a10 = zcosg * zsini;
                a2  = cosim * a7 + sinim * a8;
                a4  = cosim * a9 + sinim * a10;
                a5  = -sinim * a7 + cosim * a8;
                a6  = -sinim * a9 + cosim * a10;

                x1 = a1 * cosomm + a2 * sinomm;
                x2 = a3 * cosomm + a4 * sinomm;
                x3 = -a1 * sinomm + a2 * cosomm;
                x4 = -a3 * sinomm + a4 * cosomm;
                x5 = a5 * sinomm;
                x6 = a6 * sinomm;
                x7 = a5 * cosomm;
                x8 = a6 * cosomm;

                z31 = 12.0 * x1 * x1 - 3.0 * x3 * x3;
                z32 = 24.0 * x1 * x2 - 6.0 * x3 * x4;
                z33 = 12.0 * x2 * x2 - 3.0 * x4 * x4;
                z1  = 3.0 * (a1 * a1 + a2 * a2) + z31 * emsq;
                z2  = 6.0 * (a1 * a3 + a2 * a4) + z32 * emsq;
                z3  = 3.0 * (a3 * a3 + a4 * a4) + z33 * emsq;
                z11 = -6.0 * a1 * a5 + emsq * (-24.0 * x1 * x7 - 6.0 * x3 * x5);
                z12 = -6.0 * (a1 * a6 + a3 * a5) + emsq * (-24.0 * (x2 * x7 + x1 * x8) - 6.0 * (x3 * x6 + x4 * x5));
                z13 = -6.0 * a3 * a6 + emsq * (-24.0 * x2 * x8 - 6.0 * x4 * x6);
                z21 = 6.0 * a2 * a5 + emsq * (24.0 * x1 * x5 - 6.0 * x3 * x7);
                z22 = 6.0 * (a4 * a5 + a2 * a6) + emsq * (24.0 * (x2 * x5 + x1 * x6) - 6.0 * (x4 * x7 + x3 * x8));
                z23 = 6.0 * a4 * a6 + emsq * (24.0 * x2 * x6 - 6.0 * x4 * x8);
                z1  = z1 + z1 + betasq * z31;
                z2  = z2 + z2 + betasq * z32;
                z3  = z3 + z3 + betasq * z33;
                s3  = cc * xnoi;
                s2  = -0.5 * s3 / rtemsq;
                s4  = s3 * rtemsq;
                s1  = -15.0 * em * s4;
                s5  = x1 * x3 + x2 * x4;
                s6  = x2 * x3 + x1 * x4;
                s7  = x2 * x4 - x1 * x3;

                // Lunar terms
                if (lsflg == 1)
                {
                    ss1   = s1;
                    ss2   = s2;
                    ss3   = s3;
                    ss4   = s4;
                    ss5   = s5;
                    ss6   = s6;
                    ss7   = s7;
                    sz1   = z1;
                    sz2   = z2;
                    sz3   = z3;
                    sz11  = z11;
                    sz12  = z12;
                    sz13  = z13;
                    sz21  = z21;
                    sz22  = z22;
                    sz23  = z23;
                    sz31  = z31;
                    sz32  = z32;
                    sz33  = z33;
                    zcosg = zcosgl;
                    zsing = zsingl;
                    zcosi = zcosil;
                    zsini = zsinil;
                    zcosh = zcoshl * cnodm + zsinhl * snodm;
                    zsinh = snodm * zcoshl - cnodm * zsinhl;
                    cc    = c1l;
                }
            }

            zmol = fmod(4.7199672 + 0.22997150 * day - gam, TWOPI);
            zmos = fmod(6.2565837 + 0.017201977 * day, TWOPI);

            //  Solar terms
            se2  = 2.0 * ss1 * ss6;
            se3  = 2.0 * ss1 * ss7;
            si2  = 2.0 * ss2 * sz12;
            si3  = 2.0 * ss2 * (sz13 - sz11);
            sl2  = -2.0 * ss3 * sz2;
            sl3  = -2.0 * ss3 * (sz3 - sz1);
            sl4  = -2.0 * ss3 * (-21.0 - 9.0 * emsq) * zes;
            sgh2 = 2.0 * ss4 * sz32;
            sgh3 = 2.0 * ss4 * (sz33 - sz31);
            sgh4 = -18.0 * ss4 * zes;
            sh2  = -2.0 * ss2 * sz22;
            sh3  = -2.0 * ss2 * (sz23 - sz21);

            // Lunar terms
            ee2  = 2.0 * s1 * s6;
            e3   = 2.0 * s1 * s7;
            xi2  = 2.0 * s2 * z12;
            xi3  = 2.0 * s2 * (z13 - z11);
            xl2  = -2.0 * s3 * z2;
            xl3  = -2.0 * s3 * (z3 - z1);
            xl4  = -2.0 * s3 * (-21.0 - 9.0 * emsq) * zel;
            xgh2 = 2.0 * s4 * z32;
            xgh3 = 2.0 * s4 * (z33 - z31);
            xgh4 = -18.0 * s4 * zel;
            xh2  = -2.0 * s2 * z22;
            xh3  = -2.0 * s2 * (z23 - z21);

            // Apply deep space long period periodic contributions to the mean elements
            //            double f2, f3, sinzf,  zf, zm;
            double ses, sghl, sghs, shll, shs, sis, sls;

            // Define some constants
            const double zns = 1.19459e-5;
            const double znl = 1.5835218e-4;

            // Calculate time varying periodics
            // These results are never used, should we remove them?
            //            zm    = zmos;
            //            zf    = zm + 2.0 * zes * sin(zm);
            //            sinzf = sin(zf);
            //            f2    = 0.5 * sinzf * sinzf - 0.25;
            //            f3    = -0.5 * sinzf * cos(zf);
            //            ses   = se2 * f2 + se3 * f3;
            //            sis   = si2 * f2 + si3 * f3;
            //            sls   = sl2 * f2 + sl3 * f3 + sl4 * sinzf;
            //            sghs  = sgh2 * f2 + sgh3 * f3 + sgh4 * sinzf;
            //            shs   = sh2 * f2 + sh3 * f3;
            //            zm    = zmol;

            //            zf    = zm + 2.0 * zel * sin(zm);
            //            sinzf = sin(zf);
            //            f2    = 0.5 * sinzf * sinzf - 0.25;
            //            f3    = -0.5 * sinzf * cos(zf);
            //            sghl  = xgh2 * f2 + xgh3 * f3 + xgh4 * sinzf;
            //            shll  = xh2 * f2 + xh3 * f3;

            // Deep space contributions to mean motion dot due to geopotential resonance with half day and one day orbits
            double ainv2, aonv = 0.0, cosisq, eoc, f220, f221, f311, f321, f322, f330, f441, f442, f522, f523, f542,
                          f543, g200, g201, g211, g300, g310, g322, g410, g422, g520, g521, g532, g533, sgs, sini2,
                          temp, temp1, theta, xno2, emsqo;
            //            double emo;

            // Define some constant
            const double q22    = 1.7891679e-6;
            const double q31    = 2.1460748e-6;
            const double q33    = 2.2123015e-7;
            const double root22 = 1.7891679e-6;
            const double root44 = 7.3636953e-9;
            const double root54 = 2.1765803e-9;
            const double rptim  = 4.37526908801129966e-3; // this equates to 7.29211514668855e-5 rad/sec
            const double root32 = 3.7393792e-7;
            const double root52 = 1.1428639e-7;

            // Deep space initialization
            irez = 0;
            if ((nm < 0.0052359877) && (nm > 0.0034906585))
                irez = 1;
            if ((nm >= 8.26e-3) && (nm <= 9.24e-3) && (em >= 0.5))
                irez = 2;

            // Solar terms
            ses  = ss1 * zns * ss5;
            sis  = ss2 * zns * (sz11 + sz13);
            sls  = -zns * ss3 * (sz1 + sz3 - 14.0 - 6.0 * emsq);
            sghs = ss4 * zns * (sz31 + sz33 - 6.0);
            shs  = -zns * ss2 * (sz21 + sz23);
            if ((inclm < 5.2359877e-2) || (inclm > M_PI - 5.2359877e-2))
                shs = 0.0;
            if (sinim != 0.0)
                shs = shs / sinim;
            sgs = sghs - cosim * shs;

            // Lunar terms
            dedt = ses + s1 * znl * s5;
            didt = sis + s2 * znl * (z11 + z13);
            dmdt = sls - znl * s3 * (z1 + z3 - 14.0 - 6.0 * emsq);
            sghl = s4 * znl * (z31 + z33 - 6.0);
            shll = -znl * s2 * (z21 + z23);
            if ((inclm < 5.2359877e-2) || (inclm > M_PI - 5.2359877e-2))
                shll = 0.0;
            domdt = sgs + sghl;
            dnodt = shs;
            if (sinim != 0.0)
            {
                domdt = domdt - cosim / sinim * shll;
                dnodt = dnodt + shll / sinim;
            }

            // Calculate deep space resonance effects
            // Value never used
            //            dndt  = 0.0;
            theta = fmod(gsto + tc * rptim, TWOPI);

            // Initialize the resonance terms
            if (irez != 0)
            {
                aonv = pow(nm / XKE, X2O3);

                // Geopotential resonance for 12 hour orbits
                if (irez == 2)
                {
                    cosisq = cosim * cosim;
                    // Value never used
                    //                    emo    = em;
                    em     = m_eccentricity;
                    emsqo  = emsq;
                    emsq   = eccsq;
                    eoc    = em * emsq;
                    g201   = -0.306 - (em - 0.64) * 0.440;

                    if (em <= 0.65)
                    {
                        g211 = 3.616 - 13.2470 * em + 16.2900 * emsq;
                        g310 = -19.302 + 117.3900 * em - 228.4190 * emsq + 156.5910 * eoc;
                        g322 = -18.9068 + 109.7927 * em - 214.6334 * emsq + 146.5816 * eoc;
                        g410 = -41.122 + 242.6940 * em - 471.0940 * emsq + 313.9530 * eoc;
                        g422 = -146.407 + 841.8800 * em - 1629.014 * emsq + 1083.4350 * eoc;
                        g520 = -532.114 + 3017.977 * em - 5740.032 * emsq + 3708.2760 * eoc;
                    }
                    else
                    {
                        g211 = -72.099 + 331.819 * em - 508.738 * emsq + 266.724 * eoc;
                        g310 = -346.844 + 1582.851 * em - 2415.925 * emsq + 1246.113 * eoc;
                        g322 = -342.585 + 1554.908 * em - 2366.899 * emsq + 1215.972 * eoc;
                        g410 = -1052.797 + 4758.686 * em - 7193.992 * emsq + 3651.957 * eoc;
                        g422 = -3581.690 + 16178.110 * em - 24462.770 * emsq + 12422.520 * eoc;
                        if (em > 0.715)
                            g520 = -5149.66 + 29936.92 * em - 54087.36 * emsq + 31324.56 * eoc;
                        else
                            g520 = 1464.74 - 4664.75 * em + 3763.64 * emsq;
                    }
                    if (em < 0.7)
                    {
                        g533 = -919.22770 + 4988.6100 * em - 9064.7700 * emsq + 5542.21 * eoc;
                        g521 = -822.71072 + 4568.6173 * em - 8491.4146 * emsq + 5337.524 * eoc;
                        g532 = -853.66600 + 4690.2500 * em - 8624.7700 * emsq + 5341.4 * eoc;
                    }
                    else
                    {
                        g533 = -37995.780 + 161616.52 * em - 229838.20 * emsq + 109377.94 * eoc;
                        g521 = -51752.104 + 218913.95 * em - 309468.16 * emsq + 146349.42 * eoc;
                        g532 = -40023.880 + 170470.89 * em - 242699.48 * emsq + 115605.82 * eoc;
                    }

                    sini2 = sinim * sinim;
                    f220  = 0.75 * (1.0 + 2.0 * cosim + cosisq);
                    f221  = 1.5 * sini2;
                    f321  = 1.875 * sinim * (1.0 - 2.0 * cosim - 3.0 * cosisq);
                    f322  = -1.875 * sinim * (1.0 + 2.0 * cosim - 3.0 * cosisq);
                    f441  = 35.0 * sini2 * f220;
                    f442  = 39.3750 * sini2 * sini2;
                    f522 =
                        9.84375 * sinim *
                        (sini2 * (1.0 - 2.0 * cosim - 5.0 * cosisq) + 0.33333333 * (-2.0 + 4.0 * cosim + 6.0 * cosisq));
                    f523  = sinim * (4.92187512 * sini2 * (-2.0 - 4.0 * cosim + 10.0 * cosisq) +
                                     6.56250012 * (1.0 + 2.0 * cosim - 3.0 * cosisq));
                    f542  = 29.53125 * sinim * (2.0 - 8.0 * cosim + cosisq * (-12.0 + 8.0 * cosim + 10.0 * cosisq));
                    f543  = 29.53125 * sinim * (-2.0 - 8.0 * cosim + cosisq * (12.0 + 8.0 * cosim - 10.0 * cosisq));
                    xno2  = nm * nm;
                    ainv2 = aonv * aonv;
                    temp1 = 3.0 * xno2 * ainv2;
                    temp  = temp1 * root22;
                    d2201 = temp * f220 * g201;
                    d2211 = temp * f221 * g211;
                    temp1 = temp1 * aonv;
                    temp  = temp1 * root32;
                    d3210 = temp * f321 * g310;
                    d3222 = temp * f322 * g322;
                    temp1 = temp1 * aonv;
                    temp  = 2.0 * temp1 * root44;
                    d4410 = temp * f441 * g410;
                    d4422 = temp * f442 * g422;
                    temp1 = temp1 * aonv;
                    temp  = temp1 * root52;
                    d5220 = temp * f522 * g520;
                    d5232 = temp * f523 * g532;
                    temp  = 2.0 * temp1 * root54;
                    d5421 = temp * f542 * g521;
                    d5433 = temp * f543 * g533;
                    xlamo = fmod(m_mean_anomaly + m_ra + m_ra - theta - theta, TWOPI);
                    xfact = mdot + dmdt + 2.0 * (nodedot + dnodt - rptim) - m_mean_motion;
                    // Value never used
                    //                    em    = emo;
                    emsq  = emsqo;
                }

                if (irez == 1)
                {
                    g200  = 1.0 + emsq * (-2.5 + 0.8125 * emsq);
                    g310  = 1.0 + 2.0 * emsq;
                    g300  = 1.0 + emsq * (-6.0 + 6.60937 * emsq);
                    f220  = 0.75 * (1.0 + cosim) * (1.0 + cosim);
                    f311  = 0.9375 * sinim * sinim * (1.0 + 3.0 * cosim) - 0.75 * (1.0 + cosim);
                    f330  = 1.0 + cosim;
                    f330  = 1.875 * f330 * f330 * f330;
                    del1  = 3.0 * nm * nm * aonv * aonv;
                    del2  = 2.0 * del1 * f220 * g200 * q22;
                    del3  = 3.0 * del1 * f330 * g300 * q33 * aonv;
                    del1  = del1 * f311 * g310 * q31 * aonv;
                    xlamo = fmod(m_mean_anomaly + m_ra + m_arg_perigee - theta, TWOPI);
                    xfact = mdot + xpidot - rptim + dmdt + domdt + dnodt - m_mean_motion;
                }

                xli   = xlamo;
                xni   = m_mean_motion;
                atime = 0.0;
                // Value never used
                //                nm    = m_mean_motion + dndt;
            }
        }

        // Set variables if not deep space
        if (!isimp)
        {
            cc1sq = cc1 * cc1;
            d2    = 4.0 * ao * tsi * cc1sq;
            temp  = d2 * tsi * cc1 / 3.0;
            d3    = (17.0 * ao + sfour) * temp;
            d4    = 0.5 * temp * ao * tsi * (221.0 * ao + 31.0 * sfour) * cc1;
            t3cof = d2 + 2.0 * cc1sq;
            t4cof = 0.25 * (3.0 * d3 + cc1 * (12.0 * d2 + 10.0 * cc1sq));
            t5cof = 0.2 * (3.0 * d4 + 12.0 * cc1 * d3 + 6.0 * d2 * d2 + 15.0 * cc1sq * (2.0 * d2 + cc1sq));
        }
    }
}

int Satellite::updatePos()
{
    KStarsData *data = KStarsData::Instance();
    return sgp4((data->clock()->utc().djd() - m_tle_jd) * MINPD);
}

int Satellite::sgp4(double tsince)
{
    KStarsData *data = KStarsData::Instance();

    int ktr;
    double am, axnl, aynl, betal, cosim, cnod, cos2u, coseo1 = 0, cosi, cosip, cosisq, cossu, cosu, delm, delomg, em,
                                                      ecose, el2, eo1, ep, esine, argpm, argpp, argpdf, pl,
                                                      mrt = 0.0, mvt, rdotl, rl, rvdot, rvdotl, sinim, dndt, sin2u, sineo1 = 0, sini, sinip, sinsu, sinu, snod, su, t2,
                                                      t3, t4, tem5, temp, temp1, temp2, tempa, tempe, templ, u, ux, uy, uz, vx, vy, vz, inclm, mm, nm, nodem, xinc,
                                                      xincp, xl, xlm, mp, xmdf, xmx, xmy, nodedf, xnode, nodep, tc, sat_posx, sat_posy, sat_posz, sat_posw, sat_velx,
                                                      sat_vely, sat_velz, sinlat, obs_posx, obs_posy, obs_posz, obs_posw, /*obs_velx, obs_vely, obs_velz,*/
                                                      coslat, thetageo, sintheta, costheta, c, sq, achcp, vkmpersec;
    //    double emsq;

    const double temp4 = 1.5e-12;

    double jul_utc = data->clock()->utc().djd();

    vkmpersec = RADIUSEARTHKM * XKE / 60.0;

    // Update for secular gravity and atmospheric drag
    xmdf   = m_mean_anomaly + mdot * tsince;
    argpdf = m_arg_perigee + argpdot * tsince;
    nodedf = m_ra + nodedot * tsince;
    argpm  = argpdf;
    mm     = xmdf;
    t2     = tsince * tsince;
    nodem  = nodedf + nodecf * t2;
    tempa  = 1.0 - cc1 * tsince;
    tempe  = m_bstar * cc4 * tsince;
    templ  = t2cof * t2;

    if (!isimp)
    {
        delomg = omgcof * tsince;
        delm   = xmcof * (pow((1.0 + eta * cos(xmdf)), 3) - delmo);
        temp   = delomg + delm;
        mm     = xmdf + temp;
        argpm  = argpdf - temp;
        t3     = t2 * tsince;
        t4     = t3 * tsince;
        tempa  = tempa - d2 * t2 - d3 * t3 - d4 * t4;
        tempe  = tempe + m_bstar * cc5 * (sin(mm) - sinmao);
        templ  = templ + t3cof * t3 + t4 * (t4cof + tsince * t5cof);
    }

    nm    = m_mean_motion;
    em    = m_eccentricity;
    inclm = m_inclination;

    if (method == 'd')
    {
        tc = tsince;
        // Deep space contributions to mean elements for perturbing third body
        int iretn;
        double delt, ft, theta, x2li, x2omi, xl, xldot, xnddt, xndt, xomi;

        // Define some constants
        const double fasx2 = 0.13130908;
        const double fasx4 = 2.8843198;
        const double fasx6 = 0.37448087;
        const double g22   = 5.7686396;
        const double g32   = 0.95240898;
        const double g44   = 1.8014998;
        const double g52   = 1.0508330;
        const double g54   = 4.4108898;
        const double rptim = 4.37526908801129966e-3; // this equates to 7.29211514668855e-5 rad/sec
        const double step  = 720.0;
        const double step2 = step * step / 2;

        // Calculate deep space resonance effects
        // Value never used
        //        dndt  = 0.0;
        theta = fmod(gsto + tc * rptim, TWOPI);
        em    = em + dedt * tsince;

        inclm = inclm + didt * tsince;
        argpm = argpm + domdt * tsince;
        nodem = nodem + dnodt * tsince;
        mm    = mm + dmdt * tsince;

        // Update resonances : numerical (euler-maclaurin) integration
        ft = 0.0;
        if (irez != 0)
        {
            if ((atime == 0.0) || (tsince * atime <= 0.0) || (fabs(tsince) < fabs(atime)))
            {
                atime = 0.0;
                xni   = m_mean_motion;
                xli   = xlamo;
            }

            if (tsince > 0.0)
                delt = step;
            else
                delt = -step;

            iretn = 381; // added for do loop

            while (iretn == 381)
            {
                // Near - synchronous resonance terms
                if (irez != 2)
                {
                    xndt  = del1 * sin(xli - fasx2) + del2 * sin(2.0 * (xli - fasx4)) + del3 * sin(3.0 * (xli - fasx6));
                    xldot = xni + xfact;
                    xnddt = del1 * cos(xli - fasx2) + 2.0 * del2 * cos(2.0 * (xli - fasx4)) +
                            3.0 * del3 * cos(3.0 * (xli - fasx6));
                    xnddt = xnddt * xldot;
                }
                else
                {
                    // Near - half-day resonance terms
                    xomi  = m_arg_perigee + argpdot * atime;
                    x2omi = xomi + xomi;
                    x2li  = xli + xli;
                    xndt  = d2201 * sin(x2omi + xli - g22) + d2211 * sin(xli - g22) + d3210 * sin(xomi + xli - g32) +
                            d3222 * sin(-xomi + xli - g32) + d4410 * sin(x2omi + x2li - g44) + d4422 * sin(x2li - g44) +
                            d5220 * sin(xomi + xli - g52) + d5232 * sin(-xomi + xli - g52) +
                            d5421 * sin(xomi + x2li - g54) + d5433 * sin(-xomi + x2li - g54);
                    xldot = xni + xfact;
                    xnddt = d2201 * cos(x2omi + xli - g22) + d2211 * cos(xli - g22) + d3210 * cos(xomi + xli - g32) +
                            d3222 * cos(-xomi + xli - g32) + d5220 * cos(xomi + xli - g52) +
                            d5232 * cos(-xomi + xli - g52) +
                            2.0 * (d4410 * cos(x2omi + x2li - g44) + d4422 * cos(x2li - g44) +
                                   d5421 * cos(xomi + x2li - g54) + d5433 * cos(-xomi + x2li - g54));
                    xnddt = xnddt * xldot;
                }

                if (fabs(tsince - atime) >= step)
                {
                    iretn = 381;
                }
                else
                {
                    ft    = tsince - atime;
                    iretn = 0;
                }

                if (iretn == 381)
                {
                    xli   = xli + xldot * delt + xndt * step2;
                    xni   = xni + xndt * delt + xnddt * step2;
                    atime = atime + delt;
                }
            }

            nm = xni + xndt * ft + xnddt * ft * ft * 0.5;
            xl = xli + xldot * ft + xndt * ft * ft * 0.5;

            if (irez != 1)
            {
                mm   = xl - 2.0 * nodem + 2.0 * theta;
                dndt = nm - m_mean_motion;
            }
            else
            {
                mm   = xl - nodem - argpm + theta;
                dndt = nm - m_mean_motion;
            }

            nm = m_mean_motion + dndt;
        }
    }

    if (nm <= 0.0)
    {
        qDebug() << "Mean motion less than 0.0";
        return (2);
    }

    am = pow((XKE / nm), X2O3) * tempa * tempa;
    nm = XKE / pow(am, 1.5);
    em = em - tempe;

    if ((em >= 1.0) || (em < -0.001))
    {
        qDebug() << "Eccentricity >= 1.0 or < -0.001";
        return (1);
    }

    if (em < 1.0e-6)
        em = 1.0e-6;

    mm   = mm + m_mean_motion * templ;
    xlm  = mm + argpm + nodem;
    // Value never used
    //    emsq = em * em;
    // Value never used
    //    temp = 1.0 - emsq;

    nodem = fmod(nodem, TWOPI);
    argpm = fmod(argpm, TWOPI);
    xlm   = fmod(xlm, TWOPI);
    mm    = fmod(xlm - argpm - nodem, TWOPI);

    // Compute extra mean quantities
    sinim = sin(inclm);
    cosim = cos(inclm);

    // Add lunar-solar periodics
    ep    = em;
    xincp = inclm;
    argpp = argpm;
    nodep = nodem;
    mp    = mm;
    sinip = sinim;
    cosip = cosim;
    if (method == 'd')
    {
        double alfdp, betdp, cosip, cosop, dalf, dbet, dls, f2, f3, pe, pgh, ph, pinc, pl, sel, ses, sghl, sghs, shll,
               shs, sil, sinip, sinop, sinzf, sis, sll, sls, xls, xnoh, zf, zm;

        // Define some constants
        const double zns = 1.19459e-5;
        const double zes = 0.01675;
        const double znl = 1.5835218e-4;
        const double zel = 0.05490;

        // Calculate time varying periodics
        zm    = zmos + zns * tsince;
        zf    = zm + 2.0 * zes * sin(zm);
        sinzf = sin(zf);
        f2    = 0.5 * sinzf * sinzf - 0.25;
        f3    = -0.5 * sinzf * cos(zf);
        ses   = se2 * f2 + se3 * f3;
        sis   = si2 * f2 + si3 * f3;
        sls   = sl2 * f2 + sl3 * f3 + sl4 * sinzf;
        sghs  = sgh2 * f2 + sgh3 * f3 + sgh4 * sinzf;
        shs   = sh2 * f2 + sh3 * f3;
        zm    = zmol + znl * tsince;

        zf    = zm + 2.0 * zel * sin(zm);
        sinzf = sin(zf);
        f2    = 0.5 * sinzf * sinzf - 0.25;
        f3    = -0.5 * sinzf * cos(zf);
        sel   = ee2 * f2 + e3 * f3;
        sil   = xi2 * f2 + xi3 * f3;
        sll   = xl2 * f2 + xl3 * f3 + xl4 * sinzf;
        sghl  = xgh2 * f2 + xgh3 * f3 + xgh4 * sinzf;
        shll  = xh2 * f2 + xh3 * f3;
        pe    = ses + sel;
        pinc  = sis + sil;
        pl    = sls + sll;
        pgh   = sghs + sghl;
        ph    = shs + shll;

        pe    = pe - peo;
        pinc  = pinc - pinco;
        pl    = pl - plo;
        pgh   = pgh - pgho;
        ph    = ph - pho;
        xincp = xincp + pinc;
        ep    = ep + pe;
        sinip = sin(xincp);
        cosip = cos(xincp);

        // Apply periodics directly
        if (xincp >= 0.2)
        {
            ph    = ph / sinip;
            pgh   = pgh - cosip * ph;
            argpp = argpp + pgh;
            nodep = nodep + ph;
            mp    = mp + pl;
        }
        else
        {
            // Apply periodics with lyddane modification
            sinop = sin(nodep);
            cosop = cos(nodep);
            alfdp = sinip * sinop;
            betdp = sinip * cosop;
            dalf  = ph * cosop + pinc * cosip * sinop;
            dbet  = -ph * sinop + pinc * cosip * cosop;
            alfdp = alfdp + dalf;
            betdp = betdp + dbet;
            nodep = fmod(nodep, TWOPI);
            if (nodep < 0.0)
                nodep += TWOPI;
            xls   = mp + argpp + cosip * nodep;
            dls   = pl + pgh - pinc * nodep * sinip;
            xls   = xls + dls;
            xnoh  = nodep;
            nodep = atan2(alfdp, betdp);
            if ((nodep < 0.0))
                nodep += TWOPI;
            if (fabs(xnoh - nodep) > M_PI)
            {
                if (nodep < xnoh)
                    nodep += TWOPI;
                else
                    nodep -= TWOPI;
            }
            mp    = mp + pl;
            argpp = xls - mp - cosip * nodep;
        }

        if (xincp < 0.0)
        {
            xincp = -xincp;
            nodep = nodep + M_PI;
            argpp = argpp - M_PI;
        }

        if ((ep < 0.0) || (ep > 1.0))
        {
            qDebug() << "Eccentricity < 0.0  or > 1.0";
            return (3);
        }
    }

    // Long period periodics
    if (method == 'd')
    {
        sinip = sin(xincp);
        cosip = cos(xincp);
        aycof = -0.5 * J3OJ2 * sinip;
        if (fabs(cosip + 1.0) > 1.5e-12)
            xlcof = -0.25 * J3OJ2 * sinip * (3.0 + 5.0 * cosip) / (1.0 + cosip);
        else
            xlcof = -0.25 * J3OJ2 * sinip * (3.0 + 5.0 * cosip) / temp4;
    }
    axnl = ep * cos(argpp);
    temp = 1.0 / (am * (1.0 - ep * ep));
    aynl = ep * sin(argpp) + temp * aycof;
    xl   = mp + argpp + nodep + temp * xlcof * axnl;

    // Solve kepler's equation
    u    = fmod(xl - nodep, TWOPI);
    eo1  = u;
    tem5 = 9999.9;
    ktr  = 1;
    while ((fabs(tem5) >= 1.0e-12) && (ktr <= 10))
    {
        sineo1 = sin(eo1);
        coseo1 = cos(eo1);
        tem5   = 1.0 - coseo1 * axnl - sineo1 * aynl;
        tem5   = (u - aynl * coseo1 + axnl * sineo1 - eo1) / tem5;
        if (fabs(tem5) >= 0.95)
            tem5 = tem5 > 0.0 ? 0.95 : -0.95;
        eo1 = eo1 + tem5;
        ktr = ktr + 1;
    }

    // Short period preliminary quantities
    ecose = axnl * coseo1 + aynl * sineo1;
    esine = axnl * sineo1 - aynl * coseo1;
    el2   = axnl * axnl + aynl * aynl;
    pl    = am * (1.0 - el2);

    if (pl < 0.0)
    {
        qDebug() << "Semi-latus rectum < 0.0";
        return (4);
    }

    rl     = am * (1.0 - ecose);
    rdotl  = sqrt(am) * esine / rl;
    rvdotl = sqrt(pl) / rl;
    betal  = sqrt(1.0 - el2);
    temp   = esine / (1.0 + betal);
    sinu   = am / rl * (sineo1 - aynl - axnl * temp);
    cosu   = am / rl * (coseo1 - axnl + aynl * temp);
    su     = atan2(sinu, cosu);
    sin2u  = (cosu + cosu) * sinu;
    cos2u  = 1.0 - 2.0 * sinu * sinu;
    temp   = 1.0 / pl;
    temp1  = 0.5 * J2 * temp;
    temp2  = temp1 * temp;

    // Update for short period periodics
    if (method == 'd')
    {
        cosisq = cosip * cosip;
        con41  = 3.0 * cosisq - 1.0;
        x1mth2 = 1.0 - cosisq;
        x7thm1 = 7.0 * cosisq - 1.0;
    }
    mrt   = rl * (1.0 - 1.5 * temp2 * betal * con41) + 0.5 * temp1 * x1mth2 * cos2u;
    su    = su - 0.25 * temp2 * x7thm1 * sin2u;
    xnode = nodep + 1.5 * temp2 * cosip * sin2u;
    xinc  = xincp + 1.5 * temp2 * cosip * sinip * cos2u;
    mvt   = rdotl - nm * temp1 * x1mth2 * sin2u / XKE;
    rvdot = rvdotl + nm * temp1 * (x1mth2 * cos2u + 1.5 * con41) / XKE;

    // Orientation vectors
    sinsu = sin(su);
    cossu = cos(su);
    snod  = sin(xnode);
    cnod  = cos(xnode);
    sini  = sin(xinc);
    cosi  = cos(xinc);
    xmx   = -snod * cosi;
    xmy   = cnod * cosi;
    ux    = xmx * sinsu + cnod * cossu;
    uy    = xmy * sinsu + snod * cossu;
    uz    = sini * sinsu;
    vx    = xmx * cossu - cnod * sinsu;
    vy    = xmy * cossu - snod * sinsu;
    vz    = sini * cossu;

    // Position and velocity (in km and km/sec)
    sat_posx   = (mrt * ux) * RADIUSEARTHKM;
    sat_posy   = (mrt * uy) * RADIUSEARTHKM;
    sat_posz   = (mrt * uz) * RADIUSEARTHKM;
    sat_posw   = sqrt(sat_posx * sat_posx + sat_posy * sat_posy + sat_posz * sat_posz);
    sat_velx   = (mvt * ux + rvdot * vx) * vkmpersec;
    sat_vely   = (mvt * uy + rvdot * vy) * vkmpersec;
    sat_velz   = (mvt * uz + rvdot * vz) * vkmpersec;
    m_velocity = sqrt(sat_velx * sat_velx + sat_vely * sat_vely + sat_velz * sat_velz);

    //     printf("tsince=%.15f\n", tsince);
    //     printf("sat_posx=%.15f\n", sat_posx);
    //     printf("sat_posy=%.15f\n", sat_posy);
    //     printf("sat_posz=%.15f\n", sat_posz);
    //     printf("sat_velx=%.15f\n", sat_velx);
    //     printf("sat_vely=%.15f\n", sat_vely);
    //     printf("sat_velz=%.15f\n", sat_velz);

    if (mrt < 1.0)
    {
        qDebug() << "Satellite has decayed";
        return (6);
    }

    // Observer ECI position and velocity
    sinlat   = sin(data->geo()->lat()->radians());
    coslat   = cos(data->geo()->lat()->radians());
    thetageo = data->geo()->LMST(jul_utc);
    sintheta = sin(thetageo);
    costheta = cos(thetageo);
    c        = 1.0 / sqrt(1.0 + F * (F - 2.0) * sinlat * sinlat);
    sq       = (1.0 - F) * (1.0 - F) * c;
    achcp    = (RADIUSEARTHKM * c + MEANALT) * coslat;
    obs_posx = achcp * costheta;
    obs_posy = achcp * sintheta;
    obs_posz = (RADIUSEARTHKM * sq + MEANALT) * sinlat;
    obs_posw = sqrt(obs_posx * obs_posx + obs_posy * sat_posy + obs_posz * obs_posz);
    /*obs_velx = -MFACTOR * obs_posy;
    obs_vely = MFACTOR * obs_posx;
    obs_velz = 0.;*/

    m_altitude = sat_posw - obs_posw + MEANALT;

    // Az and Dec
    double range_posx = sat_posx - obs_posx;
    double range_posy = sat_posy - obs_posy;
    double range_posz = sat_posz - obs_posz;
    m_range           = sqrt(range_posx * range_posx + range_posy * range_posy + range_posz * range_posz);
    //     double range_velx = sat_velx - obs_velx;
    //     double range_vely = sat_velx - obs_vely;
    //     double range_velz = sat_velx - obs_velz;

    double top_s = sinlat * costheta * range_posx + sinlat * sintheta * range_posy - coslat * range_posz;
    double top_e = -sintheta * range_posx + costheta * range_posy;
    double top_z = coslat * costheta * range_posx + coslat * sintheta * range_posy + sinlat * range_posz;

    double azimuth = atan(-top_e / top_s);
    if (top_s > 0.)
        azimuth += M_PI;
    if (azimuth < 0.)
        azimuth += TWOPI;
    double elevation = arcSin(top_z / m_range);

    //     printf("azimuth=%.15f\n\r", azimuth / DEG2RAD);
    //     printf("elevation=%.15f\n\r", elevation / DEG2RAD);

    setAz(azimuth / DEG2RAD);
    setAlt(elevation / DEG2RAD);
    HorizontalToEquatorial(data->lst(), data->geo()->lat());

    // is the satellite visible ?
    // Find ECI coordinates of the sun
    double mjd, year, T, M, L, e, C, O, Lsa, nu, R, eps;

    mjd  = jul_utc - 2415020.0;
    year = 1900.0 + mjd / 365.25;
    T    = (mjd + deltaET(year) / (MINPD * 60.0)) / 36525.0;
    M    = DEG2RAD * (Modulus(358.47583 + Modulus(35999.04975 * T, 360.0) - (0.000150 + 0.0000033 * T) * T * T, 360.0));
    L    = DEG2RAD * (Modulus(279.69668 + Modulus(36000.76892 * T, 360.0) + 0.0003025 * T * T, 360.0));
    e    = 0.01675104 - (0.0000418 + 0.000000126 * T) * T;
    C    = DEG2RAD * ((1.919460 - (0.004789 + 0.000014 * T) * T) * sin(M) + (0.020094 - 0.000100 * T) * sin(2 * M) +
                      0.000293 * sin(3 * M));
    O    = DEG2RAD * (Modulus(259.18 - 1934.142 * T, 360.0));
    Lsa  = Modulus(L + C - DEG2RAD * (0.00569 - 0.00479 * sin(O)), TWOPI);
    nu   = Modulus(M + C, TWOPI);
    R    = 1.0000002 * (1.0 - e * e) / (1.0 + e * cos(nu));
    eps  = DEG2RAD * (23.452294 - (0.0130125 + (0.00000164 - 0.000000503 * T) * T) * T + 0.00256 * cos(O));
    R    = AU * R;

    double sun_posx = R * cos(Lsa);
    double sun_posy = R * sin(Lsa) * cos(eps);
    double sun_posz = R * sin(Lsa) * sin(eps);
    double sun_posw = R;

    // Calculates satellite's eclipse status and depth
    double sd_sun, sd_earth, delta, depth;

    // Determine partial eclipse
    sd_earth       = arcSin(RADIUSEARTHKM / sat_posw);
    double rho_x   = sun_posx - sat_posx;
    double rho_y   = sun_posy - sat_posy;
    double rho_z   = sun_posz - sat_posz;
    double rho_w   = sqrt(rho_x * rho_x + rho_y * rho_y + rho_z * rho_z);
    sd_sun         = arcSin(SR / rho_w);
    double earth_x = -1.0 * sat_posx;
    double earth_y = -1.0 * sat_posy;
    double earth_z = -1.0 * sat_posz;
    double earth_w = sat_posw;
    delta      = PIO2 - arcSin((sun_posx * earth_x + sun_posy * earth_y + sun_posz * earth_z) / (sun_posw * earth_w));
    depth      = sd_earth - sd_sun - delta;
    KSSun *sun = dynamic_cast<KSSun *>(data->skyComposite()->findByName(i18n("Sun")));

    m_is_eclipsed = sd_earth >= sd_sun && depth >= 0;
    m_is_visible  = !m_is_eclipsed && sun->alt().Degrees() <= -12.0 && elevation >= 0.0;

    return (0);
}

QString Satellite::sgp4ErrorString(int code)
{
    switch (code)
    {
        case 0:
            return i18n("Success");

        case 1:
        case 3:
            return i18n("Eccentricity >= 1.0 or < -0.001");

        case 2:
            return i18n("Mean motion less than 0.0");

        case 4:
            return i18n("Semi-latus rectum < 0.0");

        case 6:
            return i18n("Satellite has decayed");

        default:
            return i18n("Unknown error");
    }
}

double Satellite::arcSin(double arg)
{
    if (fabs(arg) >= 1.)
        if (arg > 0.)
            return PIO2;
        else if (arg < 0.)
            return -PIO2;
        else
            return 0.;
    else
        return (atan(arg / sqrt(1. - arg * arg)));
}

double Satellite::deltaET(double year)
{
    double delta_et;

    delta_et = 26.465 + 0.747622 * (year - 1950) + 1.886913 * sin(TWOPI * (year - 1975) / 33);

    return delta_et;
}

double Satellite::Modulus(double arg1, double arg2)
{
    int i;
    double ret_val;

    ret_val = arg1;
    i       = ret_val / arg2;
    ret_val -= i * arg2;

    if (ret_val < 0.0)
        ret_val += arg2;

    return ret_val;
}

bool Satellite::isVisible()
{
    return m_is_visible;
}

bool Satellite::selected()
{
    return m_is_selected;
}

void Satellite::setSelected(bool selected)
{
    m_is_selected = selected;
}

void Satellite::initPopupMenu(KSPopupMenu *pmenu)
{
#ifndef KSTARS_LITE
    pmenu->createSatelliteMenu(this);
#else
    Q_UNUSED(pmenu);
#endif
}

double Satellite::velocity() const
{
    return m_velocity;
}

double Satellite::altitude() const
{
    return m_altitude;
}

double Satellite::range() const
{
    return m_range;
}

QString Satellite::id() const
{
    return m_id;
}

QString Satellite::tle() const
{
    return m_tle;
}
