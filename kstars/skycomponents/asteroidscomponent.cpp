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

#include <cmath>
#include <QDebug>
#include <QStandardPaths>
#include <QHttpMultiPart>
#include <QPen>

#include <KLocalizedString>

#include "asteroidscomponent.h"

#include "auxiliary/filedownloader.h"
#include "projections/projector.h"
#include "solarsystemcomposite.h"
#include "skycomponent.h"
#include "skylabeler.h"
#include "skymap.h"
#include "skypainter.h"
#include "Options.h"
#include "skyobjects/ksasteroid.h"
#include "kstarsdata.h"
#include "ksfilereader.h"
#include "auxiliary/kspaths.h"

AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent) : SolarSystemListComponent(parent)
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
    QString name, full_name, orbit_id, orbit_class, dimensions;
    int mJD;
    double q, a, e, dble_i, dble_w, dble_N, dble_M, H, G, earth_moid;
    long double JD;
    float diameter, albedo, rot_period, period;
    bool neo;    

    emitProgressText( i18n("Loading asteroids") );

    // Clear lists
    m_ObjectList.clear();
    objectNames( SkyObject::ASTEROID ).clear();

    QList< QPair<QString, KSParser::DataTypes> > sequence;
    sequence.append(qMakePair(QString("full name"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("epoch_mjd"), KSParser::D_INT));
    sequence.append(qMakePair(QString("q"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("a"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("e"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("i"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("w"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("om"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("ma"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("tp_calc"), KSParser::D_SKIP));
    sequence.append(qMakePair(QString("orbit_id"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("H"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("G"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("neo"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("tp_calc"), KSParser::D_SKIP));
    sequence.append(qMakePair(QString("M2"), KSParser::D_SKIP));
    sequence.append(qMakePair(QString("diameter"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("extent"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("albedo"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("rot_period"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("per_y"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("moid"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("class"), KSParser::D_QSTRING));

    //QString file_name = KSPaths::locate( QStandardPaths::DataLocation,  );
    QString file_name = KSPaths::locate(QStandardPaths::GenericDataLocation, QString("asteroids.dat"));
    KSParser asteroid_parser(file_name, '#', sequence);

    QHash<QString, QVariant> row_content;
    while (asteroid_parser.HasNextRow()){
        row_content = asteroid_parser.ReadNextRow();
        full_name = row_content["full name"].toString();
        full_name = full_name.trimmed();
        int catN  = full_name.section(' ', 0, 0).toInt();

        name = full_name.section(' ', 1, -1);

        //JM temporary hack to avoid Europa,Io, and Asterope duplication
        if (name == "Europa" || name == "Io" || name == "Asterope")
            name += i18n(" (Asteroid)");

        mJD  = row_content["epoch_mjd"].toInt();
        q    = row_content["q"].toDouble();
        a    = row_content["a"].toDouble();
        e    = row_content["e"].toDouble();
        dble_i = row_content["i"].toDouble();
        dble_w = row_content["w"].toDouble();
        dble_N = row_content["om"].toDouble();
        dble_M = row_content["ma"].toDouble();
        orbit_id = row_content["orbit_id"].toString();
        H   = row_content["H"].toDouble();
        G   = row_content["G"].toDouble();
        neo = row_content["neo"].toString() == "Y";
        diameter = row_content["diameter"].toFloat();
        dimensions = row_content["extent"].toString();
        albedo  = row_content["albedo"].toFloat();
        rot_period = row_content["rot_period"].toFloat();
        period  = row_content["per_y"].toFloat();
        earth_moid  = row_content["moid"].toDouble();
        orbit_class = row_content["class"].toString();

        JD = static_cast<double>(mJD) + 2400000.5;

        KSAsteroid *new_asteroid = NULL;
        // JM: Hack since asteroid file (Generated by JPL) is missing important Pluto data
        // I emailed JPL and this hack will be removed once they update the data!
        if (name == "Pluto")
        {
           diameter = 2368;
           new_asteroid = new KSAsteroid( catN, name, "pluto", JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H, G );
         }
         else
           new_asteroid = new KSAsteroid( catN, name, QString(), JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H, G );

        new_asteroid->setPerihelion(q);
        new_asteroid->setOrbitID(orbit_id);
        new_asteroid->setNEO(neo);
        new_asteroid->setDiameter(diameter);
        new_asteroid->setDimensions(dimensions);
        new_asteroid->setAlbedo(albedo);
        new_asteroid->setRotationPeriod(rot_period);
        new_asteroid->setPeriod(period);
        new_asteroid->setEarthMOID(earth_moid);
        new_asteroid->setOrbitClass(orbit_class);
        new_asteroid->setPhysicalSize(diameter);
        //new_asteroid->setAngularSize(0.005);

        m_ObjectList.append(new_asteroid);

        // Add name to the list of object names
        objectNames(SkyObject::ASTEROID).append(name);
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

        if ( ast->mag() > Options::magLimitAsteroid() || std::isnan(ast->mag()) != 0)
            continue;

        bool drawn = false;

        if (ast->image().isNull() == false)
            drawn = skyp->drawPlanet(ast);
        else
            drawn = skyp->drawPointSource(ast,ast->mag());

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
    downloadJob = new FileDownloader();

    QObject::connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    QObject::connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    QUrl url = QUrl( "http://ssd.jpl.nasa.gov/sbdb_query.cgi" );
    QByteArray post_data = QByteArray( "obj_group=all&obj_kind=ast&obj_numbere"
    "d=num&OBJ_field=0&ORB_field=0&c1_group=OBJ&c1_item=Ai&c1_op=%3C&c1_value="
    "12&c_fields=AcBdBiBhBgBjBlBkBmBqBbAiAjAgAkAlApAqArAsBsBtCh&table_format=C"
    "SV&max_rows=10&format_option=full&query=Generate%20Table&.cgifields=forma"
    "t_option&.cgifields=field_list&.cgifields=obj_kind&.cgifields=obj_group&."
    "cgifields=obj_numbered&.cgifields=combine_mode&.cgifields=ast_orbit_class"
    "&.cgifields=table_format&.cgifields=ORB_field_set&.cgifields=OBJ_field_se"
    "t&.cgifields=preset_field_set&.cgifields=com_orbit_class" );    

    downloadJob->post(url, post_data);
}

void AsteroidsComponent::downloadReady()
{
    // Comment the first line
    QByteArray data = downloadJob->downloadedData();
    data.insert( 0, '#' );

    // Write data to asteroids.dat
    QFile file( KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "asteroids.dat" ) ;
    file.open( QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text );
    file.write( data );
    file.close();

    // Reload asteroids
    loadData();

    KStars::Instance()->data()->setFullTimeUpdate();

    downloadJob->deleteLater();
}

void AsteroidsComponent::downloadError(const QString &errorString)
{
    KMessageBox::error(0, i18n("Error downloading asteroids data: %1", errorString));

    downloadJob->deleteLater();
}
