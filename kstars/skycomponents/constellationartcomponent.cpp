/***************************************************************************
                          ConstellationArtComponent.cpp  -  K Desktop Planetarium
                             -------------------
    begin                : 2015-05-27
    copyright            : (C) 2015 by M.S.Adityan
    email                : msadityan@gmail.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "constellationartcomponent.h"
#include "kstars/auxiliary/ksfilereader.h"
#include "kstars/skymap.h"
#include "kstars/projections/projector.h"

ConstellationArtComponent::ConstellationArtComponent( SkyComposite *parent, CultureList *cultures ):SkyComponent(parent)
{
    cultureName = cultures->current();
    loadData();
}

ConstellationArtComponent::~ConstellationArtComponent()
{    while( !m_ConstList.isEmpty() ) {
        ConstellationsArt *o = m_ConstList.takeFirst();
        delete o;
    }
}

void ConstellationArtComponent::loadData(){

        QSqlDatabase skydb = QSqlDatabase::addDatabase("QSQLITE", "skycultures");
        QString dbfile = QStandardPaths::locate(QStandardPaths::DataLocation, "skycultures.sqlite");

        skydb.setDatabaseName(dbfile);
        if (skydb.open() == false)
        {
            qWarning() << "Unable to open sky cultures database file " << dbfile << endl;
            return;
        }

         QSqlQuery get_query(skydb);
         if (!get_query.exec("SELECT * FROM western"))
         {
            qDebug() << get_query.lastError();
             return;
         }

         while (get_query.next())
         {
             int midX                 = get_query.value("X1").toInt();
             int midY                 = get_query.value("Y1").toInt();
             QString midRA            = get_query.value("RA1").toString();
             QString midDEC           = get_query.value("DE1").toString();
             int X                 = get_query.value("X2").toInt();
             int Y                 = get_query.value("Y2").toInt();
             QString RA            = get_query.value("RA2").toString();
             QString DEC           = get_query.value("DE2").toString();
             QString abbreviation   = get_query.value("Abbreviation").toString();
             QString filename       = get_query.value("Filename").toString();

             dms midra = dms::fromString(midRA,false);
             dms middec = dms::fromString(midDEC,true);
             dms ra = dms::fromString(RA,false);
             dms dec = dms::fromString(DEC,true);

             // appends constellation info
             ConstellationsArt *ca = new ConstellationsArt (midX, midY, midra,middec, X,Y,ra,dec,abbreviation,filename);
             m_ConstList.append(ca);
             qDebug()<<"Successsfully read skyculture.sqlite"<<midX<<midY<<midRA<<midDEC;
         }
        skydb.close();
}

void ConstellationArtComponent::showList()
{
    int i = 0;
    for(i = 0; i < m_ConstList.size(); i++)
    {
        qDebug()<<m_ConstList[i]->getAbbrev()<<m_ConstList[i]->getImageFileName();
        qDebug()<<m_ConstList[i]->getmidx()<<m_ConstList[i]->getmidy();
        qDebug()<<m_ConstList[i]->getx()<<m_ConstList[i]->gety();
    }
}

void ConstellationArtComponent::draw(SkyPainter *skyp){

    if(Options::showConstellationArt()){
         skyp->drawConstellationArtImage(m_ConstList[0]);
    }

    //Loops through the QList containing all data required to draw western constellations.
    //There are 85 images, so m_ConstList.size() should return 85.
}
