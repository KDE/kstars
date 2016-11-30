/*  Supernova Component
    Copyright (C) 2016 Jasem Mutlaq <mutlaqja@ikarustech.com>

    Based on Samikshan Bairagya GSoC work.

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include "supernovaecomponent.h"

#include <QtConcurrent>
#include <QJsonDocument>
#include <QJsonValue>

#ifndef KSTARS_LITE
#include "skymap.h"
#else
#include "kstarslite.h"
#endif
#include "skypainter.h"
#include "skymesh.h"
#include "skylabeler.h"
#include "projections/projector.h"
#include "dms.h"
#include "Options.h"
#include "auxiliary/filedownloader.h"
#include "ksnotification.h"
//#include "notifyupdatesui.h"

#include "kstarsdata.h"
#include "auxiliary/kspaths.h"

SupernovaeComponent::SupernovaeComponent(SkyComposite* parent): ListComponent(parent)
{
    QtConcurrent::run(this, &SupernovaeComponent::loadData);
    //loadData();
}

SupernovaeComponent::~SupernovaeComponent() {}

void SupernovaeComponent::update(KSNumbers* num)
{
    if ( ! selected() )
        return;

    KStarsData *data = KStarsData::Instance();
    foreach ( SkyObject *so, m_ObjectList )
    {
        if( num )
            so->updateCoords( num );
        so->EquatorialToHorizontal( data->lst(), data->geo()->lat() );
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

    QString name, type, host, date, ra, de;
    float z, mag;

    QString sFileName = KSPaths::locate(QStandardPaths::GenericDataLocation, QString("catalog.min.json"));

    QFile sNovaFile(sFileName);

    if (sNovaFile.open(QIODevice::ReadOnly) == false)
    {
        qCritical() << "Unable to open supernova file" << sFileName;
        return;
    }        

    QJsonParseError pError;
    QJsonDocument sNova = QJsonDocument::fromJson(sNovaFile.readAll(), &pError);

    if (pError.error !=  QJsonParseError::NoError)
    {
        qCritical() << "Error parsing json document" << pError.errorString();
        return;
    }

    if (sNova.isArray() == false)
    {
        qCritical() << "Invalid document format! No JSON array.";
        return;
    }

    QJsonArray sArray = sNova.array();
    bool ok=false;

    foreach (const QJsonValue& snValue, sArray)
    {
        const QJsonObject propObject = snValue.toObject();
        mag=99.9;
        z=0;

        //if (propObject.contains("claimedtype") == false)
            //continue;

        ra   = ((propObject["ra"].toArray()[0]).toObject()["value"]).toString();
        de   = ((propObject["dec"].toArray()[0]).toObject()["value"]).toString();
        if (ra.isEmpty() || de.isEmpty())
            continue;

        name = propObject["name"].toString();        
        type = ((propObject["claimedtype"].toArray()[0]).toObject()["value"]).toString();
        host = ((propObject["host"].toArray()[0]).toObject()["value"]).toString();
        date = ((propObject["discoverdate"].toArray()[0]).toObject()["value"]).toString();
        z    = ((propObject["redshift"].toArray()[0]).toObject()["value"]).toString().toDouble(&ok);
        if (ok == false)
            z = 99.9;
        mag  = ((propObject["maxappmag"].toArray()[0]).toObject()["value"]).toString().toDouble(&ok);
        if (ok == false)
            mag = 99.9;

        Supernova * sup = new Supernova(name, dms::fromString(ra, false), dms::fromString(de, true), type, host, date, z, mag);

        objectNames(SkyObject::SUPERNOVA).append(name);

        m_ObjectList.append(sup);
        objectLists( SkyObject::SUPERNOVA ).append(QPair<QString, const SkyObject*>(name,sup));
    }
}

SkyObject* SupernovaeComponent::findByName(const QString& name)
{
    foreach (SkyObject* o, m_ObjectList)
    {
         if( QString::compare( o->name(),name, Qt::CaseInsensitive ) == 0 )
             return o;
    }
    //if no object is found then..
    return NULL;
}

SkyObject* SupernovaeComponent::objectNearest(SkyPoint* p, double& maxrad)
{
    SkyObject* oBest=0;
    double rBest=maxrad;

    foreach ( SkyObject* so, m_ObjectList)
    {
        double r = so->angularDistanceTo(p).Degrees();
        //qDebug()<<r;
        if( r < rBest )
        {
            oBest=so;
            rBest=r;
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

    return 14.0 + 2.222*( lgz - lgmin ) + 2.222*log10( static_cast<double>(Options::starDensity()) );
}


void SupernovaeComponent::draw(SkyPainter *skyp)
{
    //qDebug()<<"selected()="<<selected();
    if ( ! selected() )
        return;

    double maglim = zoomMagnitudeLimit();

    foreach ( SkyObject *so, m_ObjectList )
    {
        Supernova *sup = (Supernova*) so;
        float mag = sup->mag();

        if (mag > float( Options::magnitudeLimitShowSupernovae())) continue;

        //Do not draw if mag>maglim
        if ( mag > maglim )
            continue;
        skyp->drawSupernova(sup);
    }
}

#if 0
void SupernovaeComponent::notifyNewSupernovae()
{
#ifndef KSTARS_LITE
    //qDebug()<<"New Supernovae discovered";
    QList<SkyObject*> latestList;
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
    downloadJob = new FileDownloader();

    downloadJob->setProgressDialogEnabled(true, i18n("Supernovae Update"), i18n("Downloading Supernovae updates..."));

    QObject::connect(downloadJob, SIGNAL(downloaded()), this, SLOT(downloadReady()));
    QObject::connect(downloadJob, SIGNAL(error(QString)), this, SLOT(downloadError(QString)));

    QString output  = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "catalog.min.json";

    downloadJob->setDownloadedFileURL(QUrl::fromLocalFile(output));

    downloadJob->get(QUrl("https://sne.space/astrocats/astrocats/supernovae/output/catalog.min.json"));
}

void SupernovaeComponent::downloadReady()
{
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
    KSNotification::error(i18n("Error downloading asteroids data: %1", errorString));
    downloadJob->deleteLater();
}
