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

#include "ConstellationArtComponent.h"
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

    // Find constellation art file and open it. If it doesn't exist, output an error message.
    KSFileReader fileReader;
    if ( ! fileReader.open("constellationsart.txt" ) )
    {
        qDebug() << "No constellationsart.txt file found for sky culture";
        return;
    }

    while ( fileReader.hasMoreLines() ) {
        QString line;

        line = fileReader.readLine();
        if( line.isEmpty() )
            continue;
        QChar mode = line.at( 0 );
        //ignore lines beginning with "#":
        if( mode == '#' )
            continue;

        //reads the first column from constellationart.txt
        m_ConstList[i]->rank = line.mid(0,2).trimmed().toInt();

        //Read pixel coordinates and HD number of star 1
        m_ConstList[i]->x1 = line.mid( 3, 3 ).trimmed().toInt();
        m_ConstList[i]->y1 = line.mid( 7, 3 ).trimmed().toInt();
        m_ConstList[i]->hd1 = line.mid( 11, 6 ).trimmed().toInt();

        //Read pixel coordinates and HD number of star 2
        m_ConstList[i]->x2 = line.mid( 18, 3 ).trimmed().toInt();
        m_ConstList[i]->y2 = line.mid( 22, 3 ).trimmed().toInt();
        m_ConstList[i]->hd2 = line.mid( 26, 6 ).trimmed().toInt();

        //Read pixel coordinates and HD number of star 3
        m_ConstList[i]->x3 = line.mid( 33, 3 ).trimmed().toInt();
        m_ConstList[i]->y3 = line.mid( 37, 3 ).trimmed().toInt();
        m_ConstList[i]->hd3 = line.mid( 41, 6 ).trimmed().toInt();

        //Read abbreviation and image file name
        m_ConstList[i]->abbrev = line.mid( 48, 3 );
        m_ConstList[i]->imageFileName  = line.mid( 52 ).trimmed();

        //Make a QImage object pointing to constellation image
        m_ConstList[i]->constart_image = QImage(m_ConstList[i]->imageFileName,0);
        i++;

        //qDebug()<<i;

        //qDebug()<< "Serial number:"<<rank<<"x1:"<<x1<<"y1:"<<y1<<"HD1:"<<hd1<<"x2:"<<x2<<"y2:"<<y2<<"HD2:"<<hd2<<"x3:"<<x3<<"y3:"<<y3<<"HD3:"<<hd3<<"abbreviation:"<<abbrev<<"name"<<imageFileName;
        //testing to see if the file opens and outputs the data
        }
}

void ConstellationArtComponent::showList()
{
    int i = 0;
    for(i = 0; i < m_ConstList.size(); i++)
    {
        qDebug()<<m_ConstList[i]->rank<<m_ConstList[i]->getAbbrev()<<m_ConstList[i]->getImageFileName();
        qDebug()<<m_ConstList[i]->getx1()<<m_ConstList[i]->gety1()<<m_ConstList[i]->gethd1();
        qDebug()<<m_ConstList[i]->getx2()<<m_ConstList[i]->gety2()<<m_ConstList[i]->gethd2();
        qDebug()<<m_ConstList[i]->getx3()<<m_ConstList[i]->gety3()<<m_ConstList[i]->gethd3();

    }
}

void ConstellationArtComponent::draw(SkyPainter *skyp){

    int i = 0;
    //Loops through the QList containing all data required to draw western constellations.
    //There are 85 images, so m_ConstList.size() should return 85.
    for(i=0; i < m_ConstList.size(); i++){
            drawConstArtImage( skyp, m_ConstList[i]);
    }
}


void ConstellationArtComponent::drawConstArtImage(SkyPainter *skyp, ConstellationsArt *obj, bool drawFlag)
{
    if(drawFlag==false) return;

    SkyMap *map = SkyMap::Instance();
    const Projector *proj = map->projector();

    KStarsData *data = KStarsData::Instance();
    UpdateID updateID = data->updateID();

    skyp->setBrush( Qt::NoBrush );

    if ( obj->updateID != updateID ) {
        obj->updateID = updateID;

        int w = obj->imageWidth();
        int h = obj->imageHeight();

        QPainter::save();
        //How do I define position to translate?
        QPainter::translate(pos);
        QPainter::drawImage( QRect(-0.5*w, -0.5*h, w, h), obj->image() );
        QPainter::restore();

    }
}
