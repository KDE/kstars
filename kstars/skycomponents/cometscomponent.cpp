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

#include <QDebug>
#include <QFile>
#include <QHttpMultiPart>
#include <QPen>
#include <QStandardPaths>

#include <cmath>

CometsComponent::CometsComponent(SolarSystemComposite *parent) : SolarSystemListComponent(parent)
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
    QString name, orbit_id, orbit_class, dimensions;

    emitProgressText(i18n("Loading comets"));

    qDeleteAll(m_ObjectList);
    m_ObjectList.clear();

    objectNames(SkyObject::COMET).clear();
    objectLists(SkyObject::COMET).clear();

    QList<QPair<QString, KSParser::DataTypes>> sequence;
    sequence.append(qMakePair(QString("full name"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("epoch_mjd"), KSParser::D_INT));
    sequence.append(qMakePair(QString("q"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("e"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("i"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("w"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("om"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("tp_calc"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("orbit_id"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("neo"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("M1"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("M2"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("diameter"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("extent"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("albedo"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("rot_period"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("per_y"), KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("moid"), KSParser::D_DOUBLE));
    sequence.append(qMakePair(QString("class"), KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("H"), KSParser::D_SKIP));
    sequence.append(qMakePair(QString("G"), KSParser::D_SKIP));

    QString file_name = KSPaths::locate(QStandardPaths::AppDataLocation, QString("comets.dat"));
    KSParser cometParser(file_name, '#', sequence);

    QHash<QString, QVariant> row_content;
    while (cometParser.HasNextRow())
    {
        KSComet *com = nullptr;
        row_content  = cometParser.ReadNextRow();
        name         = row_content["full name"].toString();
        name         = name.trimmed();
        bool neo;
        double q, e, dble_i, dble_w, dble_N, Tp, earth_moid;
        float M1, M2, K1, K2, diameter, albedo, rot_period, period;
        q            = row_content["q"].toDouble();
        e            = row_content["e"].toDouble();
        dble_i       = row_content["i"].toDouble();
        dble_w       = row_content["w"].toDouble();
        dble_N       = row_content["om"].toDouble();
        Tp           = row_content["tp_calc"].toDouble();
        orbit_id     = row_content["orbit_id"].toString();
        neo          = row_content["neo"] == "Y";

        if (row_content["M1"].toFloat() == 0.0)
            M1 = 101.0;
        else
            M1 = row_content["M1"].toFloat();

        if (row_content["M2"].toFloat() == 0.0)
            M2 = 101.0;
        else
            M2 = row_content["M2"].toFloat();

        diameter    = row_content["diameter"].toFloat();
        dimensions  = row_content["extent"].toString();
        albedo      = row_content["albedo"].toFloat();
        rot_period  = row_content["rot_period"].toFloat();
        period      = row_content["per_y"].toFloat();
        earth_moid  = row_content["moid"].toDouble();
        orbit_class = row_content["class"].toString();
        K1          = row_content["H"].toFloat();
        K2          = row_content["G"].toFloat();

        com = new KSComet(name, QString(), q, e, dms(dble_i), dms(dble_w), dms(dble_N), Tp, M1, M2, K1, K2);
        com->setOrbitID(orbit_id);
        com->setNEO(neo);
        com->setDiameter(diameter);
        com->setDimensions(dimensions);
        com->setAlbedo(albedo);
        com->setRotationPeriod(rot_period);
        com->setPeriod(period);
        com->setEarthMOID(earth_moid);
        com->setOrbitClass(orbit_class);
        com->setAngularSize(0.005);
        appendListObject(com);

        // Add *short* name to the list of object names
        objectNames(SkyObject::COMET).append(com->name());
        objectLists(SkyObject::COMET).append(QPair<QString, const SkyObject *>(com->name(), com));
    }
}

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

void CometsComponent::updateDataFile(bool isAutoUpdate)
{
    delete (downloadJob);
    downloadJob = new FileDownloader();

    if (isAutoUpdate == false)
        downloadJob->setProgressDialogEnabled(true, i18n("Comets Update"), i18n("Downloading comets updates..."));
    downloadJob->registerDataVerification([&](const QByteArray & data)
    {
        return data.startsWith("full_name");
    });

    connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));

    // For auto-update, we ignore errors
    if (isAutoUpdate == false)
        connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    QUrl url             = QUrl("https://ssd.jpl.nasa.gov/sbdb_query.cgi");
    QByteArray post_data = KSUtils::getJPLQueryString("com", "AcBdBiBgBjBlBkBqBbAgAkAlApAqArAsBsBtChAmAn",
    QVector<KSUtils::JPLFilter> { { "Af", "!=", "D" } });

    downloadJob->post(url, post_data);
}

void CometsComponent::downloadReady()
{
    // Comment the first line
    QByteArray data = downloadJob->downloadedData();
    data.insert(0, '#');

    // Write data to comets.dat
    QFile file(QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("comets.dat"));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    {
        file.write(data);
        file.close();
    }
    else qCWarning(KSTARS) << "Failed writing comet data to" << file.fileName();

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
        KStarsLite::Instance()->map()->setFocusObject(KStarsLite::Instance()->data()->objectNamed(focusedComet));
#else
    if (!focusedComet.isEmpty())
        KStars::Instance()->map()->setFocusObject(KStars::Instance()->data()->objectNamed(focusedComet));
    KStars::Instance()->data()->setFullTimeUpdate();
#endif

    downloadJob->deleteLater();
}

void CometsComponent::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading asteroids data: %1", errorString));
    qCCritical(KSTARS) << errorString;
    downloadJob->deleteLater();
}
