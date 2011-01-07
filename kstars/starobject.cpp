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

#include <tqpainter.h>
#include <tqstring.h>
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

StarObject::StarObject( dms r, dms d, float m, TQString n, TQString n2, TQString sptype,
		double pmra, double pmdec, double par, bool mult, bool var )
	: SkyObject (SkyObject::STAR, r, d, m, n, n2, ""), SpType(sptype), PM_RA(pmra), PM_Dec(pmdec),
		Parallax(par), Multiplicity(mult), Variability(var) // SONAME deprecated //, soName( 0 )
{
	TQString lname;
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

StarObject::StarObject( double r, double d, float m, TQString n, TQString n2, TQString sptype,
		double pmra, double pmdec, double par, bool mult, bool var )
	: SkyObject (SkyObject::STAR, r, d, m, n, n2, ""), SpType(sptype), PM_RA(pmra), PM_Dec(pmdec),
		Parallax(par), Multiplicity(mult), Variability(var) // SONAME deprecated //, soName( 0 )
{
	TQString lname;
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

TQString StarObject::sptype( void ) const {
	return SpType;
}

TQString StarObject::gname( bool useGreekChars ) const {
	return greekLetter( useGreekChars ) + " " + constell();
}

TQString StarObject::greekLetter( bool gchar ) const {
	TQString code = name2().left(3);
	TQString letter = code;  //in case genitive name is *not* a Greek letter
	int alpha = 0x03B1;

	if ( code == "alp" ) gchar ? letter = TQString( TQChar(alpha + 0) ) : letter = i18n("alpha");
	if ( code == "bet" ) gchar ? letter = TQString( TQChar(alpha + 1) ) : letter = i18n("beta");
	if ( code == "gam" ) gchar ? letter = TQString( TQChar(alpha + 2) ) : letter = i18n("gamma");
	if ( code == "del" ) gchar ? letter = TQString( TQChar(alpha + 3) ) : letter = i18n("delta");
	if ( code == "eps" ) gchar ? letter = TQString( TQChar(alpha + 4) ) : letter = i18n("epsilon");
	if ( code == "zet" ) gchar ? letter = TQString( TQChar(alpha + 5) ) : letter = i18n("zeta");
	if ( code == "eta" ) gchar ? letter = TQString( TQChar(alpha + 6) ) : letter = i18n("eta");
	if ( code == "the" ) gchar ? letter = TQString( TQChar(alpha + 7) ) : letter = i18n("theta");
	if ( code == "iot" ) gchar ? letter = TQString( TQChar(alpha + 8) ) : letter = i18n("iota");
	if ( code == "kap" ) gchar ? letter = TQString( TQChar(alpha + 9) ) : letter = i18n("kappa");
	if ( code == "lam" ) gchar ? letter = TQString( TQChar(alpha +10) ) : letter = i18n("lambda");
	if ( code == "mu " ) gchar ? letter = TQString( TQChar(alpha +11) ) : letter = i18n("mu");
	if ( code == "nu " ) gchar ? letter = TQString( TQChar(alpha +12) ) : letter = i18n("nu");
	if ( code == "xi " ) gchar ? letter = TQString( TQChar(alpha +13) ) : letter = i18n("xi");
	if ( code == "omi" ) gchar ? letter = TQString( TQChar(alpha +14) ) : letter = i18n("omicron");
	if ( code == "pi " ) gchar ? letter = TQString( TQChar(alpha +15) ) : letter = i18n("pi");
	if ( code == "rho" ) gchar ? letter = TQString( TQChar(alpha +16) ) : letter = i18n("rho");
	//there are two unicode symbols for sigma;
	//skip the first one, the second is more widely used
	if ( code == "sig" ) gchar ? letter = TQString( TQChar(alpha +18) ) : letter = i18n("sigma");
	if ( code == "tau" ) gchar ? letter = TQString( TQChar(alpha +19) ) : letter = i18n("tau");
	if ( code == "ups" ) gchar ? letter = TQString( TQChar(alpha +20) ) : letter = i18n("upsilon");
	if ( code == "phi" ) gchar ? letter = TQString( TQChar(alpha +21) ) : letter = i18n("phi");
	if ( code == "chi" ) gchar ? letter = TQString( TQChar(alpha +22) ) : letter = i18n("chi");
	if ( code == "psi" ) gchar ? letter = TQString( TQChar(alpha +23) ) : letter = i18n("psi");
	if ( code == "ome" ) gchar ? letter = TQString( TQChar(alpha +24) ) : letter = i18n("omega");

	if ( name2().length() && name2().mid(3,1) != " " )
		letter += "[" + name2().mid(3,1) + "]";

	return letter;
}

TQString StarObject::constell( void ) const {
	TQString code = name2().mid(4,3);
	if ( code == "And" ) return TQString("Andromedae");
	if ( code == "Ant" ) return TQString("Antliae");
	if ( code == "Aps" ) return TQString("Apodis");
	if ( code == "Aqr" ) return TQString("Aquarii");
	if ( code == "Aql" ) return TQString("Aquilae");
	if ( code == "Ara" ) return TQString("Arae");
	if ( code == "Ari" ) return TQString("Arietis");
	if ( code == "Aur" ) return TQString("Aurigae");
	if ( code == "Boo" ) return TQString("Bootis");
	if ( code == "Cae" ) return TQString("Caeli");
	if ( code == "Cam" ) return TQString("Camelopardalis");
	if ( code == "Cnc" ) return TQString("Cancri");
	if ( code == "CVn" ) return TQString("Canum Venaticorum");
	if ( code == "CMa" ) return TQString("Canis Majoris");
	if ( code == "CMi" ) return TQString("Canis Minoris");
	if ( code == "Cap" ) return TQString("Capricorni");
	if ( code == "Car" ) return TQString("Carinae");
	if ( code == "Cas" ) return TQString("Cassiopeiae");
	if ( code == "Cen" ) return TQString("Centauri");
	if ( code == "Cep" ) return TQString("Cephei");
	if ( code == "Cet" ) return TQString("Ceti");
	if ( code == "Cha" ) return TQString("Chamaeleontis");
	if ( code == "Cir" ) return TQString("Circini");
	if ( code == "Col" ) return TQString("Columbae");
	if ( code == "Com" ) return TQString("Comae Berenices");
	if ( code == "CrA" ) return TQString("Coronae Austrinae");
	if ( code == "CrB" ) return TQString("Coronae Borealis");
	if ( code == "Crv" ) return TQString("Corvi");
	if ( code == "Crt" ) return TQString("Crateris");
	if ( code == "Cru" ) return TQString("Crucis");
	if ( code == "Cyg" ) return TQString("Cygni");
	if ( code == "Del" ) return TQString("Delphini");
	if ( code == "Dor" ) return TQString("Doradus");
	if ( code == "Dra" ) return TQString("Draconis");
	if ( code == "Equ" ) return TQString("Equulei");
	if ( code == "Eri" ) return TQString("Eridani");
	if ( code == "For" ) return TQString("Fornacis");
	if ( code == "Gem" ) return TQString("Geminorum");
	if ( code == "Gru" ) return TQString("Gruis");
	if ( code == "Her" ) return TQString("Herculis");
	if ( code == "Hor" ) return TQString("Horologii");
	if ( code == "Hya" ) return TQString("Hydrae");
	if ( code == "Hyi" ) return TQString("Hydri");
	if ( code == "Ind" ) return TQString("Indi");
	if ( code == "Lac" ) return TQString("Lacertae");
	if ( code == "Leo" ) return TQString("Leonis");
	if ( code == "LMi" ) return TQString("Leonis Minoris");
	if ( code == "Lep" ) return TQString("Leporis");
	if ( code == "Lib" ) return TQString("Librae");
	if ( code == "Lup" ) return TQString("Lupi");
	if ( code == "Lyn" ) return TQString("Lyncis");
	if ( code == "Lyr" ) return TQString("Lyrae");
	if ( code == "Men" ) return TQString("Mensae");
	if ( code == "Mic" ) return TQString("Microscopii");
	if ( code == "Mon" ) return TQString("Monocerotis");
	if ( code == "Mus" ) return TQString("Muscae");
	if ( code == "Nor" ) return TQString("Normae");
	if ( code == "Oct" ) return TQString("Octantis");
	if ( code == "Oph" ) return TQString("Ophiuchi");
	if ( code == "Ori" ) return TQString("Orionis");
	if ( code == "Pav" ) return TQString("Pavonis");
	if ( code == "Peg" ) return TQString("Pegasi");
	if ( code == "Per" ) return TQString("Persei");
	if ( code == "Phe" ) return TQString("Phoenicis");
	if ( code == "Pic" ) return TQString("Pictoris");
	if ( code == "Psc" ) return TQString("Piscium");
	if ( code == "PsA" ) return TQString("Piscis Austrini");
	if ( code == "Pup" ) return TQString("Puppis");
	if ( code == "Pyx" ) return TQString("Pyxidis");
	if ( code == "Ret" ) return TQString("Reticuli");
	if ( code == "Sge" ) return TQString("Sagittae");
	if ( code == "Sgr" ) return TQString("Sagittarii");
	if ( code == "Sco" ) return TQString("Scorpii");
	if ( code == "Scl" ) return TQString("Sculptoris");
	if ( code == "Sct" ) return TQString("Scuti");
	if ( code == "Ser" ) return TQString("Serpentis");
	if ( code == "Sex" ) return TQString("Sextantis");
	if ( code == "Tau" ) return TQString("Tauri");
	if ( code == "Tel" ) return TQString("Telescopii");
	if ( code == "Tri" ) return TQString("Trianguli");
	if ( code == "TrA" ) return TQString("Trianguli Australis");
	if ( code == "Tuc" ) return TQString("Tucanae");
	if ( code == "UMa" ) return TQString("Ursae Majoris");
	if ( code == "UMi" ) return TQString("Ursae Minoris");
	if ( code == "Vel" ) return TQString("Velorum");
	if ( code == "Vir" ) return TQString("Virginis");
	if ( code == "Vol" ) return TQString("Volantis");
	if ( code == "Vul" ) return TQString("Vulpeculae");

	return TQString("");
}

void StarObject::draw( TQPainter &psky, TQPixmap *sky, TQPixmap *starpix, int x, int y, bool /*showMultiple*/, double /*scale*/ ) {
	//Indicate multiple stars with a short horizontal line
	//(only draw this for stars larger than 3 pixels)
//Commenting out for now...
//	if ( showMultiple &&  starpix->width() > 3 ) {
//		int lsize = int(3*starpix->width()/4);  //size of half line segment
//		psky.drawLine( x - lsize, y, x + lsize, y );
//	}

	//Only bitBlt() if we are drawing to the sky pixmap
	if ( psky.device() == sky )
		bitBlt ((TQPaintDevice *) sky, x - starpix->width()/2, y - starpix->height()/2, starpix );
	else
		psky.drawPixmap( x - starpix->width()/2, y - starpix->height()/2, *starpix );

}

void StarObject::drawLabel( TQPainter &psky, int x, int y, double zoom, bool drawName, bool drawMag, double scale ) {
	TQString sName( i18n("star") + " " );
	if ( drawName ) {
		if ( name() != "star" ) sName = translatedName() + " ";
		else if ( longname() != "star" ) sName = translatedLongName() + " ";
	}
	if ( drawMag ) {
		if ( drawName )
			sName += TQString().sprintf("%.1f", mag() );
		else 
			sName = TQString().sprintf("%.1f", mag() );
	}

	int offset = int( scale * (6 + int(0.5*(5.0-mag())) + int(0.01*( zoom/500. )) ));

	psky.drawText( x+offset, y+offset, sName );
}

