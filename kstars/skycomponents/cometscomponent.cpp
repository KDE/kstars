/***************************************************************************
                          cometscomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2005/24/09
    copyright            : (C) 2005 by Jason Harris
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

#include "cometscomponent.h"
#include "solarsystemcomposite.h"

#include <QFile>
#include <QPen>
#include <kglobal.h>

#include "Options.h"
#include "skyobjects/kscomet.h"
#include "ksutils.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "skymap.h"
#include "skylabeler.h"
#include "skypainter.h"
#include "projections/projector.h"
#include <kio/job.h>
#include <kio/netaccess.h>
#include <kio/jobuidelegate.h>
#include <kstandarddirs.h>

CometsComponent::CometsComponent( SolarSystemComposite *parent )
        : SolarSystemListComponent( parent )
{
    loadData();
}

CometsComponent::~CometsComponent()
{}

bool CometsComponent::selected() {
    return Options::showComets();
}

/*
 * @short Initialize the asteroids list.
 * Reads in the asteroids data from the asteroids.dat file.
 *
 * Populate the list of Comets from the data file.
 * Each line in the data file is parsed as follows:
 * @li 3-37 Name [string]
 * @li 38-42 Modified Julian Day of orbital elements [int]
 * @li 44-53 semi-major axis of orbit in AU [double]
 * @li 55-64 eccentricity of orbit [double]
 * @li 66-74 inclination angle of orbit in degrees [double]
 * @li 76-84 argument of perihelion in degrees [double]
 * @li 86-94 Longitude of the Ascending Node in degrees [double]
 * @li 82-93 Date of most proximate perihelion passage (YYYYMMDD.DDD) [double]
 * @li 124-127 Absolute magnitude [float]
 * @li 129-132 Slope parameter [float]
 *
 * @note See KSComet constructor for more details.
 */
void CometsComponent::loadData() {
    QFile file;

    if ( KSUtils::openDataFile( file, "comets.dat" ) ) {
        emitProgressText( i18n("Loading comets") );

        // Clear lists
        m_ObjectList.clear();
        objectNames( SkyObject::COMET ).clear();

        KSFileReader fileReader( file );
        while( fileReader.hasMoreLines() ) {
            QString line, name, orbit_id, orbit_class, dimensions;
            QStringList fields;
            bool ok, neo;
            int mJD;
            double q, e, dble_i, dble_w, dble_N, Tp, earth_moid;
            long double JD;
            float H, G, diameter, albedo, rot_period, period;
            KSComet *com = 0;

            line = fileReader.readLine();

            // Ignore comments and too short lines
            if ( line.at( 0 ) == '#' || line.size() < 8 )
                continue;

            fields = line.split( "," );

            name   = fields.at( 0 );
            name   = name.remove( '"' ).trimmed();
            mJD    = fields.at( 1 ).toInt();
            q      = fields.at( 2 ).toDouble();
            e      = fields.at( 3 ).toDouble();
            dble_i = fields.at( 4 ).toDouble();
            dble_w = fields.at( 5 ).toDouble();
            dble_N = fields.at( 6 ).toDouble();
            Tp     = fields.at( 7 ).toDouble();
            orbit_id = fields.at( 8 );
            orbit_id.remove( '"' );
            H      = fields.at( 9 ).toFloat( &ok );
            if ( !ok ) H = -101.0; // Any absolute mag brighter than -100 should be treated as nonsense
            G      = fields.at( 10 ).toFloat( &ok );
            if ( !ok ) G = -101.0; // Same with slope parameter.
            if ( fields.at( 11 ) == "Y" )
                neo = true;
            else
                neo = false;
            diameter = fields.at( 14 ).toFloat( &ok );
            if ( !ok ) diameter = 0.0;
            dimensions = fields.at( 15 );
            albedo  = fields.at( 16 ).toFloat( &ok );
            if ( !ok ) albedo = 0.0;
            rot_period = fields.at( 17 ).toFloat( &ok );
            if ( !ok ) rot_period = 0.0;
            period  = fields.at( 18 ).toFloat( &ok );
            if ( !ok ) period = 0.0;
            earth_moid  = fields.at( 19 ).toDouble( &ok );
            if ( !ok ) earth_moid = 0.0;
            orbit_class = fields.at( 20 );

            JD = double( mJD ) + 2400000.5;

            com = new KSComet( name, QString(), JD, q, e, dms( dble_i ), dms( dble_w ), dms( dble_N ), Tp, H, G );
            com->setOrbitID( orbit_id );
            com->setNEO( neo );
            com->setDiameter( diameter );
            com->setDimensions( dimensions );
            com->setAlbedo( albedo );
            com->setRotationPeriod( rot_period );
            com->setPeriod( period );
            com->setEarthMOID( earth_moid );
            com->setOrbitClass( orbit_class );
            com->setAngularSize( 0.005 );

            m_ObjectList.append( com );

            //Add *short* name to the list of object names
            objectNames( SkyObject::COMET ).append( com->name() );
        }
    }
}

void CometsComponent::draw( SkyPainter *skyp )
{
    if( !selected() || Options::zoomFactor() < 10*MINZOOM )
        return;

    bool hideLabels =  ! Options::showCometNames() || (SkyMap::Instance()->isSlewing() && Options::hideLabels() );
    double rsunLabelLimit = Options::maxRadCometName();

    //FIXME: Should these be config'able?
    skyp->setPen( QPen( QColor( "darkcyan" ) ) );
    skyp->setBrush( QBrush( QColor( "darkcyan" ) ) );

    foreach ( SkyObject *so, m_ObjectList ) {
        KSComet *com = (KSComet*)so;
        bool drawn = skyp->drawPointSource(com,com->mag());
        if ( drawn && !(hideLabels || com->rsun() >= rsunLabelLimit) )
            SkyLabeler::AddLabel( com, SkyLabeler::COMET_LABEL );
    }
}

void CometsComponent::updateDataFile()
{
    KUrl url = KUrl( "http://ssd.jpl.nasa.gov/sbdb_query.cgi" );
    QByteArray post_data = QByteArray( "obj_group=all&obj_kind=com&obj_numbered=all&OBJ_field=0&OBJ_op=0&OBJ_value=&ORB_field=0&ORB_op=0&ORB_value=&combine_mode=AND&c1_group=OBJ&c1_item=Af&c1_op=!%3D&c1_value=D&c2_group=OBJ&c2_item=Ae&c2_op=!%3D&c2_value=SOHO&c_fields=AcBdBiBgBjBlBkBqBbAiAjAgAkAlApAqArAsBsBtCh&table_format=CSV&max_rows=10&format_option=full&query=Generate%20Table&.cgifields=format_option&.cgifields=field_list&.cgifields=obj_kind&.cgifields=obj_group&.cgifields=obj_numbered&.cgifields=combine_mode&.cgifields=ast_orbit_class&.cgifields=table_format&.cgifields=ORB_field_set&.cgifields=OBJ_field_set&.cgifields=preset_field_set&.cgifields=com_orbit_class" );
    QString content_type = "Content-Type: application/x-www-form-urlencoded";

    // Download file
    KIO::StoredTransferJob* get_job = KIO::storedHttpPost( post_data,  url );
    get_job->addMetaData("content-type", content_type );

    if( KIO::NetAccess::synchronousRun( get_job, 0 ) ) {
        // Comment the first line
        QByteArray data = get_job->data();
        data.insert( 0, '#' );

        // Write data to comets.dat
        QFile file( KStandardDirs::locateLocal( "appdata", "comets.dat" ) );
        file.open( QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text );
        file.write( data );
        file.close();

        // Reload comets
        loadData();

        KStars::Instance()->data()->setFullTimeUpdate();
    } else {
        get_job->ui()->showErrorMessage();
    }
}
