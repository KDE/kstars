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
#include "notifyupdatesui.h"

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
   /* QString serialNo, hostGalaxy, date, type, offset, SNPosition, discoverers ;
    dms ra, dec;
    float magnitude;
    qDebug()<<"Loading Supernovae data";
    //m_ObjectList.clear();
    latest.clear();
    objectNames(SkyObject::SUPERNOVA).clear();
    objectLists(SkyObject::SUPERNOVA).clear();

    //SN,  Host Galaxy,  Date,  R.A.,  Dec.,  Offset,  Mag.,  Disc.Ref.,  SN Position,  Posn.Ref.,  Typ,  SN,  Discoverer(s)
    QList< QPair<QString,KSParser::DataTypes> > sequence;
    sequence.append(qMakePair(QString("serialNo"),      KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("hostGalaxy"),    KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("date"),          KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("ra"),            KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("dec"),           KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("offset"),        KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("magnitude"),     KSParser::D_FLOAT));
    sequence.append(qMakePair(QString("ignore1"),       KSParser::D_SKIP));
    sequence.append(qMakePair(QString("SNPosition"),    KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("ignore2"),       KSParser::D_SKIP));
    sequence.append(qMakePair(QString("type"),          KSParser::D_QSTRING));
    sequence.append(qMakePair(QString("ignore3"),       KSParser::D_SKIP));
    sequence.append(qMakePair(QString("discoverers"),   KSParser::D_QSTRING));

    //KSParser snParser(file_name, '#', sequence);

    */

    objectNames(SkyObject::SUPERNOVA).clear();
    qDeleteAll(m_ObjectList);
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

    foreach (const QJsonValue& snValue, sArray)
    {
        const QJsonObject propObject = snValue.toObject();
        mag=99.9;
        z=0;

        if (propObject.contains("claimedtype") == false)
            continue;

        ra   = ((propObject["ra"].toArray()[0]).toObject()["value"]).toString();
        if (ra.isEmpty())
            continue;
        de   = ((propObject["dec"].toArray()[0]).toObject()["value"]).toString();
        name = propObject["name"].toString();        
        type = ((propObject["claimedtype"].toArray()[0]).toObject()["value"]).toString();
        host = ((propObject["host"].toArray()[0]).toObject()["value"]).toString();
        date = ((propObject["discoverdate"].toArray()[0]).toObject()["value"]).toString();
        z    = ((propObject["redshift"].toArray()[0]).toObject()["value"]).toString().toDouble();
        mag  = ((propObject["maxappmag"].toArray()[0]).toObject()["value"]).toString().toDouble();

        Supernova * sup = new Supernova(name, dms::fromString(ra, false), dms::fromString(de, true), type, host, date, z, mag);

        //sup->EquatorialToHorizontal(KStarsData::Instance()->lst(), KStarsData::Instance()->geo()->lat());

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
    return 0;
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

    foreach ( SkyObject *so, m_ObjectList ) {
        Supernova *sup = (Supernova*) so;
        float mag = sup->mag();

        if (mag > float( Options::magnitudeLimitShowSupernovae())) continue;

        //Do not draw if mag>maglim
        if ( mag > maglim ) {
            continue;
        }
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
    /*
    QString output  = KSPaths::writableLocation(QStandardPaths::GenericDataLocation) + "supernovae.dat";
    QString filename= KSPaths::locate(QStandardPaths::GenericDataLocation, "scripts/supernova_updates_parser.py") ;
    QStringList args;
    args << filename << output;
    //qDebug()<<filename;
    m_Parser = new QProcess;
    connect( m_Parser, SIGNAL( finished( int, QProcess::ExitStatus ) ), this, SLOT( slotDataFileUpdateFinished( int, QProcess::ExitStatus ) ) );
    m_Parser->start("python2", args);
    */
}

/*
void SupernovaeComponent::slotDataFileUpdateFinished( int exitCode, QProcess::ExitStatus exitStatus )
{
    Q_UNUSED(exitStatus)

    if ( exitCode ) {
        QString errmsg;
        switch ( exitCode ) {
            case -2:
                errmsg = i18n("Could not run python to update supernova information. This could be because you do not have python2 installed, or the python2 binary could not be found in the usual locations.");       
                break;
            case -1:
                errmsg = i18n("Python process that updates the supernova information crashed");
                break;
            default:
                errmsg = i18n( "Python process that updates the supernova information failed with error code %1. This could likely be because the computer is not connected to the internet or because the server containing supernova information is not responding.", QString::number( exitCode ) );
                break;
        }
        #ifndef KSTARS_LITE
        if( KStars::Instance() && SkyMap::Instance() ) // Displaying a message box causes entry of control into the Qt event loop. Can lead to segfault if we are checking for supernovae alerts during initialization!
            KMessageBox::sorry( 0, errmsg, i18n("Supernova information update failed") );
        // FIXME: There should be a better way to check if KStars is fully initialized. Maybe we should have a static boolean in the KStars class. --asimha
        #else
        if( KStarsLite::Instance() && SkyMapLite::Instance() ) // Displaying a message box causes entry of control into the Qt event loop. Can lead to segfault if we are checking for supernovae alerts during initialization!
            qDebug() << errmsg << i18n("Supernova information update failed");
        #endif
    }
    else {
        //qDebug()<<"HERE";
        latest.clear();
        loadData();
        notifyNewSupernovae();
    }
    delete m_Parser;
    m_Parser = 0;
}
*/
