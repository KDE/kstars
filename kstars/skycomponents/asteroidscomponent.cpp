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
 *Each line in the data file is parsed as follows:
 *@li 6-23 Name [string]
 *@li 24-29 Modified Julian Day of orbital elements [int]
 *@li 30-39 semi-major axis of orbit in AU [double]
 *@li 41-51 eccentricity of orbit [double]
 *@li 52-61 inclination angle of orbit in degrees [double]
 *@li 62-71 argument of perihelion in degrees [double]
 *@li 72-81 Longitude of the Ascending Node in degrees [double]
 *@li 82-93 Mean Anomaly in degrees [double]
 *@li 94-98 Magnitude [double]
 */
void AsteroidsComponent::loadData()
{

    QString line, name;
    int mJD;
    double a, e, dble_i, dble_w, dble_N, dble_M, H, G;
    long double JD;

    KSFileReader fileReader;

    if ( ! fileReader.open("asteroids.dat" ) ) return;

    emitProgressText( i18n("Loading asteroids") );

    while( fileReader.hasMoreLines() ) {
        line = fileReader.readLine();

        int catN = line.mid(0,6).toInt();
        name = line.mid( 6, 17 ).trimmed();
        mJD  = line.mid( 24, 5 ).toInt();
        a    = line.mid( 30, 9 ).toDouble();
        e    = line.mid( 41, 10 ).toDouble();
        dble_i = line.mid( 52, 9 ).toDouble();
        dble_w = line.mid( 62, 9 ).toDouble();
        dble_N = line.mid( 72, 9 ).toDouble();
        dble_M = line.mid( 82, 11 ).toDouble();
        H = line.mid( 94, 5 ).toDouble();
        G = line.mid( 102, 4 ).toDouble();

        JD = double( mJD ) + 2400000.5;

        KSAsteroid *ast = new KSAsteroid( catN, name, QString(), JD, a, e, dms(dble_i),
                                          dms(dble_w), dms(dble_N), dms(dble_M), H, G );
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

