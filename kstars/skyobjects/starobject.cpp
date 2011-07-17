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

#include <QColor>
#include <QPainter>
#include <QFontMetricsF>
#include <QPixmap>
#include <kdebug.h>

#include "kspopupmenu.h"
#include "ksnumbers.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"

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
    QByteArray spt = sptype.toAscii();
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
    QByteArray spt = sptype.toAscii();
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
    setRA( ra );
    setDec( dec );
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
        kDebug() << "TEST STAR FOUND!";
        testStar = true;
        KSNumbers num( J2000 ); // Some estimate, doesn't matter.
        long double jy;
        for( jy = -10000.0; jy <= 10000.0; jy += 500.0 ) {
            num.updateValues( J2000 + jy * 365.238 );
            double ra, dec;
            getIndexCoords( &num, &ra, &dec );
            Trail.append( new SkyPoint( ra / 15.0, dec ) );
        }
        kDebug() << "Populated the star's trail with " << Trail.size() << " entries.";
    }
    */
    // END DEBUG.


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
}

void StarObject::setNames( QString name, QString name2 ) {
    QString lname;

    setName( name );

    setName2( name2 );

    if ( hasName() ) {
        lname = name;
        if ( hasName2() ) lname += " (" + gname() + ')';
    } else if ( hasName2() )
        lname = gname();
    setLongName(lname);
}

void StarObject::initPopupMenu( KSPopupMenu *pmenu ) {
    pmenu->createStarMenu( this );
}

void StarObject::updateCoords( KSNumbers *num, bool , const dms*, const dms* ) {
    //Correct for proper motion of stars.  Determine RA and Dec offsets.
    //Proper motion is given im milliarcsec per year by the pmRA() and pmDec() functions.
    //That is numerically identical to the number of arcsec per millenium, so multiply by
    //KSNumbers::julianMillenia() to find the offsets in arcsec.

    // Correction:  The method below computes the proper motion before the
    // precession.  If we precessed first then the direction of the proper
    // motion correction would depend on how far we've precessed.  -jbb
    double saveRA = ra0().Hours();
    double saveDec = dec0().Degrees();

    double newRA, newDec;

    // Old, Incorrect Proper motion Computation:
    //    setRA0( ra0().Hours() + pmRA()*num->julianMillenia() / (15. * cos( dec0().radians() ) * 3600.) );
    //    setDec0( dec0().Degrees() + pmDec()*num->julianMillenia() / 3600. );

    getIndexCoords( num, &newRA, &newDec );
    newRA /= 15.0;                           // getIndexCoords returns in Degrees, while we want the RA in Hours
    setRA0( newRA );
    setDec0( newDec );

    SkyPoint::updateCoords( num );
    setRA0( saveRA );
    setDec0( saveDec );
}

void StarObject::getIndexCoords( KSNumbers *num, double *ra, double *dec )
{

    // Old, Incorrect Proper motion Computation:
    //    double dra = pmRA() * num->julianMillenia() / ( cos( dec0().radians() ) * 3600.0 );
    //    double ddec = pmDec() * num->julianMillenia() / 3600.0;

    // Proper Motion Correction should be implemented as motion along a great 
    // circle passing through the given (ra0, dec0) in a direction of 
    // atan2( pmRA(), pmDec() ) to an angular distance given by the Magnitude of 
    // PM times the number of Julian millenia since J2000.0

    double pm = pmMagnitude() * num->julianMillenia();   // Proper Motion in arcseconds

    if( pm < 1. ) {
        // Ignore corrections
        *ra = ra0().Degrees();
        *dec = dec0().Degrees();
        return;
    }

    double dir0 = ( pm > 0 ) ? atan2( pmRA(), pmDec() ) : atan2( -pmRA(), -pmDec() );  // Bearing, in radian

    ( pm < 0 ) && ( pm = -pm );

    double dst = pm * M_PI / ( 180.0 * 3600.0 );
    //    double phi = M_PI / 2.0 - dec0().radians();

    dms lat1, dtheta;
    lat1.setRadians( asin( dec0().sin() * cos( dst ) +
                           dec0().cos() * sin( dst ) * cos( dir0 ) ) );
    dtheta.setRadians( atan2( sin( dir0 ) * sin( dst ) * dec0().cos(),
                              cos( dst ) - dec0().sin() * lat1.sin() ) );

    // Using dms instead, to ensure that the numbers are in the right range.
    dms finalRA( ra0().Degrees() + dtheta.Degrees() );

    *ra = finalRA.Degrees();
    *dec = lat1.Degrees();

    //    *ra = ra0().Degrees() + dra;
    //    *dec = dec0().Degrees() + ddec;
}

double StarObject::pmMagnitude()
{
    double cosDec = dec().cos();
    return sqrt( cosDec * cosDec * pmRA() * pmRA() + pmDec() * pmDec() );
}

void StarObject::JITupdate( KStarsData* data )
{
    updateID = data->updateID();
    if ( updateNumID != data->updateNumID() ) {
        updateCoords( data->updateNum() );
        updateNumID = data->updateNumID();
    }
    EquatorialToHorizontal( data->lst(), data->geo()->lat() );
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

    if ( code == "alp" ) gchar ? letter = QString( QChar(alpha + 0) ) : letter = i18n("alpha");
    if ( code == "bet" ) gchar ? letter = QString( QChar(alpha + 1) ) : letter = i18n("beta");
    if ( code == "gam" ) gchar ? letter = QString( QChar(alpha + 2) ) : letter = i18n("gamma");
    if ( code == "del" ) gchar ? letter = QString( QChar(alpha + 3) ) : letter = i18n("delta");
    if ( code == "eps" ) gchar ? letter = QString( QChar(alpha + 4) ) : letter = i18n("epsilon");
    if ( code == "zet" ) gchar ? letter = QString( QChar(alpha + 5) ) : letter = i18n("zeta");
    if ( code == "eta" ) gchar ? letter = QString( QChar(alpha + 6) ) : letter = i18n("eta");
    if ( code == "the" ) gchar ? letter = QString( QChar(alpha + 7) ) : letter = i18n("theta");
    if ( code == "iot" ) gchar ? letter = QString( QChar(alpha + 8) ) : letter = i18n("iota");
    if ( code == "kap" ) gchar ? letter = QString( QChar(alpha + 9) ) : letter = i18n("kappa");
    if ( code == "lam" ) gchar ? letter = QString( QChar(alpha +10) ) : letter = i18n("lambda");
    if ( code == "mu " ) gchar ? letter = QString( QChar(alpha +11) ) : letter = i18n("mu");
    if ( code == "nu " ) gchar ? letter = QString( QChar(alpha +12) ) : letter = i18n("nu");
    if ( code == "xi " ) gchar ? letter = QString( QChar(alpha +13) ) : letter = i18n("xi");
    if ( code == "omi" ) gchar ? letter = QString( QChar(alpha +14) ) : letter = i18n("omicron");
    if ( code == "pi " ) gchar ? letter = QString( QChar(alpha +15) ) : letter = i18n("pi");
    if ( code == "rho" ) gchar ? letter = QString( QChar(alpha +16) ) : letter = i18n("rho");
    //there are two unicode symbols for sigma;
    //skip the first one, the second is more widely used
    if ( code == "sig" ) gchar ? letter = QString( QChar(alpha +18) ) : letter = i18n("sigma");
    if ( code == "tau" ) gchar ? letter = QString( QChar(alpha +19) ) : letter = i18n("tau");
    if ( code == "ups" ) gchar ? letter = QString( QChar(alpha +20) ) : letter = i18n("upsilon");
    if ( code == "phi" ) gchar ? letter = QString( QChar(alpha +21) ) : letter = i18n("phi");
    if ( code == "chi" ) gchar ? letter = QString( QChar(alpha +22) ) : letter = i18n("chi");
    if ( code == "psi" ) gchar ? letter = QString( QChar(alpha +23) ) : letter = i18n("psi");
    if ( code == "ome" ) gchar ? letter = QString( QChar(alpha +24) ) : letter = i18n("omega");

    if ( name2().length() && name2().mid(3,1) != " " )
        letter += '[' + name2().mid(3,1) + ']';

    return letter;
}

QString StarObject::constell() const {
    QString code = name2().mid(4,3);
    if ( code == "And" ) return QString("Andromedae");
    if ( code == "Ant" ) return QString("Antliae");
    if ( code == "Aps" ) return QString("Apodis");
    if ( code == "Aqr" ) return QString("Aquarii");
    if ( code == "Aql" ) return QString("Aquilae");
    if ( code == "Ara" ) return QString("Arae");
    if ( code == "Ari" ) return QString("Arietis");
    if ( code == "Aur" ) return QString("Aurigae");
    if ( code == "Boo" ) return QString("Bootis");
    if ( code == "Cae" ) return QString("Caeli");
    if ( code == "Cam" ) return QString("Camelopardalis");
    if ( code == "Cnc" ) return QString("Cancri");
    if ( code == "CVn" ) return QString("Canum Venaticorum");
    if ( code == "CMa" ) return QString("Canis Majoris");
    if ( code == "CMi" ) return QString("Canis Minoris");
    if ( code == "Cap" ) return QString("Capricorni");
    if ( code == "Car" ) return QString("Carinae");
    if ( code == "Cas" ) return QString("Cassiopeiae");
    if ( code == "Cen" ) return QString("Centauri");
    if ( code == "Cep" ) return QString("Cephei");
    if ( code == "Cet" ) return QString("Ceti");
    if ( code == "Cha" ) return QString("Chamaeleontis");
    if ( code == "Cir" ) return QString("Circini");
    if ( code == "Col" ) return QString("Columbae");
    if ( code == "Com" ) return QString("Comae Berenices");
    if ( code == "CrA" ) return QString("Coronae Austrinae");
    if ( code == "CrB" ) return QString("Coronae Borealis");
    if ( code == "Crv" ) return QString("Corvi");
    if ( code == "Crt" ) return QString("Crateris");
    if ( code == "Cru" ) return QString("Crucis");
    if ( code == "Cyg" ) return QString("Cygni");
    if ( code == "Del" ) return QString("Delphini");
    if ( code == "Dor" ) return QString("Doradus");
    if ( code == "Dra" ) return QString("Draconis");
    if ( code == "Equ" ) return QString("Equulei");
    if ( code == "Eri" ) return QString("Eridani");
    if ( code == "For" ) return QString("Fornacis");
    if ( code == "Gem" ) return QString("Geminorum");
    if ( code == "Gru" ) return QString("Gruis");
    if ( code == "Her" ) return QString("Herculis");
    if ( code == "Hor" ) return QString("Horologii");
    if ( code == "Hya" ) return QString("Hydrae");
    if ( code == "Hyi" ) return QString("Hydri");
    if ( code == "Ind" ) return QString("Indi");
    if ( code == "Lac" ) return QString("Lacertae");
    if ( code == "Leo" ) return QString("Leonis");
    if ( code == "LMi" ) return QString("Leonis Minoris");
    if ( code == "Lep" ) return QString("Leporis");
    if ( code == "Lib" ) return QString("Librae");
    if ( code == "Lup" ) return QString("Lupi");
    if ( code == "Lyn" ) return QString("Lyncis");
    if ( code == "Lyr" ) return QString("Lyrae");
    if ( code == "Men" ) return QString("Mensae");
    if ( code == "Mic" ) return QString("Microscopii");
    if ( code == "Mon" ) return QString("Monocerotis");
    if ( code == "Mus" ) return QString("Muscae");
    if ( code == "Nor" ) return QString("Normae");
    if ( code == "Oct" ) return QString("Octantis");
    if ( code == "Oph" ) return QString("Ophiuchi");
    if ( code == "Ori" ) return QString("Orionis");
    if ( code == "Pav" ) return QString("Pavonis");
    if ( code == "Peg" ) return QString("Pegasi");
    if ( code == "Per" ) return QString("Persei");
    if ( code == "Phe" ) return QString("Phoenicis");
    if ( code == "Pic" ) return QString("Pictoris");
    if ( code == "Psc" ) return QString("Piscium");
    if ( code == "PsA" ) return QString("Piscis Austrini");
    if ( code == "Pup" ) return QString("Puppis");
    if ( code == "Pyx" ) return QString("Pyxidis");
    if ( code == "Ret" ) return QString("Reticuli");
    if ( code == "Sge" ) return QString("Sagittae");
    if ( code == "Sgr" ) return QString("Sagittarii");
    if ( code == "Sco" ) return QString("Scorpii");
    if ( code == "Scl" ) return QString("Sculptoris");
    if ( code == "Sct" ) return QString("Scuti");
    if ( code == "Ser" ) return QString("Serpentis");
    if ( code == "Sex" ) return QString("Sextantis");
    if ( code == "Tau" ) return QString("Tauri");
    if ( code == "Tel" ) return QString("Telescopii");
    if ( code == "Tri" ) return QString("Trianguli");
    if ( code == "TrA" ) return QString("Trianguli Australis");
    if ( code == "Tuc" ) return QString("Tucanae");
    if ( code == "UMa" ) return QString("Ursae Majoris");
    if ( code == "UMi" ) return QString("Ursae Minoris");
    if ( code == "Vel" ) return QString("Velorum");
    if ( code == "Vir" ) return QString("Virginis");
    if ( code == "Vol" ) return QString("Volantis");
    if ( code == "Vul" ) return QString("Vulpeculae");

    return QString();
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
                return KGlobal::locale()->formatNumber( mag(), 1 );
        }
        if ( ! drawMag )
            return sName;
        else
            return sName + ' ' + KGlobal::locale()->formatNumber( mag(), 1 );
    }
    return KGlobal::locale()->formatNumber( mag(), 1 );
}

QString StarObject::customLabel( bool drawName, bool drawMag )
{
    QString sName;
    if ( translatedName() != i18n("star") && ! translatedName().isEmpty() )
        sName = translatedName();
    else if ( ! gname().trimmed().isEmpty() )
        sName = gname( true );
    else
        sName = i18n("star");

    if ( drawMag  && drawName ) {
        if ( sName == i18n("star") )
            return KGlobal::locale()->formatNumber( mag(), 1 ) + ", " + sName;
        else
            return sName + ' ' + KGlobal::locale()->formatNumber( mag(), 1 );
    }
    else if ( drawMag && ! drawName )
        return KGlobal::locale()->formatNumber( mag(), 1 ) + ", " + sName;

    return sName;
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
