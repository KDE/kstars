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
#include <kdebug.h>

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
 * @short Initialize the comets list.
 * Reads in the comets data from the comets.dat file.
 *
 * Populate the list of Comets from the data file.
 * The data file is a CSV file with the following columns :
 * @li 1 full name [string]
 * @li 2 modified julian day of orbital elements [int]
 * @li 3 perihelion distance in AU [double]
 * @li 4 eccentricity of orbit [double]
 * @li 5 inclination angle of orbit in degrees [double]
 * @li 6 argument of perihelion in degrees [double]
 * @li 7 longitude of the ascending node in degrees [double]
 * @li 8 time of perihelion passage (YYYYMMDD.DDD) [double]
 * @li 9 orbit solution ID [string]
 * @li 10 Near-Earth Object (NEO) flag [bool]
 * @li 11 comet total magnitude parameter [float]
 * @li 12 comet nuclear magnitude parameter [float]
 * @li 13 object diameter (from equivalent sphere) [float]
 * @li 14 object bi/tri-axial ellipsoid dimensions [string]
 * @li 15 geometric albedo [float]
 * @li 16 rotation period [float]
 * @li 17 orbital period [float]
 * @li 18 earth minimum orbit intersection distance [double]
 * @li 19 orbit classification [string]
 * @li 20 comet total magnitude slope parameter
 * @li 21 comet nuclear magnitude slope parameter
 * @note See KSComet constructor for more details.
 */
void CometsComponent::loadData() {
    QFile file;
    QString line, name, orbit_id, orbit_class, dimensions;
    QStringList fields;
    bool ok, neo;
    int mJD;
    double q, e, dble_i, dble_w, dble_N, Tp, earth_moid;
    long double JD;
    float M1, M2, K1, K2, diameter, albedo, rot_period, period;
    KSFileReader fileReader;
    if(!fileReader.open( "comets.dat" )) return;
    emitProgressText( i18n("Loading comets") );
    // Clear lists
    m_ObjectList.clear();
    objectNames( SkyObject::COMET ).clear();
    
    while( fileReader.hasMoreLines() )
    {
        //kDebug()<<"fileReader.lineNumber() : "<<fileReader.lineNumber()<<endl;
        KSComet *com = 0;
        line = fileReader.readLine();

        // Ignore comments and too short lines
        if ( line.size() < 8  ||  line.at( 0 ) == '#' )
            continue;
        fields = line.split( "," );
        if( fields.size() < 21 )
            continue;

        name   = fields.at( 0 );
        name   = name.remove( '"' ).trimmed();
        //kDebug()<<name<<endl;
        mJD    = fields.at( 1 ).toInt();
        q      = fields.at( 2 ).toDouble();
        e      = fields.at( 3 ).toDouble();
        dble_i = fields.at( 4 ).toDouble();
        dble_w = fields.at( 5 ).toDouble();
        dble_N = fields.at( 6 ).toDouble();
        Tp     = fields.at( 7 ).toDouble();
        orbit_id = fields.at( 8 );
        orbit_id.remove( '"' );
        
        neo = fields.at( 9 ) == "Y";
        
        if(fields.at(10).isEmpty())
            M1 = 101.0;        
        else
            M1 = fields.at( 10 ).toFloat( &ok );
        
        if(fields.at(11).isEmpty())
            M2 = 101.0; 
        else
            M2 = fields.at( 11 ).toFloat( &ok );
        
        diameter = fields.at( 12 ).toFloat( &ok );
        if ( !ok ) diameter = 0.0;
        dimensions = fields.at( 13 );
        albedo  = fields.at( 14 ).toFloat( &ok );
        if ( !ok ) albedo = 0.0;
        rot_period = fields.at( 15 ).toFloat( &ok );
        if ( !ok ) rot_period = 0.0;
        period  = fields.at( 16 ).toFloat( &ok );
        if ( !ok ) period = 0.0;
        earth_moid  = fields.at( 17 ).toDouble( &ok );
        if ( !ok ) earth_moid = 0.0;
        orbit_class = fields.at( 18 );
        
        if(fields.at(19).isEmpty())
            K1 = 0.0; 
        else
            K1 = fields.at( 19 ).toFloat( &ok );
        
        if(fields.at(20).isEmpty())
            K2 = 0.0; 
        else
            K2 = fields.at( 20 ).toFloat( &ok );
        
        JD = double( mJD ) + 2400000.5;

        com = new KSComet( name, QString(), JD, q, e, dms( dble_i ), dms( dble_w ), dms( dble_N ), Tp, M1, M2, K1, K2 );
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
    QByteArray post_data = QByteArray( "obj_group=all&obj_kind=com&obj_numbered=all&OBJ_field=0&OBJ_op=0&OBJ_value=&ORB_field=0&ORB_op=0&ORB_value=&combine_mode=AND&c1_group=OBJ&c1_item=Af&c1_op=!%3D&c1_value=D&c2_group=OBJ&c2_item=Ae&c2_op=!%3D&c2_value=SOHO&c_fields=AcBdBiBgBjBlBkBqBbAgAkAlApAqArAsBsBtChAmAn&table_format=CSV&max_rows=10&format_option=full&query=Generate%20Table&.cgifields=format_option&.cgifields=field_list&.cgifields=obj_kind&.cgifields=obj_group&.cgifields=obj_numbered&.cgifields=combine_mode&.cgifields=ast_orbit_class&.cgifields=table_format&.cgifields=ORB_field_set&.cgifields=OBJ_field_set&.cgifields=preset_field_set&.cgifields=com_orbit_class" );
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
