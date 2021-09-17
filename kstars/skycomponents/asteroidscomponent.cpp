/*
    SPDX-FileCopyrightText: 2005 Thomas Kabelmann <thomas.kabelmann@gmx.de>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "asteroidscomponent.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#endif
#include "ksfilereader.h"
#include "kstarsdata.h"
#include "kstars_debug.h"
#include "Options.h"
#include "solarsystemcomposite.h"
#include "skycomponent.h"
#include "skylabeler.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#else
#include "kstarslite.h"
#endif
#include "skypainter.h"
#include "auxiliary/kspaths.h"
#include "auxiliary/ksnotification.h"
#include "auxiliary/filedownloader.h"
#include "projections/projector.h"

#include <KLocalizedString>

#include <QDebug>
#include <QStandardPaths>
#include <QHttpMultiPart>
#include <QPen>

#include <cmath>

AsteroidsComponent::AsteroidsComponent(SolarSystemComposite *parent) : BinaryListComponent(this, "asteroids"),
    SolarSystemListComponent(parent)
{
    loadData();
}

bool AsteroidsComponent::selected()
{
    return Options::showAsteroids();
}

/*
 * @short Initialize the asteroids list.
 * Reads in the asteroids data from the asteroids.dat file
 * and writes it into the Binary File;
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
void AsteroidsComponent::loadDataFromText()
{
    QString name, full_name, orbit_id, orbit_class, dimensions;
    int mJD;
    double q, a, e, dble_i, dble_w, dble_N, dble_M, H, G, earth_moid;
    long double JD;
    float diameter, albedo, rot_period, period;
    bool neo;

    emitProgressText(i18n("Loading asteroids"));
    qCInfo(KSTARS) << "Loading asteroids";

    QList<QPair<QString, KSParser::DataTypes>> sequence;
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

    KSParser asteroid_parser(filepath_txt, '#', sequence);

    QHash<QString, QVariant> row_content;
    while (asteroid_parser.HasNextRow())
    {
        row_content = asteroid_parser.ReadNextRow();
        full_name   = row_content["full name"].toString();
        full_name   = full_name.trimmed();
        int catN    = full_name.section(' ', 0, 0).toInt();

        name = full_name.section(' ', 1, -1);

        //JM temporary hack to avoid Europa,Io, and Asterope duplication
        if (name == i18nc("Asteroid name (optional)", "Europa") || name == i18nc("Asteroid name (optional)", "Io") ||
                name == i18nc("Asteroid name (optional)", "Asterope"))
            name += i18n(" (Asteroid)");

        mJD         = row_content["epoch_mjd"].toInt();
        q           = row_content["q"].toDouble();
        a           = row_content["a"].toDouble();
        e           = row_content["e"].toDouble();
        dble_i      = row_content["i"].toDouble();
        dble_w      = row_content["w"].toDouble();
        dble_N      = row_content["om"].toDouble();
        dble_M      = row_content["ma"].toDouble();
        orbit_id    = row_content["orbit_id"].toString();
        H           = row_content["H"].toDouble();
        G           = row_content["G"].toDouble();
        neo         = row_content["neo"].toString() == "Y";
        diameter    = row_content["diameter"].toFloat();
        dimensions  = row_content["extent"].toString();
        albedo      = row_content["albedo"].toFloat();
        rot_period  = row_content["rot_period"].toFloat();
        period      = row_content["per_y"].toFloat();
        earth_moid  = row_content["moid"].toDouble();
        orbit_class = row_content["class"].toString();

        JD = static_cast<double>(mJD) + 2400000.5;

        KSAsteroid *new_asteroid = nullptr;

        // Diameter is missing from JPL data
        if (name == i18nc("Asteroid name (optional)", "Pluto"))
            diameter = 2390;

        new_asteroid = new KSAsteroid(catN, name, QString(), JD, a, e, dms(dble_i), dms(dble_w), dms(dble_N), dms(dble_M), H, G);

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

        appendListObject(new_asteroid);

        // Add name to the list of object names
        objectNames(SkyObject::ASTEROID).append(name);
        objectLists(SkyObject::ASTEROID).append(QPair<QString, const SkyObject *>(name, new_asteroid));
    }
}

void AsteroidsComponent::draw(SkyPainter *skyp)
{
    Q_UNUSED(skyp)
#ifndef KSTARS_LITE
    if (!selected())
        return;

    bool hideLabels = !Options::showAsteroidNames() || (SkyMap::Instance()->isSlewing() && Options::hideLabels());

    double showLimit     = Options::magLimitAsteroid();
    double lgmin         = log10(MINZOOM);
    double lgmax         = log10(MAXZOOM);
    double lgz           = log10(Options::zoomFactor());
    double labelMagLimit = 2.5 + Options::asteroidLabelDensity() / 5.0;
    labelMagLimit += (15.0 - labelMagLimit) * (lgz - lgmin) / (lgmax - lgmin);
    if (labelMagLimit > 10.0)
        labelMagLimit = 10.0;
    //printf("labelMagLim = %.1f\n", labelMagLimit );

    skyp->setBrush(QBrush(QColor("gray")));

    foreach (SkyObject *so, m_ObjectList)
    {
        KSAsteroid *ast = dynamic_cast<KSAsteroid *>(so);

        if (!ast->toDraw() || std::isnan(ast->mag()) || ast->mag() > showLimit)
            continue;

        bool drawn = false;

        if (ast->image().isNull() == false)
            drawn = skyp->drawPlanet(ast);
        else
            drawn = skyp->drawPointSource(ast, ast->mag());

        if (drawn && !(hideLabels || ast->mag() >= labelMagLimit))
            SkyLabeler::AddLabel(ast, SkyLabeler::ASTEROID_LABEL);
    }
#endif
}

SkyObject *AsteroidsComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    SkyObject *oBest = nullptr;

    if (!selected())
        return nullptr;

    for (auto o : m_ObjectList)
    {
        if (!((dynamic_cast<KSAsteroid*>(o)->toDraw())))
            continue;

        double r = o->angularDistanceTo(p).Degrees();
        if (r < maxrad)
        {
            oBest  = o;
            maxrad = r;
        }
    }

    return oBest;
}

void AsteroidsComponent::updateDataFile(bool isAutoUpdate)
{
    delete(downloadJob);
    downloadJob = new FileDownloader();

    if (isAutoUpdate == false)
        downloadJob->setProgressDialogEnabled(true, i18n("Asteroid Update"), i18n("Downloading asteroids updates..."));
    downloadJob->registerDataVerification([&](const QByteArray & data)
    {
        return data.startsWith("full_name");
    });

    QObject::connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    if (isAutoUpdate == false)
        QObject::connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    QUrl url = QUrl("https://ssd.jpl.nasa.gov/sbdb_query.cgi");

    QByteArray mag       = QString::number(Options::magLimitAsteroidDownload()).toUtf8();
    QByteArray post_data = KSUtils::getJPLQueryString("ast", "AcBdBiBhBgBjBlBkBmBqBbAiAjAgAkAlApAqArAsBsBtCh",
    QVector<KSUtils::JPLFilter> { { "Ai", "<", mag } });

    downloadJob->post(url, post_data);
}

void AsteroidsComponent::downloadReady()
{
    // Comment the first line
    QByteArray data = downloadJob->downloadedData();
    data.insert(0, '#');

    // Write data to asteroids.dat
    QFile file(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("asteroids.dat"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        file.write(data);
        file.close();
    }
    else qCWarning(KSTARS) << "Failed writing asteroid data to" << file.fileName();

    QString focusedAstroid;

#ifdef KSTARS_LITE
    SkyObject *foc = KStarsLite::Instance()->map()->focusObject();
    if (foc && foc->type() == SkyObject::ASTEROID)
    {
        focusedAstroid = foc->name();
        KStarsLite::Instance()->map()->setFocusObject(nullptr);
    }
#else
    SkyObject *foc = KStars::Instance()->map()->focusObject();
    if (foc && foc->type() == SkyObject::ASTEROID)
    {
        focusedAstroid = foc->name();
        KStars::Instance()->map()->setFocusObject(nullptr);
    }

#endif
    // Reload asteroids
    loadData(true);

#ifdef KSTARS_LITE
    KStarsLite::Instance()->data()->setFullTimeUpdate();
    if (!focusedAstroid.isEmpty())
        KStarsLite::Instance()->map()->setFocusObject(KStarsLite::Instance()->data()->objectNamed(focusedAstroid));
#else
    if (!focusedAstroid.isEmpty())
        KStars::Instance()->map()->setFocusObject(KStars::Instance()->data()->objectNamed(focusedAstroid));
    KStars::Instance()->data()->setFullTimeUpdate();
#endif
    downloadJob->deleteLater();
}

void AsteroidsComponent::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading asteroids data: %1", errorString));
    qDebug() << i18n("Error downloading asteroids data: %1", errorString);
    downloadJob->deleteLater();
}
