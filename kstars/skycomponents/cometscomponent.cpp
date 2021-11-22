/*
    SPDX-FileCopyrightText: 2005 Jason Harris <kstars@30doradus.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "cometscomponent.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#endif
#include "ksfilereader.h"
#include "kspaths.h"
#include "kstarsdata.h"
#include "ksutils.h"
#include "ksnotification.h"
#include "kstars_debug.h"
#ifndef KSTARS_LITE
#include "skymap.h"
#else
#include "kstarslite.h"
#endif
#include "Options.h"
#include "skylabeler.h"
#include "skypainter.h"
#include "solarsystemcomposite.h"
#include "auxiliary/filedownloader.h"
#include "auxiliary/kspaths.h"
#include "projections/projector.h"
#include "skyobjects/kscomet.h"

#include <QFile>
#include <QHttpMultiPart>
#include <QPen>
#include <QStandardPaths>

#include <cmath>

CometsComponent::CometsComponent(SolarSystemComposite *parent)
    : SolarSystemListComponent(parent)
{
    loadData();
}

bool CometsComponent::selected()
{
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
void CometsComponent::loadData()
{
    QString name, orbit_class;

    emitProgressText(i18n("Loading comets"));
    qCInfo(KSTARS) << "Loading comets";

    qDeleteAll(m_ObjectList);
    m_ObjectList.clear();

    objectNames(SkyObject::COMET).clear();
    objectLists(SkyObject::COMET).clear();

    QString file_name = KSPaths::locate(QStandardPaths::AppDataLocation, QString("cometels.json.gz"));

    try
    {
        KSUtils::MPCParser com_parser(file_name);
        com_parser.for_each(
            [&](const auto & get)
        {
            KSComet *com = nullptr;
            name = get("Designation_and_name").toString();

            int perihelion_year, perihelion_month, perihelion_day, perihelion_hour, perihelion_minute, perihelion_second;

            // Perihelion Distance in AU
            double perihelion_distance = get("Perihelion_dist").toDouble();
            // Orbital Eccentricity
            double eccentricity = get("e").toDouble();
            // Argument of perihelion, J2000.0 (degrees)
            double perihelion_argument = get("Peri").toDouble();
            // Longitude of the ascending node, J2000.0 (degrees)
            double ascending_node = get("Node").toDouble();
            // Inclination in degrees, J2000.0 (degrees)
            double inclination = get("i").toDouble();

            // Perihelion Date
            perihelion_year = get("Year_of_perihelion").toInt();
            perihelion_month = get("Month_of_perihelion").toInt();
            // Stored as double in MPC
            double peri_day = get("Day_of_perihelion").toDouble();
            perihelion_day = static_cast<int>(peri_day);
            double peri_hour = (peri_day - perihelion_day) * 24;
            perihelion_hour = static_cast<int>(peri_hour);
            perihelion_minute = static_cast<int>((peri_hour - perihelion_hour) * 60);
            perihelion_second = ( (( peri_hour - perihelion_hour) * 60) - perihelion_minute) * 60;

            long double Tp = KStarsDateTime(QDate(perihelion_year, perihelion_month, perihelion_day),
                                            QTime(perihelion_hour, perihelion_minute, perihelion_second)).djd();

            // Orbit type
            orbit_class = get("Orbit_type").toString();
            double absolute_magnitude = get("H").toDouble();
            double slope_parameter = get("G").toDouble();

            com = new KSComet(name,
                              QString(),
                              perihelion_distance,
                              eccentricity,
                              dms(inclination),
                              dms(perihelion_argument),
                              dms(ascending_node),
                              Tp,
                              absolute_magnitude,
                              101.0,
                              slope_parameter,
                              101.0);

            com->setOrbitClass(orbit_class);
            com->setAngularSize(0.005);
            appendListObject(com);

            // Add *short* name to the list of object names
            objectNames(SkyObject::COMET).append(com->name());
            objectLists(SkyObject::COMET).append(QPair<QString, const SkyObject *>(com->name(), com));
        });
    }
    catch (const std::runtime_error)
    {
        qCInfo(KSTARS) << "Loading comets failed.";
        qCInfo(KSTARS) << " -> was trying to read " + file_name;
        return;
    }
}

// Used for JPL Data
// DO NOT REMOVE, we can revert to JPL at any time.
//void CometsComponent::loadData()
//{
//    QString name, orbit_id, orbit_class, dimensions;

//    emitProgressText(i18n("Loading comets"));
//    qCInfo(KSTARS) << "Loading comets";

//    qDeleteAll(m_ObjectList);
//    m_ObjectList.clear();

//    objectNames(SkyObject::COMET).clear();
//    objectLists(SkyObject::COMET).clear();

//    QString file_name =
//        KSPaths::locate(QStandardPaths::AppDataLocation, QString("comets.dat"));

//    try
//    {
//        KSUtils::JPLParser com_parser(file_name);
//        com_parser.for_each(
//            [&](const auto &get)
//            {
//                KSComet *com = nullptr;
//                name         = get("full_name").toString();
//                name         = name.trimmed();
//                bool neo;
//                double q, e, dble_i, dble_w, dble_N, Tp, earth_moid;
//                float M1, M2, K1, K2, diameter, albedo, rot_period, period;
//                q        = get("q").toString().toDouble();
//                e        = get("e").toString().toDouble();
//                dble_i   = get("i").toString().toDouble();
//                dble_w   = get("w").toString().toDouble();
//                dble_N   = get("om").toString().toDouble();
//                Tp       = get("tp").toString().toDouble();
//                orbit_id = get("orbit_id").toString();
//                neo      = get("neo").toString() == "Y";

//                if (get("M1").toString().toFloat() == 0.0)
//                    M1 = 101.0;
//                else
//                    M1 = get("M1").toString().toFloat();

//                if (get("M2").toString().toFloat() == 0.0)
//                    M2 = 101.0;
//                else
//                    M2 = get("M2").toString().toFloat();

//                diameter    = get("diameter").toString().toFloat();
//                dimensions  = get("extent").toString();
//                albedo      = get("albedo").toString().toFloat();
//                rot_period  = get("rot_per").toString().toFloat();
//                period      = get("per.y").toDouble();
//                earth_moid  = get("moid").toString().toDouble();
//                orbit_class = get("class").toString();
//                K1          = get("H").toString().toFloat();
//                K2          = get("G").toString().toFloat();

//                com = new KSComet(name, QString(), q, e, dms(dble_i), dms(dble_w),
//                                  dms(dble_N), Tp, M1, M2, K1, K2);
//                com->setOrbitID(orbit_id);
//                com->setNEO(neo);
//                com->setDiameter(diameter);
//                com->setDimensions(dimensions);
//                com->setAlbedo(albedo);
//                com->setRotationPeriod(rot_period);
//                com->setPeriod(period);
//                com->setEarthMOID(earth_moid);
//                com->setOrbitClass(orbit_class);
//                com->setAngularSize(0.005);
//                appendListObject(com);

//                // Add *short* name to the list of object names
//                objectNames(SkyObject::COMET).append(com->name());
//                objectLists(SkyObject::COMET)
//                    .append(QPair<QString, const SkyObject *>(com->name(), com));
//            });
//    }
//    catch (const std::runtime_error &e)
//    {
//        qCInfo(KSTARS) << "Loading comets failed.";
//        qCInfo(KSTARS) << " -> was trying to read " + file_name;
//        return;
//    }
//}

void CometsComponent::draw(SkyPainter *skyp)
{
    Q_UNUSED(skyp)
#ifndef KSTARS_LITE
    if (!selected() || Options::zoomFactor() < 10 * MINZOOM)
        return;

    bool hideLabels       = !Options::showCometNames() || (SkyMap::Instance()->isSlewing() && Options::hideLabels());
    double rsunLabelLimit = Options::maxRadCometName();

    //FIXME: Should these be config'able?
    skyp->setPen(QPen(QColor("transparent")));
    skyp->setBrush(QBrush(QColor("white")));

    for (auto so : m_ObjectList)
    {
        KSComet *com = dynamic_cast<KSComet *>(so);
        double mag   = com->mag();
        if (std::isnan(mag) == 0)
        {
            bool drawn = skyp->drawComet(com);
            if (drawn && !(hideLabels || com->rsun() >= rsunLabelLimit))
                SkyLabeler::AddLabel(com, SkyLabeler::COMET_LABEL);
        }
    }
#endif
}

// DO NOT REMOVE
//void CometsComponent::updateDataFile(bool isAutoUpdate)
//{
//    delete (downloadJob);
//    downloadJob = new FileDownloader();

//    if (isAutoUpdate == false)
//        downloadJob->setProgressDialogEnabled(true, i18n("Comets Update"),
//                                              i18n("Downloading comets updates..."));
//    downloadJob->registerDataVerification([&](const QByteArray &data)
//                                          { return data.startsWith("{\"signature\""); });

//    connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));

//    // For auto-update, we ignore errors
//    if (isAutoUpdate == false)
//        connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

//    QUrl url             = QUrl("https://ssd-api.jpl.nasa.gov/sbdb_query.api");
//    QByteArray post_data = KSUtils::getJPLQueryString(
//        "c",
//        "full_name,epoch.mjd,q,e,i,w,om,tp,orbit_id,neo,"
//        "M1,M2,diameter,extent,albedo,rot_per,per.y,moid,H,G,class",
//        QVector<KSUtils::JPLFilter>{});
//    // FIXME: find out what { "Af", "!=", "D" } used to mean

//    downloadJob->post(url, post_data);
//}

void CometsComponent::updateDataFile(bool isAutoUpdate)
{
    delete (downloadJob);
    downloadJob = new FileDownloader();

    if (isAutoUpdate == false)
        downloadJob->setProgressDialogEnabled(true, i18n("Comets Update"),
                                              i18n("Downloading comets updates..."));

    connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));

    // For auto-update, we ignore errors
    if (isAutoUpdate == false)
        connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    QUrl url = QUrl("https://www.minorplanetcenter.net/Extended_Files/cometels.json.gz");
    downloadJob->get(url);
}

void CometsComponent::downloadReady()
{
    // Comment the first line
    QByteArray data = downloadJob->downloadedData();

    // Write data to cometels.json.gz
    QFile file(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation))
               .filePath("cometels.json.gz"));
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(data);
        file.close();
    }
    else
        qCWarning(KSTARS) << "Failed writing comet data to" << file.fileName();

    QString focusedComet;

#ifdef KSTARS_LITE
    SkyObject *foc = KStarsLite::Instance()->map()->focusObject();
    if (foc && foc->type() == SkyObject::COMET)
    {
        focusedComet = foc->name();
        KStarsLite::Instance()->map()->setFocusObject(nullptr);
    }
#else
    SkyObject *foc = KStars::Instance()->map()->focusObject();
    if (foc && foc->type() == SkyObject::COMET)
    {
        focusedComet = foc->name();
        KStars::Instance()->map()->setFocusObject(nullptr);
    }
#endif

    // Reload comets
    loadData();

#ifdef KSTARS_LITE
    KStarsLite::Instance()->data()->setFullTimeUpdate();
    if (!focusedComet.isEmpty())
        KStarsLite::Instance()->map()->setFocusObject(
            KStarsLite::Instance()->data()->objectNamed(focusedComet));
#else
    if (!focusedComet.isEmpty())
        KStars::Instance()->map()->setFocusObject(
            KStars::Instance()->data()->objectNamed(focusedComet));
    KStars::Instance()->data()->setFullTimeUpdate();
#endif

    downloadJob->deleteLater();
}

// DO NOT REMOVE
//void CometsComponent::downloadReady()
//{
//    // Comment the first line
//    QByteArray data = downloadJob->downloadedData();

//    // Write data to comets.dat
//    QFile file(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation))
//                   .filePath("comets.dat"));
//    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
//    {
//        file.write(data);
//        file.close();
//    }
//    else
//        qCWarning(KSTARS) << "Failed writing comet data to" << file.fileName();

//    QString focusedComet;

//#ifdef KSTARS_LITE
//    SkyObject *foc = KStarsLite::Instance()->map()->focusObject();
//    if (foc && foc->type() == SkyObject::COMET)
//    {
//        focusedComet = foc->name();
//        KStarsLite::Instance()->map()->setFocusObject(nullptr);
//    }
//#else
//    SkyObject *foc = KStars::Instance()->map()->focusObject();
//    if (foc && foc->type() == SkyObject::COMET)
//    {
//        focusedComet = foc->name();
//        KStars::Instance()->map()->setFocusObject(nullptr);
//    }
//#endif

//    // Reload comets
//    loadData();

//#ifdef KSTARS_LITE
//    KStarsLite::Instance()->data()->setFullTimeUpdate();
//    if (!focusedComet.isEmpty())
//        KStarsLite::Instance()->map()->setFocusObject(
//            KStarsLite::Instance()->data()->objectNamed(focusedComet));
//#else
//    if (!focusedComet.isEmpty())
//        KStars::Instance()->map()->setFocusObject(
//            KStars::Instance()->data()->objectNamed(focusedComet));
//    KStars::Instance()->data()->setFullTimeUpdate();
//#endif

//    downloadJob->deleteLater();
//}

void CometsComponent::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading asteroids data: %1", errorString));
    qCCritical(KSTARS) << errorString;
    downloadJob->deleteLater();
}
