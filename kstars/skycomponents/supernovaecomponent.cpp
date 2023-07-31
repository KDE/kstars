/*
    SPDX-FileCopyrightText: 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "supernovaecomponent.h"

#ifndef KSTARS_LITE
#include "kstars.h"
#include "skymap.h"
#else
#include "kstarslite.h"
#endif
#include "dms.h"
#include "kstars_debug.h"
#include "ksnotification.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skylabeler.h"
#include "skymesh.h"
#include "skypainter.h"
#include "auxiliary/filedownloader.h"
#include "projections/projector.h"
#include "auxiliary/kspaths.h"

#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonValue>

#include <zlib.h>
#include <fstream>
#include <stdio.h>

#include <csv.h>

const QString SupernovaeComponent::tnsDataFilename("tns_public_objects.csv");
const QString SupernovaeComponent::tnsDataFilenameZip("tns-daily.csv.gz");
const QString SupernovaeComponent::tnsDataUrl(
    "https://indilib.org/jdownloads/kstars/tns-daily.csv.gz");

SupernovaeComponent::SupernovaeComponent(SkyComposite *parent) : ListComponent(parent)
{
    //QtConcurrent::run(this, &SupernovaeComponent::loadData);
    //loadData(); MagnitudeLimitShowSupernovae
    connect(Options::self(), &Options::SupernovaDownloadUrlChanged, this,
            &SupernovaeComponent::loadData);
    connect(Options::self(), &Options::MagnitudeLimitShowSupernovaeChanged, this,
            &SupernovaeComponent::loadData);
}

void SupernovaeComponent::update(KSNumbers *num)
{
    if (!selected() || !m_DataLoaded)
        return;

    KStarsData *data = KStarsData::Instance();
    for (auto so : m_ObjectList)
    {
        if (num)
            so->updateCoords(num);
        so->EquatorialToHorizontal(data->lst(), data->geo()->lat());
    }
}

bool SupernovaeComponent::selected()
{
    return Options::showSupernovae();
}

void SupernovaeComponent::loadData()
{
    qDeleteAll(m_ObjectList);
    m_ObjectList.clear();

    objectNames(SkyObject::SUPERNOVA).clear();
    objectLists(SkyObject::SUPERNOVA).clear();

    auto sFileName = KSPaths::locate(QStandardPaths::AppLocalDataLocation, QString(tnsDataFilename));

    try
    {
        io::CSVReader<26, io::trim_chars<' '>, io::double_quote_escape<',', '\"'>,
        io::ignore_overflow>
        in(sFileName.toLocal8Bit());
        // skip header
        const char *line = in.next_line();
        if (line == nullptr)
        {
            qCritical() << "file is empty\n";
            return;
        }

        std::string id, name, ra_s, dec_s, type;
        double redshift;
        std::string host_name;
        double host_redshift;
        std::string reporting_group, disc_datasource, classifying_group, assoc_group,
            disc_internal_name, disc_instrument, classifying_instrument;
        int tns_at, is_public;
        std::string end_prop_period;
        double discovery_mag;
        std::string discovery_filter, discovery_date_s, sender, remarks,
            discovery_bibcode, classification_bibcode, ext_catalog;

        while (in.read_row(
                    id, name, ra_s, dec_s, type, redshift, host_name, host_redshift,
                    reporting_group, disc_datasource, classifying_group, assoc_group,
                    disc_internal_name, disc_instrument, classifying_instrument, tns_at,
                    is_public, end_prop_period, discovery_mag, discovery_filter, discovery_date_s,
                    sender, remarks, discovery_bibcode, classification_bibcode, ext_catalog))
        {
            auto discovery_date =
                QDateTime::fromString(discovery_date_s.c_str(), Qt::ISODate);
            QString qname = QString(name.c_str());
            dms ra(QString(ra_s.c_str()), false);
            dms dec(QString(dec_s.c_str()), true);

            Supernova *sup = new Supernova(
                qname, ra, dec, QString(type.c_str()), QString(host_name.c_str()),
                QString(discovery_date_s.c_str()), redshift, discovery_mag, discovery_date);

            objectNames(SkyObject::SUPERNOVA).append(QString(name.c_str()));

            appendListObject(sup);
            objectLists(SkyObject::SUPERNOVA)
            .append(QPair<QString, const SkyObject *>(qname, sup));
        }

        m_DataLoading = false;
        m_DataLoaded  = true;
    }
    catch (io::error::can_not_open_file &ex)
    {
        qCCritical(KSTARS) << "could not open file " << sFileName.toLocal8Bit() << "\n";
        return;
    }
    catch (std::exception &ex)
    {
        qCCritical(KSTARS) << "unknown exception happened:" << ex.what() << "\n";
    }
}

SkyObject *SupernovaeComponent::objectNearest(SkyPoint *p, double &maxrad)
{
    if (!selected() || !m_DataLoaded)
        return nullptr;

    SkyObject *oBest = nullptr;
    double rBest     = maxrad;

    for (auto &so : m_ObjectList)
    {
        double r = so->angularDistanceTo(p).Degrees();
        //qDebug()<<r;
        if (r < rBest)
        {
            oBest = so;
            rBest = r;
        }
    }
    maxrad = rBest;
    return oBest;
}

float SupernovaeComponent::zoomMagnitudeLimit()
{
    //adjust maglimit for ZoomLevel
    double lgmin = log10(MINZOOM);
    double lgz   = log10(Options::zoomFactor());

    return 14.0 + 2.222 * (lgz - lgmin) +
           2.222 * log10(static_cast<double>(Options::starDensity()));
}

void SupernovaeComponent::draw(SkyPainter *skyp)
{
    //qDebug()<<"selected()="<<selected();
    if (!selected())
        return;
    else if (!m_DataLoaded)
    {
        if (!m_DataLoading)
        {
            m_DataLoading = true;
            QtConcurrent::run(this, &SupernovaeComponent::loadData);
        }
        return;
    }

    double maglim = zoomMagnitudeLimit();
    double refage = Options::supernovaDetectionAge();
    bool hostOnly = Options::supernovaeHostOnly();
    bool classifiedOnly = Options::supernovaeClassifiedOnly();

    for (auto so : m_ObjectList)
    {
        Supernova *sup = dynamic_cast<Supernova *>(so);
        float mag      = sup->mag();
        float age      = sup->getAgeDays();
        QString type     = sup->getType();

        if (mag > float(Options::magnitudeLimitShowSupernovae()))
            continue;

        if (age > refage)
            continue;

        // only SN with host galaxy?
        if (hostOnly && sup->getHostGalaxy() == "")
            continue;

        // Do not draw if mag>maglim
        if (mag > maglim && Options::limitSupernovaeByZoom())
            continue;

        // classified SN only?
        if (classifiedOnly && type == "")
            continue;

        skyp->drawSupernova(sup);
    }
}

#if 0
void SupernovaeComponent::notifyNewSupernovae()
{
#ifndef KSTARS_LITE
    //qDebug()<<"New Supernovae discovered";
    QList<SkyObject *> latestList;
    foreach (SkyObject * so, latest)
    {
        Supernova * sup = (Supernova *)so;

        if (sup->getMagnitude() > float(Options::magnitudeLimitAlertSupernovae()))
        {
            //qDebug()<<"Not Bright enough to be notified";
            continue;
        }

        //qDebug()<<"Bright enough to be notified";
        latestList.append(so);
    }
    if (!latestList.empty())
    {
        NotifyUpdatesUI * ui = new NotifyUpdatesUI(0);
        ui->addItems(latestList);
        ui->show();
    }
    //     if (!latest.empty())
    //         KMessageBox::informationList(0, i18n("New Supernovae discovered!"), latestList, i18n("New Supernovae discovered!"));
#endif
}
#endif

void SupernovaeComponent::slotTriggerDataFileUpdate()
{
    delete (downloadJob);
    downloadJob    = new FileDownloader();
    auto shownames = Options::showSupernovaNames();
    auto age       = Options::supernovaDetectionAge();
    QString url    = Options::supernovaDownloadUrl();
    QString output = QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                     .filePath(tnsDataFilenameZip);

    if (!url.startsWith("file://"))
    {
        qInfo() << "fetching data from web: " << url << "\n";
        downloadJob->setProgressDialogEnabled(true, i18n("Supernovae Update"),
                                              i18n("Downloading Supernovae updates..."));

        QObject::connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
        QObject::connect(downloadJob, SIGNAL(error(QString)), this,
                         SLOT(downloadError(QString)));

        downloadJob->setDownloadedFileURL(QUrl::fromLocalFile(output));
        downloadJob->get(QUrl(url));
    }
    else
    {
        // if we have a local file as url
        // copy data manually to target location
        QString fname = url.right(url.size() - 7);
        qInfo() << "fetching data from local file at: " << fname << "\n";
        QFile::remove(fname);
        auto res = QFile::copy(fname, output);
        qInfo() << "copy returned: " << res << "\n";
        // uncompress csv
        unzipData();
        // Reload Supernova
        loadData();
    }
}

void SupernovaeComponent::unzipData()
{
    // TODO: error handling
    std::string ifpath =
        QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(tnsDataFilenameZip)
        .toStdString();
    std::string ofpath =
        QDir(KSPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
        .filePath(tnsDataFilename)
        .toStdString();
    auto fhz  = gzopen(ifpath.c_str(), "rb");
    auto fout = fopen(ofpath.c_str(), "wb");

    if (fhz == NULL)
    {
        printf("Error: Failed to gzopen %s\n", ifpath.c_str());
        exit(0);
    }
    unsigned char unzipBuffer[8192];
    unsigned int unzippedBytes;
    while (true)
    {
        unzippedBytes = gzread(fhz, unzipBuffer, 8192);
        if (unzippedBytes > 0)
        {
            fwrite(unzipBuffer, 1, unzippedBytes, fout);
        }
        else
        {
            break;
        }
    }
    gzclose(fhz);
    fclose(fout);
}

void SupernovaeComponent::downloadReady()
{
    // uncompress csv
    unzipData();
    // Reload Supernova
    loadData();
#ifdef KSTARS_LITE
    KStarsLite::Instance()->data()->setFullTimeUpdate();
#else
    KStars::Instance()->data()->setFullTimeUpdate();
#endif
    downloadJob->deleteLater();
}

void SupernovaeComponent::downloadError(const QString &errorString)
{
    KSNotification::error(i18n("Error downloading supernova data: %1", errorString));
    downloadJob->deleteLater();
}
