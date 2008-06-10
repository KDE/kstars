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

#include "skycomponents/skylabeler.h"

QMap<QString, QColor> StarObject::ColorMap;
QHash<QString, QPixmap> StarObject::StarImage;

//----- Static Methods -----
//
double StarObject::reindexInterval( double pm )
{
    if ( pm < 1.0e-6) return 1.0e6;

    // arcminutes * sec/min * milliarcsec/sec centuries/year
    // / [milliarcsec/year] = centuries

    return 25.0 * 60.0 * 10.0 / pm;
}

StarObject::StarObject( StarObject &o )
        : SkyObject (o)
{
    SpType[0] = o.SpType[0];
    SpType[1] = o.SpType[1];
    //SONAME: deprecated (?) JH
    //  soName = o.soName;
    PM_RA = o.pmRA();
    PM_Dec = o.pmDec();
    Parallax = o.parallax();
    Multiplicity = o.isMultiple();
    Variability = o.isVariable();
    updateID = updateNumID = 0;
}

StarObject::StarObject( dms r, dms d, float m,
                        const QString &n, const QString &n2,
                        const QString &sptype, double pmra, double pmdec,
                        double par, bool mult, bool var )
        : SkyObject (SkyObject::STAR, r, d, m, n, n2, QString()),
          PM_RA(pmra), PM_Dec(pmdec),
          Parallax(par), Multiplicity(mult), Variability(var)
        // SONAME deprecated //, soName( 0 )
{

    const char *spt = (const char *)sptype.toAscii();
    SpType[0] = spt[0];
    SpType[1] = spt[1];
    QString lname;
    if ( hasName() ) {
        lname = n;
        if ( hasName2() ) lname += " (" + gname() + ')';
    } else if ( hasName2() )
        lname = gname();

    //If genetive name exists, but no primary name, set primary name = genetive name.
    if ( hasName2() && !hasName() ) {
        setName( gname() );
    }

    setLongName(lname);
    updateID = updateNumID = 0;
}

StarObject::StarObject( double r, double d, float m,
                        const QString &n, const QString &n2,
                        const QString &sptype, double pmra, double pmdec,
                        double par, bool mult, bool var )
    : SkyObject (SkyObject::STAR, r, d, m, n, n2, QString()),
      PM_RA(pmra), PM_Dec(pmdec),
      Parallax(par), Multiplicity(mult), Variability(var)
      // SONAME deprecated //, soName( 0 )
{
    const char *spt = (const char *)sptype.toAscii();
    SpType[0] = spt[0];
    SpType[1] = spt[1];
    
    QString lname;
    if ( hasName() ) {
        lname = n;
        if ( hasName2() )lname += " (" + gname() + ')';
    } else if ( hasName2() )
        lname = gname();
    
    setLongName(lname);
    updateID = updateNumID = 0;
}


// DEPRECATED
/*
void StarObject::init(double r, double d, float m, const QString &sptype, double pmra, 
                      double pmdec, double par, bool mult, bool var) 
{
    setType( SkyObject::STAR );
    setMag( m );
    setRA0( r );
    setDec0( d );
    setRA( r );
    setDec( d );
    const char *spt = (const char *)sptype.toAscii();
    SpType[0] = spt[0];
    SpType[1] = spt[1];
    PM_RA = pmra;
    PM_Dec = pmdec;
    Parallax = par;
    Multiplicity = mult;
    Variability = var ;

    //    setLongName(i18n("star"));
    updateID = updateNumID = 0;
}
*/

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


void StarObject::initImages() {
    SkyMap *map = SkyMap::Instance();
    double scale = 1.0;

    if ( map && map->scale() > 1.0 ) scale = map->scale();

    if ( Options::starColorMode() == 0 ) { //Real colors
        ColorMap.insert( "O", QColor::fromRgb(   0,   0, 255 ) );
        ColorMap.insert( "B", QColor::fromRgb(   0, 200, 255 ) );
        ColorMap.insert( "A", QColor::fromRgb(   0, 255, 255 ) );
        ColorMap.insert( "F", QColor::fromRgb( 200, 255, 100 ) );
        ColorMap.insert( "G", QColor::fromRgb( 255, 255,   0 ) );
        ColorMap.insert( "K", QColor::fromRgb( 255, 100,   0 ) );
        ColorMap.insert( "M", QColor::fromRgb( 255,   0,   0 ) );
    } else if ( Options::starColorMode() == 1 ) { //Red stars
        ColorMap.insert( "O", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "B", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "A", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "F", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "G", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "K", QColor::fromRgb( 255,   0,   0 ) );
        ColorMap.insert( "M", QColor::fromRgb( 255,   0,   0 ) );
    } else if ( Options::starColorMode() == 2 ) { //Black stars
        ColorMap.insert( "O", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "B", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "A", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "F", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "G", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "K", QColor::fromRgb(   0,   0,   0 ) );
        ColorMap.insert( "M", QColor::fromRgb(   0,   0,   0 ) );
    } else if ( Options::starColorMode() == 3 ) { //White stars
        ColorMap.insert( "O", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "B", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "A", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "F", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "G", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "K", QColor::fromRgb( 255, 255, 255 ) );
        ColorMap.insert( "M", QColor::fromRgb( 255, 255, 255 ) );
    }

    foreach ( const QString &color, ColorMap.keys() ) {
        QString imKey = color+"14";
        QPixmap BigImage( int(14*scale), int(14*scale) ); 
        BigImage.fill( Qt::transparent );

        QPainter p;
        p.begin( &BigImage );
        p.setRenderHint(QPainter::Antialiasing, true );

        //Set the width of the pen according to starColorIntensity()
        float w=2.0*scale;
        if ( Options::starColorMode() == 0 ) {
            w = w*float(Options::starColorIntensity())/4.0;
        }

        p.setPen( QPen(ColorMap[color], w) );
        if ( Options::starColorMode() == 0 ) {
            p.setBrush( QColor(Qt::white) );
        } else {
            p.setBrush( p.pen().color() );
        }

        p.drawEllipse( QRectF( 2*scale, 2*scale, 10*scale, 10*scale ) );
        p.end();
        StarImage.insert( imKey, BigImage );

        for ( int size = 13; size > 0; size-- ) {
            imKey = color+QString("%1").arg(size);
            StarImage.insert( imKey, BigImage.scaled( size*scale, size*scale, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
        }
    }
}

void StarObject::showPopupMenu( KSPopupMenu *pmenu, const QPoint &pos ) {
    pmenu->createStarMenu( this ); pmenu->popup( pos );
}

void StarObject::updateCoords( KSNumbers *num, bool , const dms*, const dms* ) {
    //Correct for proper motion of stars.  Determine RA and Dec offsets.
    //Proper motion is given im milliarcsec per year by the pmRA() and pmDec() functions.
    //That is numerically identical to the number of arcsec per millenium, so multiply by
    //KSNumbers::julianMillenia() to find the offsets in arcsec.

    // Correction:  The method below computes the proper motion before the
    // precession.  If we precessed first then the direction of the proper
    // motion correction would depend on how far we've precessed.  -jbb
    double saveRA = ra0()->Hours();
    double saveDec = dec0()->Degrees();

    setRA0( ra0()->Hours() + pmRA()*num->julianMillenia() / (15. * cos( dec0()->radians() ) * 3600.) );
    setDec0( dec0()->Degrees() + pmDec()*num->julianMillenia() / 3600. );

    SkyPoint::updateCoords( num );
    setRA0( saveRA );
    setDec0( saveDec );
}

void StarObject::getIndexCoords( KSNumbers *num, double *ra, double *dec )
{
    double dra = pmRA() * num->julianMillenia() / ( cos( dec0()->radians() ) * 3600.0 );
    double ddec = pmDec() * num->julianMillenia() / 3600.0;

    *ra = ra0()->Degrees() + dra;
    *dec = dec0()->Degrees() + ddec;
}

double StarObject::pmMagnitude()
{
    return sqrt( pmRA() * pmRA() + pmDec() * pmDec() );
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
    return (QString)QByteArray(SpType, 2);
}

QString StarObject::gname( bool useGreekChars ) const {
    return greekLetter( useGreekChars ) + ' ' + constell();
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

QString StarObject::constell( void ) const {
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

QColor StarObject::color() const {
    return ColorMap[QString(QChar(SpType[0]))];
}

void StarObject::updateColors( bool desaturateColors, int saturation ) {
    //First set colors to their fully-saturated values
    ColorMap.insert( "O", QColor::fromRgb(   0,   0, 255 ) );
    ColorMap.insert( "B", QColor::fromRgb(   0, 200, 255 ) );
    ColorMap.insert( "A", QColor::fromRgb(   0, 255, 255 ) );
    ColorMap.insert( "F", QColor::fromRgb( 200, 255, 100 ) );
    ColorMap.insert( "G", QColor::fromRgb( 255, 255,   0 ) );
    ColorMap.insert( "K", QColor::fromRgb( 255, 100,   0 ) );
    ColorMap.insert( "M", QColor::fromRgb( 255,   0,   0 ) );

    if ( ! desaturateColors ) return;

    //Desaturate the star colors by mixing in white
    float sat = 0.1*saturation; //sat is between 0.0 and 1.0
    int r, g, b;
    QMapIterator<QString, QColor> it(ColorMap);
    while ( it.hasNext() ) {
        it.next();
        r = int( 255.0*(1.0 - sat) + it.value().red()*sat   );
        g = int( 255.0*(1.0 - sat) + it.value().green()*sat );
        b = int( 255.0*(1.0 - sat) + it.value().blue()*sat  );

        ColorMap[ it.key() ] = QColor::fromRgb( r, g, b );
    }
}

void StarObject::draw( QPainter &psky, float x, float y, float size,
                       bool useRealColors, int scIntensity, bool /*showMultiple*/ ) {

    int isize = int(size);
    if ( isize >= 14 ) {
        isize = 14;
    }
    QString imKey = SpType[0] + QString("%1").arg(isize);
    float offset = 0.5*StarImage[imKey].width();
    psky.drawPixmap( QPointF(x-offset, y-offset), StarImage[imKey] );

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
    return SkyMap::Instance()->scale() * 
        (6. + 0.5*( 5.0 - mag() ) + 0.01*( Options::zoomFactor()/500. ) );
}
