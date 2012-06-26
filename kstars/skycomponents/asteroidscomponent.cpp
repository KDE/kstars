/***************************************************************************
                          asteroidscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/30/08
    copyright            : (C) 2005 by Thomas Kabelmann
    email                : thomas.kabelmann@gmx.de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "asteroidscomponent.h"

#include <QPen>
#include <kglobal.h>
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kio/jobuidelegate.h>
#include <kstandarddirs.h>

#include "skycomponent.h"

#include "Options.h"
#include "skyobjects/ksasteroid.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "skymap.h"

#include "solarsystemcomposite.h"
#include "skylabeler.h"
#include "skypainter.h"
#include "projections/projector.h"


AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent ) :
    SolarSystemListComponent(parent)
{
    loadData();
}

AsteroidsComponent::~AsteroidsComponent()
{}

bool AsteroidsComponent::selected() {
    return Options::showAsteroids();
}

/*
 *@short Initialize the asteroids list.
 *Reads in the asteroids data from the asteroids.dat file.
 *
 * The data file is a CSV file with the following columns :
 * @li 1 full name [string]
 * @li 2 Modified Julian Day of orbital elements [int]
 * @li 3 perihelion distance in AU [double]
 * @li 4 semi-major axis
 * @li 5 eccentricity of orbit [double]
 * @li 6 inclination angle of orbit in degrees [double]
 * @li 7 argument of perihelion in degrees [double]
 * @li 8 longitude of the ascending node in degrees [double]
 * @li 9 mean anomaly
 * @li 10 time of perihelion passage (YYYYMMDD.DDD) [double]
 * @li 11 orbit solution ID [string]
 * @li 12 absolute magnitude [float]
 * @li 13 slope parameter [float]
 * @li 14 Near-Earth Object (NEO) flag [bool]
 * @li 15 comet total magnitude parameter [float] (we should remove this column)
 * @li 16 comet nuclear magnitude parameter [float] (we should remove this column)
 * @li 17 object diameter (from equivalent sphere) [float]
 * @li 18 object bi/tri-axial ellipsoid dimensions [string]
 * @li 19 geometric albedo [float]
 * @li 20 rotation period [float]
 * @li 21 orbital period [float]
 * @li 22 earth minimum orbit intersection distance [double]
 * @li 23 orbit classification [string]
 */
void AsteroidsComponent::loadData()
{

    QString line, name, full_name, orbit_id, orbit_class, dimensions;
    QStringList fields;
    int mJD;
    double q, a, e, dble_i, dble_w, dble_N, dble_M, H, G, earth_moid;
    long double JD;
    float diameter, albedo, rot_period, period;
    bool ok, neo;
    
    //TODO: Am I complicating things? ~~spacetime
    QList<KSParser::DataTypes> newList;
    newList.append(KSParser::D_QSTRING); //name
    newList.append(KSParser::D_INT); //epoch
    for (int i=0; i<8; i++) newList.append(KSParser::D_DOUBLE);
    newList.append(KSParser::D_QSTRING); //orbit ID
    newList.append(KSParser::D_DOUBLE); //H
    newList.append(KSParser::D_DOUBLE); //G
    newList.append(KSParser::D_QSTRING); //NEO
    newList.append(KSParser::D_DOUBLE); //m1?
    newList.append(KSParser::D_DOUBLE); //m2?
    newList.append(KSParser::D_FLOAT); //diameter
    newList.append(KSParser::D_QSTRING); //dimensions(extent)
    newList.append(KSParser::D_FLOAT); //albedo
    newList.append(KSParser::D_FLOAT); //rot_period
    newList.append(KSParser::D_FLOAT); //per_y(period)
    newList.append(KSParser::D_DOUBLE); //moid
    newList.append(KSParser::D_QSTRING); //class
    KSParser asteroidParser(QString("hello"), '#', ',', newList);
    asteroidParser.ReadNextRow();
    
    
    
    KSFileReader fileReader;

    if ( ! fileReader.open("asteroids.dat" ) ) return;

    emitProgressText( i18n("Loading asteroids") );

    // Clear lists
    m_ObjectList.clear();
    objectNames( SkyObject::ASTEROID ).clear();

    while( fileReader.hasMoreLines() ) {
        line = fileReader.readLine();

        // Ignore comments and too short lines
        if ( line.size() < 8 || line.at( 0 ) == '#' )
            continue;
        fields = line.split( "," );
        if ( fields.size() < 23 )
            continue;

        full_name = fields.at( 0 );
        full_name = full_name.remove( '"' ).trimmed();
        int catN  = full_name.section( " ", 0, 0 ).toInt();
        name = full_name.section( " ", 1, -1 );
        mJD  = fields.at( 1 ).toInt();
        q    = fields.at( 2 ).toDouble();
        a    = fields.at( 3 ).toDouble();
        e    = fields.at( 4 ).toDouble();
        dble_i = fields.at( 5 ).toDouble();
        dble_w = fields.at( 6 ).toDouble();
        dble_N = fields.at( 7 ).toDouble();
        dble_M = fields.at( 8 ).toDouble();
        orbit_id = fields.at( 10 );
        orbit_id.remove( '"' );
        H   = fields.at( 11 ).toDouble();
        G   = fields.at( 12 ).toDouble();
        neo = fields.at( 13 ) == "Y";
        diameter = fields.at( 16 ).toFloat( &ok );
        if ( !ok ) diameter = 0.0;
        dimensions = fields.at( 17 );
        albedo  = fields.at( 18 ).toFloat( &ok );
        if ( !ok ) albedo = 0.0;
        rot_period = fields.at( 19 ).toFloat( &ok );
        if ( !ok ) rot_period = 0.0;
        period  = fields.at( 20 ).toFloat( &ok );
        if ( !ok ) period = 0.0;
        earth_moid  = fields.at( 21 ).toDouble( &ok );
        if ( !ok ) earth_moid = 0.0;
        orbit_class = fields.at( 22 );

        JD = double( mJD ) + 2400000.5;

        KSAsteroid *ast = new KSAsteroid( catN, name, QString(), JD, a, e, dms(dble_i),
                                          dms(dble_w), dms(dble_N), dms(dble_M), H, G );
        ast->setPerihelion( q );
        ast->setOrbitID( orbit_id );
        ast->setNEO( neo );
        ast->setDiameter( diameter );
        ast->setDimensions( dimensions );
        ast->setAlbedo( albedo );
        ast->setRotationPeriod( rot_period );
        ast->setPeriod( period );
        ast->setEarthMOID( earth_moid );
        ast->setOrbitClass( orbit_class );
        ast->setAngularSize( 0.005 );
        m_ObjectList.append( ast );

        //Add name to the list of object names
        objectNames(SkyObject::ASTEROID).append( name );
    }
}

void AsteroidsComponent::draw( SkyPainter *skyp )
{
    if ( ! selected() ) return;

    bool hideLabels =  ! Options::showAsteroidNames() ||
                       ( SkyMap::Instance()->isSlewing() && Options::hideLabels() );

    double lgmin = log10(MINZOOM);
    double lgmax = log10(MAXZOOM);
    double lgz = log10(Options::zoomFactor());
    double labelMagLimit  = 2.5 + Options::asteroidLabelDensity() / 5.0;
    labelMagLimit += ( 15.0 - labelMagLimit ) * ( lgz - lgmin) / (lgmax - lgmin );
    if ( labelMagLimit > 10.0 ) labelMagLimit = 10.0;
    //printf("labelMagLim = %.1f\n", labelMagLimit );

    skyp->setBrush( QBrush( QColor( "gray" ) ) );

    foreach ( SkyObject *so, m_ObjectList ) {
        // FIXME: God help us!
        KSAsteroid *ast = (KSAsteroid*) so;

        if ( ast->mag() > Options::magLimitAsteroid() ) continue;

        bool drawn = skyp->drawPointSource(ast,ast->mag());
        
        if ( drawn && !( hideLabels || ast->mag() >= labelMagLimit ) )
            SkyLabeler::AddLabel( ast, SkyLabeler::ASTEROID_LABEL );
    }
}

SkyObject* AsteroidsComponent::objectNearest( SkyPoint *p, double &maxrad ) {
    SkyObject *oBest = 0;

    if ( ! selected() ) return 0;

    foreach ( SkyObject *o, m_ObjectList ) {
        if ( o->mag() > Options::magLimitAsteroid() ) continue;

        double r = o->angularDistanceTo( p ).Degrees();
        if ( r < maxrad ) {
            oBest = o;
            maxrad = r;
        }
    }

    return oBest;
}

void AsteroidsComponent::updateDataFile()
{
    KUrl url = KUrl( "http://ssd.jpl.nasa.gov/sbdb_query.cgi" );
    QByteArray post_data = QByteArray( "obj_group=all&obj_kind=ast&obj_numbered=num&OBJ_field=0&ORB_field=0&c1_group=OBJ&c1_item=Ai&c1_op=%3C&c1_value=12&c_fields=AcBdBiBhBgBjBlBkBmBqBbAiAjAgAkAlApAqArAsBsBtCh&table_format=CSV&max_rows=10&format_option=full&query=Generate%20Table&.cgifields=format_option&.cgifields=field_list&.cgifields=obj_kind&.cgifields=obj_group&.cgifields=obj_numbered&.cgifields=combine_mode&.cgifields=ast_orbit_class&.cgifields=table_format&.cgifields=ORB_field_set&.cgifields=OBJ_field_set&.cgifields=preset_field_set&.cgifields=com_orbit_class" );
    QString content_type = "Content-Type: application/x-www-form-urlencoded";

    // Download file
    KIO::StoredTransferJob* get_job = KIO::storedHttpPost( post_data,  url );
    get_job->addMetaData("content-type", content_type );

    if( KIO::NetAccess::synchronousRun( get_job, 0 ) ) {
        // Comment the first line
        QByteArray data = get_job->data();
        data.insert( 0, '#' );

        // Write data to asteroids.dat
        QFile file( KStandardDirs::locateLocal( "appdata", "asteroids.dat" ) );
        file.open( QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text );
        file.write( data );
        file.close();

        // Reload asteroids
        loadData();

        KStars::Instance()->data()->setFullTimeUpdate();
    } else {
        get_job->ui()->showErrorMessage();
    }
}
