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
#include "kspopupmenu.h"
#include "ksnumbers.h"

#include <qpainter.h>
#include <qstring.h>
#include <kdebug.h>

StarObject::StarObject( StarObject &o )
	: SkyObject (o)
{
	SpType = o.SpType;
//SONAME: deprecated (?) JH
//	soName = o.soName;
	PM_RA = o.pmRA();
	PM_Dec = o.pmDec();
	Parallax = o.parallax();
	Multiplicity = o.isMultiple();
	Variability = o.isVariable();
}

StarObject::StarObject( dms r, dms d, float m, QString n, QString n2, QString sptype,
		double pmra, double pmdec, double par, bool mult, bool var )
	: SkyObject (SkyObject::STAR, r, d, m, n, n2, ""), SpType(sptype), PM_RA(pmra), PM_Dec(pmdec),
		Parallax(par), Multiplicity(mult), Variability(var) // SONAME deprecated //, soName( 0 )
{
	QString lname;
	if ( hasName() ) {
		lname = n;
		if ( hasName2() ) lname += " (" + gname() + ")";
	} else if ( hasName2() )
		lname = gname();

	//If genetive name exists, but no primary name, set primary name = genetive name.
	if ( hasName2() && !hasName() ) {
		setName( gname() );
	}

	setLongName(lname);
}

StarObject::StarObject( double r, double d, float m, QString n, QString n2, QString sptype,
		double pmra, double pmdec, double par, bool mult, bool var )
	: SkyObject (SkyObject::STAR, r, d, m, n, n2, ""), SpType(sptype), PM_RA(pmra), PM_Dec(pmdec),
		Parallax(par), Multiplicity(mult), Variability(var) // SONAME deprecated //, soName( 0 )
{
	QString lname;
	if ( hasName() ) {
		lname = n;
		if ( hasName2() )lname += n + " (" + gname() + ")";
	} else if ( hasName2() )
		lname = gname();

	setLongName(lname);
}

void StarObject::updateCoords( KSNumbers *num, bool , const dms*, const dms* ) {
	SkyPoint::updateCoords( num );
	
	//Correct for proper motion of stars.  Determine RA and Dec offsets.
	//Proper motion is given im milliarcsec per year by the pmRA() and pmDec() functions.
	//That is numerically identical to the number of arcsec per millenium, so multiply by 
	//KSNumbers::julianMillenia() to find the offsets in arcsec.
	setRA( ra()->Hours() + pmRA()*num->julianMillenia() / 15. / cos( dec()->radians() )/3600. );
	setDec( dec()->Degrees() + pmDec()*num->julianMillenia()/3600. );
}

QString StarObject::sptype( void ) const {
	return SpType;
}

QString StarObject::gname( bool useGreekChars ) const {
	return greekLetter( useGreekChars ) + " " + constell();
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
		letter += "[" + name2().mid(3,1) + "]";

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

	return QString("");
}

void StarObject::draw( QPainter &psky, QPixmap *sky, QPixmap *starpix, int x, int y, bool /*showMultiple*/, double /*scale*/ ) {
	//Indicate multiple stars with a short horizontal line
	//(only draw this for stars larger than 3 pixels)
//Commenting out for now...
//	if ( showMultiple &&  starpix->width() > 3 ) {
//		int lsize = int(3*starpix->width()/4);  //size of half line segment
//		psky.drawLine( x - lsize, y, x + lsize, y );
//	}

	//Only bitBlt() if we are drawing to the sky pixmap
	if ( psky.device() == sky )
		bitBlt ((QPaintDevice *) sky, x - starpix->width()/2, y - starpix->height()/2, starpix );
	else
		psky.drawPixmap( x - starpix->width()/2, y - starpix->height()/2, *starpix );

}

void StarObject::drawLabel( QPainter &psky, int x, int y, double zoom, bool drawName, bool drawMag, double scale ) {
	QString sName( i18n("star") + " " );
	if ( drawName ) {
		if ( name() != "star" ) sName = translatedName() + " ";
		else if ( longname() != "star" ) sName = translatedLongName() + " ";
	}
	if ( drawMag ) {
		if ( drawName )
			sName += QString().sprintf("%.1f", mag() );
		else 
			sName = QString().sprintf("%.1f", mag() );
	}

	int offset = int( scale * (6 + int(0.5*(5.0-mag())) + int(0.01*( zoom/500. )) ));

	psky.drawText( x+offset, y+offset, sName );
}

