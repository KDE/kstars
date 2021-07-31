/***************************************************************************
                 test_starobject.cpp  -  KStars Planetarium
                             -------------------
    begin                : Mon 26 Jul 2021 22:55:07 PDT
    copyright            : (c) 2021 by Akarsh Simha
    email                : akarsh@kde.org
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "test_starobject.h"

#include "skyobjects/skypoint.h"
#include "skyobjects/starobject.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"
#include "nan.h"
#include "Options.h"
#include "config-kstars.h"

#ifdef HAVE_LIBERFA
#include <erfa.h>
#endif

#include <utility>

TestStarObject::TestStarObject() : QObject()
{
    useRelativistic = Options::useRelativistic();
    Options::setUseRelativistic(false);
}

TestStarObject::~TestStarObject()
{
    Options::setUseRelativistic(useRelativistic);
}

// FIXME: Code duplication -- duplicated from TestSkyPoint::compare
void TestStarObject::compare(QString msg, double ra1, double dec1, double ra2, double dec2, double err)
{
    qDebug() << qPrintable(QString("%1 pos1 %2, %3 pos2 %4, %5 errors %6, %7 secs").arg(msg)
                           .arg(ra1).arg(dec1).arg(ra2).arg(dec2)
                           .arg((ra1 - ra2) * 3600.0, 6, 'f', 1).arg((dec1 - dec2) * 3600., 6, 'f', 1));
    //QString str;
    //str.sprintf("%s pos1 %f, %f pos2 %f, %f errors %.1f, %.1f sec", msg.data(), ra1, dec1, ra2, dec2, (ra1 - ra2) *3600, (dec1 - dec2) * 3600 );
    //qDebug() << str;// << msg << "SP " << ra1 << ", " << dec1 << " Sp0 " << ra2 << ", " << dec2
    //<< " errors " << ((ra1 - ra2) * 3600) << ", " << ((dec1 - dec2) * 3600) << " arcsec";

    double errRa = err / cos(dec1 * M_PI / 180.0);

    QVERIFY2(fabs(ra1 - ra2) < errRa, qPrintable(QString("Ra %1, %2 error %3").arg(ra1).arg(ra2).arg(((ra1 - ra2) * 3600.0), 6,
             'f', 1)));
    QVERIFY2(fabs(dec1 - dec2) < err, qPrintable(QString("Dec %1, %2 error %3").arg(dec1).arg(dec2).arg((dec1 - dec2) * 3600.,
             6, 'f', 1)));
}

// FIXME: Code duplication -- duplicated from TestSkyPoint::compare
void TestStarObject::compare(QString msg, double number1, double number2, double tolerance)
{
    qDebug() << qPrintable(QString("%1 num1 %2 num2 %3 error %4").arg(msg)
                           .arg(number1).arg(number2).arg(fabs(number1 - number2)));
    QVERIFY2(fabs(number1 - number2) < tolerance,
             qPrintable(QString("number1 %1, number2 %2; error %3").arg(number1).arg(number2).arg(fabs(number1 - number2))));
}

void TestStarObject::testUpdateCoordsStepByStep()
{
    Options::setUseRelativistic(false);

    /*
     * Run the worked example shown in Jean Meeus' "Astronomical
     * Algorithms" 2nd Edition, step-by-step
     *
     * The test-case is based on a combination of worked examples 21.b
     * and 23.a which concern the apparent place of the star theta
     * Persei on 2028 Nov 13.19 TD
     */

    // N.B. We can neglect the difference between TD and regular UTC
    // time for the purposes of this check.

    // We check most results to arcsecond tolerance
    constexpr double subarcsecond_tolerance = 0.1 / 3600.0;

    KStarsDateTime dt = KStarsDateTime::fromString("2028-11-13T04:33");
    const KSNumbers num(dt.djd());
    StarObject p;

    /* Example 21.b */

    // ICRS coordinates
    CachingDms ra0 = dms::fromString("02:44:11.986", false);
    CachingDms dec0 = dms::fromString("+49:13:42.48", true);
    p.setRA0(ra0);
    p.setDec0(dec0);
    p.setProperMotion(0.03425 * 15.0 * 1000.0 * dec0.cos(), -0.0895 * 1000.0); // in mas/yr

    // Mean equatorial coordinates
    p.getIndexCoords(&num, ra0, dec0);

    // NOTE: We use a more accurate version of proper motion
    // correction than Meeus' example, whereby we will likely have
    // some difference
    compare(
        "Results of proper motion application",
        ra0.Degrees(), dec0.Degrees(),
        41.054063, 49.227750,
        subarcsecond_tolerance
    );

    // Set the proper motion corrected coordinates into the StarObject
    p.setRA0(ra0);
    p.setDec0(dec0);

    qDebug() << "If the above test passed, our implementation of proper motion is likely correct!";

    // Compute precession
    p.precessFromAnyEpoch(J2000L,
                          num.julianDay()); // Hmm... for some reason, SkyPoint::precess is protected, but this is public

    // Compare the resulting mean equatorial coordinates with Meeus
    compare(
        "Mean equatorial coordinates",
        p.ra().Degrees(), p.dec().Degrees(),
        41.547214, 49.348483,
        subarcsecond_tolerance
    );

    /* Example 23.a */
    dms ra = dms::fromString("02:46:11.331", false); // Meeus 23.a mean equatorial coordinates
    dms dec = dms::fromString("+49:20:54.54", true); // Meeus 23.a mean equatorial coordinates

    // Proceed to compute the nutation and aberration, checking the inputs to the computation as well
    compare("Obliquity of the Ecliptic, ε in degrees", num.obliquity()->Degrees(), 23.436, 1e-3); // epsilon
    compare("Nutation in the longitude, Δψ in arcsec", num.dEcLong() * 3600.0, 14.861, 1.0); // Delta psi (arcsec)
    compare("Nutation in the obliquity, Δε in arcsec", num.dObliq() * 3600.0, 2.705, 1.0); // Delta epsilon (arcsec)
    compare("Eccentricity of earth orbit, e (unitless)", num.earthEccentricity(), 0.01669649, 1e-4); // e
    compare("True longitude of the Sun, L or ⊙ in degrees", num.sunTrueLongitude().reduce().Degrees(), 231.328, 0.1); // L
    // FIXME: Below has very large tolerance!
    compare("Longitude of earth perihelion, P or π in degrees", num.earthPerihelionLongitude().reduce().Degrees(), 103.434,
            0.5); // P / pi

    p.nutate(&num);
    dms dra1(p.ra() - ra), ddec1(p.dec() - dec); // delta alpha 1, delta delta 1
    dms ra1(p.ra()), dec1(p.dec());

    p.aberrate(&num);
    dms dra2(p.ra() - ra1), ddec2(p.dec() - dec1); // delta alpha 2, delta delta 2

    // N.B. We are checking Meeus' approximate method with a very
    // tight tolerance. If we "upgrade" to a better method such as
    // matrices for nutation or Ron-Vondrák method for aberration,
    // these checks will fail even though the result will be
    // correct! This is acknowledged in the following flag.

    bool implementationMatchesMeeus = false;
    if (!SkyPoint::implementationIsLibnova)
    {
        implementationMatchesMeeus = true;
    }

    const double meeusCheckTolerance = (implementationMatchesMeeus ? 0.01 : 0.5);

    // N.B. We are doing this slightly differently from
    // Meeus since the corrections of aberration are applied on
    // the nutation-corrected coordinates and not on the mean
    // equatorial coordinates, but this apparently does not matter


    compare("RA Correction from Nutation in arcsec", dra1.Degrees() * 3600.0, 15.843, meeusCheckTolerance); // arcseconds
    compare("Dec Correction from Nutation in arcsec", ddec1.Degrees() * 3600.0, 6.218, meeusCheckTolerance); // arcseconds

    compare("RA Correction from Aberration in arcsec", dra2.Degrees() * 3600.0, 30.045, meeusCheckTolerance); // arcseconds
    compare("Dec Correction from Aberration in arcsec", ddec2.Degrees() * 3600.0, 6.697, meeusCheckTolerance); // arcseconds

    qDebug() << "If the above tests have passed, our implementation matches the approximate method given in Meeus to " <<
             meeusCheckTolerance << " arcseconds";

    /* End-to-end result: Combination of Example 21.b and 23.a against StarObject::updateCoords */
    ra0 = dms::fromString("02:44:11.986", false);
    dec0 = dms::fromString("+49:13:42.48", true);
    p.setRA0(ra0);
    p.setDec0(dec0);

    // Note: Since Meeus directly adds the proper motion correction to
    // the RA, it must be without the cos(dec) factor, whereas KStars
    // expects it WITH the cos(dec) factor
    p.setProperMotion(0.03425 * 15.0 * 1000.0 * dec0.cos(), -0.0895 * 1000.0); // in mas/yr
    p.updateCoordsNow(&num);
    compare(
        "End-to-end computation of StarObject::updateCoords on Meeus Example 21.b + 23.a",
        p.ra().Degrees(), p.dec().Degrees(),
        dms::fromString("02:46:14.390", false).Degrees(), dms::fromString("+49:21:07.45", true).Degrees(),
        subarcsecond_tolerance
    );
}


void TestStarObject::testUpdateCoords()
{
    /*
     * End-to-end check on StarObject::updateCoords() against examples
     * pulled from various professional sources
     */

    struct TestCase
    {
        KStarsDateTime dt;
        dms RA0, Dec0;
        dms RA, Dec;
        double pmRa, pmDec;

        TestCase(const QString &dtstr, const QString &icrs, const QString &apparent, double pmRa_ = 0., double pmDec_ = 0.)
            : dt(KStarsDateTime::fromString(dtstr)), pmRa(pmRa_), pmDec(pmDec_)
        {
            auto icrs_parts = icrs.split("|");
            bool result = RA0.setFromString(icrs_parts.at(0), false);
            result = result && Dec0.setFromString(icrs_parts.at(1), true);

            auto apparent_parts = apparent.split("|");
            result = result && RA.setFromString(apparent_parts.at(0), false);
            result = result && Dec.setFromString(apparent_parts.at(1), true);
            if (!result)
            {
                qDebug() << "NOTE: TEST CASE IS BROKEN!";
                QVERIFY(false);
            }
        }
    }; // FIXME: Move to TestStarObject::testUpdateCoords_data()

    constexpr double subarcsecond_tolerance = 0.5 / 3600.0;

    QList<TestCase> testCases;

    // The following test cases are constructed by taking apparent
    // coordinates from the "Apparent Places of Fundamnetal Stars"
    // published by Astronomiches Rechen-Institut Heidelberg:
    // https://www.worldcat.org/title/apparent-places-of-fundamental-stars/oclc/1722620

    // We query stars from the FK5 catalog (and assume FK5 ~ ICRS to
    // the accuracy promised by KStars) at the following interface to
    // obtain apparent positions:
    // https://wwwadd.zah.uni-heidelberg.de/datenbanken/ariapfs/query.php.en

    // The ICRS coordinates and proper motions are obtained via SIMBAD

    // N.B. For the test-cases after 2000, please note that we must
    // take the RA results from the "Equinox" method as that refers to
    // GCRS. The other ones seem to be for the CIRS which is a
    // different frame.

    testCases
    // Before 2000
            << TestCase("1998-01-25T19:12", "03:43:14.90|-09:45:48.21", "03:43:09.58|-09:46:27.82", -93.16,
                        743.64) // FK5 135 == HIP17378
            << TestCase("1998-01-25T00:00", "02:31:49.09|+89:15:50.79", "02:30:20.47|+89:15:33.43", 44.48, -11.85) // FK5 907 == Polaris

            // After 2000
            << TestCase("2010-11-14T00:00", "03:24:19.37|+49:51:40.24", "03:25:09.64|+49:54:05.23", 23.75, -26.23) // FK5 120 == Mirfak
            << TestCase("2021-06-10T13:12", "06:23:57.11|-52:41:44.38", "06:24:23.09|-52:42:31.17", 19.9, 23.2)    // FK5 245 == Canopus
            << TestCase("2021-01-13T19:26", "02:31:49.09|+89:15:50.79", "02:58:49.17|+89:21:23.23", 44.48, -11.85) // FK5 907 == Polaris

            // High PM stars
            << TestCase("2021-12-16T14:38", "20 14 16.62|+15 11 51.37", "20 15 15.56|+15 15 54.17", 55.03,
                        58.14)  // FK5 1526 == rho Aql
            << TestCase("2021-12-16T13:41", "19 23 53.17|-40 36 57.37", "19 25 21.39|-40 34 32.46", 30.49,
                        -119.21) // FK5 728 == alp Sgr
            << TestCase("2021-10-22T00:00", "02 07 10.40|+23 27 44.70", "02 08 24.64|+23 33 55.57", 188.55,
                        -148.08) // FK5 74 == alp Ari

            // South pole
            << TestCase("2021-11-30T16:48", "21 08 46.84|-88 57 23.41", "21 26 11.11|-88 52 16.42", 26.671, 5.612) // FK5 923 == sig Oct

            // Near NEP (for high aberration)
            << TestCase("2021-03-30T06:43", "19 12 33.30|+67 39 41.54", "19 12 32.63|+67 41 30.58", 95.74, 91.92) // FK5 723 == del Dra
            << TestCase("2021-12-22T13:12", "19 12 33.30|+67 39 41.54", "19 12 29.58|+67 42 00.62", 95.74, 91.92) // FK5 723 == del Dra
            ;

    // TODO: Add more test cases in the 1980s within FK5 reference frame

    int i = 0;
    for (auto &testCase : testCases)
    {
        StarObject s {testCase.RA0, testCase.Dec0, 0.0, "", "", "K0", testCase.pmRa, testCase.pmDec, 0.0, false, false, 0};
        KSNumbers num(testCase.dt.djd());
        qDebug() << "Computing apparent position for (" << testCase.RA0.Degrees() << ", " << testCase.Dec0.Degrees() << ")";
        s.updateCoordsNow(&num);
        compare(
            QString("Testcase %1").arg(i), s.ra().Degrees(), s.dec().Degrees(), testCase.RA.Degrees(), testCase.Dec.Degrees(),
            subarcsecond_tolerance
        );
        i++;
    }

}

#ifdef HAVE_LIBERFA
void TestStarObject::compareProperMotionAgainstErfa_data()
{
    QTest::addColumn<QString>("ra");
    QTest::addColumn<QString>("dec");
    QTest::addColumn<double>("pmRA");
    QTest::addColumn<double>("pmDec");
    QTest::addColumn<double>("epoch");

    // A selection of real stars spread across the sky
    QTest::newRow("Star 1") << "22 10 11.9852752" << "+06 11 52.307750" << 282.18 << 30.46   << 2028.3;
    QTest::newRow("Star 2") << "02 44 11.9870420" << "+49 13 42.411120" << 334.66 << -89.99  << 2074.8;
    QTest::newRow("Star 3") << "00 01 35.7015845" << "-77 03 56.609236" << -57.30 << -177.06 << 1934.2;
    QTest::newRow("Star 4") << "06 23 57.10988"   << "-52 41 44.3810"   << 19.93  << 23.24   << 1901.5;
    QTest::newRow("Star 5") << "14 51 38.3028905" << "-43 34 31.296546" << -25.20 << -27.13  << 1995.2;
    QTest::newRow("Star 6") << "01 09 43.92388"   << "+35 37 14.0075"   << 175.90 << -112.20 << 2140.3;
    QTest::newRow("Star 7") << "04 16 01.588231"  << "-51 29 11.919063" << 99.463 << 183.353 << 2093.8;
    QTest::newRow("Star 8") << "14 39 29.71993"   << "-60 49 55.9990"   << -3608. << 686.0   << 2023.0;
}

void TestStarObject::compareProperMotionAgainstErfa()
{
    QFETCH(QString, ra);
    QFETCH(QString, dec);
    QFETCH(double, pmRA);
    QFETCH(double, pmDec);
    QFETCH(double, epoch);

    dms _ra, _dec;
    bool result = _ra.setFromString(ra, false);
    result = result && _dec.setFromString(dec, true);
    Q_ASSERT(result);

    auto startPoint = std::make_pair(_ra, _dec);
    long double jdf = KStarsDateTime::epochToJd(epoch);

    std::pair<dms, dms> erfaResult;
    {
        /*
         * startPoint : pair of dms: (ra, dec)
         * pmRA       : mas/yr, inclusive of cos(dec) factor
         * pmDec      : mas/yr
         * jdf        : final (target) epoch
         */

        constexpr double masToRad = dms::DegToRad / (3600.0 * 1000.0);
        double ra1 = startPoint.first.reduce().radians();
        double dec1 = startPoint.second.radians();
        double pmr1 = pmRA * masToRad / cos(dec1);
        double pmd1 = pmDec * masToRad;

        double resRa {NaN::d}, resDec {NaN::d}, resPmRa {NaN::d}, resPmDec {NaN::d}, resPx, resRv;
        /*
         * Note: eraStarpm does not like a zero parallax. It replaces
         * it with a 1e-7 parallax (which would be fine), but then
         * concludes that the spatial velocity corresponding to the
         * provided proper motions is relativistic as a result. So a
         * typical parallax needs to be provided that is reasonable
         * for most high-PM stars. I've chosen to set that to 0.1,
         * which should correspond to 10pc ~ 30ly. This distance `r`
         * will divide out as long as the radial velocity is zero: in
         * other words, it does not matter what size of sphere we pick
         *
         * Another note: The documentation reads "The RA proper motion
         * is in terms of coordinate angle, not true angle.  If the
         * catalog uses arcseconds for both RA and Dec proper motions,
         * the RA proper motion will need to be divided by cos(Dec)
         * before use."
         */
        int result = eraStarpm(
            ra1, dec1, pmr1, pmd1,
            0.1, 0., // No parallax, radial velocity info
            J2000, 0, // Starting epoch is J2000
            double(jdf), 0, // Ending epoch is jdf
            &resRa, &resDec, &resPmRa, &resPmDec,
            &resPx, &resRv
            ); // return value irrelevant to us

        if (result > 0)
        {
            qDebug() << "ERFA reports status " << result << ", correction (degrees) dRA = " << (resRa - ra1) / dms::DegToRad << ", dDec = " << (resDec - dec1) / dms::DegToRad;
        }

        erfaResult = std::make_pair(dms { resRa / dms::DegToRad }, dms { resDec / dms::DegToRad });
    }

    CachingDms ra_f, dec_f;
    KSNumbers num(jdf);
    StarObject s {startPoint.first, startPoint.second, 0.0, "", "", "K0", pmRA, pmDec, 0.0, false, false, 0};
    s.getIndexCoords(&num, ra_f, dec_f); // dms version
    double ra_fd, dec_fd;
    s.getIndexCoords(&num, &ra_fd, &dec_fd); // Degrees version

    compare("KStars internal consistency: getIndexCoords degree vs dms implementation",
            ra_f.reduce().Degrees(), dec_f.Degrees(),
            dms(ra_fd).reduce().Degrees(), dec_fd,
            1e-8); // Same algorithm, should match exactly!

    constexpr double arcsecond_tolerance = 1.0/3600.0;
    compare(
        "Proper Motion KStars vs ERFA",
        ra_f.reduce().Degrees(), dec_f.Degrees(),
        erfaResult.first.reduce().Degrees(), erfaResult.second.Degrees(),
        arcsecond_tolerance
    );

}
#endif

QTEST_GUILESS_MAIN(TestStarObject)
