/***************************************************************************
                          starobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue Sep 18 2001
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

#include "starobject.h"

#include <typeinfo>

#include <cmath>
#include <QColor>
#include <QPainter>
#include <QFontMetricsF>
#include <QPixmap>
#include <QDebug>

#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif
#include "ksnumbers.h"
#include "kstarsdata.h"
#include "kstarsdatetime.h"
#include "Options.h"
#include "skymap.h"
#include "ksutils.h"

#ifdef PROFILE_UPDATECOORDS
double StarObject::updateCoordsCpuTime = 0.;
unsigned int StarObject::starsUpdated = 0;
#include <cstdlib>
#include <ctime>
#endif

// DEBUG EDIT. Uncomment for testing Proper Motion
//#include "skycomponents/skymesh.h"
// END DEBUG

#include "skycomponents/skylabeler.h"

// DEBUG EDIT. Uncomment for testing Proper Motion
// You will also need to uncomment all related blocks
// from this file, starobject.h and also the trixel-boundaries
// block from lines 253 - 257 of skymapcomposite.cpp
//QVector<SkyPoint *> StarObject::Trail;
// END DEBUG

#include <KLocalizedString>

//----- Static Methods -----
//
double StarObject::reindexInterval( double pm )
{
    if ( pm < 1.0e-6) return 1.0e6;

    // arcminutes * sec/min * milliarcsec/sec centuries/year
    // / [milliarcsec/year] = centuries

    return 25.0 * 60.0 * 10.0 / pm;
}

StarObject::StarObject( dms r, dms d, float m,
                        const QString &n, const QString &n2,
                        const QString &sptype, double pmra, double pmdec,
                        double par, bool mult, bool var, int hd )
        : SkyObject (SkyObject::STAR, r, d, m, n, n2, QString()),
          PM_RA(pmra), PM_Dec(pmdec),
          Parallax(par), Multiplicity(mult), Variability(var)
{
    QByteArray spt = sptype.toLatin1();
    SpType[0] = spt[0];
    SpType[1] = spt[1];
    QString lname;
    if ( hasName() ) {
        lname = n;
        if ( hasName2() ) lname += " (" + gname() + ')';
    } else if ( hasName2() ) {
        lname = gname();
        //If genetive name exists, but no primary name, set primary name = genetive name.
        setName( gname() );
    }

    HD = hd;

    setLongName(lname);
    updateID = updateNumID = 0;
}

StarObject::StarObject( double r, double d, float m,
                        const QString &n, const QString &n2,
                        const QString &sptype, double pmra, double pmdec,
                        double par, bool mult, bool var, int hd )
    : SkyObject (SkyObject::STAR, r, d, m, n, n2, QString()),
      PM_RA(pmra), PM_Dec(pmdec),
      Parallax(par), Multiplicity(mult), Variability(var)
{
    QByteArray spt = sptype.toLatin1();
    SpType[0] = spt[0];
    SpType[1] = spt[1];

    QString lname;
    if ( hasName() ) {
        lname = n;
        if ( hasName2() )lname += " (" + gname() + ')';
    } else if ( hasName2() ) {
        lname = gname();
        //If genetive name exists, but no primary name, set primary name = genetive name.
        setName( gname() );
    }

    HD = hd;

    setLongName(lname);
    updateID = updateNumID = 0;
}

StarObject::StarObject( const StarObject &o ) :
    SkyObject (o),
    PM_RA(o.PM_RA),
    PM_Dec(o.PM_Dec),
    Parallax(o.Parallax),
    Multiplicity(o.Multiplicity),
    Variability(o.Variability),
    HD(o.HD)
{
    SpType[0] = o.SpType[0];
    SpType[1] = o.SpType[1];
    updateID = updateNumID = 0;
}

StarObject* StarObject::clone() const
{
    Q_ASSERT( typeid( this ) == typeid( static_cast<const StarObject *>( this ) ) ); // Ensure we are not slicing a derived class
    return new StarObject(*this);
}

void StarObject::init( const starData *stardata )
{
    double ra, dec;
    ra = stardata->RA / 1000000.0;
    dec = stardata->Dec / 100000.0;
    setType( SkyObject::STAR );
    setMag( stardata->mag / 100.0 );
    setRA0( ra );
    setDec0( dec );
    setRA( ra0() );
    setDec( dec0() );
    SpType[0] = stardata->spec_type[0];
    SpType[1] = stardata->spec_type[1];
    PM_RA = stardata->dRA / 10.0;
    PM_Dec = stardata->dDec / 10.0;
    Parallax = stardata->parallax / 10.0;
    Multiplicity = stardata->flags & 0x02;
    Variability = stardata->flags & 0x04 ;
    updateID = updateNumID = 0;
    HD = stardata->HD;
    B = V = 99.9;

    // DEBUG Edit. For testing proper motion. Uncomment all related blocks to test.
    // WARNING: You can debug only ONE STAR AT A TIME, because
    //          the StarObject::Trail is static. It has to be
    //          static, because otherwise, we can run into segfaults
    //          due to the memcpy() that we do to create stars
    /*
    testStar = false;
    if( stardata->HD == 103095 && Trail.size() == 0 ) {
      // Populate Trail with various positions
        qDebug() << "TEST STAR FOUND!";
        testStar = true;
        KSNumbers num( J2000 ); // Some estimate, doesn't matter.
        long double jy;
        for( jy = -10000.0; jy <= 10000.0; jy += 500.0 ) {
            num.updateValues( J2000 + jy * 365.238 );
            double ra, dec;
            getIndexCoords( &num, &ra, &dec );
            Trail.append( new SkyPoint( ra / 15.0, dec ) );
        }
        qDebug() << "Populated the star's trail with " << Trail.size() << " entries.";
    }
    */
    // END DEBUG.

    lastPrecessJD = J2000;

}

void StarObject::init( const deepStarData *stardata )
{
    double ra, dec, BV_Index;

    ra = stardata->RA / 1000000.0;
    dec = stardata->Dec / 100000.0;
    setType( SkyObject::STAR );

    if( stardata->V == 30000 && stardata->B != 30000 )
      setMag( ( stardata->B - 1600 ) / 1000.0 ); // FIXME: Is it okay to make up stuff like this?
    else
      setMag( stardata->V / 1000.0 );

    setRA0( ra );
    setDec0( dec );
    setRA( ra );
    setDec( dec );

    SpType[1] = '?';
    SpType[0] = 'B';
    if( stardata->B == 30000 || stardata->V == 30000 ) {
      BV_Index = -100;
      SpType[0] = '?';
    }
    else {
      BV_Index = ( stardata->B - stardata->V ) / 1000.0;
      ( BV_Index > 0.0 ) && ( SpType[0] = 'A' );
      ( BV_Index > 0.325 ) && ( SpType[0] = 'F' );
      ( BV_Index > 0.575 ) && ( SpType[0] = 'G' );
      ( BV_Index > 0.975 ) && ( SpType[0] = 'K' );
      ( BV_Index > 1.6 ) && ( SpType[0] = 'M' );
    }

    PM_RA = stardata->dRA / 100.0;
    PM_Dec = stardata->dDec / 100.0;
    Parallax = 0.0;
    Multiplicity = 0;
    Variability = 0;
    updateID = updateNumID = 0;
    B = stardata->B / 1000.0;
    V = stardata->V / 1000.0;
    lastPrecessJD = J2000;
}

void StarObject::setNames( QString name, QString name2 ) {
    QString lname;

    setName( name );

    setName2( name2 );

    if ( hasName() && name.startsWith("HD")==false) {
        lname = name;
        if ( hasName2() ) lname += " (" + gname() + ')';
    } else if ( hasName2() )
        lname = gname();
    setLongName(lname);
}
void StarObject::initPopupMenu( KSPopupMenu *pmenu ) {
#ifdef KSTARS_LITE
    Q_UNUSED(pmenu)
#else
    pmenu->createStarMenu( this );
#endif
}

void StarObject::updateCoords( const KSNumbers *num, bool , const CachingDms*, const CachingDms*, bool ) {
    //Correct for proper motion of stars.  Determine RA and Dec offsets.
    //Proper motion is given im milliarcsec per year by the pmRA() and pmDec() functions.
    //That is numerically identical to the number of arcsec per millenium, so multiply by
    //KSNumbers::julianMillenia() to find the offsets in arcsec.

    // Correction:  The method below computes the proper motion before the
    // precession.  If we precessed first then the direction of the proper
    // motion correction would depend on how far we've precessed.  -jbb
#ifdef PROFILE_UPDATECOORDS
    std::clock_t start, stop;
    start = std::clock();
#endif
    CachingDms saveRA = ra0(), saveDec = dec0();
    CachingDms newRA, newDec;

    getIndexCoords( num, newRA, newDec );

    setRA0( newRA );
    setDec0( newDec );
    SkyPoint::updateCoords( num );
    setRA0( saveRA );
    setDec0( saveDec );

#ifdef PROFILE_UPDATECOORDS
    stop = std::clock();
    updateCoordsCpuTime += double( stop - start )/double( CLOCKS_PER_SEC );
    ++starsUpdated;
#endif
}

bool StarObject::getIndexCoords( const KSNumbers *num, CachingDms &ra, CachingDms &dec )
{
    static double pmms;

    // =================== NOTE: CODE DUPLICATION ====================
    // If you modify this, please also modify the other getIndexCoords
    // ===============================================================
    //
    // Reason for code duplication is as follows:
    //
    // This method is designed to use CachingDms, i.e. we know we are
    // going to use the sine and cosine of the returned values.
    //
    // The other method is designed to avoid CachingDms and try to
    // compute as little trigonometry as possible when the ra/dec has
    // to be returned in double (used in SkyMesh::indexStar() for
    // example)
    //
    // Thus, the philosophy of writing code is different. Granted, we
    // don't need to optimize for the smaller star catalogs (which use
    // SkyMesh::indexStar()), but it is nevertheless a good idea,
    // given that getIndexCoords() shows up in callgrind as one of the
    // slightly more expensive operations.

    // Old, Incorrect Proper motion Computation.  We retain this in a
    // comment because we might want to use it to come up with a
    // linear approximation that's faster.
    //    double dra = pmRA() * num->julianMillenia() / ( cos( dec0().radians() ) * 3600.0 );
    //    double ddec = pmDec() * num->julianMillenia() / 3600.0;

    // Proper Motion Correction should be implemented as motion along a great
    // circle passing through the given (ra0, dec0) in a direction of
    // atan2( pmRA(), pmDec() ) to an angular distance given by the Magnitude of
    // PM times the number of Julian millenia since J2000.0

    pmms = pmMagnitudeSquared();

    if( std::isnan( pmms ) || pmms * num->julianMillenia() * num->julianMillenia() < 1. ) {
        // Ignore corrections
        ra = ra0();
        dec = dec0();
        return false;
    }

    double pm = pmMagnitude() * num->julianMillenia();   // Proper Motion in arcseconds

    double dir0 = ( ( pm > 0 ) ? atan2( pmRA(), pmDec() ) : atan2( -pmRA(), -pmDec() ) );  // Bearing, in radian

    ( pm < 0 ) && ( pm = -pm );

    double dst = (  pm * M_PI / ( 180.0 * 3600.0 ) );
    //    double phi = M_PI / 2.0 - dec0().radians();


    // Note: According to callgrind, dms::dms() + dms::setRadians()
    // takes ~ 40 CPU cycles, whereas, the advantage afforded by using
    // sincos() instead of sin() and cos() calls seems to be about 30
    // CPU cycles.

    // So it seems like it is not worth turning dir0 and dst into dms
    // objects and using SinCos(). However, caching the values of sin
    // and cos if we are going to reuse them avoids expensive (~120
    // CPU cycle) recomputation!
    CachingDms lat1, dtheta;
    double sinDst = sin( dst ), cosDst = cos( dst );
    lat1.setUsing_asin( dec0().sin() * cosDst + dec0().cos() * sinDst * cos( dir0 ) );
    dtheta.setUsing_atan2( sin( dir0 ) * sinDst * dec0().cos(),
                              cosDst - dec0().sin() * lat1.sin() );

    ra = ra0() + dtheta; // Use operator + to avoid trigonometry
    dec = lat1; // Need variable lat1 because dec may refer to dec0, so cannot construct result in-place

    //    *ra = ra0().Degrees() + dra;
    //    *dec = dec0().Degrees() + ddec;
    return true;
}


bool StarObject::getIndexCoords( const KSNumbers *num, double *ra, double *dec )
{
    static double pmms;

    // =================== NOTE: CODE DUPLICATION ====================
    // If you modify this, please also modify the other getIndexCoords
    // ===============================================================
    //
    // Reason for code duplication is as follows:
    //
    // This method is designed to avoid CachingDms and try to compute
    // as little trigonometry as possible when the ra/dec has to be
    // returned in double (used in SkyMesh::indexStar() for example)
    //
    // The other method is designed to use CachingDms, i.e. we know we
    // are going to use the sine and cosine of the returned values.
    //
    // Thus, the philosophy of writing code is different. Granted, we
    // don't need to optimize for the smaller star catalogs (which use
    // SkyMesh::indexStar()), but it is nevertheless a good idea,
    // given that getIndexCoords() shows up in callgrind as one of the
    // slightly more expensive operations.

    // Old, Incorrect Proper motion Computation.  We retain this in a
    // comment because we might want to use it to come up with a
    // linear approximation that's faster.
    //    double dra = pmRA() * num->julianMillenia() / ( cos( dec0().radians() ) * 3600.0 );
    //    double ddec = pmDec() * num->julianMillenia() / 3600.0;

    // Proper Motion Correction should be implemented as motion along a great
    // circle passing through the given (ra0, dec0) in a direction of
    // atan2( pmRA(), pmDec() ) to an angular distance given by the Magnitude of
    // PM times the number of Julian millenia since J2000.0

    pmms = pmMagnitudeSquared();

    if( std::isnan( pmms ) || pmms * num->julianMillenia() * num->julianMillenia() < 1. ) {
        // Ignore corrections
        *ra = ra0().Degrees();
        *dec = dec0().Degrees();
        return false;
    }

    double pm = pmMagnitude() * num->julianMillenia();   // Proper Motion in arcseconds

    double dir0 = ( ( pm > 0 ) ? atan2( pmRA(), pmDec() ) : atan2( -pmRA(), -pmDec() ) );  // Bearing, in radian

    ( pm < 0 ) && ( pm = -pm );

    double dst = (  pm * M_PI / ( 180.0 * 3600.0 ) );
    //    double phi = M_PI / 2.0 - dec0().radians();


    // Note: According to callgrind, dms::dms() + dms::setRadians()
    // takes ~ 40 CPU cycles, whereas, the advantage afforded by using
    // sincos() instead of sin() and cos() calls seems to be about 30
    // CPU cycles.

    // So it seems like it is not worth turning dir0 and dst into dms
    // objects and using SinCos(). However, caching the values of sin
    // and cos if we are going to reuse them avoids expensive (~120
    // CPU cycle) recomputation!
    dms lat1, dtheta;
    double sinDst = sin( dst ), cosDst = cos( dst );
    double sinLat1 = dec0().sin() * cosDst + dec0().cos() * sinDst * cos( dir0 );
    lat1.setRadians( asin( sinLat1 ) );
    dtheta.setRadians( atan2( sin( dir0 ) * sinDst * dec0().cos(),
                              cosDst - dec0().sin() * sinLat1 ) );

    // Using dms instead, to ensure that the numbers are in the right range.
    dms finalRA( ra0().Degrees() + dtheta.Degrees() );

    *ra = finalRA.Degrees();
    *dec = lat1.Degrees();

    //    *ra = ra0().Degrees() + dra;
    //    *dec = dec0().Degrees() + ddec;
    return true;
}

void StarObject::JITupdate()
{
    static KStarsData *data = KStarsData::Instance();

    if ( updateNumID != data->updateNumID() ) {
        // TODO: This can be optimized and reorganized further in a better manner.
        // Maybe we should do this only for stars, since this is really a slow step only for stars
        Q_ASSERT( std::isfinite( lastPrecessJD ) );

        if  ( Options::alwaysRecomputeCoordinates() ||
            ( Options::useRelativistic() && checkBendLight() ) ||
            fabs(lastPrecessJD - data->updateNum()->getJD() ) >= 0.00069444) // Update is once per solar minute
       {
            // Short circuit right here, if recomputing coordinates is not required. NOTE: POTENTIALLY DANGEROUS
            updateCoords( data->updateNum() );
       }

        updateNumID = data->updateNumID();
    }
    EquatorialToHorizontal( data->lst(), data->geo()->lat() );
    updateID = data->updateID();
}

QString StarObject::sptype( void ) const {
    return QString( QByteArray(SpType, 2) );
}

char StarObject::spchar() const
{
    return SpType[0];
}

QString StarObject::gname( bool useGreekChars ) const {
    if(!name2().isEmpty())
        return greekLetter( useGreekChars ) + ' ' + constell();
    else
        return QString();
}

QString StarObject::greekLetter( bool gchar ) const {
    QString code = name2().left(3);
    QString letter = code;  //in case genitive name is *not* a Greek letter
    int alpha = 0x03B1;

    auto checkAndGreekify = [&code, gchar, alpha, &letter]( const QString &abbrev, int unicodeOffset, const QString &expansion ) {
        if ( code == abbrev ) gchar ? letter = QString( QChar( alpha + unicodeOffset ) ) : letter = expansion;
    };

    checkAndGreekify( "alp", 0, i18n("alpha") );
    checkAndGreekify( "bet", 1, i18n("beta") );
    checkAndGreekify( "gam", 2, i18n("gamma") );
    checkAndGreekify( "del", 3, i18n("delta") );
    checkAndGreekify( "eps", 4, i18n("epsilon") );
    checkAndGreekify( "zet", 5, i18n("zeta") );
    checkAndGreekify( "eta", 6, i18n("eta") );
    checkAndGreekify( "the", 7, i18n("theta") );
    checkAndGreekify( "iot", 8, i18n("iota") );
    checkAndGreekify( "kap", 9, i18n("kappa") );
    checkAndGreekify( "lam",10, i18n("lambda") );
    checkAndGreekify( "mu ",11, i18n("mu") );
    checkAndGreekify( "nu ",12, i18n("nu") );
    checkAndGreekify( "xi ",13, i18n("xi") );
    checkAndGreekify( "omi",14, i18n("omicron") );
    checkAndGreekify( "pi ",15, i18n("pi") );
    checkAndGreekify( "rho",16, i18n("rho") );
    //there are two unicode symbols for sigma;
    //skip the first one, the second is more widely used
    checkAndGreekify( "sig",18, i18n("sigma") );
    checkAndGreekify( "tau",19, i18n("tau") );
    checkAndGreekify( "ups",20, i18n("upsilon") );
    checkAndGreekify( "phi",21, i18n("phi") );
    checkAndGreekify( "chi",22, i18n("chi") );
    checkAndGreekify( "psi",23, i18n("psi") );
    checkAndGreekify( "ome",24, i18n("omega") );

    if ( name2().length() && name2().mid(3,1) != " " )
        letter += '[' + name2().mid(3,1) + ']';

    return letter;
}

QString StarObject::constell() const { // FIXME: Move this somewhere else, make this static, and give it a better name. Mostly for code cleanliness. Also, try to put it in a DB.
    QString code = name2().mid(4,3);

    return KSUtils::constGenetiveFromAbbrev( code );
}

// The two routines below seem overly complicated but at least they are doing
// the right thing now.  Please resist the temptation to simplify them unless
// you are prepared to ensure there is no ugly label overlap for all 8 cases
// they deal with ( drawName x DrawMag x star-has-name).  -jbb

QString StarObject::nameLabel( bool drawName, bool drawMag ) const
{
    QString sName;
    if ( drawName ) {
        if ( translatedName() != i18n("star") && ! translatedName().isEmpty() )
            sName = translatedName();
        else if ( ! gname().trimmed().isEmpty() )
            sName = gname( true );
        else {
            if ( drawMag )
                return ("[" + QLocale().toString( mag(), 'f', 1 ) + "m]");
        }
        if ( ! drawMag )
            return sName;
        else
            return sName + " [" + QLocale().toString( mag(), 'f', 1 ) + "m]";
    }
    return ("[" + QLocale().toString( mag(), 'f', 1 ) + "m]");
}

//If this works, we can maybe get rid of customLabel() and nameLabel()??
QString StarObject::labelString() const {
    return nameLabel( Options::showStarNames(), Options::showStarMagnitudes() );
}

double StarObject::labelOffset() const {
    return (6. + 0.5*( 5.0 - mag() ) + 0.01*( Options::zoomFactor()/500. ) );
}

SkyObject::UID StarObject::getUID() const
{
    // mag takes 10 bit
    SkyObject::UID m = mag()*10;
    if( m < 0 ) m = 0;

    // Both RA & dec fits in 24-bits
    SkyObject::UID ra  = ra0().Degrees() * 36000;
    SkyObject::UID dec = (ra0().Degrees()+91) * 36000;

    Q_ASSERT("Magnitude is expected to fit into 10bits" && m>=0 && m<(1<<10));
    Q_ASSERT("RA should fit into 24bits"  && ra>=0  && ra <(1<<24));
    Q_ASSERT("Dec should fit into 24bits" && dec>=0 && dec<(1<<24));

    return (SkyObject::UID_STAR << 60) | (m << 48) | (ra << 24) | dec;
}
