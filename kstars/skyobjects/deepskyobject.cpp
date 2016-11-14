/***************************************************************************
                          deepskyobject.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Sun Feb 11 2001
    copyright            : (C) 2001 by Jason Harris
    email                : jharris@30doradus.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "deepskyobject.h"

#include <typeinfo>

#include <QFile>
#include <QRegExp>
#include <QPainter>
#include <QImage>

#include <assert.h>

#include "kstarsdata.h"
#include "ksutils.h"
#include "dms.h"
#ifndef KSTARS_LITE
#include "kspopupmenu.h"
#endif
#include "Options.h"
#include "skymap.h"
#include "texturemanager.h"
#include "catalogentrydata.h"

#include <QDebug>
#include <KLocalizedString>

DeepSkyObject::DeepSkyObject( const DeepSkyObject &o )
    : SkyObject( o )
    , PositionAngle( o.PositionAngle )
    , m_image( o.m_image )
    , UGC( o.UGC )
    , PGC( o.PGC )
    , MajorAxis( o.MajorAxis )
    , MinorAxis( o.MinorAxis )
    , Catalog( o.Catalog )
{
    customCat = NULL;
    Flux = o.flux();
    setMag( o.mag() );
    updateID = updateNumID = 0;
}

DeepSkyObject::DeepSkyObject( int t, dms r, dms d, float m,
                              const QString &n, const QString &n2,
                              const QString &lname, const QString &cat,
                              float a, float b, double pa, int pgc, int ugc )
        : SkyObject( t, r, d, m, n, n2, lname )
{
    MajorAxis = a;
    MinorAxis = b;
    PositionAngle = pa;
    PGC = pgc;
    UGC = ugc;
    setCatalog( cat );
    updateID = updateNumID = 0;
    customCat = NULL;
    Flux = 0;

    // Disable image loading on init
    //loadImage();
}

DeepSkyObject::DeepSkyObject( const CatalogEntryData &data, CatalogComponent *cat )
{
    // FIXME: This assumes that CatalogEntryData coordinates have
    // J2000.0 as epoch as opposed to the catalog's epoch!!! -- asimha
    qWarning() << "Creating a DeepSkyObject from CatalogEntryData assumes that coordinates are J2000.0";
    setType( data.type );
    setRA0( data.ra/15.0 ); // NOTE: CatalogEntryData stores RA in degrees, whereas setRA0() wants it in hours.
    setDec0( data.dec );
    setLongName( data.long_name );
    if( ! data.catalog_name.isEmpty() )
        setName( data.catalog_name + ' ' + QString::number( data.ID ) );
    else {
        setName( data.long_name );
        setLongName( QString() );
    }
    MajorAxis = data.major_axis;
    MinorAxis = data.minor_axis;
    PositionAngle = data.position_angle;
    setMag( data.magnitude );
    PGC = 0;
    UGC = 0;
    setCatalog( data.catalog_name );
    updateID = updateNumID = 0;
    customCat = cat;
    Flux = data.flux;

    // Disable image loading on init
    //loadImage();
}

DeepSkyObject* DeepSkyObject::clone() const
{
    Q_ASSERT( typeid( this ) == typeid( static_cast<const DeepSkyObject *>( this ) ) ); // Ensure we are not slicing a derived class
    return new DeepSkyObject(*this);
}

void DeepSkyObject::initPopupMenu( KSPopupMenu *pmenu ) {
#ifndef KSTARS_LITE
    pmenu->createDeepSkyObjectMenu( this );
#endif
}

float DeepSkyObject::e() const {
    if ( MajorAxis==0.0 || MinorAxis==0.0 )
        return 1.0; //assume circular
    return MinorAxis / MajorAxis;
}

QString DeepSkyObject::catalog() const {
    if ( isCatalogM() ) return QString("M");
    if ( isCatalogNGC() ) return QString("NGC");
    if ( isCatalogIC() ) return QString("IC");
    return QString();
}

void DeepSkyObject::setCatalog( const QString &cat ) {
    if ( cat.toUpper() == "M" ) Catalog = (unsigned char)CAT_MESSIER;
    else if ( cat.toUpper() == "NGC" ) Catalog = (unsigned char)CAT_NGC;
    else if ( cat.toUpper() == "IC"  ) Catalog = (unsigned char)CAT_IC;
    else Catalog = (unsigned char)CAT_UNKNOWN;
}

void DeepSkyObject::loadImage()
{
    QString tname = name().toLower().remove(' ');
    m_image = TextureManager::getImage( tname );
    imageLoaded=true;
}

double DeepSkyObject::labelOffset() const {
    //Calculate object size in pixels
    double majorAxis = a();
    double minorAxis = b();
    if ( majorAxis == 0.0 && type() == 1 ) { //catalog stars
      majorAxis = 1.0;
      minorAxis = 1.0;
    }
    double size = ((majorAxis + minorAxis) / 2.0 ) * dms::PI * Options::zoomFactor()/10800.0;
    return 0.5*size + 4.;
}

QString DeepSkyObject::labelString() const
{
    QString oName;
    if( Options::showDeepSkyNames() )
    {
        if( Options::deepSkyLongLabels() && translatedLongName() != translatedName() )
            oName = translatedLongName() + " (" + translatedName() + ')';
        else
            oName = translatedName();
    }

    if( Options::showDeepSkyMagnitudes() )
    {
        if( Options::showDeepSkyNames() )
            oName += " ";
        oName += "[" + QLocale().toString( mag(), 'f', 1 ) + "m]";
    }

    return oName;
}


SkyObject::UID DeepSkyObject::getUID() const
{
    // mag takes 10 bit
    SkyObject::UID m = mag()*10;
    if( m < 0 ) m = 0;

    // Both RA & dec fits in 24-bits
    SkyObject::UID ra  = ra0().Degrees() * 36000;
    SkyObject::UID dec = (ra0().Degrees()+91) * 36000;

    assert("Magnitude is expected to fit into 10bits" && m>=0 && m<(1<<10));
    assert("RA should fit into 24bits"  && ra>=0  && ra <(1<<24));
    assert("Dec should fit into 24bits" && dec>=0 && dec<(1<<24));

    // Choose kind of
    SkyObject::UID kind = type() == SkyObject::GALAXY ? SkyObject::UID_GALAXY : SkyObject::UID_DEEPSKY;
    return (kind << 60) | (m << 48) | (ra << 24) | dec;
}
