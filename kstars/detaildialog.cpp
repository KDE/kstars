/***************************************************************************
                          detaildialog.cpp  -  description
                             -------------------
    begin                : Sun May 5 2002
    copyright            : (C) 2002 by Jason Harris
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

#include <qdatetime.h>
#include <qfont.h>
#include <qframe.h>
#include <qlabel.h>
#include <qlayout.h>
#include "geolocation.h"
#include "ksutils.h"
#include "skymap.h"
#include "skyobject.h"
#include "starobject.h"

#include "detaildialog.h"

DetailDialog::DetailDialog(SkyObject *o, QDateTime lt, GeoLocation *geo,
		QWidget *parent, const char *name ) : KDialogBase( KDialogBase::Plain, i18n( "Object Details" ), Ok, Ok, parent, name ) {

	QFrame *page = plainPage();

	ut = lt.addSecs( int( 3600*geo->TZ() ) );
	jd = KSUtils::UTtoJulian( ut );

	Coords = new CoordBox( o, lt, page );
	RiseSet = new RiseSetBox( o, lt, geo, page );

	StarObject *s;
	QString pname, oname;
//arguments to NameBox depend on type of object
	switch ( o->type() ) {
	case 0: //stars
		s = (StarObject *)o;
		pname = s->translatedName();
		if ( pname == i18n( "star" ) ) pname = i18n( "Unnamed star" );
		Names = new NameBox( pname, s->gname(),
				i18n( "Spectral type:" ), s->sptype(),
				QString("%1").arg( s->mag() ), page );
//		ProperMotion = new ProperMotionBox( s );
		break;
	case 2: //planets
		//Want to add distance from Earth, Mass, angular size.
		//Perhaps: list of moons
		Names = new NameBox( o->translatedName(), "", i18n( "Object type:" ),
				o->typeName(), "--", page );
		break;
	default: //deep-sky objects
		if ( ! o->longname().isEmpty() ) {
			pname = o->longname();
			oname = o->name() + ", ";
		} else {
			pname = o->name();
		}
		if ( ! o->name2().isEmpty() ) oname += o->name2();
		if ( o->ugc() != 0 ) oname += ", UGC " + QString("%1").arg( o->ugc() );
		if ( o->pgc() != 0 ) oname += ", PGC " + QString("%1").arg( o->pgc() );

		Names = new NameBox( pname, oname, i18n( "Object type:" ),
				o->typeName(), QString("%1").arg(o->mag()), page );
		break;
	}

//Layout manager
	vlay = new QVBoxLayout( page, 2 );
	vlay->addWidget( Names );
	vlay->addWidget( Coords );
	vlay->addWidget( RiseSet );
	vlay->activate();

}

DetailDialog::NameBox::NameBox( QString pname, QString oname,
		QString typelabel, QString type, QString mag,
		QWidget *parent, const char *name )
		: QGroupBox( i18n( "General" ), parent, name ) {

	PrimaryName = new QLabel( pname, this );
	OtherNames = new QLabel( oname, this );

	TypeLabel = new QLabel( typelabel, this );
	Type = new QLabel( type, this );
	Type->setAlignment( AlignRight );
	QFont boldFont = Type->font();
	boldFont.setWeight( QFont::Bold );
	PrimaryName->setFont( boldFont );
	Type->setFont( boldFont );
	MagLabel = new QLabel( i18n( "Magnitude:" ), this );
	Mag = new QLabel( mag, this );
	Mag->setAlignment( AlignRight );
	Mag->setFont( boldFont );

//Layout
	hlayType = new QHBoxLayout( 2 );
	hlayMag  = new QHBoxLayout( 2 );
	glay     = new QGridLayout( 2, 2, 2 );
	vlay     = new QVBoxLayout( this, 12 );

	hlayType->addWidget( TypeLabel );
	hlayType->addWidget( Type );
	hlayMag->addWidget( MagLabel );
	hlayMag->addWidget( Mag );

	glay->addWidget( PrimaryName, 0, 0);
	glay->addWidget( OtherNames, 1, 0);
	glay->addLayout( hlayType, 0, 1 );
	glay->addLayout( hlayMag, 1, 1 );
	vlay->addSpacing( 20 );
	vlay->addLayout( glay );
}

//In the printf() statements, the "176" refers to the ASCII code for the degree symbol

DetailDialog::CoordBox::CoordBox( SkyObject *o, QDateTime t, QWidget *parent,
		const char *name ) : QGroupBox( i18n( "Coordinates" ), parent, name ) {

	double epoch = t.date().year() + double( t.date().dayOfYear() )/365.25;
	RALabel = new QLabel( i18n( "RA (%1):" ).arg( epoch, 7, 'f', 2 ), this );
	DecLabel = new QLabel( i18n( "Dec (%1):" ).arg( epoch, 7, 'f', 2 ), this );
	RA = new QLabel( QString().sprintf( "%02dh %02dm %02ds", o->ra().hour(), o->ra().minute(), o->ra().second() ), this );
	Dec = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", o->dec().degree(), 176, o->dec().getArcMin(), o->dec().getArcSec() ), this );

	AzLabel = new QLabel( i18n( "Azimuth:" ), this );
	AltLabel = new QLabel( i18n( "Altitude:" ), this );
	Az = new QLabel( QString().sprintf( "%02d%c %02d\'%02d\"", o->az().degree(), 176, o->az().getArcMin(), o->az().getArcSec() ), this );
	Alt = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", o->alt().degree(), 176, o->alt().getArcMin(), o->alt().getArcSec() ), this );

	QFont boldFont = RA->font();
	boldFont.setWeight( QFont::Bold );
	RA->setFont( boldFont );
	Dec->setFont( boldFont );
	Az->setFont( boldFont );
	Alt->setFont( boldFont );
	RA->setAlignment( AlignRight );
	Dec->setAlignment( AlignRight );
	Az->setAlignment( AlignRight );
	Alt->setAlignment( AlignRight );

//Layouts
	glayCoords = new QGridLayout( 4, 2, 2 );
	vlayMain = new QVBoxLayout( this, 12 );

	glayCoords->addWidget( RALabel, 0, 0 );
	glayCoords->addWidget( DecLabel, 1, 0 );
	glayCoords->addWidget( RA, 0, 1 );
	glayCoords->addWidget( Dec, 1, 1 );
	glayCoords->addWidget( AzLabel, 0, 2 );
	glayCoords->addWidget( AltLabel, 1, 2 );
	glayCoords->addWidget( Az, 0, 3 );
	glayCoords->addWidget( Alt, 1, 3 );
	vlayMain->addSpacing( 20 );
	vlayMain->addLayout( glayCoords );
}

DetailDialog::RiseSetBox::RiseSetBox( SkyObject *o, QDateTime lt, GeoLocation *geo,
		QWidget *parent, const char *name ) : QGroupBox( i18n( "Rise/Set/Transit" ), parent, name ) {

	QDateTime ut = lt.addSecs( int( -3600*geo->TZ() ) );
	long double jd = KSUtils::UTtoJulian( ut );
	QTime LST = KSUtils::UTtoLST( ut, geo->lng() );
	dms LSTd; LSTd.setH( LST.hour(), LST.minute(), LST.second() );
	QTime rt = o->riseTime( jd, geo );
	//QTime tt = o->transitTime( lt, LSTd );
	QTime tt = o->transitTime( jd, geo );
	QTime st = o->setTime( jd, geo );

	dms raz = o->riseSetTimeAz(jd, geo, true ); //true = use rise time
	dms saz = o->riseSetTimeAz(jd, geo, false ); //false = use set time
	//dms talt = o->transitAltitude( geo );
	dms talt = o->transitAltitude( jd, geo );

	RTimeLabel = new QLabel( i18n( "Rise time:" ), this );
	TTimeLabel = new QLabel( i18n( "Transit time:" ), this );
	STimeLabel = new QLabel( i18n( "Set time:" ), this );
	RAzLabel = new QLabel( i18n( "Azimuth at rise:" ), this );
	TAltLabel = new QLabel( i18n( "Altitude at transit:" ), this );
	SAzLabel = new QLabel( i18n( "Azimuth at set:" ), this );

	if ( rt.isValid() ) {
		RTime = new QLabel( QString().sprintf( "%02d:%02d", rt.hour(), rt.minute() ), this );
		STime = new QLabel( QString().sprintf( "%02d:%02d", st.hour(), st.minute() ), this );
		RAz = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", raz.degree(), 176, raz.getArcMin(), raz.getArcSec() ), this );
		SAz = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", saz.degree(), 176, saz.getArcMin(), saz.getArcSec() ), this );
	} else {
		QString rs, ss;
		if ( o->alt().Degrees() > 0.0 ) {
			rs = i18n( "Circumpolar" );
			ss = i18n( "Circumpolar" );
		} else {
			rs = i18n( "Never rises" );
			ss = i18n( "Never rises" );
		}

		RTime = new QLabel( rs, this );
		STime = new QLabel( ss, this );
		RAz = new QLabel( i18n( "Not Applicalble", "N/A" ), this );
		SAz = new QLabel( i18n( "Not Applicalble", "N/A" ), this );
	}

	TTime = new QLabel( QString().sprintf( "%02d:%02d", tt.hour(), tt.minute() ), this );
	TAlt = new QLabel( QString().sprintf( "%02d%c %02d\' %02d\"", talt.degree(), 176, talt.getArcMin(), talt.getArcSec() ), this );


	QFont boldFont = RTime->font();
	boldFont.setWeight( QFont::Bold );
	RTime->setFont( boldFont );
	TTime->setFont( boldFont );
	STime->setFont( boldFont );
	RAz->setFont( boldFont );
	TAlt->setFont( boldFont );
	SAz->setFont( boldFont );
	RTime->setAlignment( AlignRight );
	TTime->setAlignment( AlignRight );
	STime->setAlignment( AlignRight );
	RAz->setAlignment( AlignRight );
	TAlt->setAlignment( AlignRight );
	SAz->setAlignment( AlignRight );

//Layout
	vlay = new QVBoxLayout( this, 12 );
	glay = new QGridLayout( 4, 3, 2 ); //nrows, ncols, spacing
	glay->addWidget( RTimeLabel, 0, 0 );
	glay->addWidget( TTimeLabel, 1, 0 );
	glay->addWidget( STimeLabel, 2, 0 );
	glay->addWidget( RTime, 0, 1 );
	glay->addWidget( TTime, 1, 1 );
	glay->addWidget( STime, 2, 1 );
	glay->addWidget( RAzLabel, 0, 2 );
	glay->addWidget( TAltLabel, 1, 2 );
	glay->addWidget( SAzLabel, 2, 2 );
	glay->addWidget( RAz, 0, 3 );
	glay->addWidget( TAlt, 1, 3 );
	glay->addWidget( SAz, 2, 3 );
	vlay->addSpacing( 20 );
	vlay->addLayout( glay );
}

#include "detaildialog.moc"
