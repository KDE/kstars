/***************************************************************************
                          ksutils.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Mon Jan  7 10:48:09 EST 2002
    copyright            : (C) 2002 by Mark Hollomon
    email                : mhh@mindspring.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "ksutils.h"

#include "deepskyobject.h"
#include "skyobject.h"
#include "starobject.h"
#include "Options.h"

#include <QFile>
#include <QtNumeric>
#include <QUrl>
#include <QStandardPaths>

#ifndef KSTARS_LITE
#include <KMessageBox>
#endif

#include <cmath>
#include "auxiliary/kspaths.h"

namespace KSUtils {


bool openDataFile( QFile &file, const QString &s ) {
    QString FileName = KSPaths::locate(QStandardPaths::GenericDataLocation, s );
    if ( !FileName.isNull() ) {
        file.setFileName( FileName );
        return file.open( QIODevice::ReadOnly );
    }
    return false;
}

QString getDSSURL( const SkyPoint * const p ) {
        const DeepSkyObject *dso = 0;
        double height, width;

        double dss_default_size = Options::defaultDSSImageSize();
        double dss_padding = Options::dSSPadding();

        Q_ASSERT( p );
        Q_ASSERT( dss_default_size > 0.0 && dss_padding >= 0.0 );

        dso = dynamic_cast<const DeepSkyObject *>( p );

        // Decide what to do about the height and width
        if( dso ) {
            // For deep-sky objects, use their height and width information
            double a, b, pa;
            a = dso->a();
            b = dso->a() * dso->e(); // Use a * e instead of b, since e() returns 1 whenever one of the dimensions is zero. This is important for circular objects
            pa = dso->pa() * dms::DegToRad;

            // We now want to convert a, b, and pa into an image
            // height and width -- i.e. a dRA and dDec.
            // DSS uses dDec for height and dRA for width. (i.e. "top" is north in the DSS images, AFAICT)
            // From some trigonometry, assuming we have a rectangular object (worst case), we need:
            width = a * sin( pa ) + b * cos( pa );
            height = a * cos( pa ) + b * sin( pa );
            // 'a' and 'b' are in arcminutes, so height and width are in arcminutes

            // Pad the RA and Dec, so that we show more of the sky than just the object.
            height += dss_padding;
            width += dss_padding;
        }
        else {
            // For a generic sky object, we don't know what to do. So
            // we just assume the default size.
            height = width = dss_default_size;
        }
        // There's no point in tiny DSS images that are smaller than dss_default_size
        if( height < dss_default_size )
            height = dss_default_size;
        if( width < dss_default_size )
            width = dss_default_size;

        return getDSSURL( p->ra0(), p->dec0(), width, height );
}

QString getDSSURL( const dms &ra, const dms &dec, float width, float height, const QString & type) {
    const QString URLprefix( "http://archive.stsci.edu/cgi-bin/dss_search?" );
    QString URLsuffix = QString( "&e=J2000&f=%1&c=none&fov=NONE" ).arg(type);
    const double dss_default_size = Options::defaultDSSImageSize();

    char decsgn = ( dec.Degrees() < 0.0 ) ? '-' : '+';
    int dd = abs( dec.degree() );
    int dm = abs( dec.arcmin() );
    int ds = abs( dec.arcsec() );

    // Infinite and NaN sizes are replaced by the default size and tiny DSS images are resized to default size
    if( !qIsFinite( height ) || height <= 0.0 )
        height = dss_default_size;
    if( !qIsFinite( width ) || width <= 0.0)
        width = dss_default_size;

    // DSS accepts images that are no larger than 75 arcminutes
    if( height > 75.0 )
        height = 75.0;
    if( width > 75.0 )
        width = 75.0;

    QString RAString, DecString, SizeString;
    DecString = DecString.sprintf( "&d=%c%02d+%02d+%02d", decsgn, dd, dm, ds );
    RAString  = RAString.sprintf( "r=%02d+%02d+%02d", ra.hour(), ra.minute(), ra.second() );
    SizeString = SizeString.sprintf( "&h=%02.1f&w=%02.1f", height, width );

    return ( URLprefix + RAString + DecString + SizeString + URLsuffix );
}

QString toDirectionString( dms angle ) {
    // TODO: Instead of doing it this way, it would be nicer to
    // compute the string to arbitrary precision. Although that will
    // not be easy to localize.  (Consider, for instance, Indian
    // languages that have special names for the intercardinal points)
    // -- asimha

    static const char *directions[] = {
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "N"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NNE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "ENE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "E"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "ESE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SSE"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "S"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SSW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "SW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "WSW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "W"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "WNW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NW"),
        I18N_NOOP2( "Abbreviated cardinal / intercardinal etc. direction", "NNW"),
        I18N_NOOP2( "Unknown cardinal / intercardinal direction", "???")
    };

    int index = (int)( (angle.reduce().Degrees() + 11.25) / 22.5); // A number between 0 and 16 (inclusive) is expected
    if( index < 0 || index > 16 )
        index = 17; // Something went wrong.
    else
        index = ( ( index == 16 ) ? 0 : index );

    return i18nc( "Abbreviated cardinal / intercardinal etc. direction", directions[ index ] );
}

QList<SkyObject *> * castStarObjListToSkyObjList( QList<StarObject *> *starObjList ) {
    QList<SkyObject *> *skyObjList = new QList<SkyObject *>();
    foreach( StarObject *so, *starObjList ) {
        skyObjList->append( so );
    }
    return skyObjList;
}


QString constGenetiveFromAbbrev( const QString &code ) {
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
    return code;
}

QString constNameFromAbbrev( const QString &code ) {
    if ( code == "And" ) return QString("Andromeda");
    if ( code == "Ant" ) return QString("Antlia");
    if ( code == "Aps" ) return QString("Apus");
    if ( code == "Aqr" ) return QString("Aquarius");
    if ( code == "Aql" ) return QString("Aquila");
    if ( code == "Ara" ) return QString("Ara");
    if ( code == "Ari" ) return QString("Aries");
    if ( code == "Aur" ) return QString("Auriga");
    if ( code == "Boo" ) return QString("Bootes");
    if ( code == "Cae" ) return QString("Caelum");
    if ( code == "Cam" ) return QString("Camelopardalis");
    if ( code == "Cnc" ) return QString("Cancer");
    if ( code == "CVn" ) return QString("Canes Venatici");
    if ( code == "CMa" ) return QString("Canis Major");
    if ( code == "CMi" ) return QString("Canis Minor");
    if ( code == "Cap" ) return QString("Capricornus");
    if ( code == "Car" ) return QString("Carina");
    if ( code == "Cas" ) return QString("Cassiopeia");
    if ( code == "Cen" ) return QString("Centaurus");
    if ( code == "Cep" ) return QString("Cepheus");
    if ( code == "Cet" ) return QString("Cetus");
    if ( code == "Cha" ) return QString("Chamaeleon");
    if ( code == "Cir" ) return QString("Circinus");
    if ( code == "Col" ) return QString("Columba");
    if ( code == "Com" ) return QString("Coma Berenices");
    if ( code == "CrA" ) return QString("Corona Australis");
    if ( code == "CrB" ) return QString("Corona Borealis");
    if ( code == "Crv" ) return QString("Corvus");
    if ( code == "Crt" ) return QString("Crater");
    if ( code == "Cru" ) return QString("Crux");
    if ( code == "Cyg" ) return QString("Cygnus");
    if ( code == "Del" ) return QString("Delphinus");
    if ( code == "Dor" ) return QString("Doradus");
    if ( code == "Dra" ) return QString("Draco");
    if ( code == "Equ" ) return QString("Equuleus");
    if ( code == "Eri" ) return QString("Eridanus");
    if ( code == "For" ) return QString("Fornax");
    if ( code == "Gem" ) return QString("Gemini");
    if ( code == "Gru" ) return QString("Grus");
    if ( code == "Her" ) return QString("Hercules");
    if ( code == "Hor" ) return QString("Horologium");
    if ( code == "Hya" ) return QString("Hydra");
    if ( code == "Hyi" ) return QString("Hydrus");
    if ( code == "Ind" ) return QString("Indus");
    if ( code == "Lac" ) return QString("Lacerta");
    if ( code == "Leo" ) return QString("Leo");
    if ( code == "LMi" ) return QString("Leo Minor");
    if ( code == "Lep" ) return QString("Lepus");
    if ( code == "Lib" ) return QString("Libra");
    if ( code == "Lup" ) return QString("Lupus");
    if ( code == "Lyn" ) return QString("Lynx");
    if ( code == "Lyr" ) return QString("Lyra");
    if ( code == "Men" ) return QString("Mensa");
    if ( code == "Mic" ) return QString("Microscopium");
    if ( code == "Mon" ) return QString("Monoceros");
    if ( code == "Mus" ) return QString("Musca");
    if ( code == "Nor" ) return QString("Norma");
    if ( code == "Oct" ) return QString("Octans");
    if ( code == "Oph" ) return QString("Ophiuchus");
    if ( code == "Ori" ) return QString("Orion");
    if ( code == "Pav" ) return QString("Pavo");
    if ( code == "Peg" ) return QString("Pegasus");
    if ( code == "Per" ) return QString("Perseus");
    if ( code == "Phe" ) return QString("Phoenix");
    if ( code == "Pic" ) return QString("Pictor");
    if ( code == "Psc" ) return QString("Pisces");
    if ( code == "PsA" ) return QString("Piscis Austrinus");
    if ( code == "Pup" ) return QString("Puppis");
    if ( code == "Pyx" ) return QString("Pyxis");
    if ( code == "Ret" ) return QString("Reticulum");
    if ( code == "Sge" ) return QString("Sagitta");
    if ( code == "Sgr" ) return QString("Sagittarius");
    if ( code == "Sco" ) return QString("Scorpius");
    if ( code == "Scl" ) return QString("Sculptor");
    if ( code == "Sct" ) return QString("Scutum");
    if ( code == "Ser" ) return QString("Serpens");
    if ( code == "Sex" ) return QString("Sextans");
    if ( code == "Tau" ) return QString("Taurus");
    if ( code == "Tel" ) return QString("Telescopium");
    if ( code == "Tri" ) return QString("Triangulum");
    if ( code == "TrA" ) return QString("Triangulum Australe");
    if ( code == "Tuc" ) return QString("Tucana");
    if ( code == "UMa" ) return QString("Ursa Major");
    if ( code == "UMi" ) return QString("Ursa Minor");
    if ( code == "Vel" ) return QString("Vela");
    if ( code == "Vir" ) return QString("Virgo");
    if ( code == "Vol" ) return QString("Volans");
    if ( code == "Vul" ) return QString("Vulpecula");
    return code;
}

QString constNameToAbbrev( const QString &fullName_ ) {
    QString fullName = fullName_.toLower();
    if ( fullName == "andromeda" ) return QString( "And" );
    if ( fullName == "antlia" ) return QString( "Ant" );
    if ( fullName == "apus" ) return QString( "Aps" );
    if ( fullName == "aquarius" ) return QString( "Aqr" );
    if ( fullName == "aquila" ) return QString( "Aql" );
    if ( fullName == "ara" ) return QString( "Ara" );
    if ( fullName == "aries" ) return QString( "Ari" );
    if ( fullName == "auriga" ) return QString( "Aur" );
    if ( fullName == "bootes" ) return QString( "Boo" );
    if ( fullName == "caelum" ) return QString( "Cae" );
    if ( fullName == "camelopardalis" ) return QString( "Cam" );
    if ( fullName == "cancer" ) return QString( "Cnc" );
    if ( fullName == "canes venatici" ) return QString( "CVn" );
    if ( fullName == "canis major" ) return QString( "CMa" );
    if ( fullName == "canis minor" ) return QString( "CMi" );
    if ( fullName == "capricornus" ) return QString( "Cap" );
    if ( fullName == "carina" ) return QString( "Car" );
    if ( fullName == "cassiopeia" ) return QString( "Cas" );
    if ( fullName == "centaurus" ) return QString( "Cen" );
    if ( fullName == "cepheus" ) return QString( "Cep" );
    if ( fullName == "cetus" ) return QString( "Cet" );
    if ( fullName == "chamaeleon" ) return QString( "Cha" );
    if ( fullName == "circinus" ) return QString( "Cir" );
    if ( fullName == "columba" ) return QString( "Col" );
    if ( fullName == "coma berenices" ) return QString( "Com" );
    if ( fullName == "corona australis" ) return QString( "CrA" );
    if ( fullName == "corona borealis" ) return QString( "CrB" );
    if ( fullName == "corvus" ) return QString( "Crv" );
    if ( fullName == "crater" ) return QString( "Crt" );
    if ( fullName == "crux" ) return QString( "Cru" );
    if ( fullName == "cygnus" ) return QString( "Cyg" );
    if ( fullName == "delphinus" ) return QString( "Del" );
    if ( fullName == "doradus" ) return QString( "Dor" );
    if ( fullName == "draco" ) return QString( "Dra" );
    if ( fullName == "equuleus" ) return QString( "Equ" );
    if ( fullName == "eridanus" ) return QString( "Eri" );
    if ( fullName == "fornax" ) return QString( "For" );
    if ( fullName == "gemini" ) return QString( "Gem" );
    if ( fullName == "grus" ) return QString( "Gru" );
    if ( fullName == "hercules" ) return QString( "Her" );
    if ( fullName == "horologium" ) return QString( "Hor" );
    if ( fullName == "hydra" ) return QString( "Hya" );
    if ( fullName == "hydrus" ) return QString( "Hyi" );
    if ( fullName == "indus" ) return QString( "Ind" );
    if ( fullName == "lacerta" ) return QString( "Lac" );
    if ( fullName == "leo" ) return QString( "Leo" );
    if ( fullName == "leo minor" ) return QString( "LMi" );
    if ( fullName == "lepus" ) return QString( "Lep" );
    if ( fullName == "libra" ) return QString( "Lib" );
    if ( fullName == "lupus" ) return QString( "Lup" );
    if ( fullName == "lynx" ) return QString( "Lyn" );
    if ( fullName == "lyra" ) return QString( "Lyr" );
    if ( fullName == "mensa" ) return QString( "Men" );
    if ( fullName == "microscopium" ) return QString( "Mic" );
    if ( fullName == "monoceros" ) return QString( "Mon" );
    if ( fullName == "musca" ) return QString( "Mus" );
    if ( fullName == "norma" ) return QString( "Nor" );
    if ( fullName == "octans" ) return QString( "Oct" );
    if ( fullName == "ophiuchus" ) return QString( "Oph" );
    if ( fullName == "orion" ) return QString( "Ori" );
    if ( fullName == "pavo" ) return QString( "Pav" );
    if ( fullName == "pegasus" ) return QString( "Peg" );
    if ( fullName == "perseus" ) return QString( "Per" );
    if ( fullName == "phoenix" ) return QString( "Phe" );
    if ( fullName == "pictor" ) return QString( "Pic" );
    if ( fullName == "pisces" ) return QString( "Psc" );
    if ( fullName == "piscis austrinus" ) return QString( "PsA" );
    if ( fullName == "puppis" ) return QString( "Pup" );
    if ( fullName == "pyxis" ) return QString( "Pyx" );
    if ( fullName == "reticulum" ) return QString( "Ret" );
    if ( fullName == "sagitta" ) return QString( "Sge" );
    if ( fullName == "sagittarius" ) return QString( "Sgr" );
    if ( fullName == "scorpius" ) return QString( "Sco" );
    if ( fullName == "sculptor" ) return QString( "Scl" );
    if ( fullName == "scutum" ) return QString( "Sct" );
    if ( fullName == "serpens" ) return QString( "Ser" );
    if ( fullName == "sextans" ) return QString( "Sex" );
    if ( fullName == "taurus" ) return QString( "Tau" );
    if ( fullName == "telescopium" ) return QString( "Tel" );
    if ( fullName == "triangulum" ) return QString( "Tri" );
    if ( fullName == "triangulum australe" ) return QString( "TrA" );
    if ( fullName == "tucana" ) return QString( "Tuc" );
    if ( fullName == "ursa major" ) return QString( "UMa" );
    if ( fullName == "ursa minor" ) return QString( "UMi" );
    if ( fullName == "vela" ) return QString( "Vel" );
    if ( fullName == "virgo" ) return QString( "Vir" );
    if ( fullName == "volans" ) return QString( "Vol" );
    if ( fullName == "vulpecula" ) return QString( "Vul" );
    return fullName_;
}

QString constGenetiveToAbbrev( const QString &genetive_ ) {
    QString genetive = genetive_.toLower();
    if ( genetive == "andromedae" ) return QString("And");
    if ( genetive == "antliae" ) return QString("Ant");
    if ( genetive == "apodis" ) return QString("Aps");
    if ( genetive == "aquarii" ) return QString("Aqr");
    if ( genetive == "aquilae" ) return QString("Aql");
    if ( genetive == "arae" ) return QString("Ara");
    if ( genetive == "arietis" ) return QString("Ari");
    if ( genetive == "aurigae" ) return QString("Aur");
    if ( genetive == "bootis" ) return QString("Boo");
    if ( genetive == "caeli" ) return QString("Cae");
    if ( genetive == "camelopardalis" ) return QString("Cam");
    if ( genetive == "cancri" ) return QString("Cnc");
    if ( genetive == "canum venaticorum" ) return QString("CVn");
    if ( genetive == "canis majoris" ) return QString("CMa");
    if ( genetive == "canis minoris" ) return QString("CMi");
    if ( genetive == "capricorni" ) return QString("Cap");
    if ( genetive == "carinae" ) return QString("Car");
    if ( genetive == "cassiopeiae" ) return QString("Cas");
    if ( genetive == "centauri" ) return QString("Cen");
    if ( genetive == "cephei" ) return QString("Cep");
    if ( genetive == "ceti" ) return QString("Cet");
    if ( genetive == "chamaeleontis" ) return QString("Cha");
    if ( genetive == "circini" ) return QString("Cir");
    if ( genetive == "columbae" ) return QString("Col");
    if ( genetive == "comae berenices" ) return QString("Com");
    if ( genetive == "coronae austrinae" ) return QString("CrA");
    if ( genetive == "coronae borealis" ) return QString("CrB");
    if ( genetive == "corvi" ) return QString("Crv");
    if ( genetive == "crateris" ) return QString("Crt");
    if ( genetive == "crucis" ) return QString("Cru");
    if ( genetive == "cygni" ) return QString("Cyg");
    if ( genetive == "delphini" ) return QString("Del");
    if ( genetive == "doradus" ) return QString("Dor");
    if ( genetive == "draconis" ) return QString("Dra");
    if ( genetive == "equulei" ) return QString("Equ");
    if ( genetive == "eridani" ) return QString("Eri");
    if ( genetive == "fornacis" ) return QString("For");
    if ( genetive == "geminorum" ) return QString("Gem");
    if ( genetive == "gruis" ) return QString("Gru");
    if ( genetive == "herculis" ) return QString("Her");
    if ( genetive == "horologii" ) return QString("Hor");
    if ( genetive == "hydrae" ) return QString("Hya");
    if ( genetive == "hydri" ) return QString("Hyi");
    if ( genetive == "indi" ) return QString("Ind");
    if ( genetive == "lacertae" ) return QString("Lac");
    if ( genetive == "leonis" ) return QString("Leo");
    if ( genetive == "leonis minoris" ) return QString("LMi");
    if ( genetive == "leporis" ) return QString("Lep");
    if ( genetive == "librae" ) return QString("Lib");
    if ( genetive == "lupi" ) return QString("Lup");
    if ( genetive == "lyncis" ) return QString("Lyn");
    if ( genetive == "lyrae" ) return QString("Lyr");
    if ( genetive == "mensae" ) return QString("Men");
    if ( genetive == "microscopii" ) return QString("Mic");
    if ( genetive == "monocerotis" ) return QString("Mon");
    if ( genetive == "muscae" ) return QString("Mus");
    if ( genetive == "normae" ) return QString("Nor");
    if ( genetive == "octantis" ) return QString("Oct");
    if ( genetive == "ophiuchi" ) return QString("Oph");
    if ( genetive == "orionis" ) return QString("Ori");
    if ( genetive == "pavonis" ) return QString("Pav");
    if ( genetive == "pegasi" ) return QString("Peg");
    if ( genetive == "persei" ) return QString("Per");
    if ( genetive == "phoenicis" ) return QString("Phe");
    if ( genetive == "pictoris" ) return QString("Pic");
    if ( genetive == "piscium" ) return QString("Psc");
    if ( genetive == "piscis austrini" ) return QString("PsA");
    if ( genetive == "puppis" ) return QString("Pup");
    if ( genetive == "pyxidis" ) return QString("Pyx");
    if ( genetive == "reticuli" ) return QString("Ret");
    if ( genetive == "sagittae" ) return QString("Sge");
    if ( genetive == "sagittarii" ) return QString("Sgr");
    if ( genetive == "scorpii" ) return QString("Sco");
    if ( genetive == "sculptoris" ) return QString("Scl");
    if ( genetive == "scuti" ) return QString("Sct");
    if ( genetive == "serpentis" ) return QString("Ser");
    if ( genetive == "sextantis" ) return QString("Sex");
    if ( genetive == "tauri" ) return QString("Tau");
    if ( genetive == "telescopii" ) return QString("Tel");
    if ( genetive == "trianguli" ) return QString("Tri");
    if ( genetive == "trianguli australis" ) return QString("TrA");
    if ( genetive == "tucanae" ) return QString("Tuc");
    if ( genetive == "ursae majoris" ) return QString("UMa");
    if ( genetive == "ursae minoris" ) return QString("UMi");
    if ( genetive == "velorum" ) return QString("Vel");
    if ( genetive == "virginis" ) return QString("Vir");
    if ( genetive == "volantis" ) return QString("Vol");
    if ( genetive == "vulpeculae" ) return QString("Vul");
    return genetive_;
}


  QString Logging::_filename;

  void Logging::UseFile()
  {
      if (_filename.isEmpty())
      {
          QDir dir;
          QString path = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "logs/" + QDateTime::currentDateTime().toString("yyyy-MM-dd");
          dir.mkpath(path);
          QString name = "log_" + QDateTime::currentDateTime().toString("HH:mm:ss") + ".txt";
          _filename = path + QStringLiteral("/") + name;

          // Clear file contents
          QFile file(_filename);
          file.open(QFile::WriteOnly);
          file.close();
      }

      qInstallMessageHandler(File);
  }

  void Logging::File(QtMsgType type, const QMessageLogContext &, const QString &msg)
  {
    QFile file(_filename);
    if(file.open(QFile::Append | QIODevice::Text))
    {
      QTextStream stream(&file);
      Write(stream, type, msg);
    }
  }

  void Logging::UseStdout()
  {
    qInstallMessageHandler(Stdout);
  }

  void Logging::Stdout(QtMsgType type, const QMessageLogContext &, const QString &msg)
  {
    QTextStream stream(stdout, QIODevice::WriteOnly);
    Write(stream, type, msg);
  }

  void Logging::UseStderr()
  {
    qInstallMessageHandler(Stderr);
  }

  void Logging::Stderr(QtMsgType type, const QMessageLogContext &, const QString &msg)
  {
    QTextStream stream(stderr, QIODevice::WriteOnly);
    Write(stream, type, msg);
  }

  void Logging::Write(QTextStream &stream, QtMsgType type, const QString &msg)
  {
    stream << QDateTime::currentDateTime().toString("yyyy-MM-ddThh:mm:ss.zzz") << " - ";

    switch(type) {
      case QtDebugMsg:
        stream << "DEBG - ";
        break;
      case QtWarningMsg:
        stream << "WARN - ";
        break;
      case QtCriticalMsg:
        stream << "CRIT - ";
        break;
      case QtFatalMsg:
        stream << "FATL - ";
        break;
      default:
        stream << "UNKN - ";
    }

    stream << msg << endl;
  }

  void Logging::UseDefault()
  {
    qInstallMessageHandler(0);
  }

  void Logging::Disable()
  {
    qInstallMessageHandler(Disabled);
  }

  void Logging::Disabled(QtMsgType, const QMessageLogContext &, const QString &)
  {
  }

#ifdef Q_OS_OSX
bool copyDataFolderFromAppBundleIfNeeded()  //The method returns true if the data directory is good to go.
{
    QString dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
    if(dataLocation.isEmpty()) { //If there is no kstars user data directory
        if (KMessageBox::warningYesNo(0,
                          i18n("There is not a KStars User Data Directory currently at ~/Library/Application Support/kstars \n" \
                               "Do you want to create a new one?"),
                          i18n("KStars User Data Directory Error."))
            == KMessageBox::No) {
                return false;
        } else {
            QString dataSourceLocation=QDir(QCoreApplication::applicationDirPath()+"/../Resources/data").absolutePath();
            if(dataSourceLocation.isEmpty()){ //If there is no default data directory in the app bundle
                KMessageBox::sorry(0, i18n("Error! There was no default data directory found in the app bundle!"));
                return false;
            }
            QDir writableDir;
            writableDir.mkdir(KSPaths::writableLocation(QStandardPaths::GenericDataLocation));
            dataLocation=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "kstars", QStandardPaths::LocateDirectory);
            if(dataLocation.isEmpty()){  //If there *still* is not a kstars data directory
                KMessageBox::sorry(0, i18n("Error! There was a problem creating the data directory ~/Library/Application Support/ !"));
                return false;
            }
            KSUtils::copyRecursively(dataSourceLocation, dataLocation);
            Options::setIndiServerIsInternal(true);
            Options::setIndiDriversAreInternal(true);
            Options::setAstrometrySolverIsInternal(true);
            Options::setAstrometryConfFileIsInternal(true);
            Options::setWcsIsInternal(true);
            Options::setXplanetIsInternal(true);
            KMessageBox::sorry(0, "KStars User Data Directory created at: " + dataLocation);
            if (KMessageBox::warningYesNo(0,i18n("Do you want to set up the internal Astrometry.net for plate solving images?"))== KMessageBox::Yes)
                configureDefaultAstrometry();
            return true;  //This means the data directory is good to go now that we created it from the default.
        }
    }
    return true;//This means the data directory was good to go from the start.
}

void configureDefaultAstrometry(){
    QDir writableDir;
    QString astrometryPath=QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/Astrometry/";
    if(!astrometryPath.isEmpty()){
        writableDir.mkdir(astrometryPath);
        astrometryPath=QStandardPaths::locate(QStandardPaths::GenericDataLocation, "Astrometry", QStandardPaths::LocateDirectory);
        if(astrometryPath.isEmpty())
            KMessageBox::sorry(0, i18n("Error!  The Astrometry Index File Directory does not exist and was not able to be created."));
        else{
            if(QDir(astrometryPath).count()<3)
                KMessageBox::sorry(0, "Astrometry Index Directory is at: " + astrometryPath +"\n Be sure to put Astrometry index files there in order to solve images.");

            QString confPath=QCoreApplication::applicationDirPath()+"/astrometry/bin/astrometry.cfg";
            QFile confFile(confPath);
            QString contents;
            if (confFile.open(QIODevice::ReadOnly) == false)
                KMessageBox::error(0, i18n("Internal Astrometry Configuration File Read Error."));
            else{
                QTextStream in(&confFile);
                QString line;
                bool foundPathBefore=false;
                bool fileNeedsUpdating=false;
                while ( !in.atEnd() )
                {
                      line = in.readLine();
                      if (line.trimmed().startsWith("add_path"))
                      {
                          if(!foundPathBefore){  //This will ensure there is not more than one add_path line in the file.
                              foundPathBefore=true;
                              QString dataDir = line.trimmed().mid(9).trimmed();
                              if(dataDir!=astrometryPath){ //Update to the correct path.
                                  contents += "add_path " + astrometryPath + "\n";
                                  fileNeedsUpdating=true;
                              }
                          }
                      }else{
                          contents += line + "\n";
                      }
                 }
                confFile.close();
                if(fileNeedsUpdating){
                    if (confFile.open(QIODevice::WriteOnly) == false)
                        KMessageBox::error(0, i18n("Internal Astrometry Configuration File Write Error."));
                    else{
                        QTextStream out(&confFile);
                        out << contents;
                        confFile.close();
                    }
                }
            }
        }
    }
}

bool copyRecursively(QString sourceFolder, QString destFolder)
{
    bool success = false;
    QDir sourceDir(sourceFolder);

    if(!sourceDir.exists())
        return false;

    QDir destDir(destFolder);
    if(!destDir.exists())
        destDir.mkdir(destFolder);

    QStringList files = sourceDir.entryList(QDir::Files);
    for(int i = 0; i< files.count(); i++) {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        success = QFile::copy(srcName, destName);
        if(!success)
            return false;
    }

    files.clear();
    files = sourceDir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
    for(int i = 0; i< files.count(); i++)
    {
        QString srcName = sourceFolder + QDir::separator() + files[i];
        QString destName = destFolder + QDir::separator() + files[i];
        success = copyRecursively(srcName, destName);
        if(!success)
            return false;
    }

    return true;
}
#endif

}
