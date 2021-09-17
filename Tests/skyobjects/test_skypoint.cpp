/*
    SPDX-FileCopyrightText: 2016 Akarsh Simha <akarsh.simha@kdemail.net>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

/* Project Includes */
#include "test_skypoint.h"
#include "ksnumbers.h"
#include "time/kstarsdatetime.h"
#include "auxiliary/dms.h"
#include "Options.h"
#include <libnova/libnova.h>
TestSkyPoint::TestSkyPoint() : QObject()
{
    useRelativistic = Options::useRelativistic();
}

TestSkyPoint::~TestSkyPoint()
{
    Options::setUseRelativistic(useRelativistic);
}

void TestSkyPoint::testPrecession()
{
    /*
     * NOTE: These test cases were picked randomly and generated using
     * https://ned.ipac.caltech.edu/forms/calculator.html
     */

    auto verify = [](const SkyPoint & p, const double targetRA, const double targetDec, const double tolerance)
    {
        qDebug() << p.ra().toHMSString() << " " << p.dec().toDMSString();
        QVERIFY(fabs(p.ra().Degrees() - targetRA) < tolerance * 15.);
        QVERIFY(fabs(p.dec().Degrees() - targetDec) < tolerance);
    };

    /* Precession of equinoxes within FK5 system */

    // Check precession from J2000.0 to J1992.5

    // NOTE: (FIXME?) I set 1950.0 as the observation epoch in the
    // calculator. Changing it to 1992.5 made no difference; probably
    // required only for FK5 to FK4 conversion. The help is not
    // extremely clear on this. -- asimha

    constexpr double arcsecPrecision = 3.e-4;

    long double jd = KStarsDateTime::epochToJd(1992.5, KStarsDateTime::JULIAN);
    KSNumbers num(jd);
    SkyPoint p(dms("18:22:54", false), dms("34:23:19", true)); // J2000.0 coordinates
    p.precess(&num);
    verify(p, 275.65734620, 34.38447020, arcsecPrecision);

    // Check precession from J2000.0 to J2016.8
    jd = KStarsDateTime::epochToJd(2016.8);
    num.updateValues(jd);
    p.set(dms("23:24:25", false), dms("-08:07:06", true));
    p.precess(&num);
    verify(p, 351.32145112, (-8.02590004), arcsecPrecision);

    // Check precession from J2000.0 to J2109.8
    jd = KStarsDateTime::epochToJd(2109.8);
    num.updateValues(jd);
    p.precess(&num);
    verify(p, 352.52338626, (-7.51340424), arcsecPrecision);

    /* Precession of equinoxes + conversion from FK4 to FK5 */

    // Conversion from B1950.0 to J2000.0
    p.set(dms("11:22:33", false), dms("44:55:66", true)); // Sets all of RA0,Dec0,RA,Dec
    p.B1950ToJ2000(); // Assumes (RA, Dec) are referenced to FK4 @ B1950 and stores the FK5 @ J2000 result in (RA, Dec)
    verify(p, 171.32145647, 44.66010982, arcsecPrecision);

    // Conversion from B1950.0 to J2016.8
    // Current of B1950ToJ2000 is to NOT alter (RA0,Dec0), so we shouldn't have to reset it
    jd = KStarsDateTime::epochToJd(2016.8);
    p.precessFromAnyEpoch(
        B1950, jd); // Takes (RA0, Dec0) and precesses to (RA, Dec) applying FK4 <--> FK5 conversions as necessary
    verify(p, 171.55045618, 44.56762158, arcsecPrecision);

    /* Precession of equinoxes + conversion from FK5 to FK4 */
    p.set(dms("11:22:33", false), dms("44:55:66", true)); // Sets all of RA0,Dec0,RA,Dec
    p.J2000ToB1950(); // Assumes (RA, Dec) are referenced to FK5 @ J2000 and stores the FK4 @ B1950 result in (RA, Dec)
    // NOTE: I set observation epoch = 2000.0 in NED's calculator. This time, it made a difference whether I set 2000.0 or 1950.0, but the difference was very small.
    verify(p, 169.94978888, 45.20928652, arcsecPrecision);

    // Conversion from J2016.8 to B1950.0
    // Current behavior of J2000ToB1950 is to NOT alter (RA0,Dec0), so we shouldn't have to reset it
    jd = KStarsDateTime::epochToJd(2016.8);
    p.precessFromAnyEpoch(
        jd, B1950); // Takes (RA0, Dec0) and precesses to (RA, Dec) applying FK4 <--> FK5 conversions as necessary
    // NOTE: I set observation epoch = 2016.8 in NED's calculator. It made a difference whether I set 2000.0 or 2016.8, but the difference was very small.
    verify(p, 169.71785991, 45.30132855, arcsecPrecision);
}

void TestSkyPoint::compareNovas()
{
    Options::setUseRelativistic(false);

    KStarsDateTime kdt = KStarsDateTime::currentDateTimeUtc();
    long double jd = kdt.djd();

    double lnJd = ln_get_julian_from_sys();
    // compare JD provided by KStarsDateTime and libnova
    qDebug() << "KDT JD " << static_cast<double>(jd) << "  LN JD " << lnJd << " Diff " << (static_cast<double>
             (jd) - lnJd) * 86400 << "secs";
    QVERIFY(fabs(static_cast<double>(jd) - lnJd) * 86400 < 1);

    qDebug() << "Check conversion in Align";
    // the align process is a bit like this:
    // Catalogue position
    SkyPoint target(4, 20); // ra in hours, dec in degrees

    // transform to JNow in EKOS
    // this is wrong because aberration and nutation are handled incorrectly
    target.apparentCoord(static_cast<long double>(J2000), jd);
    double raN = target.ra().Degrees();
    double decN = target.dec().Degrees();
    qDebug() << "SkyPoint J2000 to JNow " << raN << ", " << decN;

    // should be:
    // remove aberration

    // remove nutation

    // this is passed to the CCD simulator where it is transformed to J2K using libnova
    ln_equ_posn JnowPos { 0, 0 }, J2kPos { 0, 0 };
    JnowPos.ra = raN;
    JnowPos.dec = decN;
    ln_get_equ_prec2(&JnowPos, lnJd, JD2000, &J2kPos);
    double ra0 = J2kPos.ra;
    double dec0 = J2kPos.dec;
    qDebug() << "Libnova JNow to J2000" << ra0 << ", " << dec0;

    // this is used to generate a star image which is solved
    // assume this solve is perfect

    // Align converts this J2000 position back to JNow:
    SkyPoint ac(ra0 / 15., dec0);
    ac.apparentCoord(static_cast<long double>(J2000), jd);
    double ran2 = ac.ra().Degrees();
    double decn2 = ac.dec().Degrees();
    qDebug() << "SkyPoint J2000 to JNow" << ran2 << ", " << decn2;
    // get the error, this appears as the align error
    qDebug() << "Error (arcsec)" << (raN - ran2) * 3600 << ", " << (decN - decn2) * 3600;

    // check the difference
    //QVERIFY(fabs(decN - decn2) * 3600. < 1);
    //QVERIFY(fabs(raN - ran2) * 3600. < 1);

    qDebug();
    qDebug() << "Using libnova throughout";
    J2kPos.ra = 60;
    J2kPos.dec = 20;
    // convert to JNow
    ln_get_equ_prec2(&J2kPos, JD2000, lnJd, &JnowPos);
    qDebug() << "libnova J2000 to JNow " << JnowPos.ra << ", " << JnowPos.dec;
    // convert to J2000
    ln_get_equ_prec2(&JnowPos, lnJd, JD2000, &J2kPos);
    qDebug() << " libnova JNow to J2000" << J2kPos.ra << ", " << J2kPos.dec;

    // 'solve'

    // convert back to Jnow
    ln_equ_posn Jnow2Pos { 0, 0 };
    ln_get_equ_prec2(&J2kPos, JD2000, lnJd, &Jnow2Pos);
    qDebug() << "libnova J2000 to JNow " << Jnow2Pos.ra << ", " << Jnow2Pos.dec;

    qDebug() << "Error (arcsec)" << (JnowPos.ra - Jnow2Pos.ra) * 3600 << ", " << (JnowPos.dec - Jnow2Pos.dec) * 3600;

    // Using SkyPoint won't help:
    qDebug();
    qDebug() << "using SkyPoint both ways";
    // do the conversion from the previously computed JNow position
    SkyPoint ccd;
    ccd.setRA(raN / 15.);
    ccd.setDec(decN);
    KSNumbers num(jd);
    ccd.deprecess(&num);
    qDebug() << "SkyPoint Jnow to J2000 " << ccd.ra0().Degrees() << ", " << ccd.dec0().Degrees();
    // and to Jnow
    ccd.apparentCoord(static_cast<long double>(J2000), jd);
    qDebug() << "SkyPoint J2000 to JNow " << ccd.ra().Degrees() << ", " << ccd.dec().Degrees();

    qDebug() << "Error " << (raN - ccd.ra().Degrees()) * 3600 << ", " << (decN - ccd.dec().Degrees()) * 3600;

    //QVERIFY(fabs(p.ra().Degrees() - targetRA) < tolerance * 15.);
    //QVERIFY(fabs(p.dec().Degrees() - targetDec) < tolerance);

    //qCDebug(KSTARS_EKOS_ALIGN) << "libnova errors " <<
    //        (epochPos.ra - targetCoord.ra().Degrees()) * 3600. << "as, " << (epochPos.dec - targetCoord.dec().Degrees()) * 3600. << "as";


    Options::setUseRelativistic(useRelativistic);
}

void TestSkyPoint::testPrecess_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");

    QTest::newRow("normal") << 4.0 << 20.0;
    QTest::newRow("south") << 15.0 << -35.0;
    QTest::newRow("near Pole") << 22.0 << 85.0;
    QTest::newRow("near S Pole") << 22.0 << -85.0;
}

void TestSkyPoint::testPrecess()
{
    auto jd = KStarsDateTime::currentDateTimeUtc().djd();
    KSNumbers now(jd);
    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);

    sp = SkyPoint(Ra, Dec);

    sp.precess(&now);

    qDebug() << "precess dRa " << sp.ra0().Degrees() - sp.ra().Degrees() << ", dDec " << sp.dec0().Degrees() -
             sp.dec().Degrees();

    SkyPoint spn = sp.deprecess(&now);

    compare("precess-deprecess ", sp.ra0().Degrees(), sp.dec0().Degrees(), spn.ra().Degrees(), spn.dec().Degrees());
}

void TestSkyPoint::testPrecessFromAnyEpoch_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");

    QTest::newRow("normal") << 4.0 << 20.0;
    QTest::newRow("south") << 15.0 << -35.0;
    QTest::newRow("near Pole") << 22.0 << 85.0;
    QTest::newRow("near S Pole") << 22.0 << -85.0;
}

void TestSkyPoint::testPrecessFromAnyEpoch()
{
    auto jd = KStarsDateTime::currentDateTimeUtc().djd();
    KSNumbers now(jd);

    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    sp = SkyPoint(Ra, Dec);

    sp.precessFromAnyEpoch(J2000L, jd);

    qDebug() << "PFAE dRa " << sp.ra0().Degrees() - sp.ra().Degrees() << ", dDec " << sp.dec0().Degrees() - sp.dec().Degrees();

    SkyPoint spn = SkyPoint(sp.ra(), sp.dec());
    spn.precessFromAnyEpoch(jd, J2000L);

    compare("dePFAE ", sp.ra0().Degrees(), sp.dec0().Degrees(), spn.ra().Degrees(), spn.dec().Degrees());
}

void TestSkyPoint::testNutate_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");

    QTest::newRow("normal") << 4.0 << 20.0;
    QTest::newRow("south") << 15.0 << -35.0;
    QTest::newRow("near Pole") << 22.0 << 85.0;
    QTest::newRow("near S Pole") << 22.0 << -85.0;
}

void TestSkyPoint::testNutate()
{
    long double jd = KStarsDateTime::currentDateTimeUtc().djd();
    KSNumbers now(jd);

    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    sp = SkyPoint(Ra, Dec);

    sp.nutate(&now);

    qDebug() << "nutate dRa " << sp.ra0().Degrees() - sp.ra().Degrees() << ", dDec " << sp.dec0().Degrees() -
             sp.dec().Degrees();

    sp.nutate(&now, true);

    compare("nutate-denutate", &sp);
}

void TestSkyPoint::testAberrate_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");

    QTest::newRow("normal") << 4.0 << 20.0;
    QTest::newRow("south") << 15.0 << -35.0;
    QTest::newRow("near Pole") << 22.0 << 85.0;
    QTest::newRow("near S Pole") << 22.0 << -85.0;
}

void TestSkyPoint::testAberrate()
{
    long double jd = KStarsDateTime::epochToJd(2028.3);
    KSNumbers now(jd);

    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    sp = SkyPoint(Ra, Dec);

    sp.aberrate(&now);

    // deaberrate
    sp.aberrate(&now, true);

    compare("aberrate-deaberrate", &sp);
}

// test results obtained using the ASCOM Transform component
// to convert from J2000 to Apparent
// this uses SOFA so should be fairly good
void TestSkyPoint::testApparentCatalogue_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");
    QTest::addColumn<double>("RaOut");
    QTest::addColumn<double>("DecOut");

    QTest::newRow("Apparent") << 4.0 << 20.0 << 4.0274 << 20.0791;
    QTest::newRow("south") << 10.0 << -35.0 << 10.0208 << -35.1418;
    QTest::newRow("south") << 10.0 << -55.0 << 10.01698 << -55.14256;
    QTest::newRow("near Pole") << 22.0 << 85.0 << 21.9601 << 85.1317;
    QTest::newRow("near S Pole") << 15.0 << -85.0 << 15.1159 << -85.1112;
}

void TestSkyPoint::testApparentCatalogue()
{
    Options::setUseRelativistic(false);

    //long double jd = KStarsDateTime::currentDateTimeUtc().djd();
    auto dt = KStarsDateTime::fromString("2028-04-27T06:30");
    long double jd = dt.djd();

    double rjd = static_cast<double>(jd - J2000L);
    //qDebug() << "DT " << dt.toString() << " JD " << static_cast<double>(jd) << " RJD " << rjd;

    QVERIFY(fabs(rjd - 10343.771) < 0.1);

    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    sp = SkyPoint(Ra, Dec);

    // J2000 catalogue to Apparent
    sp.apparentCoord(J2000L, jd);
    //    qDebug() << "apparent ra0 " << sp.ra0().Hours() << ", dec0 " << sp.dec0().Degrees() <<
    //                " ra " << sp.ra().Hours() << ", dec " << sp.dec().Degrees();
    QFETCH(double, RaOut);
    QFETCH(double, DecOut);

    compare("J2000 to aparrent", RaOut, DecOut, sp.ra().Hours(), sp.dec().Degrees());

    SkyPoint spn = SkyPoint(sp.ra(), sp.dec());
    qDebug() << "spn ra0 " << spn.ra0().Degrees() << ", dec0 " << spn.dec0().Degrees() <<
             " ra " << spn.ra().Degrees() << ", dec " << spn.dec().Degrees();

    // apparent on jd to J2000Catalogue
    spn.catalogueCoord(jd);

    compare("J2K to app to catalogue", sp.ra0().Degrees(), sp.dec0().Degrees(), spn.ra0().Degrees(), spn.dec0().Degrees());
}

void TestSkyPoint::testApparentCatalogueInversion_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");
    QTest::addColumn<double>("Epoch");

    QTest::newRow("Random1") << 15.32 << 34.50 << 2012.34;
    QTest::newRow("Random2") << 72.96 << 24.10 << 2022.39;
    QTest::newRow("Random3") << 188.52 << -36.58 << 2042.86;
    QTest::newRow("Random4") << 239.22 << -47.32 << 2139.23;
    QTest::newRow("Random5") << 278.78 << 01.68 << 2058.62;
    QTest::newRow("Near J2000 NCP") << 172.59 << 88.95 << 2029.88;
    QTest::newRow("Near J2000 SCP") << 145.22 << -88.86 << 2034.72;
    QTest::newRow("Near Current NEP") << 280.74 << 70.81 << 2021.23;
}

void TestSkyPoint::testApparentCatalogueInversion()
{
    Options::setUseRelativistic(false);

    // This test tests the conversion of J2000 -> arbitrary ICRS epoch -> back to J2000 by inversion
    constexpr double sub_arcsecond_tolerance = 0.1 / 3600.0;
    SkyPoint sp_forward;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    QFETCH(double, Epoch);
    long double targetJd = KStarsDateTime::epochToJd(Epoch);
    dms dms_ra {Ra};
    KSNumbers num(targetJd);
    sp_forward.setRA0(dms_ra);
    sp_forward.setDec0(Dec);
    sp_forward.updateCoordsNow(&num);
    SkyPoint sp_backward {sp_forward.catalogueCoord(targetJd)};
    compare(
        QString("Apparent to Catalogue Inversion Check for epoch %1").arg(Epoch, 7, 'f', 2),
        sp_backward.ra0().reduce().Degrees(), sp_backward.dec0().Degrees(),
        dms_ra.reduce().Degrees(), Dec,
        sub_arcsecond_tolerance
    );
}

void TestSkyPoint::compareSkyPointLibNova_data()
{
    QTest::addColumn<double>("Ra");
    QTest::addColumn<double>("Dec");
    QTest::addColumn<double>("RaOut");
    QTest::addColumn<double>("DecOut");

    QTest::newRow("Apparent") << 4.0 << 20.0 << 4.0274 << 20.0791;
    QTest::newRow("south") << 10.0 << -35.0 << 10.0208 << -35.1418;
    QTest::newRow("south") << 10.0 << -55.0 << 10.01698 << -55.14256;
    QTest::newRow("near Pole") << 22.0 << 85.0 << 21.9601 << 85.1317;
    QTest::newRow("near S Pole") << 15.0 << -85.0 << 15.1159 << -85.1112;
}
void TestSkyPoint::compareSkyPointLibNova()
{
    // skypoint JD
    auto dt = KStarsDateTime::fromString("2028-04-27T06:30");
    long double ljd = dt.djd();
    double jd = static_cast<double>(ljd);

    SkyPoint sp;
    QFETCH(double, Ra);
    QFETCH(double, Dec);
    sp = SkyPoint(Ra, Dec);
    ln_equ_posn LnPos { Ra * 15.0, Dec }, observed { 0, 0 }, J2000Pos { 0, 0 };

    ln_equ_posn tempPosn;

    KSNumbers num(ljd);

    // apply precession from J2000 to jd
    ln_get_equ_prec2(&LnPos, JD2000, jd, &tempPosn);
    SkyPoint spp = sp;
    spp.precessFromAnyEpoch(J2000L, ljd);
    compare("precess", spp.ra().Degrees(), spp.dec().Degrees(), tempPosn.ra, tempPosn.dec);

    // apply nutation (needs new function)
    ln_get_equ_nut(&tempPosn, jd);
    SkyPoint spn = spp;
    spn.nutate(&num);
    compare("nutate", spn.ra().Degrees(), spn.dec().Degrees(), tempPosn.ra, tempPosn.dec);

    // apply aberration
    ln_get_equ_aber(&tempPosn, jd, &observed);
    SkyPoint spa = spn;
    spa.aberrate(&num);
    compare("aberrate", spa.ra().Degrees(), spa.dec().Degrees(), observed.ra, observed.dec);

    sp.apparentCoord(J2000L, ljd);

    // compare sp and observed with each other
    compare("sp and LN ", sp.ra().Degrees(), sp.dec().Degrees(), observed.ra, observed.dec);
    QFETCH(double, RaOut);
    QFETCH(double, DecOut);
    // compare sp with expected, use degrees
    compare("sp expected ", RaOut * 15., DecOut, sp.ra().Degrees(), sp.Dec.Degrees(), 0.0005);
    // compare ln with expected
    compare("ln expected ", RaOut * 15, DecOut, observed.ra, observed.dec, 0.0005);

    // now to J2000

    // remove the aberration
    ln_get_equ_aber(&observed, jd, &tempPosn);
    // this conversion has added the aberration, we want to subtract it
    tempPosn.ra = observed.ra - (tempPosn.ra - observed.ra);
    tempPosn.dec = observed.dec * 2 - tempPosn.dec;
    //sp.aberrate(&num, true);


    // remove the nutation
    ln_get_equ_nut(&tempPosn, jd, true);

    //sp.nutate(&num, true);

    // precess from now to J2000
    ln_get_equ_prec2(&tempPosn, jd, JD2000, &J2000Pos);

    // the start position needs to be in RA0,Dec0
    //sp.RA0 = sp.RA;
    //sp.Dec0 = sp.Dec;
    // from now to J2000
    //sp.precessFromAnyEpoch(jd, J2000L);
    // the J2000 position is in RA,Dec, move to RA0, Dec0
    //sp.RA0 = sp.RA;
    //sp.Dec0 = sp.Dec;
    //sp.lastPrecessJD = J2000;

    sp.catalogueCoord(ljd);

    //compare sp and the j2000Pos with each other and the original
    compare("Round trip ", &sp, &J2000Pos);
    compare("original", Ra, Dec, sp.RA0.Hours(), sp.Dec0.Degrees());
}

void TestSkyPoint::compare(QString msg, SkyPoint *sp, SkyPoint *sp1)
{
    compare(msg, sp->ra0().Degrees(), sp->dec0().Degrees(), sp1->ra().Degrees(), sp1->dec().Degrees());
}

void TestSkyPoint::compare(QString msg, SkyPoint *sp, ln_equ_posn *lnp)
{
    compare(msg, sp->ra0().Degrees(), sp->dec0().Degrees(), lnp->ra, lnp->dec);
}

void TestSkyPoint::compare(QString msg, SkyPoint *sp)
{
    compare(msg, sp->ra0().Degrees(), sp->dec0().Degrees(), sp->ra().Degrees(), sp->dec().Degrees());
}

void TestSkyPoint::compare(QString msg, double ra1, double dec1, double ra2, double dec2, double err)
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

// apply or remove nutation
void TestSkyPoint::ln_get_equ_nut(ln_equ_posn *posn, double jd, bool reverse)
{
    // code lifted from libnova ln_get_equ_nut
    // with the option to add or remove nutation
    struct ln_nutation nut;
    ln_get_nutation (jd, &nut);

    double mean_ra, mean_dec, delta_ra, delta_dec;

    mean_ra = ln_deg_to_rad(posn->ra);
    mean_dec = ln_deg_to_rad(posn->dec);

    // Equ 22.1

    double nut_ecliptic = ln_deg_to_rad(nut.ecliptic + nut.obliquity);
    double sin_ecliptic = sin(nut_ecliptic);

    double sin_ra = sin(mean_ra);
    double cos_ra = cos(mean_ra);

    double tan_dec = tan(mean_dec);

    delta_ra = (cos (nut_ecliptic) + sin_ecliptic * sin_ra * tan_dec) * nut.longitude - cos_ra * tan_dec * nut.obliquity;
    delta_dec = (sin_ecliptic * cos_ra) * nut.longitude + sin_ra * nut.obliquity;

    // the sign changed to remove nutation
    if (reverse)
    {
        delta_ra = -delta_ra;
        delta_dec = -delta_dec;
    }
    posn->ra += delta_ra;
    posn->dec += delta_dec;
}

void TestSkyPoint::testUpdateCoords()
{
    Options::setUseRelativistic(false);

    dms ra, dec;
    //    ra.setH(11, 50, 3, 0); Min RA
    ra.setH(11, 55, 28, 0);
    //    ra.setH(12, 00, 53, 0); Max RA

    //    dec.setD(-89,52,41,0); Min abs(dec)
    dec.setD(-89, 53, 02, 0);
    //    dec.setD(-89,53,23,0); Max abs(Dec)

    auto dt = KStarsDateTime::fromString("2021-01-24T00:00");
    int numtest = 100;
    int numdays = 100;

    long double jd = dt.djd();
    double jdfrac, jdint;
    KSNumbers num(jd);
    SkyPoint sp;
    sp.setRA0(ra);
    sp.setDec0(dec);
    for(int i = 0; i < numtest; i++)
    {
        sp.updateCoordsNow(&num);
        jdfrac = modf(static_cast<double>(dt.djd()), &jdint);
        if (fabs(sp.dec().Degrees()) > 90.0)
            qDebug() << "i" << i << " jdfrac" << jdfrac << ": sp ra0 " << sp.ra0().Degrees() << ", dec " << sp.dec0().Degrees() <<
                     " ra " << sp.ra().Degrees() << ", dec " << sp.dec().Degrees();
        QVERIFY(fabs(sp.dec().Degrees()) <= 90.0);

        dt = dt.addSecs(numdays * 86400 / numtest);
        jd = dt.djd();
        num.updateValues(jd);
    }

}

QTEST_GUILESS_MAIN(TestSkyPoint)
