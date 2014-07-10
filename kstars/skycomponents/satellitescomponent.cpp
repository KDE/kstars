/***************************************************************************
                          satellitescomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : Tue 02 Mar 2011
    copyright            : (C) 2009 by Jerome SONRIER
    email                : jsid@emor3j.fr.eu.org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "satellitescomponent.h"

#include <QStringList>
#include <QObject>
#include <QProgressDialog>

#include <kjob.h>
#include <kio/job.h>
#include <kio/copyjob.h>
#include <kio/netaccess.h>
#include <kio/jobuidelegate.h>

#include "klocale.h"

#include "satellitegroup.h"
#include "Options.h"
#include "ksfilereader.h"
#include "skylabeler.h"
#include "kstarsdata.h"

SatellitesComponent::SatellitesComponent( SkyComposite *parent ) :
    SkyComponent( parent )
{
    KSFileReader fileReader;
    QString line;
    QStringList group_infos;
    
    if ( ! fileReader.open( "satellites.dat" ) ) return;

    emitProgressText( xi18n("Loading satellites" ) );
    

    while ( fileReader.hasMoreLines() ) {
        line = fileReader.readLine();
        if ( line.trimmed().isEmpty() || line.at( 0 ) == '#' )
            continue;
        group_infos = line.split( ';' );
        m_groups.append( new SatelliteGroup( group_infos.at( 0 ), group_infos.at( 1 ), KUrl( group_infos.at( 2 ) ) ) );
    }
}

SatellitesComponent::~SatellitesComponent()
{

}

bool SatellitesComponent::selected() {
    return Options::showSatellites();
}

void SatellitesComponent::update( KSNumbers * )
{
    // Return if satellites must not be draw
    if( ! selected() )
        return;
    
    foreach( SatelliteGroup *group, m_groups ) {
        group->updateSatellitesPos();
    }
}

void SatellitesComponent::draw( SkyPainter *skyp )
{
    // Return if satellites must not be draw
    if( ! selected() )
        return;

    foreach( SatelliteGroup *group, m_groups ) {
        for ( int i=0; i<group->size(); i++ ) {
            Satellite *sat = group->at( i );
            if ( sat->selected() ) {
                if ( Options::showVisibleSatellites() ) {
                    if ( sat->isVisible() )
                        skyp->drawSatellite( sat );
                } else {
                    skyp->drawSatellite( sat );
                }
            }
        }
    }
}

void SatellitesComponent::drawLabel( Satellite *sat, QPointF pos )
{
    SkyLabeler *labeler = SkyLabeler::Instance();
    labeler->setPen( KStarsData::Instance()->colorScheme()->colorNamed( "SatLabelColor" ) );
    labeler->drawNameLabel( sat, pos );
}

void SatellitesComponent::drawTrails( SkyPainter *skyp ) {
    Q_UNUSED(skyp);
}

void SatellitesComponent::updateTLEs()
{
    int i = 0;
    
    QProgressDialog progressDlg( xi18n( "Update TLEs..." ), xi18n( "Abort" ), 0, m_groups.count() );
    progressDlg.setWindowModality( Qt::WindowModal );
    progressDlg.setValue( 0 );
        
    foreach ( SatelliteGroup *group, m_groups ) {
        if ( progressDlg.wasCanceled() )
            return;

        if( group->tleUrl().isEmpty() )
            continue;
        
        progressDlg.setLabelText( xi18n( "Update %1 satellites", group->name() ) );
        KIO::Job* getJob = KIO::file_copy( group->tleUrl(), group->tleFilename(), -1, KIO::Overwrite | KIO::HideProgressInfo );
        if( KIO::NetAccess::synchronousRun( getJob, 0 ) ) {
            group->readTLE();
            group->updateSatellitesPos();
            progressDlg.setValue( ++i );
        } else {
            getJob->ui()->showErrorMessage();
        }   
    }
}

QList<SatelliteGroup*> SatellitesComponent::groups()
{
    return m_groups;
}

Satellite* SatellitesComponent::findSatellite( QString name )
{
    foreach ( SatelliteGroup *group, m_groups ) {
        for ( int i=0; i<group->size(); i++ ) {
            Satellite *sat = group->at( i );
            if ( sat->name() == name )
                return sat;
        }
    }

    return 0;
}

SkyObject* SatellitesComponent::objectNearest( SkyPoint* p, double& maxrad ) {
    if ( ! selected() )
        return 0;

    //KStarsData* data = KStarsData::Instance();

    SkyObject *oBest = 0;
    double rBest = maxrad;
    double r;

    foreach ( SatelliteGroup *group, m_groups ) {
        for ( int i=0; i<group->size(); i++ ) {
            Satellite *sat = group->at( i );
            if ( ! sat->selected() )
                continue;

            r = sat->angularDistanceTo( p ).Degrees();
            //qDebug() << sat->name();
            //qDebug() << "r = " << r << " - max = " << rBest;
            //qDebug() << "ra2=" << sat->ra().Degrees() << " - dec2=" << sat->dec().Degrees();
            if ( r < rBest ) {
                rBest = r;
                oBest = sat;
            }
        }
    }

    maxrad = rBest;
    return oBest;
}
