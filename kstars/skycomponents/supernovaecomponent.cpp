/***************************************************************************
                          supernovaecomponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2011/18/06
    copyright            : (C) 2011 by Samikshan Bairagya
    email                : samikshan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "supernovaecomponent.h"
#include "skymap.h"
#include "skypainter.h"
#include "skymesh.h"
#include "skylabeler.h"
#include "projections/projector.h"
#include "dms.h"
#include "Options.h"
#include "notifyupdatesui.h"

#include "kdebug.h"
#include "ksfilereader.h"
#include "kprocess.h"
#include "kstandarddirs.h"
#include "kstarsdata.h"

SupernovaeComponent::SupernovaeComponent(SkyComposite* parent): ListComponent(parent)
{
    loadData();
}

SupernovaeComponent::~SupernovaeComponent() {}

void SupernovaeComponent::update(KSNumbers* num)
{
    if ( ! selected() )
        return;
    KStarsData *data = KStarsData::Instance();
    foreach ( SkyObject *so, m_ObjectList ) {
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
    QString line;
    QString serialNo, hostGalaxy, date, type, offset, SNPosition, discoverers ;
    QStringList fields;
    dms ra, dec;
    float magnitude;
    KSFileReader fileReader;
    bool ok;
    kDebug()<<"Loading Supernovae data"<<endl;
    if( !fileReader.open("supernovae.dat")) return;
    //m_ObjectList.clear();
    latest.clear();
    objectNames(SkyObject::SUPERNOVA).clear();

    while (fileReader.hasMoreLines())
    {
        line=fileReader.readLine();
        Supernova *sup=0;
        //kDebug()<<line[0]<<" "<<line.size()<<endl;
        if(line[0]=='#' || line.size()<10) continue;
        fields=line.split(',');
        serialNo=fields.at(0);
        hostGalaxy=fields.at(1);
        date=fields.at(2);

        ra=dms(fields.at(3),false);
        dec=dms(fields.at(4));

        offset=fields.at(5);

        if(fields.at(6).isEmpty())
            magnitude=99;
        else
            magnitude=fields.at(6).toFloat(&ok);

        SNPosition=fields.at(8);
        type=fields.at(10);
        discoverers=fields.at(12);

        sup=new Supernova(ra, dec, date, magnitude, serialNo, type, hostGalaxy, offset, discoverers);

        if (!m_ObjectList.empty())
        {
            if ( findByName(sup->name() ) == 0 )
            {
                //kDebug()<<"List of supernovae not empty. Found new supernova";
                m_ObjectList.append(sup);
                latest.append(sup);
            }/*
            else
                m_ObjectList.append(sup);*/
        }
        else             //if the list is empty
        {
            m_ObjectList.append(sup);
            latest.append(sup);
            //notifyNewSupernovae();
        }

        objectNames(SkyObject::SUPERNOVA).append(sup->name());
    }
    //notifyNewSupernovae();
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
        //kDebug()<<r;
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
    //kDebug()<<"selected()="<<selected();
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

void SupernovaeComponent::notifyNewSupernovae()
{
    //kDebug()<<"New Supernovae discovered";
    QList<SkyObject*> latestList;
    foreach (SkyObject * so, latest)
    {
        Supernova * sup = (Supernova *)so;

        if (sup->getMagnitude() > float(Options::magnitudeLimitAlertSupernovae())) 
        {
            kDebug()<<"Not Bright enough to be notified";
            continue;
        }

        kDebug()<<"Bright enough to be notified";
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
}


void SupernovaeComponent::updateDataFile()
{
    KProcess *parser=new KProcess;
    QString filename= KStandardDirs::locate("appdata","scripts/supernova_updates_parser.py") ;
    kDebug()<<filename;
    int execstatus=parser->execute("python",QStringList(filename));
    if ( execstatus!=0 ) {
        QString errmsg;
        switch (execstatus) {
            case -2:
                errmsg = "Could not run python to update supernova information";
                break;
            case -1:
                errmsg = "Python process that updates the supernova information crashed";
                break;
            default:
                errmsg = "Python process that updates the supernova information failed with error code " + QString::number(execstatus);
                break;
        }
        KMessageBox::sorry(0,errmsg,i18n("Supernova information update failed"));
    }
    parser->close();
    kDebug()<<"HERE";
    latest.clear();
    loadData();
    notifyNewSupernovae();
}
