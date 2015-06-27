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

ConstellationArtComponent::ConstellationArtComponent( SkyComposite *parent ):SkyComponent(parent)
{
    loadData();
}

ConstellationArtComponent::~ConstellationArtComponent()
{    while( !m_ConstList.isEmpty() ) {
        ConstellationsArt *o = m_ConstList.takeFirst();
        delete o;
    }
}

void ConstellationArtComponent::loadData(){
        int i = 0;

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
             int X1                 = get_query.value("X1").toInt();
             int Y1                 = get_query.value("Y1").toInt();
             QString RA1            = get_query.value("RA1").toString();
             QString DEC1           = get_query.value("DE1").toString();
             int X2                 = get_query.value("X2").toInt();
             int Y2                 = get_query.value("Y2").toInt();
             QString RA2            = get_query.value("RA2").toString();
             QString DEC2           = get_query.value("DE2").toString();
             QString abbreviation   = get_query.value("Abbreviation").toString();
             QString filename       = get_query.value("Filename").toString();

             dms ra1 = dms::fromString(RA1,false);
             dms dec1 = dms::fromString(DEC1,true);
             dms ra2 = dms::fromString(RA2,false);
             dms dec2 = dms::fromString(DEC2,true);

             // appends constellation info
             ConstellationsArt *ca = new ConstellationsArt (X1, Y1, ra1,dec1, X2,Y2,ra2,dec2,abbreviation,filename);
             m_ConstList.append(ca);

             //Make a QImage object pointing to constellation image
            m_ConstList[i]->loadImage();
            i++;
         }
        skydb.close();
}

void ConstellationArtComponent::showList()
{
    int i = 0;
    for(i = 0; i < m_ConstList.size(); i++)
    {
        qDebug()<<m_ConstList[i]->getAbbrev()<<m_ConstList[i]->getImageFileName();
        qDebug()<<m_ConstList[i]->getx1()<<m_ConstList[i]->gety1()<<m_ConstList[i]->gethd1();
        qDebug()<<m_ConstList[i]->getx2()<<m_ConstList[i]->gety2()<<m_ConstList[i]->gethd2();
    }
}

void ConstellationArtComponent::draw(SkyPainter *skyp){

    int i = 0;
    //(i=0; i < m_ConstList.size(); i++){
            drawConstArtImage( skyp, m_ConstList[i], true);
    //}
    //Loops through the QList containing all data required to draw western constellations.
    //There are 85 images, so m_ConstList.size() should return 85.
}


void ConstellationArtComponent::drawConstArtImage(SkyPainter *skyp, ConstellationsArt *obj, bool drawFlag)
{
    if(drawFlag==false) return;
    SkyMap *map = SkyMap::Instance();
    const Projector *proj = map->projector();
    skyp->setBrush( Qt::NoBrush );

    SkyPoint *s1 = new SkyPoint;
    SkyPoint *s2 = new SkyPoint;
    s1 = obj->star1;
    s2 = obj->star2;

    int w = obj->imageWidth();
    int h = obj->imageHeight();

    //UpdateID updateID = data->updateID();
    //if ( obj->updateID != updateID ) {
        //obj->updateID = updateID;

     if( (proj->checkVisibility(s1)==true) && (proj->checkVisibility(s2)==true) ){
    //Draw Image

    QPointF position1 = map->projector()->toScreen(s1);
    QPointF position2 = map->projector()->toScreen(s2);

        QPainter painter;
        painter.save();
        //How do I define position to translate?
        painter.translate(position1);
        painter.drawImage( QRect(-0.5*w, -0.5*h, w, h), obj->image() );
        painter.restore();
        painter.end();

   }
}
